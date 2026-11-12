#ifndef DONT_USE_STB
    #include <mod/thirdparty/stb_sprintf.h>
    #define sprintf stbsp_sprintf
    #define snprintf stbsp_snprintf
#endif

#include <mod/amlmod.h>
#include <mod/logger.h>
#include <iaudio_desc.h>

#include <vector>
#include <deque>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>

#ifndef AUDIO_MAX_VOICES
    #define AUDIO_MAX_VOICES                32
#endif
#ifndef AUDIO_STREAM_FRAMES
    #define AUDIO_STREAM_FRAMES             32768 // power of 2
#endif
#define AAUDIO_FORMAT_PCM_FLOAT             2
#define AAUDIO_PERFORMANCE_MODE_LOW_LATENCY 12
#define PCM_ENCODING_16BIT                  2
#define PCM_ENCODING_8BIT                   3
#define PCM_ENCODING_FLOAT                  4
#define PCM_ENCODING_24BIT                  21
#define PCM_ENCODING_32BIT                  22

extern char g_szDataDirPath[256];
extern jobject appContext;
JNIEnv* GetCurrentJNI();

enum eVoiceState
{
    VOICE_FREE = 0,
    VOICE_ACTIVE,
    VOICE_ENDED,
};
enum eEventType
{
    EVENT_END = 0,
    EVENT_PAUSED,
    EVENT_RESUMED,
};
struct AudioStream
{
    int fd = -1;
    AMediaExtractor* pExtractor = NULL;
    AMediaCodec* pCodec = NULL;
    int32_t nRate = 0, nChannels = 0, encoding = 0;
    bool bInputDone = false, bDecoderEOS = false;
    std::vector<int16_t> pending;
    size_t pendingPos = 0;
    std::vector<int16_t> ring = std::vector<int16_t>(AUDIO_STREAM_FRAMES * 2);
    std::atomic<size_t> readPos{0}, writePos{0};
    std::atomic<int32_t> nMixRate{0};
    std::atomic<bool> bActive{false}, bLoop{false}, bRestart{false}, bEOS{false};
};
struct AudioSound
{
    std::vector<int16_t> samples;
    int32_t nRate = 0, nChannels = 0, nFrames = 0;
    bool bReady = false, bFailed = false;
    std::atomic<bool> bUnloading{false};
    std::string path;
    std::vector<uint8_t> dataCopy;
    const uint8_t* pData = NULL;
    size_t dataSize = 0;
    AudioLoadedFn fnLoaded = NULL;
    AudioUnloadedFn fnUnloaded = NULL;
    AudioVoiceEndFn fnVoiceEnd = NULL;
    AudioVoicePausedFn fnVoicePaused = NULL;
    AudioVoiceResumedFn fnVoiceResumed = NULL;
    void *pLoadedData = NULL, *pUnloadedData = NULL, *pVoiceEndData = NULL, *pVoicePausedData = NULL, *pVoiceResumedData = NULL;
    AudioStream* pStream = NULL;
};
struct AudioVoice
{
    AudioSound* pSound;
    double fPos;
    float fVolume;
    bool bLoop, bPaused;
    int nState;
    int64_t nStopIn, nPauseIn;
};
struct AudioEvent
{
    AudioVoice* pVoice;
    int nType, nReason;
    void* fn;
    void* pUserData;
};
struct AudioJob
{
    AudioSound* pSound;
    bool bUnload;
};

static std::mutex g_audioMutex, g_jobMutex;
static std::condition_variable g_eventCond, g_jobCond;
static std::deque<AudioJob> g_jobs;
static std::once_flag g_audioInitFlag;
static std::atomic<bool> g_bAudioRestart{false}, g_bAudioEvents{false};
static AudioVoice g_voices[AUDIO_MAX_VOICES];
static AudioEvent g_events[AUDIO_MAX_VOICES * 4];
static int g_nEvents = 0;
struct UnloadEvent
{
    AudioSound* pSound;
    AudioUnloadedFn fn;
    void* pUserData;
};
static std::vector<UnloadEvent> g_unloadEvents;
static float g_fGlobalVolume = 1.0f;
static int32_t g_nOutRate = 48000;

static void PushEvent(AudioVoice& v, int nType, int nReason)
{
    AudioSound* snd = v.pSound;
    void* fn = (void*)((nType == EVENT_END) ? (void*)snd->fnVoiceEnd : (nType == EVENT_PAUSED) ? (void*)snd->fnVoicePaused : (void*)snd->fnVoiceResumed);
    void* pUserData = (nType == EVENT_END) ? snd->pVoiceEndData : (nType == EVENT_PAUSED) ? snd->pVoicePausedData : snd->pVoiceResumedData;

    // END is always queued (frees the slot), space for it is reserved
    if(nType != EVENT_END && (!fn || g_nEvents >= AUDIO_MAX_VOICES * 3)) return;
    g_events[g_nEvents++] = { &v, nType, nReason, fn, pUserData };
    g_bAudioEvents = true;
}
static void EndVoice(AudioVoice& v, int nReason)
{
    PushEvent(v, EVENT_END, nReason);
    if(v.pSound->pStream) v.pSound->pStream->bActive = false;
    v.nState = VOICE_ENDED;
    v.pSound = NULL;
}
static void SetPaused(AudioVoice& v, bool bPaused)
{
    v.bPaused = bPaused;
    PushEvent(v, bPaused ? EVENT_PAUSED : EVENT_RESUMED, 0);
}

/* Mixer */

