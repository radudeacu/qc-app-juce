#include "QualityChecks.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace qc
{
    namespace
    {
        constexpr double kCorrelationBlockSeconds = 0.1;

        // Below this, a block is silence rather than out-of-phase content, and its
        // correlation is undefined. Counting it as negative would flag every fade.
        constexpr double kCorrelationSilenceFloor = 1.0e-10;

        double toDecibels (double magnitude) noexcept
        {
            if (! (magnitude > 0.0))
                return -std::numeric_limits<double>::infinity();

            return 20.0 * std::log10 (magnitude);
        }
    }

    QualityAnalyser::QualityAnalyser (double sampleRateToUse,
                                      int numChannelsToUse,
                                      double clipThresholdDb,
                                      int minimumClipRunToUse)
        : sampleRate (sampleRateToUse),
          numChannels (numChannelsToUse),
          minimumClipRun (minimumClipRunToUse)
    {
        if (! (sampleRate > 0.0))
            throw std::invalid_argument ("QualityAnalyser requires a positive sample rate");

        if (numChannels < 1)
            throw std::invalid_argument ("QualityAnalyser requires at least one channel");

        if (minimumClipRun < 1)
            throw std::invalid_argument ("Minimum clip run must be at least one sample");

        clipThresholdLinear = std::pow (10.0, clipThresholdDb / 20.0);
        correlationBlockSamples = std::max (1, static_cast<int> (std::lround (sampleRate * kCorrelationBlockSeconds)));

        peakMagnitude.assign (static_cast<std::size_t> (numChannels), 0.0);
        currentClipRun.assign (static_cast<std::size_t> (numChannels), 0);
    }

    void QualityAnalyser::closeCorrelationBlock()
    {
        const double denominator = std::sqrt (blockSumLL * blockSumRR);

        if (blockSumLL > kCorrelationSilenceFloor
            && blockSumRR > kCorrelationSilenceFloor
            && denominator > 0.0)
        {
            ++correlationBlocks;

            if (blockSumLR / denominator < 0.0)
                ++negativeCorrelationBlocks;
        }

        blockSumLR = 0.0;
        blockSumLL = 0.0;
        blockSumRR = 0.0;
        samplesIntoBlock = 0;
    }

    void QualityAnalyser::process (const float* const* channelData, int numSamples)
    {
        if (channelData == nullptr)
            throw std::invalid_argument ("process() called with null channel data");

        if (numSamples < 0)
            throw std::invalid_argument ("process() called with a negative sample count");

        for (int channel = 0; channel < numChannels; ++channel)
            if (channelData[channel] == nullptr)
                throw std::invalid_argument ("process() called with a null channel pointer");

        const bool stereo = numChannels >= 2;

        for (int i = 0; i < numSamples; ++i)
        {
            const std::int64_t absoluteIndex = samplesProcessed + i;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const double sample = static_cast<double> (channelData[channel][i]);
                const double magnitude = std::abs (sample);

                auto& peak = peakMagnitude[static_cast<std::size_t> (channel)];
                peak = std::max (peak, magnitude);

                auto& run = currentClipRun[static_cast<std::size_t> (channel)];

                if (magnitude >= clipThresholdLinear)
                {
                    ++run;
                    ++clippedSampleCount;
                }
                else
                {
                    if (run >= minimumClipRun)
                    {
                        ClipEvent event;
                        event.startSeconds = static_cast<double> (absoluteIndex - run) / sampleRate;
                        event.lengthInSamples = run;
                        event.channel = channel;
                        clipEvents.push_back (event);
                    }

                    run = 0;
                }
            }

            if (stereo)
            {
                const double left = static_cast<double> (channelData[0][i]);
                const double right = static_cast<double> (channelData[1][i]);

                blockSumLR += left * right;
                blockSumLL += left * left;
                blockSumRR += right * right;

                totalSumLR += left * right;
                totalSumLL += left * left;
                totalSumRR += right * right;

                if (++samplesIntoBlock == correlationBlockSamples)
                    closeCorrelationBlock();
            }
        }

        samplesProcessed += numSamples;
    }

    QualityMeasurements QualityAnalyser::getMeasurements() const
    {
        QualityMeasurements measurements;
        measurements.isStereo = numChannels >= 2;

        measurements.samplePeakDb.reserve (peakMagnitude.size());
        for (double magnitude : peakMagnitude)
            measurements.samplePeakDb.push_back (toDecibels (magnitude));

        measurements.clipEvents = clipEvents;
        measurements.clippedSampleCount = clippedSampleCount;

        // A clip run still open at end of file is a real event; the streaming loop can
        // only close a run when it sees a sample below threshold.
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const int run = currentClipRun[static_cast<std::size_t> (channel)];

            if (run >= minimumClipRun)
            {
                ClipEvent event;
                event.startSeconds = static_cast<double> (samplesProcessed - run) / sampleRate;
                event.lengthInSamples = run;
                event.channel = channel;
                measurements.clipEvents.push_back (event);
            }
        }

        std::sort (measurements.clipEvents.begin(), measurements.clipEvents.end(),
                   [] (const ClipEvent& a, const ClipEvent& b)
                   {
                       if (a.startSeconds != b.startSeconds)
                           return a.startSeconds < b.startSeconds;

                       return a.channel < b.channel;
                   });

        if (measurements.isStereo)
        {
            const double denominator = std::sqrt (totalSumLL * totalSumRR);
            measurements.correlation = denominator > 0.0 ? totalSumLR / denominator : 1.0;

            if (correlationBlocks > 0)
                measurements.negativeCorrelationFraction = static_cast<double> (negativeCorrelationBlocks)
                                                         / static_cast<double> (correlationBlocks);
        }

        return measurements;
    }
}
