#include "TestHarness.h"
#include "SignalUtils.h"

#include "App/PlaybackEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

using namespace qctest;

namespace
{
    constexpr double kRate = 48000.0;

    class TemporaryWavFile
    {
    public:
        TemporaryWavFile (double seconds)
            : file (juce::File::createTempFile ("qc_playback.wav"))
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                format.createWriterFor (new juce::FileOutputStream (file), kRate, 2, 16, {}, 0));

            if (writer == nullptr)
                qctest::fail ("could not write the playback fixture");

            const int numSamples = static_cast<int> (std::lround (kRate * seconds));
            juce::AudioBuffer<float> buffer (2, numSamples);

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < numSamples; ++i)
                    buffer.setSample (channel, i,
                                      static_cast<float> (0.2 * std::sin (2.0 * kPi * 440.0
                                                                          * static_cast<double> (i) / kRate)));

            writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        }

        ~TemporaryWavFile() { file.deleteFile(); }

        const juce::File& get() const { return file; }

    private:
        juce::File file;
    };

    void pump (int milliseconds)
    {
        const auto deadline = juce::Time::getMillisecondCounter()
                            + static_cast<juce::uint32> (milliseconds);

        while (juce::Time::getMillisecondCounter() < deadline)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    }

    /** Machines without an output device - a build agent, a locked-down desktop - can
        still run everything else. Reporting the skip is deliberate: a silently skipped
        test is indistinguishable from a passing one.
    */
    bool deviceUnavailable (const juce::String& error)
    {
        if (error.contains ("No audio output"))
        {
            std::cout << "           skipped: no audio output device on this machine\n";
            return true;
        }

        return false;
    }
}

QC_TEST (playbackLoadsAFileAndReportsItsLength)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryWavFile wav (3.0);
    qc::PlaybackEngine engine;
    juce::String error;

    if (! engine.load (wav.get(), error))
    {
        if (deviceUnavailable (error))
            return;

        fail ("load failed: " + error.toStdString());
    }

    check (engine.hasFile(), "a loaded file should be reported as loaded");
    checkClose (engine.getLengthSeconds(), 3.0, 0.05, "transport length");
    check (! engine.isPlaying(), "loading must not start playback on its own");
    checkClose (engine.getPositionSeconds(), 0.0, 0.01, "a fresh file starts at zero");
}

QC_TEST (playbackAdvancesThePosition)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryWavFile wav (5.0);
    qc::PlaybackEngine engine;
    juce::String error;

    if (! engine.load (wav.get(), error))
    {
        if (deviceUnavailable (error))
            return;

        fail ("load failed: " + error.toStdString());
    }

    engine.play();
    check (engine.isPlaying(), "the transport should report that it is running");

    pump (500);

    const auto position = engine.getPositionSeconds();
    engine.stop();

    // The real proof that audio is being pulled: without a working device and
    // read-ahead thread the position never moves.
    check (position > 0.1, "position should have advanced while playing, got "
                               + std::to_string (position));
    check (position < 2.0, "and should not have jumped, got " + std::to_string (position));
}

QC_TEST (seekingMovesThePlayhead)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryWavFile wav (5.0);
    qc::PlaybackEngine engine;
    juce::String error;

    if (! engine.load (wav.get(), error))
    {
        if (deviceUnavailable (error))
            return;

        fail ("load failed: " + error.toStdString());
    }

    engine.setPositionSeconds (2.5);
    checkClose (engine.getPositionSeconds(), 2.5, 0.05, "seek should land where asked");

    // Past the end and before the start are both clamped rather than rejected: the
    // graph hands over whatever time is under the pointer.
    engine.setPositionSeconds (99.0);
    check (engine.getPositionSeconds() <= 5.01, "seeking past the end should clamp");

    engine.setPositionSeconds (-4.0);
    checkClose (engine.getPositionSeconds(), 0.0, 0.01, "seeking before the start should clamp");
}

QC_TEST (stopReturnsToTheStart)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryWavFile wav (4.0);
    qc::PlaybackEngine engine;
    juce::String error;

    if (! engine.load (wav.get(), error))
    {
        if (deviceUnavailable (error))
            return;

        fail ("load failed: " + error.toStdString());
    }

    engine.setPositionSeconds (2.0);
    engine.stop();

    check (! engine.isPlaying(), "stop should halt the transport");
    checkClose (engine.getPositionSeconds(), 0.0, 0.01, "and rewind it");
}

QC_TEST (unplayableFileIsRejectedWithAReason)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto bogus = juce::File::createTempFile ("qc_playback.zzz");
    bogus.replaceWithText ("not audio");

    qc::PlaybackEngine engine;
    juce::String error;
    const auto loaded = engine.load (bogus, error);
    bogus.deleteFile();

    check (! loaded, "a text file should not load");
    check (error.isNotEmpty(), "and the failure must come with a reason");
    check (! engine.hasFile(), "nothing should be left loaded after a failure");
}

QC_TEST (unloadReleasesTheFile)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryWavFile wav (2.0);
    qc::PlaybackEngine engine;
    juce::String error;

    if (! engine.load (wav.get(), error))
    {
        if (deviceUnavailable (error))
            return;

        fail ("load failed: " + error.toStdString());
    }

    engine.unload();

    check (! engine.hasFile(), "unload should clear the loaded file");
    check (! engine.isPlaying(), "and stop playback");
}
