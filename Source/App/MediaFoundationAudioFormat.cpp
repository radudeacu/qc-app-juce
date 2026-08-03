#include "MediaFoundationAudioFormat.h"

#include <algorithm>
#include <vector>

#if JUCE_WINDOWS
 #include <mfapi.h>
 #include <mfidl.h>
 #include <mfreadwrite.h>
 #include <mferror.h>
#endif

namespace qc
{
    namespace
    {
        // Bare extensions: juce::AudioFormat prepends the "*." itself when it builds the
        // wildcard, and passing "*.m4a" here yields "*.*.m4a", which matches nothing.
        //
        // Deliberately audio-only. Media Foundation would happily pull the audio track
        // out of an .mp4 as well, but video containers are out of scope for this build;
        // adding "mp4" to this list is all it would take.
        const char* const kExtensions = ".m4a;.aac;.adts;.m4b";
    }

#if JUCE_WINDOWS

    namespace
    {
        constexpr juce::int64 kHundredNanosecondsPerSecond = 10000000;

        // The stream selectors are anonymous-enum constants that the API then takes as
        // DWORD; naming them once here keeps the conversion in a single place.
        constexpr DWORD kAllStreams = static_cast<DWORD> (MF_SOURCE_READER_ALL_STREAMS);
        constexpr DWORD kFirstAudioStream = static_cast<DWORD> (MF_SOURCE_READER_FIRST_AUDIO_STREAM);
        constexpr DWORD kMediaSource = static_cast<DWORD> (MF_SOURCE_READER_MEDIASOURCE);

        /** Minimal owning COM pointer. Written out rather than pulled from a framework
            so the ownership rules here are visible at the point of use.
        */
        template <typename Type>
        class ComPtr
        {
        public:
            ComPtr() = default;
            ~ComPtr() { reset(); }

            ComPtr (const ComPtr&) = delete;
            ComPtr& operator= (const ComPtr&) = delete;

            Type** resetAndGetAddress() noexcept { reset(); return &pointer; }
            Type* operator->() const noexcept { return pointer; }
            Type* get() const noexcept { return pointer; }
            explicit operator bool() const noexcept { return pointer != nullptr; }

            void reset() noexcept
            {
                if (pointer != nullptr)
                {
                    pointer->Release();
                    pointer = nullptr;
                }
            }

        private:
            Type* pointer { nullptr };
        };

        /** Brings COM and Media Foundation up for as long as a reader is alive.

            MFStartup and MFShutdown are reference counted internally, so holding one of
            these per reader is safe even with several files being analysed at once.
        */
        class MediaFoundationScope
        {
        public:
            MediaFoundationScope()
            {
                // Analysis runs on a pool thread that has no COM apartment of its own.
                // RPC_E_CHANGED_MODE means something already initialised this thread in
                // another mode, which is usable - we simply must not tear it down.
                const auto comResult = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
                ownsComInitialisation = SUCCEEDED (comResult);

                started = SUCCEEDED (MFStartup (MF_VERSION, MFSTARTUP_LITE));
            }

            ~MediaFoundationScope()
            {
                if (started)
                    MFShutdown();

                if (ownsComInitialisation)
                    CoUninitialize();
            }

            MediaFoundationScope (const MediaFoundationScope&) = delete;
            MediaFoundationScope& operator= (const MediaFoundationScope&) = delete;

            bool isOk() const noexcept { return started; }

        private:
            bool ownsComInitialisation { false };
            bool started { false };
        };

        /** Decodes through IMFSourceReader, presenting float samples to JUCE.

            Whatever the decoder hands back - 16-bit PCM or float - is converted to float
            on the way in, so the rest of the app never has to care which path Media
            Foundation chose.
        */
        class MediaFoundationReader final : public juce::AudioFormatReader
        {
        public:
            explicit MediaFoundationReader (const juce::File& file)
                : juce::AudioFormatReader (nullptr, "AAC (Media Foundation)")
            {
                openedOk = open (file);
            }

            bool isOpenedOk() const noexcept { return openedOk; }

