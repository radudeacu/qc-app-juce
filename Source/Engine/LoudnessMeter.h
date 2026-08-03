#pragma once

#include "KWeighting.h"

#include <cstddef>
#include <limits>
#include <vector>

namespace qc
{
    /** Returned when no measurement block survives gating — a silent or near-silent
        file. Callers must render this as "—" rather than a number.
    */
    constexpr double kNoLoudness = -std::numeric_limits<double>::infinity();

    inline bool isMeasured (double lufs) noexcept { return lufs > kNoLoudness; }

    struct LoudnessMeasurements
    {
        double integratedLufs { kNoLoudness };
        double loudnessRangeLu { 0.0 };
        double maxMomentaryLufs { kNoLoudness };
        double maxShortTermLufs { kNoLoudness };

        /** Momentary (400 ms) and short-term (3 s) series, both stepped every 100 ms.
            These drive the loudness-over-time graph; index i starts at i * 0.1 s.
        */
        std::vector<double> momentaryLufs;
        std::vector<double> shortTermLufs;

        static constexpr double seriesStepSeconds = 0.1;
    };

    /** A gating block is 400 ms and the series hops 100 ms, so each block spans four
        sub-blocks and consecutive blocks overlap by three.
    */
    constexpr int kGatingWindowSubBlocks = 4;

    /** Streaming ITU-R BS.1770-4 / EBU Tech 3341-3342 loudness meter for mono or
        stereo material.

        Audio is consumed in arbitrary-sized chunks and reduced immediately to one
        mean-square value per channel per 100 ms sub-block, so memory grows with
        duration rather than with sample count — an hour of stereo costs well under a
        megabyte. Every published window (400 ms momentary, 3 s short-term, the
        overlapping gating blocks, and the LRA windows) is an average over a whole
        number of those sub-blocks.
    */
    class LoudnessMeter
    {
    public:
        /** @throws std::invalid_argument for unsupported sample rates or channel counts. */
        LoudnessMeter (double sampleRate, int numChannels);

        /** Consumes numSamples frames of non-interleaved audio.
            channelData must hold at least numChannels valid pointers.
        */
        void process (const float* const* channelData, int numSamples);

        /** Measurements over everything processed so far.

            A trailing partial sub-block is deliberately excluded: BS.1770-4 gates over
            complete blocks only, and including a short final block would bias the
            result upward on files that do not divide evenly into 100 ms.
        */
        LoudnessMeasurements getMeasurements() const;

        /** Weighted power of every 400 ms gating block, hopped 100 ms — the block series
            BS.1770-4 gates over. Exposed so that dialogue gating can restrict the same
            blocks to the intervals where a dialogue stem is active.
        */
        std::vector<double> getGatingBlockPowers() const;

        int getNumChannels() const noexcept { return numChannels; }
        double getSampleRate() const noexcept { return sampleRate; }

    private:
        /** Mean weighted power for each window position, in linear units.
            windowBlocks is the window length and stepBlocks the hop, both in sub-blocks.
        */
        std::vector<double> windowedPower (int windowBlocks, int stepBlocks) const;

        double sampleRate;
        int numChannels;
        int subBlockSamples;

        std::vector<KWeightingFilter> filters;
        std::vector<double> sumOfSquares;              // running, per channel
        std::vector<std::vector<double>> subBlockPower; // [channel][sub-block index]
        int samplesIntoSubBlock { 0 };
    };

    /** Loudness of a linear weighted-power sum, per BS.1770-4 equation 2. */
    double loudnessFromPower (double power) noexcept;

    /** Indices of the gating blocks that survive both the absolute (-70 LUFS) and
        relative (-10 LU) gates.
    */
    std::vector<std::size_t> gatedBlockIndices (const std::vector<double>& blockPowers);

    /** Integrated loudness over the given gating blocks. Returns kNoLoudness when
        nothing survives. If restrictTo is non-empty, only those block indices are
        considered — this is how dialogue gating narrows the measurement.
    */
    double gatedLoudness (const std::vector<double>& blockPowers,
                          const std::vector<std::size_t>& restrictTo = {});
}
