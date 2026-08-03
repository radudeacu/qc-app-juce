#include "TestHarness.h"
#include "SignalUtils.h"

#include "App/BatchAnalyser.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

using namespace qctest;

namespace
{
    constexpr double kRate = 48000.0;

    /** A throwaway folder of real audio, so the collector and the batch runner are
        exercised against a filesystem rather than a list of names.
    */
    class TemporaryFolder
    {
    public:
        TemporaryFolder()
            : folder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("qc_batch_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000))))
        {
            folder.createDirectory();
        }

        ~TemporaryFolder() { folder.deleteRecursively(); }

        juce::File writeWav (const juce::String& name, double seconds, double amplitude,
                             int numChannels = 2)
        {
            const auto file = folder.getChildFile (name);
            file.getParentDirectory().createDirectory();

            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                format.createWriterFor (new juce::FileOutputStream (file), kRate,
                                        static_cast<unsigned int> (numChannels), 24, {}, 0));

            if (writer == nullptr)
                qctest::fail ("could not write " + name.toStdString());

            const int numSamples = static_cast<int> (std::lround (kRate * seconds));
            juce::AudioBuffer<float> buffer (numChannels, numSamples);

            for (int channel = 0; channel < numChannels; ++channel)
                for (int i = 0; i < numSamples; ++i)
                    buffer.setSample (channel, i,
                                      static_cast<float> (amplitude
                                          * std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / kRate)));

            writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
            return file;
        }

        juce::File writeText (const juce::String& name, const juce::String& contents)
        {
            const auto file = folder.getChildFile (name);
            file.getParentDirectory().createDirectory();
            file.replaceWithText (contents);
            return file;
        }

        const juce::File& get() const { return folder; }

    private:
        juce::File folder;
    };

    qc::Target makeTarget (const juce::String& id, double lufs, double tolerance, double ceiling)
    {
        qc::Target target;
        target.id = id.toStdString();
        target.name = id.toStdString();
        target.integratedLufs = lufs;
        target.toleranceLu = tolerance;
        target.maxTruePeakDb = ceiling;
        return target;
    }

    /** Pumps the message loop until the batch reports finished, or the deadline passes. */
    bool runUntilFinished (qc::BatchAnalyser& analyser, int timeoutMilliseconds = 30000)
    {
        bool finished = false;
        analyser.onFinished = [&finished] { finished = true; };

        const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (timeoutMilliseconds);

        while (! finished && juce::Time::getMillisecondCounter() < deadline)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

        return finished;
    }
}

QC_TEST (collectorTakesAudioAndIgnoresEverythingElse)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryFolder folder;
    folder.writeWav ("b_second.wav", 1.0, 0.4);
    folder.writeWav ("a_first.wav", 1.0, 0.4);
    folder.writeText ("notes.txt", "not audio");
    folder.writeText ("cover.jpg", "not audio either");

    juce::StringArray paths;
    paths.add (folder.get().getFullPathName());

    const auto files = qc::collectAudioFiles (paths, false);

    check (files.size() == 2, "only the two WAVs should be collected, got "
                                  + std::to_string (files.size()));

    // Filesystem order is not reading order.
    check (files[0].getFileName() == "a_first.wav", "results should be sorted by name");
    check (files[1].getFileName() == "b_second.wav", "results should be sorted by name");
}

QC_TEST (subfoldersAreExcludedUnlessAskedFor)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryFolder folder;
    folder.writeWav ("top.wav", 1.0, 0.4);
    folder.writeWav ("stems/bounce.wav", 1.0, 0.4);

    juce::StringArray paths;
    paths.add (folder.get().getFullPathName());

    // The default matters: dropping a project folder must not silently pull in every
    // stem and bounce nested underneath it.
    check (qc::collectAudioFiles (paths, false).size() == 1, "recursion off should find one file");
    check (qc::collectAudioFiles (paths, true).size() == 2, "recursion on should find both");
}