            bool readSamples (int* const* destChannels,
                              int numDestChannels,
                              int startOffsetInDestBuffer,
                              juce::int64 startSampleInFile,
                              int numSamples) override
            {
                clearSamplesBeyondAvailableLength (destChannels, numDestChannels, startOffsetInDestBuffer,
                                                   startSampleInFile, numSamples, lengthInSamples);

                if (numSamples <= 0)
                    return true;

                if (! openedOk)
                    return false;

                if (! advanceTo (startSampleInFile))
                    return false;

                int destPosition = startOffsetInDestBuffer;
                int remaining = numSamples;

                while (remaining > 0)
                {
                    if (getAvailableFrames() == 0 && ! ensureDataAvailable())
                        break;

                    // Parenthesised because windows.h defines min as a macro.
                    const int chunk = (std::min) (remaining, getAvailableFrames());

                    if (chunk <= 0)
                        break;

                    copyFrames (destChannels, numDestChannels, destPosition, chunk);
                    consumeFrames (chunk);

                    destPosition += chunk;
                    remaining -= chunk;
                }

                // A compressed file's duration is metadata, and encoder padding means it
                // can overstate the samples that actually decode. Zero-filling the
                // shortfall keeps the measurement honest rather than repeating the last
                // block or failing the whole read.
                if (remaining > 0)
                    clearChannels (destChannels, numDestChannels, destPosition, remaining);

                return true;
            }

        private:
            int getAvailableFrames() const noexcept
            {
                const auto floatsLeft = decoded.size() - readOffset;
                return static_cast<int> (floatsLeft / static_cast<std::size_t> (numChannels));
            }

            static void clearChannels (int* const* destChannels, int numDestChannels,
                                       int destPosition, int numFrames)
            {
                for (int channel = 0; channel < numDestChannels; ++channel)
                    if (destChannels[channel] != nullptr)
                        juce::FloatVectorOperations::clear (
                            reinterpret_cast<float*> (destChannels[channel]) + destPosition, numFrames);
            }

            void copyFrames (int* const* destChannels, int numDestChannels,
                             int destPosition, int numFrames) const
            {
                for (int channel = 0; channel < numDestChannels; ++channel)
                {
                    auto* destination = reinterpret_cast<float*> (destChannels[channel]);

                    if (destination == nullptr)
                        continue;

                    if (channel >= static_cast<int> (numChannels))
                    {
                        juce::FloatVectorOperations::clear (destination + destPosition, numFrames);
                        continue;
                    }

                    for (int frame = 0; frame < numFrames; ++frame)
                        destination[destPosition + frame] =
                            decoded[readOffset
                                    + static_cast<std::size_t> (frame) * static_cast<std::size_t> (numChannels)
                                    + static_cast<std::size_t> (channel)];
                }
            }

            void consumeFrames (int numFrames)
            {
                readOffset += static_cast<std::size_t> (numFrames) * static_cast<std::size_t> (numChannels);
                nextSampleInFile += numFrames;

                // Compact occasionally rather than on every read: erasing from the front
                // of a vector is linear, and the decoder hands over small blocks.
                if (readOffset > 65536)
                {
                    decoded.erase (decoded.begin(), decoded.begin() + static_cast<std::ptrdiff_t> (readOffset));
                    readOffset = 0;
                }
            }

            bool advanceTo (juce::int64 targetSample)
            {
                if (targetSample == nextSampleInFile)
                    return true;

                if (targetSample < nextSampleInFile && ! seekToSample (targetSample))
                    return false;

                while (nextSampleInFile < targetSample)
                {
                    if (getAvailableFrames() == 0 && ! ensureDataAvailable())
                        return true; // Past the end; the caller's shortfall gets zeroed.

                    const auto toSkip = static_cast<int> (std::min<juce::int64> (getAvailableFrames(),
                                                                                 targetSample - nextSampleInFile));

                    if (toSkip <= 0)
                        return true;

                    consumeFrames (toSkip);
                }

                return true;
            }

            bool ensureDataAvailable()
            {
                // Media Foundation can legitimately return a call with no sample - a
                // format change or a gap - so retry, but never spin forever.
                for (int attempt = 0; attempt < 128; ++attempt)
                {
                    if (getAvailableFrames() > 0)
                        return true;

                    if (atEndOfStream || ! decodeNextBlock())
                        return getAvailableFrames() > 0;
                }

                return getAvailableFrames() > 0;
            }

