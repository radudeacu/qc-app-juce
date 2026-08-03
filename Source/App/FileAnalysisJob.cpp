#include "FileAnalysisJob.h"

#include "../Engine/AudioAnalyser.h"
#include "../Engine/LoudnessMeter.h"
#include "MediaFoundationAudioFormat.h"

namespace qc
{
    namespace
    {
        constexpr int kReadBlockSamples = 32768;

        juce::AudioFormatManager& getFormatManager()
        {
            static juce::AudioFormatManager manager;
            static bool initialised = false;

            if (! initialised)
            {
                manager.registerBasicFormats();

                // registerBasicFormats covers WAV, AIFF, FLAC, Ogg, MP3 and - on Windows -
                // the WMA family only. AAC and M4A, which is what podcast and streaming
                // deliverables actually arrive as, need this.
                if (MediaFoundationAudioFormat::isAvailable())
                    manager.registerFormat (new MediaFoundationAudioFormat(), false);

                initialised = true;
            }

            return manager;
        }

        /** Opens a reader, or explains precisely why it could not. */
        std::unique_ptr<juce::AudioFormatReader> createReader (const juce::File& file,
                                                               juce::String& errorMessage)
        {
            if (! file.existsAsFile())
            {
                errorMessage = "File not found: " + file.getFullPathName();
                return nullptr;
            }

            if (file.getSize() == 0)
            {
                errorMessage = file.getFileName() + " is empty.";
                return nullptr;
            }

            std::unique_ptr<juce::AudioFormatReader> reader (getFormatManager().createReaderFor (file));

            if (reader == nullptr)
            {
                const auto extension = file.getFileExtension().isEmpty()
                                     ? juce::String ("(no extension)")
                                     : file.getFileExtension();

                errorMessage = "Cannot decode " + extension + " on this machine. Supported here: "
                             + getSupportedFormatWildcard() + ".";
                return nullptr;
            }

            return reader;
        }

        juce::String describeUnsupportedLayout (int numChannels)
        {
            if (numChannels < 1)
                return "The file reports no audio channels.";

            return "This build measures mono and stereo only; the file has "
                 + juce::String (numChannels) + " channels.";
        }

        /** Streams a file through a callback that consumes non-interleaved blocks. */
        bool readInBlocks (juce::AudioFormatReader& reader,
                           const std::function<void (const float* const*, int)>& consume,
                           const std::function<bool()>& shouldCancel,
                           const std::function<void (double)>& onProgress)
        {
            const auto numChannels = static_cast<int> (reader.numChannels);
            const auto totalSamples = reader.lengthInSamples;

            juce::AudioBuffer<float> buffer (numChannels, kReadBlockSamples);
            juce::int64 position = 0;

            while (position < totalSamples)
            {
                if (shouldCancel && shouldCancel())
                    return false;

                const auto remaining = totalSamples - position;
                const auto toRead = static_cast<int> (juce::jmin (static_cast<juce::int64> (kReadBlockSamples),
                                                                  remaining));

                buffer.clear();

                if (! reader.read (&buffer, 0, toRead, position, true, numChannels > 1))
                    return false;

                consume (buffer.getArrayOfReadPointers(), toRead);
                position += toRead;

                if (onProgress && totalSamples > 0)
                    onProgress (static_cast<double> (position) / static_cast<double> (totalSamples));
            }

            return true;
        }
    }

    juce::String getSupportedFormatWildcard()
    {
        return getFormatManager().getWildcardForAllFormats();
    }

    FileAnalysisOutcome analyseFile (const juce::File& file,
                                     const juce::File& dialogueStem,
                                     std::function<bool()> shouldCancel,
                                     std::function<void (double)> onProgress)
    {
        FileAnalysisOutcome outcome;

        auto reader = createReader (file, outcome.errorMessage);

        if (reader == nullptr)
            return outcome;

        const auto numChannels = static_cast<int> (reader->numChannels);

        if (numChannels < 1 || numChannels > 2)
        {
            outcome.errorMessage = describeUnsupportedLayout (numChannels);
            return outcome;
        }

        if (reader->lengthInSamples <= 0)
        {
            outcome.errorMessage = file.getFileName() + " contains no audio.";
            return outcome;
        }

        AudioAnalyser analyser (reader->sampleRate, numChannels);

        const bool completed = readInBlocks (*reader,
                                             [&analyser] (const float* const* channels, int numSamples)
                                             {
                                                 analyser.process (channels, numSamples);
                                             },
                                             shouldCancel,
                                             onProgress);

        if (! completed)
        {
            outcome.errorMessage = (shouldCancel && shouldCancel())
                                 ? "Analysis cancelled."
                                 : "Reading " + file.getFileName() + " failed part way through.";
            return outcome;
        }

        SourceInfo info;
        info.filePath = file.getFullPathName().toStdString();
        info.formatName = reader->getFormatName().toStdString();
        info.bitDepth = static_cast<int> (reader->bitsPerSample);

        outcome.result = analyser.getResult (info);

        if (dialogueStem != juce::File())
        {
            juce::String stemError;
            auto stemReader = createReader (dialogueStem, stemError);

            if (stemReader == nullptr)
            {
                outcome.errorMessage = "Dialogue stem: " + stemError;
                return outcome;
            }

            // A stem at a different rate or length is not a stem of this mix. Gating
            // against it would produce a confident number about the wrong intervals.
            if (stemReader->sampleRate != reader->sampleRate)
            {
                outcome.errorMessage = "Dialogue stem sample rate ("
                                     + juce::String (stemReader->sampleRate, 0)
                                     + " Hz) does not match the mix ("
                                     + juce::String (reader->sampleRate, 0) + " Hz).";
                return outcome;
            }

            if (stemReader->lengthInSamples != reader->lengthInSamples)
            {
                outcome.errorMessage = "Dialogue stem is "
                                     + juce::String (static_cast<double> (stemReader->lengthInSamples)
                                                     / stemReader->sampleRate, 2)
                                     + " s but the mix is "
                                     + juce::String (static_cast<double> (reader->lengthInSamples)
                                                     / reader->sampleRate, 2)
                                     + " s. They must be the same length.";
                return outcome;
            }

            const auto stemChannels = static_cast<int> (stemReader->numChannels);

            if (stemChannels < 1 || stemChannels > 2)
            {
                outcome.errorMessage = "Dialogue stem: " + describeUnsupportedLayout (stemChannels);
                return outcome;
            }

            LoudnessMeter stemMeter (stemReader->sampleRate, stemChannels);

            const bool stemCompleted = readInBlocks (*stemReader,
                                                     [&stemMeter] (const float* const* channels, int numSamples)
                                                     {
                                                         stemMeter.process (channels, numSamples);
                                                     },
                                                     shouldCancel,
                                                     nullptr);

            if (! stemCompleted)
            {
                outcome.errorMessage = "Reading the dialogue stem failed part way through.";
                return outcome;
            }

            const auto dialogueGated = computeDialogueGatedLoudness (analyser.getGatingBlockPowers(),
                                                                     stemMeter.getGatingBlockPowers());

            outcome.result.hasDialogueGatedLoudness = isMeasured (dialogueGated);
            outcome.result.dialogueGatedLufs = dialogueGated;
        }

        outcome.succeeded = true;
        return outcome;
    }
}
