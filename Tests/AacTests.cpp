#include "TestHarness.h"
#include "SignalUtils.h"

#include "App/FileAnalysisJob.h"
#include "App/MediaFoundationAudioFormat.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>

using namespace qctest;

namespace
{
    constexpr double kRate = 48000.0;
    constexpr juce::int64 kHundredNanosecondsPerSecond = 10000000;

    struct MediaFoundationSession
    {
        MediaFoundationSession()
        {
            ownsCom = SUCCEEDED (CoInitializeEx (nullptr, COINIT_MULTITHREADED));
            started = SUCCEEDED (MFStartup (MF_VERSION, MFSTARTUP_LITE));
        }

        ~MediaFoundationSession()
        {
            if (started)
                MFShutdown();

            if (ownsCom)
                CoUninitialize();
        }

        bool ownsCom { false };
        bool started { false };
    };

    /** Encodes a sine to AAC in an .m4a container using the system encoder.

        The test suite generates its own fixture rather than committing a binary: an
        encoded file checked into the repo tells you nothing about whether the decoder
        works on the machine running the tests.
    */
    class TemporaryAac
    {
    public:
        TemporaryAac (double sampleRate, int numChannels, double seconds, double amplitudeToUse)
            : file (juce::File::createTempFile ("qc_test.m4a")), amplitude (amplitudeToUse)
        {
            file.deleteFile();
            encoded = encode (sampleRate, numChannels, seconds);
        }

        ~TemporaryAac() { file.deleteFile(); }

        const juce::File& getFile() const { return file; }
        bool isEncoded() const { return encoded; }

    private:
        bool encode (double sampleRate, int numChannels, double seconds)
        {
            IMFSinkWriter* writerRaw = nullptr;

            if (FAILED (MFCreateSinkWriterFromURL (file.getFullPathName().toWideCharPointer(),
                                                   nullptr, nullptr, &writerRaw)))
                return false;

            std::unique_ptr<IMFSinkWriter, void (*) (IMFSinkWriter*)> writer (
                writerRaw, [] (IMFSinkWriter* w) { if (w != nullptr) w->Release(); });

            const auto rate = static_cast<UINT32> (sampleRate);
            const auto channels = static_cast<UINT32> (numChannels);

            IMFMediaType* outTypeRaw = nullptr;
            if (FAILED (MFCreateMediaType (&outTypeRaw)))
                return false;

            std::unique_ptr<IMFMediaType, void (*) (IMFMediaType*)> outType (
                outTypeRaw, [] (IMFMediaType* t) { if (t != nullptr) t->Release(); });

            outType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            outType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_AAC);
            outType->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
            outType->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
            outType->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, channels);
            outType->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000); // 192 kbps

            DWORD streamIndex = 0;
            if (FAILED (writer->AddStream (outType.get(), &streamIndex)))
                return false;

            IMFMediaType* inTypeRaw = nullptr;
            if (FAILED (MFCreateMediaType (&inTypeRaw)))
                return false;

            std::unique_ptr<IMFMediaType, void (*) (IMFMediaType*)> inType (
                inTypeRaw, [] (IMFMediaType* t) { if (t != nullptr) t->Release(); });

            const UINT32 blockAlign = channels * 2;

            inType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            inType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
            inType->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
            inType->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
            inType->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, channels);
            inType->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign);
            inType->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, rate * blockAlign);
            inType->SetUINT32 (MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

            if (FAILED (writer->SetInputMediaType (streamIndex, inType.get(), nullptr)))
                return false;

            if (FAILED (writer->BeginWriting()))
                return false;

            const auto totalFrames = static_cast<juce::int64> (std::llround (sampleRate * seconds));
            const int framesPerBlock = 1024;
            juce::int64 frame = 0;

            while (frame < totalFrames)
            {
                const auto framesThisBlock = static_cast<int> (std::min<juce::int64> (framesPerBlock,
                                                                                      totalFrames - frame));
                const DWORD byteCount = static_cast<DWORD> (framesThisBlock) * blockAlign;

                IMFMediaBuffer* bufferRaw = nullptr;
                if (FAILED (MFCreateMemoryBuffer (byteCount, &bufferRaw)))
                    return false;

                std::unique_ptr<IMFMediaBuffer, void (*) (IMFMediaBuffer*)> buffer (
                    bufferRaw, [] (IMFMediaBuffer* b) { if (b != nullptr) b->Release(); });

                BYTE* data = nullptr;
                DWORD maxLength = 0;
                DWORD currentLength = 0;

                if (FAILED (buffer->Lock (&data, &maxLength, &currentLength)))
                    return false;

                auto* samples = reinterpret_cast<juce::int16*> (data);

                for (int i = 0; i < framesThisBlock; ++i)
                {
                    const double t = static_cast<double> (frame + i) / sampleRate;
                    const auto value = static_cast<juce::int16> (
                        std::lround (amplitude * std::sin (2.0 * kPi * 1000.0 * t) * 32767.0));

                    for (UINT32 channel = 0; channel < channels; ++channel)
                        samples[static_cast<UINT32> (i) * channels + channel] = value;
                }

                buffer->Unlock();
                buffer->SetCurrentLength (byteCount);

                IMFSample* sampleRaw = nullptr;
                if (FAILED (MFCreateSample (&sampleRaw)))
                    return false;

                std::unique_ptr<IMFSample, void (*) (IMFSample*)> sample (
                    sampleRaw, [] (IMFSample* s) { if (s != nullptr) s->Release(); });

                sample->AddBuffer (buffer.get());
                sample->SetSampleTime ((frame * kHundredNanosecondsPerSecond)
                                       / static_cast<juce::int64> (sampleRate));
                sample->SetSampleDuration ((static_cast<juce::int64> (framesThisBlock)
                                            * kHundredNanosecondsPerSecond)
                                           / static_cast<juce::int64> (sampleRate));

                if (FAILED (writer->WriteSample (streamIndex, sample.get())))
                    return false;

                frame += framesThisBlock;
            }

            return SUCCEEDED (writer->Finalize());
        }

        juce::File file;
        double amplitude;
        bool encoded { false };
    };
}

