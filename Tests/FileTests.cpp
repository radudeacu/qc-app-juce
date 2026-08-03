#include "TestHarness.h"
#include "SignalUtils.h"

#include "App/FileAnalysisJob.h"

#include <juce_audio_formats/juce_audio_formats.h>

using namespace qctest;

namespace
{
    constexpr double kRate = 48000.0;

    /** Writes a real WAV to a temporary location so the reader path is exercised end to
        end rather than mocked.
    */
    class TemporaryWav
    {
    public:
        TemporaryWav (double sampleRate, int numChannels, double seconds, double amplitude,
                      int bitsPerSample = 24)
            : file (juce::File::createTempFile ("qc_test.wav"))
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                format.createWriterFor (new juce::FileOutputStream (file),
                                        sampleRate, static_cast<unsigned int> (numChannels),
                                        bitsPerSample, {}, 0));

            if (writer == nullptr)
                qctest::fail ("could not create a WAV writer for the test file");

            const int numSamples = static_cast<int> (std::lround (sampleRate * seconds));
            juce::AudioBuffer<float> buffer (numChannels, numSamples);

            for (int channel = 0; channel < numChannels; ++channel)
                for (int i = 0; i < numSamples; ++i)
                    buffer.setSample (channel, i,
                                      static_cast<float> (amplitude
                                          * std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / sampleRate)));

            writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        }

        ~TemporaryWav() { file.deleteFile(); }

        const juce::File& getFile() const { return file; }

    private:
        juce::File file;
    };
}

QC_TEST (promisedInputFormatsAreActuallySupported)
{
    const auto wildcard = qc::getSupportedFormatWildcard();
    std::cout << "           formats available here: " << wildcard << "\n";

    // The PRD commits to these. If a JUCE version or build flag stops providing one,
    // this should fail loudly rather than surface as "cannot decode" to a user.
    for (const auto* extension : { "*.wav", "*.aiff", "*.mp3" })
        check (wildcard.contains (extension),
               std::string ("the PRD promises ") + extension + " support, but the format manager reports: "
                   + wildcard.toStdString());
}

QC_TEST (readsAWavFileAndMeasuresIt)
{
    const double amplitude = 0.5;
    TemporaryWav wav (kRate, 2, 8.0, amplitude);

    const auto outcome = qc::analyseFile (wav.getFile());

    check (outcome.succeeded, "a plain 24-bit stereo WAV should analyse: " + outcome.errorMessage.toStdString());

    const auto& result = outcome.result;

    // 24-bit quantisation of a 0.5 sine is far below the 0.05 dB tolerance, so the file
    // must measure what the theory predicts for the same signal in memory.
    checkClose (result.loudness.integratedLufs,
                predictedSineLoudness (1000.0, amplitude, kRate, 2),
                0.05, "integrated loudness read from disk");

    checkClose (result.source.durationSeconds, 8.0, 0.01, "duration");
    check (result.source.numChannels == 2, "channel count");
    check (result.source.bitDepth == 24, "bit depth");
    check (result.source.formatName == "WAV file", "format name: " + result.source.formatName);
    check (result.oversamplingFactor == 4, "48 kHz should oversample 4x");
}

QC_TEST (readsMonoAndOddSampleRates)
{
    TemporaryWav wav (44100.0, 1, 5.0, 0.25, 16);

    const auto outcome = qc::analyseFile (wav.getFile());

    check (outcome.succeeded, "mono 44.1 kHz should analyse: " + outcome.errorMessage.toStdString());
    checkClose (outcome.result.loudness.integratedLufs,
                predictedSineLoudness (1000.0, 0.25, 44100.0, 1),
                0.05, "mono integrated loudness at 44.1 kHz");
}

QC_TEST (missingFileIsReportedByName)
{
    const auto missing = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("definitely_not_here_9f2a.wav");

    const auto outcome = qc::analyseFile (missing);

    check (! outcome.succeeded, "a missing file cannot analyse");
    check (outcome.errorMessage.contains ("File not found"),
           "the error should say what went wrong: " + outcome.errorMessage.toStdString());
}

