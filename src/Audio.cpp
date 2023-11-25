#include <Canis/Audio.hpp>
#include <Canis/Debug.hpp>

// https://wiki.libsdl.org/SDL2/SDL_AudioSpec

namespace Canis
{
namespace Audio
{
    AudioObject::AudioObject(const SampleInfo& _info, IAudioData* _data) :
        m_audioPos(0),
        m_audioLength(_data->GetAudioLength()),
        m_sampleInfo(_info),
        m_audioData(_data) {}

    bool AudioObject::GenerateSamples(float* _stream, size_t _streamLength)
    {
        m_audioPos = m_audioData->GenerateSamples(_stream, _streamLength, 
                m_audioPos, m_sampleInfo);

        if(m_audioPos == (size_t)-1)
        {
            m_audioPos = 0;
            return false;
        }

        return true;
    }

    void AudioObject::SetPos(double _pos)
    {
        if(_pos < 0.0)
        {
            _pos = 0.0;
        }
        else if(_pos > 1.0)
        {
            _pos = 1.0;
        }

        m_audioPos = PosToAbsolutePos(_pos);
    }

    size_t AudioObject::PosToAbsolutePos(double _pos)
    {
        return (size_t)(_pos * m_audioLength);
    }

    /////////////////
    SDLWAVAudioData::SDLWAVAudioData(const std::string& fileName, bool streamFromFile)
    {
        // TODO: Handle streamFromFile

        SDL_AudioSpec wavSpec;
        Uint8* wavStart;
        Uint32 wavLength;

        if(SDL_LoadWAV(fileName.c_str(), &wavSpec, &wavStart, &wavLength) == NULL)
        {
            // According to the docu of SDL_LoadWav only supported wave formats are MS-ADPCM and IMA-ADPCM.
            Canis::FatalError(fileName + " could not be loaded as an audio file\n" +
                                "if the file is there  check if it is formated as MS-ADPCM or IMA-ADPCM\n");// + std::string(SDL_GetError()));
        }

        m_pos = wavStart;
        m_start = wavStart;
        m_end = m_start + wavLength;
    }

    SDLWAVAudioData::~SDLWAVAudioData()
    {
        SDL_FreeWAV(m_start);
    }

    size_t SDLWAVAudioData::GenerateSamples(float* stream, size_t streamLength, 
            size_t pos, const SampleInfo& info)
    {
        float pitch = (float)info.pitch;
        m_pos = m_start + pos;
        
        if(m_pos >= m_end || m_pos < m_start)
        {
            return (size_t)-1;
        }

        Uint32 length = (Uint32)streamLength;
        Uint32 lengthLeft = (Uint32)((m_end - m_pos)/pitch);
        length = (length > lengthLeft) ? lengthLeft : length;

        // TODO: Make this more general
        Sint16* samples = (Sint16*)m_pos;
        float sampleIndex = 0;

        float factor = (float)info.volume * 1.0f/32768.0f;
        for(Uint32 i = 0; i < length; i++)
        {
            stream[i] += (samples[(size_t)sampleIndex]) * factor;
            sampleIndex += pitch;
        }

        m_pos = (Uint8*)(samples + (size_t)sampleIndex);
        return (size_t)(m_pos - m_start);
    }

    size_t SDLWAVAudioData::GetAudioLength()
    {
        return (size_t)(m_end - m_start);
    }

    /////////////
    IAudioData* SDLAudioDevice::CreateAudioFromFile(const std::string& filePath)
    {
        return new SDLWAVAudioData(filePath, false);
    }

    void SDLAudioDevice::ReleaseAudio(IAudioData* audioData)
    {
        if(audioData)
        {
            delete audioData;
        }
    }

    /////////////
    static void SDLAudioContext_AudioCallback(void* userData, Uint8* streamIn, int length)
    {
        SDLAudioContext* context = (SDLAudioContext*)userData;
        context->GenerateSamples(streamIn, length);
    }

    SDLAudioContext::SDLAudioContext()
    {
        SDL_AudioSpec spec;

        // TODO: Don't hardcode these values!
        SDL_zero(spec);
        spec.freq = 44100;
        spec.format = AUDIO_S16SYS;
        spec.channels = 2;
        spec.samples = 2048;
        spec.callback = SDLAudioContext_AudioCallback;
        spec.userdata = this;

        // TODO: Handle different specs
        m_device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
        if(m_device == 0)
        {
            // TODO: Proper error handling
            throw SDL_GetError();
        }

        SDL_PauseAudioDevice(m_device, 0);
    }

    SDLAudioContext::~SDLAudioContext()
    {
        SDL_CloseAudioDevice(m_device);
    }

    void SDLAudioContext::PlayAudio(AudioObject& ao)
    {
        SDL_LockAudioDevice(m_device);

        // This prevents duplicates
        RemoveAudio(ao);
        m_playingAudio.push_back(&ao);

        SDL_UnlockAudioDevice(m_device);
    }

    void SDLAudioContext::PauseAudio(AudioObject& ao)
    {
        SDL_LockAudioDevice(m_device);

        RemoveAudio(ao);

        SDL_UnlockAudioDevice(m_device);
    }

    void SDLAudioContext::StopAudio(AudioObject& ao)
    {
        SDL_LockAudioDevice(m_device);

        if(RemoveAudio(ao))
        {
            ao.SetPos(0.0);
        }

        SDL_UnlockAudioDevice(m_device);
    }

    void SDLAudioContext::GenerateSamples(Uint8* streamIn, int streamInLen)
    {
        size_t streamLen = (size_t)(streamInLen/2);

        m_stream.reserve(streamLen);
        float* floatStream = *(float**)(&m_stream);

        for(size_t i = 0; i < streamLen; i++)
        {
            floatStream[i] = 0.0f;
        }

        std::vector<AudioObject*>::iterator it = m_playingAudio.begin();
        std::vector<AudioObject*>::iterator end = m_playingAudio.end();
        for(; it != end; ++it)
        {
            if(!(*it)->GenerateSamples(floatStream, streamLen))
            {
                RemoveAudio(*(*it));
            }
        }

        Sint16* stream = (Sint16*)streamIn;
        for(size_t i = 0; i < streamLen; i++)
        {
            float val = floatStream[i];

            if(val > 1.0f)
            {
                val = 1.0f;
            }
            else if(val < -1.0f)
            {
                val = -1.0f;
            }

            stream[i] = (Sint16)(val * 32767);
        }
    }

    bool SDLAudioContext::RemoveAudio(AudioObject& ao)
    {
        std::vector<AudioObject*>::iterator it = m_playingAudio.begin();
        std::vector<AudioObject*>::iterator end = m_playingAudio.end();

        for(; it != end; ++it)
        {
            if(*it == &ao)
            {
                m_playingAudio.erase(it);
                return true;
            }
        }

        return false;
    }
} // end of Audio namespace
} // end of Canis namespace