QC_TEST (aacExtensionsAreRegistered)
{
    const auto wildcard = qc::getSupportedFormatWildcard();

    // Compared token by token, not as a substring: a malformed "*.*.m4a" entry contains
    // "*.m4a" while matching nothing at all, and that is exactly the bug this catches.
    juce::StringArray tokens;
    tokens.addTokens (wildcard, ";", "");

    for (const auto* extension : { "*.m4a", "*.aac" })
        check (tokens.contains (extension),
               std::string ("the PRD promises ") + extension + ", but the format manager reports: "
                   + wildcard.toStdString());
}

QC_TEST (decodesAnAacFileToTheRightLevel)
{
    MediaFoundationSession session;
    check (session.started, "Media Foundation should start");

    const double amplitude = 0.5;
    TemporaryAac aac (kRate, 2, 10.0, amplitude);

    check (aac.isEncoded(), "the system AAC encoder should produce a fixture");
    check (aac.getFile().getSize() > 1000, "the encoded file should not be trivially small");

    const auto outcome = qc::analyseFile (aac.getFile());

    check (outcome.succeeded, "an .m4a should now analyse: " + outcome.errorMessage.toStdString());

    const auto& result = outcome.result;

    check (result.source.numChannels == 2, "channel count survives the round trip");
    checkClose (result.source.sampleRate, kRate, 0.5, "sample rate");

    // AAC at 192 kbps on a steady 1 kHz tone is transparent well beyond the tolerance
    // any delivery spec cares about, so the decoded level must match the source signal.
    checkClose (result.loudness.integratedLufs,
                predictedSineLoudness (1000.0, amplitude, kRate, 2),
                0.5, "integrated loudness decoded from AAC");

    // Encoder delay and padding shift the length slightly; a whole second would mean
    // the duration metadata is being misread.
    checkClose (result.source.durationSeconds, 10.0, 0.5, "duration");

    check (result.source.formatName.find ("AAC") != std::string::npos,
           "the format should be identified as AAC: " + result.source.formatName);
}

QC_TEST (decodesMonoAac)
{
    MediaFoundationSession session;

    TemporaryAac aac (44100.0, 1, 6.0, 0.25);
    check (aac.isEncoded(), "mono AAC fixture");

    const auto outcome = qc::analyseFile (aac.getFile());

    check (outcome.succeeded, "mono .m4a should analyse: " + outcome.errorMessage.toStdString());
    check (outcome.result.source.numChannels == 1, "mono stays mono");
    checkClose (outcome.result.loudness.integratedLufs,
                predictedSineLoudness (1000.0, 0.25, 44100.0, 1),
                0.5, "mono integrated loudness decoded from AAC");
}

QC_TEST (truncatedAacFileFailsCleanly)
{
    MediaFoundationSession session;

    TemporaryAac aac (kRate, 2, 4.0, 0.4);
    check (aac.isEncoded(), "fixture");

    // Chop the file in half: the container index lives at the end of an .m4a, so this
    // produces something the decoder genuinely cannot open.
    const auto truncated = juce::File::createTempFile ("qc_truncated.m4a");
    juce::MemoryBlock contents;
    aac.getFile().loadFileAsData (contents);
    truncated.replaceWithData (contents.getData(), contents.getSize() / 2);

    const auto outcome = qc::analyseFile (truncated);
    truncated.deleteFile();

    if (! outcome.succeeded)
        check (outcome.errorMessage.isNotEmpty(), "a failure must come with an explanation");
    else
        check (outcome.result.source.durationSeconds > 0.0,
               "if the decoder recovers something, it must still be a usable measurement");
}