static bool MixStream(AudioVoice& v, AudioStream* st, float* out, int32_t frames, float vol)
{
    if(st->bRestart) return false;

    bool bEOS = st->bEOS.load();
    size_t r = st->readPos.load(), avail = st->writePos.load() - r;
    double step = (double)st->nMixRate / g_nOutRate;
    bool bEnded = false;
    for(int32_t i = 0; i < frames; ++i)
    {
        size_t idx = (size_t)v.fPos;
        if(idx + 1 >= avail)
        {
            if(!bEOS) break;
            if(idx >= avail)
            {
                EndVoice(v, AUDIO_END_FINISHED);
                bEnded = true;
                break;
            }
        }
        size_t next = (idx + 1 < avail) ? idx + 1 : idx;
        const int16_t* a = &st->ring[((r + idx) & (AUDIO_STREAM_FRAMES - 1)) * 2];
        const int16_t* b = &st->ring[((r + next) & (AUDIO_STREAM_FRAMES - 1)) * 2];
        float frac = (float)(v.fPos - idx);
        out[i * 2]     += (a[0] + (b[0] - a[0]) * frac) * vol;
        out[i * 2 + 1] += (a[1] + (b[1] - a[1]) * frac) * vol;
        v.fPos += step;
    }
    size_t consumed = (size_t)v.fPos;
    v.fPos -= consumed;
    st->readPos = r + consumed;
    return bEnded;
}
static bool MixSound(AudioVoice& v, AudioSound* snd, float* out, int32_t frames, float vol)
{
    int32_t ch = snd->nChannels;
    double step = (double)snd->nRate / g_nOutRate;
    for(int32_t i = 0; i < frames; ++i)
    {
        if(v.fPos >= snd->nFrames)
        {
            if(!v.bLoop)
            {
                EndVoice(v, AUDIO_END_FINISHED);
                return true;
            }
            v.fPos = fmod(v.fPos, snd->nFrames);
        }
        int32_t idx = (int32_t)v.fPos;
        int32_t next = (idx + 1 < snd->nFrames) ? idx + 1 : (v.bLoop ? 0 : idx);
        float frac = (float)(v.fPos - idx);
        const int16_t* a = &snd->samples[idx * ch];
        const int16_t* b = &snd->samples[next * ch];
        float l = a[0] + (b[0] - a[0]) * frac;
        float r = (ch > 1) ? a[1] + (b[1] - a[1]) * frac : l;
        out[i * 2]     += l * vol;
        out[i * 2 + 1] += r * vol;
        v.fPos += step;
    }
    return false;
}
static void MixVoices(float* out, int32_t frames)
{
    memset(out, 0, frames * 2 * sizeof(float));
    std::unique_lock<std::mutex> lock(g_audioMutex, std::try_to_lock);
    if(!lock.owns_lock()) return;

    bool bEvents = false;
    for(AudioVoice& v : g_voices)
    {
        AudioSound* snd = v.pSound;
        if(v.nState != VOICE_ACTIVE || v.bPaused || !snd->bReady) continue;

        int32_t n = frames;
        if(v.nStopIn >= 0 && v.nStopIn < n) n = (int32_t)v.nStopIn;
        if(v.nPauseIn >= 0 && v.nPauseIn < n) n = (int32_t)v.nPauseIn;
        float vol = v.fVolume * g_fGlobalVolume / 32768.0f;
        if(snd->pStream ? MixStream(v, snd->pStream, out, n, vol) : MixSound(v, snd, out, n, vol))
        {
            bEvents = true;
            continue;
        }

        if(v.nStopIn >= 0) v.nStopIn -= n;
        if(v.nPauseIn >= 0) v.nPauseIn -= n;
        if(v.nStopIn == 0)
        {
            EndVoice(v, AUDIO_END_STOPPED);
            bEvents = true;
        }
        else if(v.nPauseIn == 0)
        {
            v.nPauseIn = -1;
            SetPaused(v, true);
            bEvents = true;
        }
    }
    lock.unlock();
    if(bEvents) g_eventCond.notify_one();
    for(int32_t i = 0; i < frames * 2; ++i) out[i] = fminf(fmaxf(out[i], -1.0f), 1.0f);
}

/* AAudio */

struct AAudioStream;
struct AAudioStreamBuilder;
typedef int32_t (*AAudioDataCallback)(AAudioStream*, void*, void*, int32_t);
typedef void (*AAudioErrorCallback)(AAudioStream*, void*, int32_t);

static struct
{
    int32_t (*createStreamBuilder)(AAudioStreamBuilder**);
    void    (*setPerformanceMode)(AAudioStreamBuilder*, int32_t);
    void    (*setFormat)(AAudioStreamBuilder*, int32_t);
    void    (*setChannelCount)(AAudioStreamBuilder*, int32_t);
    void    (*setDataCallback)(AAudioStreamBuilder*, AAudioDataCallback, void*);
    void    (*setErrorCallback)(AAudioStreamBuilder*, AAudioErrorCallback, void*);
    int32_t (*openStream)(AAudioStreamBuilder*, AAudioStream**);
    int32_t (*deleteBuilder)(AAudioStreamBuilder*);
    int32_t (*requestStart)(AAudioStream*);
    int32_t (*requestPause)(AAudioStream*);
    int32_t (*close)(AAudioStream*);
    int32_t (*getSampleRate)(AAudioStream*);
    int32_t (*getFormat)(AAudioStream*);
    int32_t (*getFramesPerBurst)(AAudioStream*);
    int32_t (*setBufferSizeInFrames)(AAudioStream*, int32_t);
} aa;
static AAudioStream* g_pAAStream = NULL;

