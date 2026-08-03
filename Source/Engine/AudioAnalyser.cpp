#include "AudioAnalyser.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qc
{
    namespace
    {
        const double kMonoDownmixGain = 1.0 / std::sqrt (2.0);
    }

    AudioAnalyser::AudioAnalyser (double sampleRateToUse, int numChannelsToUse)
        : sampleRate (sampleRateToUse),
          numChannels (numChannelsToUse),
          loudnessMeter (sampleRateToUse, numChannelsToUse),
          truePeakDetector (sampleRateToUse, numChannelsToUse),
          qualityAnalyser (sampleRateToUse, numChannelsToUse)
    {
        if (numChannels == 2)
            monoDownmixMeter = std::make_unique<LoudnessMeter> (sampleRate, 1);
    }

    void AudioAnalyser::process (const float* const* channelData, int numSamples)
    {
        if (numSamples <= 0)
            return;

        loudnessMeter.process (channelData, numSamples);
        truePeakDetector.process (channelData, numSamples);
        qualityAnalyser.process (channelData, numSamples);

        if (monoDownmixMeter != nullptr)
        {
            if (static_cast<int> (downmixScratch.size()) < numSamples)
                downmixScratch.resize (static_cast<std::size_t> (numSamples));

            for (int i = 0; i < numSamples; ++i)
                downmixScratch[static_cast<std::size_t> (i)] =
                    static_cast<float> ((static_cast<double> (channelData[0][i])
                                         + static_cast<double> (channelData[1][i])) * kMonoDownmixGain);

            const float* monoChannel = downmixScratch.data();
            monoDownmixMeter->process (&monoChannel, numSamples);
        }

        samplesProcessed += numSamples;
    }

    AnalysisResult AudioAnalyser::getResult (const SourceInfo& info) const
    {
        AnalysisResult result;
        result.source = info;
        result.source.sampleRate = sampleRate;
        result.source.numChannels = numChannels;
        result.source.durationSeconds = static_cast<double> (samplesProcessed) / sampleRate;

        result.loudness = loudnessMeter.getMeasurements();
        result.quality = qualityAnalyser.getMeasurements();
        result.truePeakDb = truePeakDetector.getTruePeakDb();
        result.oversamplingFactor = truePeakDetector.getOversamplingFactor();
        result.truePeakEnvelope = truePeakDetector.getEnvelope();

        if (isMeasured (result.loudness.integratedLufs))
            result.peakToLoudnessRatioDb = result.truePeakDb - result.loudness.integratedLufs;

        if (monoDownmixMeter != nullptr && isMeasured (result.loudness.integratedLufs))
        {
            const double monoLufs = monoDownmixMeter->getMeasurements().integratedLufs;

            // A stereo signal that reads normally but whose mono sum falls below the
            // gate has cancelled itself out entirely — the worst possible mono
            // compatibility, so it reports as infinite loss rather than as no loss.
            result.monoCompatibilityLossDb = isMeasured (monoLufs)
                                           ? result.loudness.integratedLufs - monoLufs
                                           : std::numeric_limits<double>::infinity();
        }

        return result;
    }

    namespace
    {
        /** Discards blocks within `radius` of the edge of an active run.

            Gating blocks are 400 ms hopped 100 ms, so the blocks straddling the start or
            end of a dialogue passage are only partly dialogue. The stem still passes them
            — a quarter of a phrase is far above the gate — but in the programme those same
            blocks contain whatever else is playing. On a mix where dialogue gives way to
            loud music, counting three straddling blocks pulls the reading more than a
            decibel high. Trimming one window length from each edge leaves only blocks that
            are dialogue the whole way through.

            The cost is that dialogue passages shorter than about 0.7 s drop out entirely.
            That is the right way to be wrong: a dialogue-gated number that is slightly
            under-sampled beats one contaminated by music.
        */
        std::vector<std::size_t> erodeRuns (const std::vector<std::size_t>& active,
                                            std::size_t totalBlocks,
                                            int radius)
        {
            std::vector<bool> isActive (totalBlocks, false);

            for (std::size_t index : active)
                if (index < totalBlocks)
                    isActive[index] = true;

            std::vector<std::size_t> eroded;
            eroded.reserve (active.size());

            for (std::size_t index : active)
            {
                if (index < static_cast<std::size_t> (radius)
                    || index + static_cast<std::size_t> (radius) >= totalBlocks)
                    continue;

                const std::size_t first = index - static_cast<std::size_t> (radius);
                const std::size_t last = index + static_cast<std::size_t> (radius);
                bool neighboursActive = true;

                for (std::size_t neighbour = first; neighbour <= last && neighboursActive; ++neighbour)
                    neighboursActive = isActive[neighbour];

                if (neighboursActive)
                    eroded.push_back (index);
            }

            return eroded;
        }
    }

    double computeDialogueGatedLoudness (const std::vector<double>& programmeBlockPowers,
                                         const std::vector<double>& dialogueStemBlockPowers)
    {
        if (programmeBlockPowers.size() != dialogueStemBlockPowers.size())
            throw std::invalid_argument ("Dialogue stem and programme must be the same duration");

        const auto activeBlocks = erodeRuns (gatedBlockIndices (dialogueStemBlockPowers),
                                             dialogueStemBlockPowers.size(),
                                             kGatingWindowSubBlocks - 1);

        if (activeBlocks.empty())
            return kNoLoudness;

        return gatedLoudness (programmeBlockPowers, activeBlocks);
    }
}
