

#include <string>
#include <vector>
#include <SDL.h>
//#include <cstddef>

namespace Canis
{
namespace Audio
{
    // sample info
    struct SampleInfo
    {
        double volume;
        double pitch;
    };

    // audio data
    class IAudioData
    {
    public:
        virtual ~IAudioData() {}
        virtual size_t GenerateSamples(float* _stream, size_t _streamLength, size_t _pos, const SampleInfo& info) = 0;
        virtual size_t GetAudioLength() = 0;
    };

    // audio device
    class IAudioDevice
    {
    public:
        virtual ~IAudioDevice() {}
        virtual IAudioData* CreateAudioFromFile(const std::string& _filePath) = 0;
        virtual void ReleaseAudio(IAudioData* _audioData) = 0;
    };

    // audio object
    class AudioObject
    {
    public:
        AudioObject(const SampleInfo& _info, IAudioData* _data);

        bool GenerateSamples(float* _stream, size_t _streamLength);
        void SetPos(double _pos);
    private:
        size_t      m_audioPos;
        size_t      m_audioLength;
        SampleInfo  m_sampleInfo;
        IAudioData* m_audioData;

        size_t PosToAbsolutePos(double _pos);
    };

    // audio context
    class IAudioContext
    {
    public:
        virtual ~IAudioContext() {}
        virtual void PlayAudio(AudioObject& _ao) = 0;
        virtual void PauseAudio(AudioObject& _ao) = 0;
        virtual void StopAudio(AudioObject& _ao) = 0;
    };

    // sdl
    class SDLWAVAudioData : public IAudioData
    {
    public:
        SDLWAVAudioData(const std::string& fileName, bool streamFromFile);
        virtual ~SDLWAVAudioData();

        virtual size_t GenerateSamples(float* stream, size_t streamLength, size_t pos, const SampleInfo& info);
        virtual size_t GetAudioLength();
    private:
        u_int8_t* m_pos;
        u_int8_t* m_start;
        u_int8_t* m_end;
        //Uint32 m_length;

        SDLWAVAudioData(SDLWAVAudioData& other) { (void)other; }
        void operator=(const SDLWAVAudioData& other) { (void)other;}
    };

    class SDLAudioDevice : public IAudioDevice
    {
    public:
        IAudioData* CreateAudioFromFile(const std::string& filePath);
        virtual void ReleaseAudio(IAudioData* audioData);
    };

    class SDLAudioContext : public IAudioContext
    {
    public:
        SDLAudioContext();
        virtual ~SDLAudioContext();

        virtual void PlayAudio(AudioObject& ao);
        virtual void PauseAudio(AudioObject& ao);
        virtual void StopAudio(AudioObject& ao);

        void GenerateSamples(Uint8* stream, int streamLen);
    private:
        SDL_AudioDeviceID         m_device;
        std::vector<float>        m_stream;
        std::vector<AudioObject*> m_playingAudio;

        bool RemoveAudio(AudioObject& ao);

        SDLAudioContext(SDLAudioContext& other) { (void)other; }
        void operator=(const SDLAudioContext& other) { (void)other;}
    };

} // end of Audio namespace
} // end of Canis namespace