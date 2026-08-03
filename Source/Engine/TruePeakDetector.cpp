#include "TruePeakDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qc
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        // 24 taps per phase puts the stopband well below the measurement tolerance;
        // BS.1770-4's own table uses 12, so this is deliberately on the safe side.
        constexpr int kTapsPerPhase = 24;

        // ~-90 dB sidelobes.
        constexpr double kKaiserBeta = 9.0;

        double besselI0 (double x) noexcept
        {
            double sum = 1.0;
            double term = 1.0;

            for (int k = 1; k < 64; ++k)
            {
                const double ratio = x / (2.0 * k);
                term *= ratio * ratio;
                sum += term;

                if (term < 1.0e-16 * sum)
                    break;
            }

            return sum;
        }

        double sinc (double x) noexcept
        {
            if (std::abs (x) < 1.0e-12)
                return 1.0;

            const double piX = kPi * x;
            return std::sin (piX) / piX;
        }

        double toDecibels (double magnitude) noexcept
        {
            if (! (magnitude > 0.0))
                return -std::numeric_limits<double>::infinity();

            return 20.0 * std::log10 (magnitude);
        }
    }

    TruePeakDetector::TruePeakDetector (double sampleRateToUse, int numChannelsToUse)
        : sampleRate (sampleRateToUse), numChannels (numChannelsToUse), tapsPerPhase (kTapsPerPhase)
    {
        if (! (sampleRate > 0.0))
            throw std::invalid_argument ("TruePeakDetector requires a positive sample rate");

        if (numChannels < 1)
            throw std::invalid_argument ("TruePeakDetector requires at least one channel");

        // BS.1770-4 asks for an effective rate of at least 192 kHz. Beyond that the
        // inter-sample peaks are already resolved and further oversampling buys nothing.
        if (sampleRate < 96000.0)
            oversamplingFactor = 4;
        else if (sampleRate < 192000.0)
            oversamplingFactor = 2;
        else
            oversamplingFactor = 1;

        envelopeSamples = std::max (1, static_cast<int> (std::lround (sampleRate * kEnvelopeSeconds)));

        // The interpolator is symmetric, so its output lags the input by half its
        // length. Timestamps are corrected by that amount or every reported over points
        // slightly past the event that caused it.
        groupDelaySeconds = (static_cast<double> (tapsPerPhase - 1) / 2.0) / sampleRate;

        designPolyphaseFilter();

        history.assign (static_cast<std::size_t> (numChannels),
                        std::vector<double> (static_cast<std::size_t> (tapsPerPhase), 0.0));
        envelope.resize (static_cast<std::size_t> (numChannels));
        currentSlicePeak.assign (static_cast<std::size_t> (numChannels), 0.0);
    }

    void TruePeakDetector::designPolyphaseFilter()
    {
        const int prototypeLength = tapsPerPhase * oversamplingFactor;
        const double cutoff = 1.0 / static_cast<double> (oversamplingFactor); // 2 * fc at the upsampled rate
        const double centre = static_cast<double> (prototypeLength - 1) / 2.0;
        const double denominator = besselI0 (kKaiserBeta);

        std::vector<double> prototype (static_cast<std::size_t> (prototypeLength), 0.0);

        for (int n = 0; n < prototypeLength; ++n)
        {
            const double offset = static_cast<double> (n) - centre;
            const double normalised = offset / centre;
            const double window = besselI0 (kKaiserBeta * std::sqrt (std::max (0.0, 1.0 - normalised * normalised)))
                                / denominator;

            prototype[static_cast<std::size_t> (n)] = cutoff * sinc (cutoff * offset) * window;
        }

        phaseCoefficients.assign (static_cast<std::size_t> (oversamplingFactor),
                                  std::vector<double> (static_cast<std::size_t> (tapsPerPhase), 0.0));

        for (int phase = 0; phase < oversamplingFactor; ++phase)
        {
            auto& coefficients = phaseCoefficients[static_cast<std::size_t> (phase)];
            double sum = 0.0;

            for (int tap = 0; tap < tapsPerPhase; ++tap)
            {
                const double value = prototype[static_cast<std::size_t> (tap * oversamplingFactor + phase)];
                coefficients[static_cast<std::size_t> (tap)] = value;
                sum += value;
            }

            // Normalise each branch to unit DC gain. Without this the branches differ by
            // a fraction of a dB and a steady tone appears to wobble in level.
            if (std::abs (sum) > 1.0e-12)
                for (double& coefficient : coefficients)
                    coefficient /= sum;
        }
    }

    void TruePeakDetector::closeEnvelopeSlice()
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto& peak = currentSlicePeak[static_cast<std::size_t> (channel)];
            envelope[static_cast<std::size_t> (channel)].push_back (static_cast<float> (peak));
            peak = 0.0;
        }

        samplesIntoSlice = 0;
    }

    void TruePeakDetector::process (const float* const* channelData, int numSamples)
    {
        if (channelData == nullptr)
            throw std::invalid_argument ("process() called with null channel data");

        if (numSamples < 0)
            throw std::invalid_argument ("process() called with a negative sample count");

        for (int channel = 0; channel < numChannels; ++channel)
            if (channelData[channel] == nullptr)
                throw std::invalid_argument ("process() called with a null channel pointer");

        for (int i = 0; i < numSamples; ++i)
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto& channelHistory = history[static_cast<std::size_t> (channel)];

                for (int tap = tapsPerPhase - 1; tap > 0; --tap)
                    channelHistory[static_cast<std::size_t> (tap)] = channelHistory[static_cast<std::size_t> (tap - 1)];

                const double input = static_cast<double> (channelData[channel][i]);
                channelHistory[0] = input;

                // The raw sample is a lower bound on the true peak; including it
                // guarantees the reported figure is never below the sample peak.
                double localMaximum = std::abs (input);

                for (int phase = 0; phase < oversamplingFactor; ++phase)
                {
                    const auto& coefficients = phaseCoefficients[static_cast<std::size_t> (phase)];
                    double accumulator = 0.0;

                    for (int tap = 0; tap < tapsPerPhase; ++tap)
                        accumulator += coefficients[static_cast<std::size_t> (tap)]
                                     * channelHistory[static_cast<std::size_t> (tap)];

                    localMaximum = std::max (localMaximum, std::abs (accumulator));
                }

                highestMagnitude = std::max (highestMagnitude, localMaximum);

                auto& slicePeak = currentSlicePeak[static_cast<std::size_t> (channel)];
                slicePeak = std::max (slicePeak, localMaximum);
            }

            if (++samplesIntoSlice == envelopeSamples)
                closeEnvelopeSlice();
        }
    }

    double TruePeakDetector::getTruePeakDb() const noexcept
    {
        return toDecibels (highestMagnitude);
    }

    TruePeakEnvelope TruePeakDetector::getEnvelope() const
    {
        TruePeakEnvelope snapshot;
        snapshot.sliceSeconds = kEnvelopeSeconds;
        snapshot.groupDelaySeconds = groupDelaySeconds;
        snapshot.channels = envelope;
        return snapshot;
    }

    std::vector<OverEvent> TruePeakDetector::getOverEvents (double ceilingDb) const
    {
        return findOverEvents (getEnvelope(), ceilingDb);
    }

    std::vector<OverEvent> findOverEvents (const TruePeakEnvelope& envelope, double ceilingDb)
    {
        std::vector<OverEvent> events;
        const double ceilingLinear = std::pow (10.0, ceilingDb / 20.0);
        const auto numChannels = static_cast<int> (envelope.channels.size());

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto& slices = envelope.channels[static_cast<std::size_t> (channel)];
            std::size_t index = 0;

            while (index < slices.size())
            {
                if (static_cast<double> (slices[index]) <= ceilingLinear)
                {
                    ++index;
                    continue;
                }

                const std::size_t start = index;
                double peak = 0.0;

                while (index < slices.size() && static_cast<double> (slices[index]) > ceilingLinear)
                {
                    peak = std::max (peak, static_cast<double> (slices[index]));
                    ++index;
                }

                OverEvent event;
                event.startSeconds = std::max (0.0, static_cast<double> (start) * envelope.sliceSeconds
                                                        - envelope.groupDelaySeconds);
                event.endSeconds = std::max (event.startSeconds,
                                             static_cast<double> (index) * envelope.sliceSeconds
                                                 - envelope.groupDelaySeconds);
                event.peakDb = toDecibels (peak);
                event.channel = channel;
                events.push_back (event);
            }
        }

        std::sort (events.begin(), events.end(),
                   [] (const OverEvent& a, const OverEvent& b)
                   {
                       if (a.startSeconds != b.startSeconds)
                           return a.startSeconds < b.startSeconds;

                       return a.channel < b.channel;
                   });

        return events;
    }

    int countOverSlices (const TruePeakEnvelope& envelope, double ceilingDb) noexcept
    {
        const double ceilingLinear = std::pow (10.0, ceilingDb / 20.0);
        int count = 0;

        for (const auto& slices : envelope.channels)
            for (float value : slices)
                if (static_cast<double> (value) > ceilingLinear)
                    ++count;

        return count;
    }
}