static bool LoadAAudio()
{
    void* lib = dlopen("libaaudio.so", RTLD_NOW);
    if(!lib) return false;

    #define AA_SYM(_field, _name) if(!(*(void**)&aa._field = dlsym(lib, _name))) return false
    AA_SYM(createStreamBuilder,   "AAudio_createStreamBuilder");
    AA_SYM(setPerformanceMode,    "AAudioStreamBuilder_setPerformanceMode");
    AA_SYM(setFormat,             "AAudioStreamBuilder_setFormat");
    AA_SYM(setChannelCount,       "AAudioStreamBuilder_setChannelCount");
    AA_SYM(setDataCallback,       "AAudioStreamBuilder_setDataCallback");
    AA_SYM(setErrorCallback,      "AAudioStreamBuilder_setErrorCallback");
    AA_SYM(openStream,            "AAudioStreamBuilder_openStream");
    AA_SYM(deleteBuilder,         "AAudioStreamBuilder_delete");
    AA_SYM(requestStart,          "AAudioStream_requestStart");
    AA_SYM(requestPause,          "AAudioStream_requestPause");
    AA_SYM(close,                 "AAudioStream_close");
    AA_SYM(getSampleRate,         "AAudioStream_getSampleRate");
    AA_SYM(getFormat,             "AAudioStream_getFormat");
    AA_SYM(getFramesPerBurst,     "AAudioStream_getFramesPerBurst");
    AA_SYM(setBufferSizeInFrames, "AAudioStream_setBufferSizeInFrames");
    #undef AA_SYM
    return true;
}
static int32_t AAudioDataCB(AAudioStream*, void*, void* data, int32_t frames)
{
    MixVoices((float*)data, frames);
    return 0;
}
static void AAudioErrorCB(AAudioStream*, void*, int32_t)
{
    g_bAudioRestart = true;
}
static bool OpenAAudio()
{
    AAudioStreamBuilder* builder;
    if(aa.createStreamBuilder(&builder) != 0) return false;

    aa.setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    aa.setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    aa.setChannelCount(builder, 2);
    aa.setDataCallback(builder, AAudioDataCB, NULL);
    aa.setErrorCallback(builder, AAudioErrorCB, NULL);
    int32_t res = aa.openStream(builder, &g_pAAStream);
    aa.deleteBuilder(builder);
    if(res != 0)
    {
        g_pAAStream = NULL;
        return false;
    }

    if(aa.getFormat(g_pAAStream) == AAUDIO_FORMAT_PCM_FLOAT)
    {
        aa.setBufferSizeInFrames(g_pAAStream, aa.getFramesPerBurst(g_pAAStream) * 2);
        int32_t rate = aa.getSampleRate(g_pAAStream);
        if(rate > 0) g_nOutRate = rate;
        if(aa.requestStart(g_pAAStream) == 0) return true;
    }
    aa.close(g_pAAStream);
    g_pAAStream = NULL;
    return false;
}

/* OpenSL ES */

static SLObjectItf g_slEngineObj = NULL, g_slMixObj = NULL, g_slPlayerObj = NULL;
static SLPlayItf g_slPlay = NULL;
static std::vector<float> g_slMix;
static std::vector<int16_t> g_slBuffer;
static int32_t g_nSLFrames = 256, g_nSLBufferIdx = 0;

