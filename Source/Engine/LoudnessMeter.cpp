#include "LoudnessMeter.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace qc
{
    namespace
    {
        // BS.1770-4 equation 2. The offset aligns the K-weighted scale with LKFS/LUFS.
        constexpr double kLoudnessOffsetDb = -0.691;

        constexpr double kSubBlockSeconds = 0.1;

        constexpr int kMomentaryBlocks = kGatingWindowSubBlocks;  // 400 ms
        constexpr int kShortTermBlocks = 30;                      // 3 s

        // BS.1770-4 gating: blocks below -70 LUFS never contribute, and the relative
        // gate sits 10 LU below the ungated mean of the survivors.
        constexpr double kAbsoluteGateLufs = -70.0;
        constexpr double kRelativeGateLu = -10.0;

        // EBU Tech 3342 reuses the absolute gate but widens the relative one to 20 LU,
        // then takes the spread between the 10th and 95th percentiles.
        constexpr double kLraRelativeGateLu = -20.0;
        constexpr int kLraStepBlocks = 10;    // 3 s window advanced 1 s at a time
        constexpr double kLraLowPercentile = 0.10;
        constexpr double kLraHighPercentile = 0.95;

        double percentile (const std::vector<double>& ascending, double fraction) noexcept
        {
            if (ascending.empty())
                return kNoLoudness;

            if (ascending.size() == 1)
                return ascending.front();

            const double position = fraction * static_cast<double> (ascending.size() - 1);
            const auto lower = static_cast<std::size_t> (std::floor (position));
            const auto upper = std::min (lower + 1, ascending.size() - 1);
            const double weight = position - static_cast<double> (lower);

            return ascending[lower] + weight * (ascending[upper] - ascending[lower]);
        }

        double meanOf (const std::vector<double>& values) noexcept
        {
            if (values.empty())
                return 0.0;

            return std::accumulate (values.begin(), values.end(), 0.0)
                 / static_cast<double> (values.size());
        }
    }

    double loudnessFromPower (double power) noexcept
    {
        if (! (power > 0.0))
            return kNoLoudness;

        return kLoudnessOffsetDb + 10.0 * std::log10 (power);
    }

    std::vector<std::size_t> gatedBlockIndices (const std::vector<double>& blockPowers)
    {
        std::vector<std::size_t> absoluteGated;
        absoluteGated.reserve (blockPowers.size());

        for (std::size_t i = 0; i < blockPowers.size(); ++i)
            if (loudnessFromPower (blockPowers[i]) > kAbsoluteGateLufs)
                absoluteGated.push_back (i);

        if (absoluteGated.empty())
            return absoluteGated;

        double sum = 0.0;
        for (std::size_t index : absoluteGated)
            sum += blockPowers[index];

        const double ungatedMean = sum / static_cast<double> (absoluteGated.size());
        const double relativeThreshold = loudnessFromPower (ungatedMean) + kRelativeGateLu;

        std::vector<std::size_t> retained;
        retained.reserve (absoluteGated.size());

        for (std::size_t index : absoluteGated)
            if (loudnessFromPower (blockPowers[index]) > relativeThreshold)
                retained.push_back (index);

        return retained;
    }

    double gatedLoudness (const std::vector<double>& blockPowers,
                          const std::vector<std::size_t>& restrictTo)
    {
        if (blockPowers.empty())
            return kNoLoudness;

        std::vector<double> considered;

        if (restrictTo.empty())
        {
            considered = blockPowers;
        }
        else
        {
            considered.reserve (restrictTo.size());

            for (std::size_t index : restrictTo)
                if (index < blockPowers.size())
                    considered.push_back (blockPowers[index]);
        }

        const auto retained = gatedBlockIndices (considered);

        if (retained.empty())
            return kNoLoudness;

        double sum = 0.0;
        for (std::size_t index : retained)
            sum += considered[index];

        return loudnessFromPower (sum / static_cast<double> (retained.size()));
    }

    LoudnessMeter::LoudnessMeter (double sampleRateToUse, int numChannelsToUse)
        : sampleRate (sampleRateToUse), numChannels (numChannelsToUse)
    {
        if (numChannels != 1 && numChannels != 2)
            throw std::invalid_argument ("LoudnessMeter supports mono and stereo only");

        // Also validates the sample rate and throws if the filters cannot be designed.
        filters.reserve (static_cast<std::size_t> (numChannels));
        for (int channel = 0; channel < numChannels; ++channel)
            filters.emplace_back (sampleRate);

        subBlockSamples = static_cast<int> (std::lround (sampleRate * kSubBlockSeconds));
        if (subBlockSamples <= 0)
            throw std::invalid_argument ("Sample rate is too low to form a 100 ms sub-block");

        sumOfSquares.assign (static_cast<std::size_t> (numChannels), 0.0);
        subBlockPower.resize (static_cast<std::size_t> (numChannels));
    }

    void LoudnessMeter::process (const float* const* channelData, int numSamples)
    {
        if (channelData == nullptr)
            throw std::invalid_argument ("process() called with null channel data");

        if (numSamples < 0)
            throw std::invalid_argument ("process() called with a negative sample count");

        for (int channel = 0; channel < numChannels; ++channel)
            if (channelData[channel] == nullptr)
                throw std::invalid_argument ("process() called with a null channel pointer");

        int offset = 0;

        while (offset < numSamples)
        {
            const int remainingInSubBlock = subBlockSamples - samplesIntoSubBlock;
            const int chunk = std::min (remainingInSubBlock, numSamples - offset);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto& filter = filters[static_cast<std::size_t> (channel)];
                const float* source = channelData[channel] + offset;
                double accumulator = sumOfSquares[static_cast<std::size_t> (channel)];

                for (int i = 0; i < chunk; ++i)
                {
                    const double filtered = filter.process (static_cast<double> (source[i]));
                    accumulator += filtered * filtered;
                }

                sumOfSquares[static_cast<std::size_t> (channel)] = accumulator;
            }

            samplesIntoSubBlock += chunk;
            offset += chunk;

            if (samplesIntoSubBlock == subBlockSamples)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto& runningSum = sumOfSquares[static_cast<std::size_t> (channel)];
                    subBlockPower[static_cast<std::size_t> (channel)]
                        .push_back (runningSum / static_cast<double> (subBlockSamples));
                    runningSum = 0.0;
                }

                samplesIntoSubBlock = 0;
            }
        }
    }

    std::vector<double> LoudnessMeter::windowedPower (int windowBlocks, int stepBlocks) const
    {
        std::vector<double> result;

        const auto available = static_cast<int> (subBlockPower.front().size());
        if (available < windowBlocks || stepBlocks <= 0)
            return result;

        result.reserve (static_cast<std::size_t> ((available - windowBlocks) / stepBlocks + 1));

        for (int start = 0; start + windowBlocks <= available; start += stepBlocks)
        {
            // Channel weights G are 1.0 for left, right and centre, so the weighted sum
            // is a plain sum here. Surround weighting would apply per-channel gains.
            double weightedPower = 0.0;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const auto& blocks = subBlockPower[static_cast<std::size_t> (channel)];
                double channelPower = 0.0;

                for (int i = 0; i < windowBlocks; ++i)
                    channelPower += blocks[static_cast<std::size_t> (start + i)];

                weightedPower += channelPower / static_cast<double> (windowBlocks);
            }

            result.push_back (weightedPower);
        }

        return result;
    }

    std::vector<double> LoudnessMeter::getGatingBlockPowers() const
    {
        if (subBlockPower.empty() || subBlockPower.front().empty())
            return {};

        return windowedPower (kMomentaryBlocks, 1);
    }

    LoudnessMeasurements LoudnessMeter::getMeasurements() const
    {
        LoudnessMeasurements measurements;

        if (subBlockPower.empty() || subBlockPower.front().empty())
            return measurements;

        const auto momentaryPower = windowedPower (kMomentaryBlocks, 1);
        const auto shortTermPower = windowedPower (kShortTermBlocks, 1);

        measurements.momentaryLufs.reserve (momentaryPower.size());
        for (double power : momentaryPower)
            measurements.momentaryLufs.push_back (loudnessFromPower (power));

        measurements.shortTermLufs.reserve (shortTermPower.size());
        for (double power : shortTermPower)
            measurements.shortTermLufs.push_back (loudnessFromPower (power));

        if (! measurements.momentaryLufs.empty())
            measurements.maxMomentaryLufs = *std::max_element (measurements.momentaryLufs.begin(),
                                                               measurements.momentaryLufs.end());

        if (! measurements.shortTermLufs.empty())
            measurements.maxShortTermLufs = *std::max_element (measurements.shortTermLufs.begin(),
                                                               measurements.shortTermLufs.end());

        // Integrated loudness. The gating blocks are the 400 ms windows at 75% overlap,
        // which is exactly the momentary series.
        measurements.integratedLufs = gatedLoudness (momentaryPower);

        // Loudness range, EBU Tech 3342.
        const auto lraPower = windowedPower (kShortTermBlocks, kLraStepBlocks);

        std::vector<double> lraAbsoluteGated;
        lraAbsoluteGated.reserve (lraPower.size());

        for (double power : lraPower)
            if (loudnessFromPower (power) > kAbsoluteGateLufs)
                lraAbsoluteGated.push_back (power);

        if (! lraAbsoluteGated.empty())
        {
            const double relativeThreshold = loudnessFromPower (meanOf (lraAbsoluteGated)) + kLraRelativeGateLu;

            std::vector<double> retained;
            retained.reserve (lraAbsoluteGated.size());

            for (double power : lraAbsoluteGated)
            {
                const double loudness = loudnessFromPower (power);
                if (loudness > relativeThreshold)
                    retained.push_back (loudness);
            }

            if (retained.size() > 1)
            {
                std::sort (retained.begin(), retained.end());
                measurements.loudnessRangeLu = percentile (retained, kLraHighPercentile)
                                             - percentile (retained, kLraLowPercentile);
            }
        }

        return measurements;
    }
}
