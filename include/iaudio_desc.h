#include <mod/iaudio.h>

class Audio : public IAudio
{
public:
    AudioSoundHandle LoadFromFile(const char* szPath, AudioLoadedFn fnLoaded = NULL, void* pUserData = NULL);
    AudioSoundHandle LoadFromMemory(const void* pData, size_t size, AudioLoadedFn fnLoaded = NULL, void* pUserData = NULL);
    AudioSoundHandle LoadFromMemoryNoCopy(const void* pData, size_t size, AudioLoadedFn fnLoaded = NULL, void* pUserData = NULL);
    AudioSoundHandle LoadStreamFromFile(const char* szPath, AudioLoadedFn fnLoaded = NULL, void* pUserData = NULL);
    void             Unload(AudioSoundHandle hSound);
    bool             IsLoaded(AudioSoundHandle hSound);
    void             SetUnloadedCallback(AudioSoundHandle hSound, AudioUnloadedFn fnUnloaded, void* pUserData = NULL);
    void             SetVoiceEndCallback(AudioSoundHandle hSound, AudioVoiceEndFn fnVoiceEnd, void* pUserData = NULL); // for all voices of this sound
    void             SetVoicePausedCallback(AudioSoundHandle hSound, AudioVoicePausedFn fnVoicePaused, void* pUserData = NULL);
    void             SetVoiceResumedCallback(AudioSoundHandle hSound, AudioVoiceResumedFn fnVoiceResumed, void* pUserData = NULL);
    AudioVoiceHandle Play(AudioSoundHandle hSound, float fVolume = 1.0f, bool bLoop = false);
    void             Stop(AudioVoiceHandle hVoice, float fDelay = 0.0f);
    void             Pause(AudioVoiceHandle hVoice, float fDelay = 0.0f);
    void             Resume(AudioVoiceHandle hVoice);
    bool             IsPlaying(AudioVoiceHandle hVoice);
    bool             IsPaused(AudioVoiceHandle hVoice);
    float            GetStopDelay(AudioVoiceHandle hVoice);
    float            GetPauseDelay(AudioVoiceHandle hVoice);
    void             SetVolume(AudioVoiceHandle hVoice, float fVolume);
    void             SetGlobalVolume(float fVolume);
    float            GetGlobalVolume();
};