QC_TEST (undecodableFileNamesTheFormat)
{
    const auto bogus = juce::File::createTempFile ("qc_test.xyz");
    bogus.replaceWithText ("this is not audio");

    const auto outcome = qc::analyseFile (bogus);
    bogus.deleteFile();

    check (! outcome.succeeded, "a text file is not audio");
    check (outcome.errorMessage.contains (".xyz"),
           "the error should name the extension: " + outcome.errorMessage.toStdString());
    check (outcome.errorMessage.contains ("Supported here"),
           "and say what would work: " + outcome.errorMessage.toStdString());
}

QC_TEST (surroundFilesAreRejectedWithTheChannelCount)
{
    TemporaryWav wav (kRate, 6, 2.0, 0.3);

    const auto outcome = qc::analyseFile (wav.getFile());

    check (! outcome.succeeded, "5.1 is out of scope for this build");
    check (outcome.errorMessage.contains ("6 channels"),
           "the error should state the channel count: " + outcome.errorMessage.toStdString());
}

QC_TEST (dialogueStemOfTheWrongLengthIsRejected)
{
    TemporaryWav mix (kRate, 2, 8.0, 0.5);
    TemporaryWav stem (kRate, 2, 4.0, 0.2);

    const auto outcome = qc::analyseFile (mix.getFile(), stem.getFile());

    check (! outcome.succeeded, "a stem of a different length is not a stem of this mix");
    check (outcome.errorMessage.contains ("same length"),
           "the error should explain: " + outcome.errorMessage.toStdString());
}

QC_TEST (dialogueStemAtTheWrongSampleRateIsRejected)
{
    TemporaryWav mix (kRate, 2, 4.0, 0.5);
    TemporaryWav stem (44100.0, 2, 4.0, 0.2);

    const auto outcome = qc::analyseFile (mix.getFile(), stem.getFile());

    check (! outcome.succeeded, "sample rates must match");
    check (outcome.errorMessage.contains ("sample rate"),
           "the error should explain: " + outcome.errorMessage.toStdString());
}

QC_TEST (matchingDialogueStemProducesAGatedFigure)
{
    TemporaryWav mix (kRate, 2, 12.0, 0.5);
    TemporaryWav stem (kRate, 2, 12.0, 0.5);

    const auto outcome = qc::analyseFile (mix.getFile(), stem.getFile());

    check (outcome.succeeded, "a matching stem should analyse: " + outcome.errorMessage.toStdString());
    check (outcome.result.hasDialogueGatedLoudness, "a dialogue-gated figure should be present");

    // The stem is the mix here, so gating to it changes nothing.
    checkClose (outcome.result.dialogueGatedLufs, outcome.result.loudness.integratedLufs,
                0.1, "gating to a stem identical to the mix");
}

QC_TEST (analysisCanBeCancelled)
{
    TemporaryWav wav (kRate, 2, 30.0, 0.5);

    const auto outcome = qc::analyseFile (wav.getFile(), {}, [] { return true; });

    check (! outcome.succeeded, "a cancelled analysis does not produce a result");
    check (outcome.errorMessage.contains ("cancelled"),
           "and says so: " + outcome.errorMessage.toStdString());
}

QC_TEST (progressIsReportedAndReachesCompletion)
{
    TemporaryWav wav (kRate, 2, 10.0, 0.5);

    double lastProgress = 0.0;
    int callbacks = 0;

    const auto outcome = qc::analyseFile (wav.getFile(), {}, {},
                                          [&] (double progress)
                                          {
                                              lastProgress = progress;
                                              ++callbacks;
                                          });

    check (outcome.succeeded, "analysis should succeed");
    check (callbacks > 1, "progress should be reported more than once");
    checkClose (lastProgress, 1.0, 1.0e-9, "progress should finish at 1.0");
}
