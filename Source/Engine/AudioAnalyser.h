#pragma once

#include "AnalysisResult.h"

#include <memory>
#include <vector>

namespace qc
{
    /** Drives every meter over one pass of the audio.

        Callers push arbitrary-sized blocks of non-interleaved float samples; the class
        owns the loudness meter, the true-peak detector, the sample-domain checks, and —
        for stereo sources — a second loudness meter fed the mono downmix.

        The downmix is (L + R) / sqrt(2) rather than (L + R) / 2. That normalisation is
        power-preserving, so identical channels sum to the same loudness as the stereo
        pair and report zero loss, while fully decorrelated content lands at 3 dB. With a
        /2 downmix every mono-compatible file would report a 3 dB loss and the check
        would be useless.
    */
    class AudioAnalyser
    {
    public:
        /** @throws std::invalid_argument for unsupported sample rates or channel counts. */
        AudioAnalyser (double sampleRate, int numChannels);

        void process (const float* const* channelData, int numSamples);

        /** @param info  Source metadata to embed in the result; durationSeconds is
                         recalculated from the samples actually seen.
        */
        AnalysisResult getResult (const SourceInfo& info = {}) const;

        /** Gating-block powers of the main programme, needed to apply dialogue gating
            against a separately analysed stem.
        */
        std::vector<double> getGatingBlockPowers() const { return loudnessMeter.getGatingBlockPowers(); }

        double getSampleRate() const noexcept { return sampleRate; }
        int getNumChannels() const noexcept { return numChannels; }

    private:
        double sampleRate;
        int numChannels;

        LoudnessMeter loudnessMeter;
        TruePeakDetector truePeakDetector;
        QualityAnalyser qualityAnalyser;

        std::unique_ptr<LoudnessMeter> monoDownmixMeter;
        mutable std::vector<float> downmixScratch;

        std::int64_t samplesProcessed { 0 };
    };

    /** Integrated loudness of the programme restricted to the intervals where the
        dialogue stem is active.

        "Active" means the stem's own gating blocks that survive standard BS.1770-4
        gating — the stem is dialogue and nothing else, so its gated blocks are exactly
        the blocks containing speech. Those block indices then select which of the
        programme's blocks are integrated.

        This is not Dolby Dialog Intelligence and will not agree with it sample for
        sample; it is an honest measurement of the mix over the dialogue intervals the
        supplied stem defines.

        @throws std::invalid_argument if the two block series differ in length, which
                means the stem and the mix are not the same duration.
    */
    double computeDialogueGatedLoudness (const std::vector<double>& programmeBlockPowers,
                                         const std::vector<double>& dialogueStemBlockPowers);
}