            bool decodeNextBlock()
            {
                DWORD streamFlags = 0;
                LONGLONG timestamp = 0;
                ComPtr<IMFSample> sample;

                const auto hr = sourceReader->ReadSample (kFirstAudioStream,
                                                          0, nullptr, &streamFlags, &timestamp,
                                                          sample.resetAndGetAddress());

                if (FAILED (hr))
                    return false;

                if ((streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
                {
                    atEndOfStream = true;
                    return false;
                }

                if (! sample)
                    return true;

                ComPtr<IMFMediaBuffer> buffer;

                if (FAILED (sample->ConvertToContiguousBuffer (buffer.resetAndGetAddress())))
                    return false;

                BYTE* data = nullptr;
                DWORD maxLength = 0;
                DWORD currentLength = 0;

                if (FAILED (buffer->Lock (&data, &maxLength, &currentLength)))
                    return false;

                // Trust the decoder's own timestamp when nothing is buffered: after a
                // seek it is the only thing that says where we actually landed.
                if (getAvailableFrames() == 0)
                {
                    decoded.clear();
                    readOffset = 0;
                    nextSampleInFile = (static_cast<juce::int64> (timestamp) * static_cast<juce::int64> (sampleRate))
                                     / kHundredNanosecondsPerSecond;
                }

                appendSamples (data, currentLength);

                buffer->Unlock();
                return true;
            }

            void appendSamples (const BYTE* data, DWORD numBytes)
            {
                if (decoderProducesFloat)
                {
                    const auto numFloats = numBytes / sizeof (float);
                    const auto* source = reinterpret_cast<const float*> (data);
                    decoded.insert (decoded.end(), source, source + numFloats);
                    return;
                }

                const auto numShorts = numBytes / sizeof (juce::int16);
                const auto* source = reinterpret_cast<const juce::int16*> (data);

                decoded.reserve (decoded.size() + numShorts);

                for (std::size_t i = 0; i < numShorts; ++i)
                    decoded.push_back (static_cast<float> (source[i]) / 32768.0f);
            }

            bool seekToSample (juce::int64 targetSample)
            {
                PROPVARIANT position;
                PropVariantInit (&position);
                position.vt = VT_I8;
                position.hVal.QuadPart = (targetSample * kHundredNanosecondsPerSecond)
                                       / static_cast<juce::int64> (sampleRate);

                const auto hr = sourceReader->SetCurrentPosition (GUID_NULL, position);
                PropVariantClear (&position);

                if (FAILED (hr))
                    return false;

                decoded.clear();
                readOffset = 0;
                atEndOfStream = false;

                return ensureDataAvailable();
            }

            bool setOutputFormat (const GUID& subtype, UINT32 bitsPerSampleToRequest)
            {
                ComPtr<IMFMediaType> type;

                if (FAILED (MFCreateMediaType (type.resetAndGetAddress())))
                    return false;

                if (FAILED (type->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio))
                    || FAILED (type->SetGUID (MF_MT_SUBTYPE, subtype))
                    || FAILED (type->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSampleToRequest)))
                    return false;

                return SUCCEEDED (sourceReader->SetCurrentMediaType (kFirstAudioStream,
                                                                     nullptr, type.get()));
            }

            bool open (const juce::File& file)
            {
                if (! session.isOk())
                    return false;

                if (FAILED (MFCreateSourceReaderFromURL (file.getFullPathName().toWideCharPointer(),
                                                         nullptr,
                                                         sourceReader.resetAndGetAddress())))
                    return false;

                sourceReader->SetStreamSelection (kAllStreams, FALSE);
                sourceReader->SetStreamSelection (kFirstAudioStream, TRUE);

                // Float avoids a quantisation step the measurement would otherwise
                // inherit. Not every decoder chain offers it, so 16-bit PCM is the
                // fallback and gets converted on the way in.
                decoderProducesFloat = setOutputFormat (MFAudioFormat_Float, 32);

                if (! decoderProducesFloat && ! setOutputFormat (MFAudioFormat_PCM, 16))
                    return false;

                ComPtr<IMFMediaType> actualType;

                if (FAILED (sourceReader->GetCurrentMediaType (kFirstAudioStream,
                                                               actualType.resetAndGetAddress())))
                    return false;

                UINT32 rate = 0;
                UINT32 channels = 0;
                UINT32 bits = 0;

                if (FAILED (actualType->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate))
                    || FAILED (actualType->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &channels))
                    || rate == 0 || channels == 0)
                    return false;