static int GetAudioProperty(JNIEnv* env, jobject audioManager, const char* szKey, int nDefault)
{
    jclass cls = env->GetObjectClass(audioManager);
    jmethodID getProperty = env->GetMethodID(cls, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring key = env->NewStringUTF(szKey);
    jstring val = (jstring)env->CallObjectMethod(audioManager, getProperty, key);
    int ret = 0;
    if(val)
    {
        const char* sz = env->GetStringUTFChars(val, NULL);
        ret = atoi(sz);
        env->ReleaseStringUTFChars(val, sz);
        env->DeleteLocalRef(val);
    }
    env->DeleteLocalRef(key);
    env->DeleteLocalRef(cls);
    return (ret > 0) ? ret : nDefault;
}
static void QueryNativeAudioParams()
{
    JNIEnv* env = GetCurrentJNI();
    if(!env || !appContext) return;

    jclass ctxCls = env->GetObjectClass(appContext);
    jmethodID getService = env->GetMethodID(ctxCls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring name = env->NewStringUTF("audio");
    jobject audioManager = env->CallObjectMethod(appContext, getService, name);
    if(audioManager)
    {
        g_nOutRate = GetAudioProperty(env, audioManager, "android.media.property.OUTPUT_SAMPLE_RATE", g_nOutRate);
        g_nSLFrames = GetAudioProperty(env, audioManager, "android.media.property.OUTPUT_FRAMES_PER_BUFFER", g_nSLFrames);
        env->DeleteLocalRef(audioManager);
    }
    if(env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(ctxCls);
}
static void SLESCallback(SLAndroidSimpleBufferQueueItf queue, void*)
{
    MixVoices(g_slMix.data(), g_nSLFrames);
    int16_t* buf = &g_slBuffer[g_nSLBufferIdx * g_nSLFrames * 2];
    for(int32_t i = 0; i < g_nSLFrames * 2; ++i) buf[i] = (int16_t)(g_slMix[i] * 32767.0f);
    (*queue)->Enqueue(queue, buf, g_nSLFrames * 2 * sizeof(int16_t));
    g_nSLBufferIdx ^= 1;
}
static void CloseSLES()
{
    if(g_slPlayerObj) (*g_slPlayerObj)->Destroy(g_slPlayerObj);
    if(g_slMixObj) (*g_slMixObj)->Destroy(g_slMixObj);
    if(g_slEngineObj) (*g_slEngineObj)->Destroy(g_slEngineObj);
    g_slPlayerObj = g_slMixObj = g_slEngineObj = NULL;
    g_slPlay = NULL;
}
static bool OpenSLES()
{
    QueryNativeAudioParams();
    g_slMix.resize(g_nSLFrames * 2);
    g_slBuffer.resize(g_nSLFrames * 2 * 2);

    SLEngineItf engine;
    SLAndroidSimpleBufferQueueItf queue;
    SLDataLocator_AndroidSimpleBufferQueue queueLoc = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
    SLDataFormat_PCM format = { SL_DATAFORMAT_PCM, 2, (SLuint32)g_nOutRate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN };
    SLDataSource source = { &queueLoc, &format };
    SLDataLocator_OutputMix mixLoc = { SL_DATALOCATOR_OUTPUTMIX, NULL };
    SLDataSink sink = { &mixLoc, NULL };
    const SLInterfaceID ids[] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
    const SLboolean req[] = { SL_BOOLEAN_TRUE };

    #define SL_CHECK(_x) if((_x) != SL_RESULT_SUCCESS) { CloseSLES(); return false; }
    SL_CHECK(slCreateEngine(&g_slEngineObj, 0, NULL, 0, NULL, NULL));
    SL_CHECK((*g_slEngineObj)->Realize(g_slEngineObj, SL_BOOLEAN_FALSE));
    SL_CHECK((*g_slEngineObj)->GetInterface(g_slEngineObj, SL_IID_ENGINE, &engine));
    SL_CHECK((*engine)->CreateOutputMix(engine, &g_slMixObj, 0, NULL, NULL));
    SL_CHECK((*g_slMixObj)->Realize(g_slMixObj, SL_BOOLEAN_FALSE));
    mixLoc.outputMix = g_slMixObj;
    SL_CHECK((*engine)->CreateAudioPlayer(engine, &g_slPlayerObj, &source, &sink, 1, ids, req));
    SL_CHECK((*g_slPlayerObj)->Realize(g_slPlayerObj, SL_BOOLEAN_FALSE));
    SL_CHECK((*g_slPlayerObj)->GetInterface(g_slPlayerObj, SL_IID_PLAY, &g_slPlay));
    SL_CHECK((*g_slPlayerObj)->GetInterface(g_slPlayerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &queue));
    SL_CHECK((*queue)->RegisterCallback(queue, SLESCallback, NULL));
    SLESCallback(queue, NULL);
    SLESCallback(queue, NULL);
    SL_CHECK((*g_slPlay)->SetPlayState(g_slPlay, SL_PLAYSTATE_PLAYING));
    #undef SL_CHECK
    return true;
}

/* Output control */

static void SetOutputPaused(bool bPause)
{
    if(g_pAAStream) bPause ? aa.requestPause(g_pAAStream) : aa.requestStart(g_pAAStream);
    else if(g_slPlay) (*g_slPlay)->SetPlayState(g_slPlay, bPause ? SL_PLAYSTATE_PAUSED : SL_PLAYSTATE_PLAYING);
}
static bool IsAppInForeground()
{
    static jclass amCls = NULL, infoCls = NULL;
    static jmethodID getMemoryState, infoCtor;
    static jfieldID importanceField;

    JNIEnv* env = GetCurrentJNI();
    if(!env) return true;
    if(!amCls)
    {
        jclass cls = env->FindClass("android/app/ActivityManager");
        amCls = (jclass)env->NewGlobalRef(cls);
        env->DeleteLocalRef(cls);
        cls = env->FindClass("android/app/ActivityManager$RunningAppProcessInfo");
        infoCls = (jclass)env->NewGlobalRef(cls);
        env->DeleteLocalRef(cls);
        getMemoryState = env->GetStaticMethodID(amCls, "getMyMemoryState", "(Landroid/app/ActivityManager$RunningAppProcessInfo;)V");
        infoCtor = env->GetMethodID(infoCls, "<init>", "()V");
        importanceField = env->GetFieldID(infoCls, "importance", "I");
    }

    jobject info = env->NewObject(infoCls, infoCtor);
    env->CallStaticVoidMethod(amCls, getMemoryState, info);
    int importance = env->GetIntField(info, importanceField);
    env->DeleteLocalRef(info);
    if(env->ExceptionCheck())
    {
        env->ExceptionClear();
        return true;
    }
    return importance <= 100; // IMPORTANCE_FOREGROUND
}
/* Decoding */

struct AMediaDataSource;
struct MemSource
{
    const uint8_t* pData;
    size_t size;
};
static struct
{
    AMediaDataSource* (*create)();
    void (*setUserdata)(AMediaDataSource*, void*);
    void (*setReadAt)(AMediaDataSource*, ssize_t (*)(void*, off64_t, void*, size_t));
    void (*setGetSize)(AMediaDataSource*, ssize_t (*)(void*));
    void (*destroy)(AMediaDataSource*);
    media_status_t (*setDataSourceCustom)(AMediaExtractor*, AMediaDataSource*);
} ds;

static void LoadDataSourceAPI()
{
    void* lib = dlopen("libmediandk.so", RTLD_NOW);
    if(!lib) return;

    *(void**)&ds.create              = dlsym(lib, "AMediaDataSource_new");
    *(void**)&ds.setUserdata         = dlsym(lib, "AMediaDataSource_setUserdata");
    *(void**)&ds.setReadAt           = dlsym(lib, "AMediaDataSource_setReadAt");
    *(void**)&ds.setGetSize          = dlsym(lib, "AMediaDataSource_setGetSize");
    *(void**)&ds.destroy             = dlsym(lib, "AMediaDataSource_delete");
    *(void**)&ds.setDataSourceCustom = dlsym(lib, "AMediaExtractor_setDataSourceCustom");
    if(!ds.setUserdata || !ds.setReadAt || !ds.setGetSize || !ds.destroy || !ds.setDataSourceCustom) ds.create = NULL;
}
static ssize_t MemReadAt(void* userdata, off64_t offset, void* buffer, size_t size)
{
    MemSource* src = (MemSource*)userdata;
    if(offset < 0) return -1;
    if((size_t)offset >= src->size) return 0;

    size_t n = (size < src->size - offset) ? size : src->size - offset;
    memcpy(buffer, src->pData + offset, n);
    return n;
}
static ssize_t MemGetSize(void* userdata)
{
    return ((MemSource*)userdata)->size;
}
static void AppendPCM(std::vector<int16_t>& out, const uint8_t* data, int32_t size, int32_t encoding)
{
    switch(encoding)
    {
        case PCM_ENCODING_8BIT:
            for(int32_t i = 0; i < size; ++i) out.push_back((int16_t)((data[i] - 128) << 8));
            break;
        case PCM_ENCODING_FLOAT:
            for(int32_t i = 0; i < size / 4; ++i) out.push_back((int16_t)(fminf(fmaxf(((const float*)data)[i], -1.0f), 1.0f) * 32767.0f));
            break;
        case PCM_ENCODING_24BIT:
            for(int32_t i = 0; i + 2 < size; i += 3) out.push_back((int16_t)(data[i + 1] | (data[i + 2] << 8)));
            break;
        case PCM_ENCODING_32BIT:
            for(int32_t i = 0; i < size / 4; ++i) out.push_back((int16_t)(((const int32_t*)data)[i] >> 16));
            break;
        default:
            out.insert(out.end(), (const int16_t*)data, (const int16_t*)data + size / 2);
            break;
    }
}
static bool OpenTrack(AMediaExtractor* ex, AMediaCodec** pCodec, int32_t& rate, int32_t& channels, int32_t& encoding, int64_t& duration)
{
    AMediaFormat* format = NULL;
    const char* mime = NULL;
    size_t count = AMediaExtractor_getTrackCount(ex);
    for(size_t i = 0; i < count; ++i)
    {
        format = AMediaExtractor_getTrackFormat(ex, i);
        if(AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime) && !strncmp(mime, "audio/", 6))
        {
            AMediaExtractor_selectTrack(ex, i);
            break;
        }
        AMediaFormat_delete(format);
        format = NULL;
    }
    if(!format) return false;

    encoding = PCM_ENCODING_16BIT;
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &rate);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
    AMediaFormat_getInt32(format, "pcm-encoding", &encoding);
    AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &duration);
    *pCodec = NULL;
    if(!strcmp(mime, "audio/raw"))
    {
        AMediaFormat_delete(format);
        return true;
    }

    AMediaCodec* codec = AMediaCodec_createDecoderByType(mime);
    bool bStarted = codec && AMediaCodec_configure(codec, format, NULL, NULL, 0) == AMEDIA_OK && AMediaCodec_start(codec) == AMEDIA_OK;
    AMediaFormat_delete(format);
    if(!bStarted)
    {
        if(codec) AMediaCodec_delete(codec);
        return false;
    }
    *pCodec = codec;
    encoding = PCM_ENCODING_16BIT;
    return true;
}
static void CloseCodec(AMediaCodec* codec)
{
    if(!codec) return;
    AMediaCodec_stop(codec);
    AMediaCodec_delete(codec);
}

