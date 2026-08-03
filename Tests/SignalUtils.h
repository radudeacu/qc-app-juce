#pragma once

#include <cmath>
#include <complex>
#include <vector>

#include "Engine/KWeighting.h"

namespace qctest
{
    constexpr double kPi = 3.14159265358979323846;

    /** Non-interleaved test signal with the pointer array the meters expect. */
    class Signal
    {
    public:
        Signal (int numChannels, int numSamples)
            : channels (static_cast<std::size_t> (numChannels),
                        std::vector<float> (static_cast<std::size_t> (numSamples), 0.0f))
        {
            pointers.reserve (channels.size());
            for (auto& channel : channels)
                pointers.push_back (channel.data());
        }

        std::vector<float>& operator[] (int channel) { return channels[static_cast<std::size_t> (channel)]; }

        const std::vector<float>& operator[] (int channel) const
        {
            return channels[static_cast<std::size_t> (channel)];
        }

        const float* const* data() const { return pointers.data(); }

        int numSamples() const { return static_cast<int> (channels.front().size()); }
        int numChannels() const { return static_cast<int> (channels.size()); }

    private:
        std::vector<std::vector<float>> channels;
        std::vector<const float*> pointers;
    };

    /** Fills every channel with a sine of the given peak amplitude. */
    inline Signal makeSine (double sampleRate, double frequency, double amplitude,
                            double seconds, int numChannels = 1, double phase = 0.0)
    {
        const int numSamples = static_cast<int> (std::lround (sampleRate * seconds));
        Signal signal (numChannels, numSamples);

        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < numSamples; ++i)
                signal[channel][static_cast<std::size_t> (i)] =
                    static_cast<float> (amplitude * std::sin (2.0 * kPi * frequency
                                                              * static_cast<double> (i) / sampleRate + phase));

        return signal;
    }

    /** Magnitude response of a biquad at a given frequency. Used to predict what the
        meter should read from first principles rather than from a recorded fixture.
    */
    inline double biquadMagnitude (const qc::BiquadCoefficients& c, double frequency, double sampleRate)
    {
        const double omega = 2.0 * kPi * frequency / sampleRate;
        const std::complex<double> z1 = std::polar (1.0, -omega);
        const std::complex<double> z2 = std::polar (1.0, -2.0 * omega);

        const std::complex<double> numerator = c.b0 + c.b1 * z1 + c.b2 * z2;
        const std::complex<double> denominator = 1.0 + c.a1 * z1 + c.a2 * z2;

        return std::abs (numerator / denominator);
    }

    /** K-weighted gain applied to a steady tone at the given frequency. */
    inline double kWeightedGain (double frequency, double sampleRate)
    {
        return biquadMagnitude (qc::designKWeightingShelf (sampleRate), frequency, sampleRate)
             * biquadMagnitude (qc::designKWeightingHighPass (sampleRate), frequency, sampleRate);
    }

    /** Loudness a steady sine should produce, derived from the filter response and the
        BS.1770-4 summation rather than from the meter under test.
    */
    inline double predictedSineLoudness (double frequency, double amplitude,
                                         double sampleRate, int numChannels)
    {
        const double gain = kWeightedGain (frequency, sampleRate);
        const double meanSquarePerChannel = gain * gain * amplitude * amplitude / 2.0;
        return -0.691 + 10.0 * std::log10 (static_cast<double> (numChannels) * meanSquarePerChannel);
    }
}