                actualType->GetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);

                sampleRate = static_cast<double> (rate);
                numChannels = channels;
                bitsPerSample = bits != 0 ? bits : (decoderProducesFloat ? 32u : 16u);
                usesFloatingPointData = true;

                PROPVARIANT duration;
                PropVariantInit (&duration);

                if (SUCCEEDED (sourceReader->GetPresentationAttribute (kMediaSource,
                                                                       MF_PD_DURATION, &duration)))
                {
                    const auto hundredNanoseconds = static_cast<juce::int64> (duration.uhVal.QuadPart);
                    lengthInSamples = (hundredNanoseconds * static_cast<juce::int64> (rate))
                                    / kHundredNanosecondsPerSecond;
                }

                PropVariantClear (&duration);

                return lengthInSamples > 0;
            }

            MediaFoundationScope session;
            ComPtr<IMFSourceReader> sourceReader;

            std::vector<float> decoded;     // interleaved, from readOffset onwards
            std::size_t readOffset { 0 };
            juce::int64 nextSampleInFile { 0 };

            bool decoderProducesFloat { true };
            bool atEndOfStream { false };
            bool openedOk { false };
        };
    }

    bool MediaFoundationAudioFormat::isAvailable() { return true; }

    std::unique_ptr<juce::AudioFormatReader> MediaFoundationAudioFormat::createReaderForFile (const juce::File& file)
    {
        auto reader = std::make_unique<MediaFoundationReader> (file);

        if (! reader->isOpenedOk())
            return nullptr;

        return reader;
    }

#else

    bool MediaFoundationAudioFormat::isAvailable() { return false; }

    std::unique_ptr<juce::AudioFormatReader> MediaFoundationAudioFormat::createReaderForFile (const juce::File&)
    {
        return nullptr;
    }

#endif

    MediaFoundationAudioFormat::MediaFoundationAudioFormat()
        : juce::AudioFormat ("AAC (Media Foundation)", juce::StringArray::fromTokens (kExtensions, ";", ""))
    {
    }

    juce::Array<int> MediaFoundationAudioFormat::getPossibleSampleRates()
    {
        return { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000 };
    }

    juce::Array<int> MediaFoundationAudioFormat::getPossibleBitDepths()
    {
        return { 16, 32 };
    }

    juce::AudioFormatReader* MediaFoundationAudioFormat::createReaderFor (juce::InputStream* sourceStream,
                                                                          bool deleteStreamIfOpeningFails)
    {
        // Media Foundation decodes from a path, so the stream has to be one we can name.
        // Every route into this app opens a file from disk, so this is not a limitation
        // in practice - but it must fail cleanly rather than silently for anything else.
        std::unique_ptr<juce::InputStream> owned (sourceStream);

        auto* fileStream = dynamic_cast<juce::FileInputStream*> (sourceStream);

        if (fileStream == nullptr)
        {
            if (! deleteStreamIfOpeningFails)
                owned.release();

            return nullptr;
        }

        const auto file = fileStream->getFile();
        auto reader = createReaderForFile (file);

        if (reader == nullptr && ! deleteStreamIfOpeningFails)
            owned.release();

        return reader.release();
    }

    std::unique_ptr<juce::AudioFormatWriter> MediaFoundationAudioFormat::createWriterFor (
        std::unique_ptr<juce::OutputStream>&, const juce::AudioFormatWriterOptions&)
    {
        // The app measures files; it never writes audio. Returning null leaves the
        // caller's stream untouched, which is what the contract asks for.
        return nullptr;
    }
}