enum eDecodeResult
{
    DECODE_AGAIN = 0,
    DECODE_DATA,
    DECODE_EOS,
};
static int DecodeStep(AMediaExtractor* ex, AMediaCodec* codec, bool& bInputDone, int32_t& rate, int32_t& channels, int32_t& encoding, std::vector<int16_t>& out, int64_t timeoutUs)
{
    if(!codec)
    {
        thread_local std::vector<uint8_t> buf(256 * 1024);
        ssize_t n = AMediaExtractor_readSampleData(ex, buf.data(), buf.size());
        if(n < 0) return DECODE_EOS;
        AppendPCM(out, buf.data(), n, encoding);
        return AMediaExtractor_advance(ex) ? DECODE_DATA : DECODE_EOS;
    }

    ssize_t idx;
    while(!bInputDone && (idx = AMediaCodec_dequeueInputBuffer(codec, 0)) >= 0)
    {
        size_t bufSize;
        uint8_t* buf = AMediaCodec_getInputBuffer(codec, idx, &bufSize);
        ssize_t n = AMediaExtractor_readSampleData(ex, buf, bufSize);
        if(n < 0)
        {
            AMediaCodec_queueInputBuffer(codec, idx, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
            bInputDone = true;
        }
        else
        {
            AMediaCodec_queueInputBuffer(codec, idx, 0, n, AMediaExtractor_getSampleTime(ex), 0);
            AMediaExtractor_advance(ex);
        }
    }

    AMediaCodecBufferInfo info;
    idx = AMediaCodec_dequeueOutputBuffer(codec, &info, timeoutUs);
    if(idx >= 0)
    {
        uint8_t* buf = AMediaCodec_getOutputBuffer(codec, idx, NULL);
        if(buf && info.size > 0) AppendPCM(out, buf + info.offset, info.size, encoding);
        AMediaCodec_releaseOutputBuffer(codec, idx, false);
        return (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) ? DECODE_EOS : DECODE_DATA;
    }
    if(idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
    {
        AMediaFormat* format = AMediaCodec_getOutputFormat(codec);
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &rate);
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
        AMediaFormat_getInt32(format, "pcm-encoding", &encoding);
        AMediaFormat_delete(format);
        return DECODE_DATA;
    }
    return DECODE_AGAIN;
}
static bool DecodeTrack(AMediaExtractor* ex, AudioSound* snd)
{
    AMediaCodec* codec;
    int32_t encoding;
    int64_t duration = 0;
    if(!OpenTrack(ex, &codec, snd->nRate, snd->nChannels, encoding, duration)) return false;
    if(duration > 0) snd->samples.reserve((size_t)(duration * snd->nRate / 1000000 + 1) * snd->nChannels);

    bool bInputDone = false;
    int res, tries = 0;
    while((res = DecodeStep(ex, codec, bInputDone, snd->nRate, snd->nChannels, encoding, snd->samples, 10000)) != DECODE_EOS && tries < 100)
    {
        tries = (res == DECODE_AGAIN) ? tries + 1 : 0;
    }
    CloseCodec(codec);
    return res == DECODE_EOS;
}
static bool DecodeExtractor(AMediaExtractor* ex, bool bSourceOk, AudioSound* snd)
{
    bool bOk = bSourceOk && DecodeTrack(ex, snd);
    AMediaExtractor_delete(ex);
    if(!bOk || snd->nRate <= 0 || snd->nChannels <= 0 || (int32_t)snd->samples.size() < snd->nChannels) return false;

    snd->nFrames = snd->samples.size() / snd->nChannels;
    return true;
}
static bool DecodeFromFd(int fd, off64_t length, AudioSound* snd)
{
    AMediaExtractor* ex = AMediaExtractor_new();
    bool bOk = DecodeExtractor(ex, AMediaExtractor_setDataSourceFd(ex, fd, 0, length) == AMEDIA_OK, snd);
    close(fd);
    return bOk;
}
static bool DecodeFromMemory(AudioSound* snd)
{
    MemSource src = { snd->pData, snd->dataSize };
    AMediaDataSource* source = ds.create();
    ds.setUserdata(source, &src);
    ds.setReadAt(source, MemReadAt);
    ds.setGetSize(source, MemGetSize);

    AMediaExtractor* ex = AMediaExtractor_new();
    bool bOk = DecodeExtractor(ex, ds.setDataSourceCustom(ex, source) == AMEDIA_OK, snd);
    ds.destroy(source);
    return bOk;
}
static bool DecodeFromTempFile(AudioSound* snd)
{
    static std::atomic<int> nTmpCounter{0};
    char path[384];
    snprintf(path, sizeof(path), "%s/aml_audio_%d.tmp", g_szDataDirPath, nTmpCounter++);
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if(fd < 0) return false;
    unlink(path);

    if(write(fd, snd->pData, snd->dataSize) != (ssize_t)snd->dataSize)
    {
        close(fd);
        return false;
    }
    return DecodeFromFd(fd, snd->dataSize, snd);
}
static bool DecodeSound(AudioSound* snd)
{
    if(!snd->path.empty())
    {
        int fd = open(snd->path.c_str(), O_RDONLY);
        return fd >= 0 && DecodeFromFd(fd, lseek64(fd, 0, SEEK_END), snd);
    }
    return ds.create ? DecodeFromMemory(snd) : DecodeFromTempFile(snd);
}

/* Streaming */

static std::vector<AudioSound*> g_streams;
static std::mutex g_streamMutex;
static std::condition_variable g_streamCond;
static std::atomic<bool> g_bStreamWake{false};
static std::once_flag g_streamInitFlag;

static bool OpenStream(AudioSound* snd)
{
    AudioStream* st = snd->pStream;
    st->fd = open(snd->path.c_str(), O_RDONLY);
    if(st->fd < 0) return false;

    int64_t duration = 0;
    st->pExtractor = AMediaExtractor_new();
    if(AMediaExtractor_setDataSourceFd(st->pExtractor, st->fd, 0, lseek64(st->fd, 0, SEEK_END)) != AMEDIA_OK ||
       !OpenTrack(st->pExtractor, &st->pCodec, st->nRate, st->nChannels, st->encoding, duration)) return false;
    st->nMixRate = st->nRate;
    return st->nRate > 0 && st->nChannels > 0;
}
static void CloseStream(AudioStream* st)
{
    CloseCodec(st->pCodec);
    if(st->pExtractor) AMediaExtractor_delete(st->pExtractor);
    if(st->fd >= 0) close(st->fd);
    delete st;
}
static void RewindStream(AudioStream* st)
{
    AMediaExtractor_seekTo(st->pExtractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
    if(st->pCodec) AMediaCodec_flush(st->pCodec);
    st->bInputDone = st->bDecoderEOS = false;
    st->pending.clear();
    st->pendingPos = 0;
}
static void FillStream(AudioStream* st)
{
    if(st->bRestart)
    {
        RewindStream(st);
        st->readPos = 0;
        st->writePos = 0;
        st->bEOS = false;
        st->bRestart = false;
    }
    if(!st->bActive || st->bEOS) return;

    while(true)
    {
        size_t w = st->writePos.load(), space = AUDIO_STREAM_FRAMES - (w - st->readPos.load());
        int32_t ch = st->nChannels;
        for(; st->pendingPos + ch <= st->pending.size() && space > 0; st->pendingPos += ch, ++w, --space)
        {
            int16_t* dst = &st->ring[(w & (AUDIO_STREAM_FRAMES - 1)) * 2];
            dst[0] = st->pending[st->pendingPos];
            dst[1] = st->pending[st->pendingPos + (ch > 1 ? 1 : 0)];
        }
        st->nMixRate = st->nRate;
        st->writePos = w;
        if(st->pendingPos + ch <= st->pending.size()) return;
        st->pending.clear();
        st->pendingPos = 0;

        if(st->bDecoderEOS)
        {
            if(!st->bLoop)
            {
                st->bEOS = true;
                return;
            }
            RewindStream(st);
        }
        int res = DecodeStep(st->pExtractor, st->pCodec, st->bInputDone, st->nRate, st->nChannels, st->encoding, st->pending, 2000);
        if(res == DECODE_EOS) st->bDecoderEOS = true;
        else if(res == DECODE_AGAIN) return;
    }
}
static void* AudioStreamThread(void*)
{
    bool bAnyActive = false;
    while(true)
    {
        std::unique_lock<std::mutex> lock(g_streamMutex);
        auto wake = []{ return g_bStreamWake.load(); };
        if(bAnyActive) g_streamCond.wait_for(lock, std::chrono::milliseconds(10), wake);
        else g_streamCond.wait(lock, wake);
        g_bStreamWake = false;

        bAnyActive = false;
        for(AudioSound* snd : g_streams)
        {
            FillStream(snd->pStream);
            bAnyActive |= snd->pStream->bActive;
        }
    }
    return NULL;
}
static void StartStreamThread()
{
    pthread_t thread;
    if(pthread_create(&thread, NULL, AudioStreamThread, NULL) == 0) pthread_detach(thread);
    else logger->Error("Failed to start audio stream thread!");
}
static void WakeStreamThread()
{
    g_bStreamWake = true;
    g_streamCond.notify_one();
}

/* Threads */

static void* AudioEventThread(void*)
{
    std::vector<AudioEvent> events;
    std::vector<UnloadEvent> unloads;
    events.reserve(AUDIO_MAX_VOICES * 4);
    auto lastCheck = std::chrono::steady_clock::now();
    bool bPaused = false;
    while(true)
    {
        {
            std::unique_lock<std::mutex> lock(g_audioMutex);
            g_eventCond.wait_for(lock, std::chrono::milliseconds(250), []{ return g_bAudioEvents.load(); });
            g_bAudioEvents = false;
            events.assign(g_events, g_events + g_nEvents);
            g_nEvents = 0;
            unloads.swap(g_unloadEvents);
        }
        for(AudioEvent& e : events)
        {
            if(!e.fn) continue;
            if(e.nType == EVENT_END) ((AudioVoiceEndFn)e.fn)(e.pVoice, (eAudioEndReason)e.nReason, e.pUserData);
            else ((AudioVoicePausedFn)e.fn)(e.pVoice, e.pUserData);
        }
        if(!events.empty())
        {
            std::lock_guard<std::mutex> lock(g_audioMutex);
            for(AudioEvent& e : events)
            {
                if(e.nType == EVENT_END) e.pVoice->nState = VOICE_FREE;
            }
        }
        events.clear();
        for(UnloadEvent& e : unloads) e.fn(e.pSound, e.pUserData);
        unloads.clear();

        auto now = std::chrono::steady_clock::now();
        if(now - lastCheck < std::chrono::milliseconds(250)) continue;
        lastCheck = now;

        if(g_bAudioRestart.exchange(false) && g_pAAStream)
        {
            aa.close(g_pAAStream);
            if(!OpenAAudio()) logger->Error("Failed to reopen AAudio stream!");
            bPaused = false;
        }
        bool bForeground = IsAppInForeground();
        if(bForeground == bPaused)
        {
            bPaused = !bForeground;
            SetOutputPaused(bPaused);
        }
    }
    return NULL;
}
static void* AudioLoaderThread(void*)
{
    if(LoadAAudio() && OpenAAudio()) logger->Info("Audio output: AAudio, %d Hz", g_nOutRate);
    else if(OpenSLES()) logger->Info("Audio output: OpenSL ES, %d Hz, %d frames", g_nOutRate, g_nSLFrames);
    else logger->Error("Failed to initialize audio output!");
    LoadDataSourceAPI();

    pthread_t thread;
    if(pthread_create(&thread, NULL, AudioEventThread, NULL) == 0) pthread_detach(thread);

    while(true)
    {
        AudioJob job;
        {
            std::unique_lock<std::mutex> lock(g_jobMutex);
            g_jobCond.wait(lock, []{ return !g_jobs.empty(); });
            job = g_jobs.front();
            g_jobs.pop_front();
        }

        AudioSound* snd = job.pSound;
        if(job.bUnload)
        {
            if(snd->pStream)
            {
                std::lock_guard<std::mutex> lock(g_streamMutex);
                for(size_t i = 0; i < g_streams.size(); ++i)
                {
                    if(g_streams[i] == snd) g_streams.erase(g_streams.begin() + i);
                }
                CloseStream(snd->pStream);
            }
            {
                std::lock_guard<std::mutex> lock(g_audioMutex);
                if(snd->fnUnloaded) g_unloadEvents.push_back({ snd, snd->fnUnloaded, snd->pUnloadedData });
                g_bAudioEvents = true;
            }
            delete snd;
            g_eventCond.notify_one();
            continue;
        }

        bool bOk = !snd->bUnloading && (snd->pStream ? OpenStream(snd) : DecodeSound(snd));
        if(!bOk && !snd->bUnloading) logger->Error("Failed to load sound %s", snd->path.empty() ? "from memory" : snd->path.c_str());
        std::string().swap(snd->path);
        std::vector<uint8_t>().swap(snd->dataCopy);
        snd->pData = NULL;

        AudioLoadedFn fnLoaded;
        void* pUserData;
        {
            std::lock_guard<std::mutex> lock(g_audioMutex);
            snd->bReady = bOk;
            snd->bFailed = !bOk;
            fnLoaded = snd->fnLoaded;
            pUserData = snd->pLoadedData;
            for(AudioVoice& v : g_voices)
            {
                if(!bOk && v.nState == VOICE_ACTIVE && v.pSound == snd) EndVoice(v, AUDIO_END_LOAD_FAILED);
            }
        }
        g_eventCond.notify_one();
        if(bOk && snd->pStream)
        {
            std::call_once(g_streamInitFlag, StartStreamThread);
            {
                std::lock_guard<std::mutex> lock(g_streamMutex);
                g_streams.push_back(snd);
            }
            WakeStreamThread();
        }
        if(fnLoaded) fnLoaded(snd, bOk, pUserData);
    }
    return NULL;
}
static void StartAudioThreads()
{
    pthread_t thread;
    if(pthread_create(&thread, NULL, AudioLoaderThread, NULL) == 0) pthread_detach(thread);
    else logger->Error("Failed to start audio thread!");
}
static AudioSoundHandle QueueLoad(AudioSound* snd, AudioLoadedFn fnLoaded, void* pUserData)
{
    snd->fnLoaded = fnLoaded;
    snd->pLoadedData = pUserData;
    std::call_once(g_audioInitFlag, StartAudioThreads);
    {
        std::lock_guard<std::mutex> lock(g_jobMutex);
        g_jobs.push_back({ snd, false });
    }
    g_jobCond.notify_one();
    return snd;
}

/* Interface */

AudioSoundHandle Audio::LoadFromFile(const char* szPath, AudioLoadedFn fnLoaded, void* pUserData)
{
    if(!szPath || !szPath[0]) return NULL;

    AudioSound* snd = new AudioSound();
    snd->path = szPath;
    return QueueLoad(snd, fnLoaded, pUserData);
}
AudioSoundHandle Audio::LoadStreamFromFile(const char* szPath, AudioLoadedFn fnLoaded, void* pUserData)
{
    if(!szPath || !szPath[0]) return NULL;

    AudioSound* snd = new AudioSound();
    snd->path = szPath;
    snd->pStream = new AudioStream();
    return QueueLoad(snd, fnLoaded, pUserData);
}
AudioSoundHandle Audio::LoadFromMemory(const void* pData, size_t size, AudioLoadedFn fnLoaded, void* pUserData)
{
    if(!pData || !size) return NULL;

    AudioSound* snd = new AudioSound();
    snd->dataCopy.assign((const uint8_t*)pData, (const uint8_t*)pData + size);
    snd->pData = snd->dataCopy.data();
    snd->dataSize = size;
    return QueueLoad(snd, fnLoaded, pUserData);
}
AudioSoundHandle Audio::LoadFromMemoryNoCopy(const void* pData, size_t size, AudioLoadedFn fnLoaded, void* pUserData)
{
    if(!pData || !size) return NULL;

    AudioSound* snd = new AudioSound();
    snd->pData = (const uint8_t*)pData;
    snd->dataSize = size;
    return QueueLoad(snd, fnLoaded, pUserData);
}
void Audio::Unload(AudioSoundHandle hSound)
{
    AudioSound* snd = hSound;
    if(!snd || snd->bUnloading.exchange(true)) return;
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        for(AudioVoice& v : g_voices)
        {
            if(v.nState == VOICE_ACTIVE && v.pSound == snd) EndVoice(v, AUDIO_END_UNLOADED);
        }
    }
    g_eventCond.notify_one();
    {
        std::lock_guard<std::mutex> lock(g_jobMutex);
        g_jobs.push_back({ snd, true });
    }
    g_jobCond.notify_one();
}
bool Audio::IsLoaded(AudioSoundHandle hSound)
{
    if(!hSound) return false;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    return hSound->bReady;
}
void Audio::SetUnloadedCallback(AudioSoundHandle hSound, AudioUnloadedFn fnUnloaded, void* pUserData)
{
    if(!hSound) return;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    hSound->fnUnloaded = fnUnloaded;
    hSound->pUnloadedData = pUserData;
}
void Audio::SetVoiceEndCallback(AudioSoundHandle hSound, AudioVoiceEndFn fnVoiceEnd, void* pUserData)
{
    if(!hSound) return;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    hSound->fnVoiceEnd = fnVoiceEnd;
    hSound->pVoiceEndData = pUserData;
}
void Audio::SetVoicePausedCallback(AudioSoundHandle hSound, AudioVoicePausedFn fnVoicePaused, void* pUserData)
{
    if(!hSound) return;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    hSound->fnVoicePaused = fnVoicePaused;
    hSound->pVoicePausedData = pUserData;
}
void Audio::SetVoiceResumedCallback(AudioSoundHandle hSound, AudioVoiceResumedFn fnVoiceResumed, void* pUserData)
{
    if(!hSound) return;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    hSound->fnVoiceResumed = fnVoiceResumed;
    hSound->pVoiceResumedData = pUserData;
}
AudioVoiceHandle Audio::Play(AudioSoundHandle hSound, float fVolume, bool bLoop)
{
    AudioSound* snd = hSound;
    if(!snd || snd->bUnloading) return NULL;

    AudioVoice* voice = NULL;
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        if(snd->bFailed || (snd->pStream && snd->pStream->bActive)) return NULL;
        for(AudioVoice& v : g_voices)
        {
            if(v.nState != VOICE_FREE) continue;
            v = { snd, 0.0, fmaxf(fVolume, 0.0f), bLoop, false, VOICE_ACTIVE, -1, -1 };
            voice = &v;
            break;
        }
        if(voice && snd->pStream)
        {
            snd->pStream->bLoop = bLoop;
            snd->pStream->bRestart = true;
            snd->pStream->bActive = true;
        }
    }
    if(voice && snd->pStream) WakeStreamThread();
    return voice;
}
static int64_t DelayToFrames(float fDelay)
{
    int64_t n = (int64_t)(fDelay * g_nOutRate);
    return (n > 0) ? n : 1;
}
void Audio::Stop(AudioVoiceHandle hVoice, float fDelay)
{
    if(!hVoice) return;
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        if(hVoice->nState != VOICE_ACTIVE) return;
        if(fDelay < 0.0f) hVoice->nStopIn = -1;
        else if(fDelay > 0.0f) hVoice->nStopIn = DelayToFrames(fDelay);
        else EndVoice(*hVoice, AUDIO_END_STOPPED);
    }
    g_eventCond.notify_one();
}
void Audio::Pause(AudioVoiceHandle hVoice, float fDelay)
{
    if(!hVoice) return;
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        if(hVoice->nState != VOICE_ACTIVE) return;
        if(fDelay < 0.0f) hVoice->nPauseIn = -1;
        else if(fDelay > 0.0f) hVoice->nPauseIn = DelayToFrames(fDelay);
        else if(!hVoice->bPaused)
        {
            hVoice->nPauseIn = -1;
            SetPaused(*hVoice, true);
        }
    }
    g_eventCond.notify_one();
}
void Audio::Resume(AudioVoiceHandle hVoice)
{
    if(!hVoice) return;
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        if(hVoice->nState != VOICE_ACTIVE || !hVoice->bPaused) return;
        SetPaused(*hVoice, false);
    }
    g_eventCond.notify_one();
}
bool Audio::IsPlaying(AudioVoiceHandle hVoice)
{
    if(!hVoice) return false;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    return hVoice->nState == VOICE_ACTIVE && !hVoice->bPaused;
}
float Audio::GetStopDelay(AudioVoiceHandle hVoice)
{
    if(!hVoice) return -1.0f;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    return (hVoice->nState == VOICE_ACTIVE && hVoice->nStopIn >= 0) ? (float)hVoice->nStopIn / g_nOutRate : -1.0f;
}
float Audio::GetPauseDelay(AudioVoiceHandle hVoice)
{
    if(!hVoice) return -1.0f;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    return (hVoice->nState == VOICE_ACTIVE && hVoice->nPauseIn >= 0) ? (float)hVoice->nPauseIn / g_nOutRate : -1.0f;
}
bool Audio::IsPaused(AudioVoiceHandle hVoice)
{
    if(!hVoice) return false;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    return hVoice->nState == VOICE_ACTIVE && hVoice->bPaused;
}
void Audio::SetVolume(AudioVoiceHandle hVoice, float fVolume)
{
    if(!hVoice) return;
    std::lock_guard<std::mutex> lock(g_audioMutex);
    hVoice->fVolume = fmaxf(fVolume, 0.0f);
}
void Audio::SetGlobalVolume(float fVolume)
{
    std::lock_guard<std::mutex> lock(g_audioMutex);
    g_fGlobalVolume = fmaxf(fVolume, 0.0f);
}
float Audio::GetGlobalVolume()
{
    std::lock_guard<std::mutex> lock(g_audioMutex);
    return g_fGlobalVolume;
}
