#include "KWeighting.h"

#include <cmath>
#include <stdexcept>

namespace qc
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        // Analogue prototype parameters for the BS.1770-4 stage 1 shelf. These are the
        // values that, put through the bilinear transform at 48 kHz, reproduce the
        // coefficients printed in Table 1 of the recommendation.
        constexpr double kShelfCentreHz = 1681.974450955533;
        constexpr double kShelfGainDb   = 3.999843853973347;
        constexpr double kShelfQ        = 0.7071752369554196;

        // Stage 2, the RLB high-pass of Table 2, expressed the same way.
        constexpr double kHighPassCentreHz = 38.13547087602444;
        constexpr double kHighPassQ        = 0.5003270373238773;

        void requireValidSampleRate (double sampleRate)
        {
            if (! (sampleRate > 0.0))
                throw std::invalid_argument ("K-weighting requires a positive sample rate");

            // Both stages are pre-warped with tan(pi * f0 / fs). Above the point where
            // the shelf's centre frequency approaches Nyquist the transform folds over
            // and the filter is meaningless rather than merely inaccurate.
            if (sampleRate <= 2.0 * kShelfCentreHz)
                throw std::invalid_argument ("K-weighting requires a sample rate above 3364 Hz");
        }
    }

    BiquadCoefficients designKWeightingShelf (double sampleRate)
    {
        requireValidSampleRate (sampleRate);

        const double K  = std::tan (kPi * kShelfCentreHz / sampleRate);
        const double Vh = std::pow (10.0, kShelfGainDb / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / kShelfQ + K * K;

        BiquadCoefficients c;
        c.b0 = (Vh + Vb * K / kShelfQ + K * K) / a0;
        c.b1 = 2.0 * (K * K - Vh) / a0;
        c.b2 = (Vh - Vb * K / kShelfQ + K * K) / a0;
        c.a1 = 2.0 * (K * K - 1.0) / a0;
        c.a2 = (1.0 - K / kShelfQ + K * K) / a0;
        return c;
    }

    BiquadCoefficients designKWeightingHighPass (double sampleRate)
    {
        requireValidSampleRate (sampleRate);

        const double K  = std::tan (kPi * kHighPassCentreHz / sampleRate);
        const double a0 = 1.0 + K / kHighPassQ + K * K;

        BiquadCoefficients c;
        c.b0 = 1.0;
        c.b1 = -2.0;
        c.b2 = 1.0;
        c.a1 = 2.0 * (K * K - 1.0) / a0;
        c.a2 = (1.0 - K / kHighPassQ + K * K) / a0;
        return c;
    }

    KWeightingFilter::KWeightingFilter (double sampleRate)
        : shelf (designKWeightingShelf (sampleRate)),
          highPass (designKWeightingHighPass (sampleRate))
    {
    }

    void KWeightingFilter::reset() noexcept
    {
        shelf.reset();
        highPass.reset();
    }
}
