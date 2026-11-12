#ifndef _IAUDIO
#define _IAUDIO

#include <stddef.h>

#define AUDIO_INTERFACE_STR "AMLAudio"

typedef struct AudioSound* AudioSoundHandle;
typedef struct AudioVoice* AudioVoiceHandle;

enum eAudioEndReason
{
    AUDIO_END_FINISHED = 0, // reached the end
    AUDIO_END_STOPPED,      // Stop() called
    AUDIO_END_UNLOADED,     // Unload() called on its sound
    AUDIO_END_LOAD_FAILED,  // its sound failed to load
};

// Callbacks are called from AML audio threads, not the game thread
typedef void (*AudioLoadedFn)(AudioSoundHandle hSound, bool bSuccess, void* pUserData);
typedef void (*AudioUnloadedFn)(AudioSoundHandle hSound, void* pUserData);
typedef void (*AudioVoiceEndFn)(AudioVoiceHandle hVoice, eAudioEndReason reason, void* pUserData);
typedef void (*AudioVoicePausedFn)(AudioVoiceHandle hVoice, void* pUserData);
typedef void (*AudioVoiceResumedFn)(AudioVoiceHandle hVoice, void* pUserData);

class IAudio
{
public:
    virtual AudioSoundHandle LoadFromFile(const char* szPath, AudioLoadedFn fnLoaded = NULL, void* pUserData = NULL) = 0; // wav, mp3, ogg, flac, aac/m4a, opus, amr, mid...
    virtual AudioSoundHandle LoadFromMemory(const void* pData, size_t size, AudioLoadedFn fnLoaded = NULL, void* pUserData = NULL) = 0; // data is copied
    virtual AudioSoundHandle LoadFromMemoryNoCopy(const void* pData, size_t size, AudioLoadedFn fnLoaded = NULL, void* pUserData = NULL) = 0; // data must stay valid until loaded
    virtual AudioSoundHandle LoadStreamFromFile(const char* szPath, AudioLoadedFn fnLoaded = NULL, void* pUserData = NULL) = 0; // decoded while playing, one voice at a time
    virtual void             Unload(AudioSoundHandle hSound) = 0;
    virtual bool             IsLoaded(AudioSoundHandle hSound) = 0;
    virtual void             SetUnloadedCallback(AudioSoundHandle hSound, AudioUnloadedFn fnUnloaded, void* pUserData = NULL) = 0;
    virtual void             SetVoiceEndCallback(AudioSoundHandle hSound, AudioVoiceEndFn fnVoiceEnd, void* pUserData = NULL) = 0; // for all voices of this sound
    virtual void             SetVoicePausedCallback(AudioSoundHandle hSound, AudioVoicePausedFn fnVoicePaused, void* pUserData = NULL) = 0;
    virtual void             SetVoiceResumedCallback(AudioSoundHandle hSound, AudioVoiceResumedFn fnVoiceResumed, void* pUserData = NULL) = 0;
    virtual AudioVoiceHandle Play(AudioSoundHandle hSound, float fVolume = 1.0f, bool bLoop = false) = 0; // returns voice, starts when loaded
    virtual void             Stop(AudioVoiceHandle hVoice, float fDelay = 0.0f) = 0; // seconds of playback, 0 = now, < 0 cancels a timed stop
    virtual void             Pause(AudioVoiceHandle hVoice, float fDelay = 0.0f) = 0; // seconds of playback, 0 = now, < 0 cancels a timed pause
    virtual void             Resume(AudioVoiceHandle hVoice) = 0;
    virtual bool             IsPlaying(AudioVoiceHandle hVoice) = 0; // active and not paused
    virtual bool             IsPaused(AudioVoiceHandle hVoice) = 0;
    virtual float            GetStopDelay(AudioVoiceHandle hVoice) = 0; // seconds left, -1 if none
    virtual float            GetPauseDelay(AudioVoiceHandle hVoice) = 0; // seconds left, -1 if none
    virtual void             SetVolume(AudioVoiceHandle hVoice, float fVolume) = 0;
    virtual void             SetGlobalVolume(float fVolume) = 0;
    virtual float            GetGlobalVolume() = 0;
};

#endif // _IAUDIO
