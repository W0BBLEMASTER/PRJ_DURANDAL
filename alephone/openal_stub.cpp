#include <AL/al.h>
#include <AL/alc.h>
#include <stddef.h>
#include <string.h>

// Dummy function for GetProcAddress
extern "C" void dummy_al_func() {}

extern "C" {
    // ALC
    ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar *devicename) { return (ALCdevice*)1; }
    ALCboolean ALC_APIENTRY alcCloseDevice(ALCdevice *device) { return ALC_TRUE; }
    ALCcontext* ALC_APIENTRY alcCreateContext(ALCdevice *device, const ALCint *attrlist) { return (ALCcontext*)1; }
    ALCboolean ALC_APIENTRY alcMakeContextCurrent(ALCcontext *context) { return ALC_TRUE; }
    void ALC_APIENTRY alcDestroyContext(ALCcontext *context) {}
    ALCdevice* ALC_APIENTRY alcGetContextsDevice(ALCcontext *context) { return (ALCdevice*)1; }
    ALCenum ALC_APIENTRY alcGetError(ALCdevice *device) { return ALC_NO_ERROR; }
    
    ALCboolean ALC_APIENTRY alcIsExtensionPresent(ALCdevice *device, const ALCchar *extname) { return ALC_TRUE; }
    void* ALC_APIENTRY alcGetProcAddress(ALCdevice *device, const ALCchar *funcname) { return (void*)dummy_al_func; }
    
    ALCenum ALC_APIENTRY alcGetEnumValue(ALCdevice *device, const ALCchar *enumname) {
        if (!enumname) return 0;
        if (strcmp(enumname, "AL_FORMAT_MONO8") == 0) return 0x1100;
        if (strcmp(enumname, "AL_FORMAT_MONO16") == 0) return 0x1101;
        if (strcmp(enumname, "AL_FORMAT_STEREO8") == 0) return 0x1102;
        if (strcmp(enumname, "AL_FORMAT_STEREO16") == 0) return 0x1103;
        return 0;
    }
    
    void ALC_APIENTRY alcGetIntegerv(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *values) {
        if (values && size > 0) {
            for (int i = 0; i < size; i++) values[i] = 0;
        }
    }

    // MISSING: Added alcIsRenderFormatSupportedSOFT
    ALCboolean ALC_APIENTRY alcIsRenderFormatSupportedSOFT(ALCdevice *device, ALCsizei freq, ALCenum channels, ALCenum type) {
        return ALC_TRUE;
    }

    // AL
    void AL_APIENTRY alGenBuffers(ALsizei n, ALuint *buffers) { for(int i=0; i<n; i++) buffers[i] = i+1; }
    void AL_APIENTRY alDeleteBuffers(ALsizei n, const ALuint *buffers) {}
    ALboolean AL_APIENTRY alIsBuffer(ALuint buffer) { return AL_TRUE; }
    void AL_APIENTRY alBufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq) {}
    void AL_APIENTRY alGenSources(ALsizei n, ALuint *sources) { for(int i=0; i<n; i++) sources[i] = i+1; }
    void AL_APIENTRY alDeleteSources(ALsizei n, const ALuint *sources) {}
    ALboolean AL_APIENTRY alIsSource(ALuint source) { return AL_TRUE; }
    void AL_APIENTRY alSourcef(ALuint source, ALenum param, ALfloat value) {}
    void AL_APIENTRY alSource3f(ALuint source, ALenum param, ALfloat v1, ALfloat v2, ALfloat v3) {}
    void AL_APIENTRY alSourcefv(ALuint source, ALenum param, const ALfloat *values) {}
    void AL_APIENTRY alSourcei(ALuint source, ALenum param, ALint value) {}
    void AL_APIENTRY alSource3i(ALuint source, ALenum param, ALint v1, ALint v2, ALint v3) {}
    void AL_APIENTRY alSourcePlay(ALuint source) {}
    void AL_APIENTRY alSourceStop(ALuint source) {}
    void AL_APIENTRY alSourceRewind(ALuint source) {}
    void AL_APIENTRY alSourcePause(ALuint source) {}
    
    void AL_APIENTRY alSourceQueueBuffers(ALuint source, ALsizei nb, const ALuint *buffers) {}
    void AL_APIENTRY alSourceUnqueueBuffers(ALuint source, ALsizei nb, ALuint *buffers) {}
    
    void AL_APIENTRY alListenerf(ALenum param, ALfloat value) {}
    void AL_APIENTRY alListener3f(ALenum param, ALfloat v1, ALfloat v2, ALfloat v3) {}
    void AL_APIENTRY alListener3i(ALenum param, ALint v1, ALint v2, ALint v3) {}
    void AL_APIENTRY alListenerfv(ALenum param, const ALfloat *values) {}
    
    void AL_APIENTRY alGetSourcei(ALuint source, ALenum param, ALint *value) { 
        if(param == AL_SOURCE_STATE) *value = AL_STOPPED; 
        else if (value) *value = 0;
    }
    ALenum AL_APIENTRY alGetError(void) { return AL_NO_ERROR; }
    
    ALboolean AL_APIENTRY alIsExtensionPresent(const ALchar *extname) { return ALC_TRUE; }
    void* AL_APIENTRY alGetProcAddress(const ALchar *fname) { return (void*)dummy_al_func; }
    
    const ALchar* AL_APIENTRY alGetString(ALenum name) {
        if (name == AL_EXTENSIONS) return "AL_EXT_OFFSET AL_EXT_LINEAR_DISTANCE AL_SOFT_loopback";
        return "";
    }
}