QC_TEST (droppingIndividualFilesSkipsUnsupportedOnes)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryFolder folder;
    const auto wav = folder.writeWav ("mix.wav", 1.0, 0.4);
    const auto text = folder.writeText ("readme.md", "hello");

    juce::StringArray paths;
    paths.add (wav.getFullPathName());
    paths.add (text.getFullPathName());

    const auto files = qc::collectAudioFiles (paths, false);
    check (files.size() == 1, "the markdown file should be left out silently");
}

QC_TEST (batchAnalysesEveryFileAndKeepsGoingAfterAFailure)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryFolder folder;
    folder.writeWav ("one.wav", 3.0, 0.5);
    folder.writeWav ("two.wav", 3.0, 0.1);

    // A file with an audio extension that is not audio: exactly the case where one bad
    // file in a delivery folder must not cost the results for the rest.
    folder.writeText ("broken.wav", "this is not a wave file");

    juce::StringArray paths;
    paths.add (folder.get().getFullPathName());

    const auto files = qc::collectAudioFiles (paths, false);
    check (files.size() == 3, "all three should be queued");

    qc::BatchAnalyser analyser;
    analyser.start (files, { makeTarget ("r128", -23.0, 0.5, -1.0) }, {});

    check (runUntilFinished (analyser), "the batch should finish");

    check (analyser.getCompletedCount() == 3, "every entry should be accounted for");
    check (analyser.getFailedCount() == 1, "exactly one file should have failed");

    int completed = 0;
    int failed = 0;

    for (int i = 0; i < analyser.getNumEntries(); ++i)
    {
        const auto& entry = analyser.getEntry (i);

        if (entry.state == qc::BatchEntry::State::completed)
        {
            ++completed;
            check (! entry.verdicts.empty(), "a completed entry should carry verdicts");
            check (qc::isMeasured (entry.result.loudness.integratedLufs),
                   "and a real measurement");
        }
        else if (entry.state == qc::BatchEntry::State::failed)
        {
            ++failed;
            check (entry.errorMessage.isNotEmpty(), "a failure must say why");
            check (entry.overall == qc::Status::notMeasured,
                   "an unreadable file is not a failed verdict, it is an absent one");
        }
    }

    check (completed == 2, "two files should have measurements");
    check (failed == 1, "one should be marked as an error");
    checkClose (analyser.getProgress(), 1.0, 1.0e-9, "progress should reach 1.0");
}

QC_TEST (reevaluatingChangesVerdictsWithoutReReadingFiles)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TemporaryFolder folder;
    folder.writeWav ("mix.wav", 4.0, 0.5);

    juce::StringArray paths;
    paths.add (folder.get().getFullPathName());

    qc::BatchAnalyser analyser;
    analyser.start (qc::collectAudioFiles (paths, false),
                    { makeTarget ("strict", -40.0, 0.5, -20.0) }, {});

    check (runUntilFinished (analyser), "the batch should finish");
    check (analyser.getEntry (0).overall == qc::Status::fail,
           "a -40 LUFS target should reject this file");

    const auto measurementBefore = analyser.getEntry (0).result.loudness.integratedLufs;

    // A generous target the same file should pass, applied without touching the disk.
    analyser.reevaluate ({ makeTarget ("loose", measurementBefore, 3.0, 0.0) });

    check (analyser.getEntry (0).overall == qc::Status::pass,
           "the same measurement should now pass");
    checkClose (analyser.getEntry (0).result.loudness.integratedLufs, measurementBefore, 1.0e-12,
                "re-judging must not alter the measurement");
    check (analyser.getEntry (0).verdicts.size() == 1, "and should carry the new target");
    check (analyser.getEntry (0).verdicts[0].targetId == "loose", "the new target id");
}

QC_TEST (emptyBatchFinishesImmediately)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    qc::BatchAnalyser analyser;

    bool finished = false;
    analyser.onFinished = [&finished] { finished = true; };
    analyser.start ({}, {}, {});

    check (finished, "an empty run should report finished rather than hanging");
    check (! analyser.isRunning(), "and should not be left in a running state");
}
