#pragma once

namespace qc
{
    /** Direct-form biquad coefficients, normalised so that a0 == 1. */
    struct BiquadCoefficients
    {
        double b0 { 1.0 };
        double b1 { 0.0 };
        double b2 { 0.0 };
        double a1 { 0.0 };
        double a2 { 0.0 };
    };

    /** Stage 1 of the K-weighting chain: the high-frequency shelf.

        ITU-R BS.1770-4 Table 1 publishes coefficients for 48 kHz only. Reusing those
        values at another sample rate shifts the filter's corner frequency and biases
        every measurement, so the analogue prototype is re-fitted and bilinear-
        transformed at whatever rate is passed in. At 48 kHz this reproduces the
        published table.
    */
    BiquadCoefficients designKWeightingShelf (double sampleRate);

    /** Stage 2 of the K-weighting chain: the RLB high-pass (BS.1770-4 Table 2).
        Re-derived per sample rate for the same reason as the shelf.
    */
    BiquadCoefficients designKWeightingHighPass (double sampleRate);

    /** Transposed direct-form II biquad. Double precision throughout: the RLB
        high-pass sits at 38 Hz, and in single precision its state decays into the
        noise floor over long files.
    */
    class Biquad
    {
    public:
        explicit Biquad (const BiquadCoefficients& coefficients) : coeffs (coefficients) {}

        void reset() noexcept { z1 = 0.0; z2 = 0.0; }

        double process (double x) noexcept
        {
            const double y = coeffs.b0 * x + z1;
            z1 = coeffs.b1 * x - coeffs.a1 * y + z2;
            z2 = coeffs.b2 * x - coeffs.a2 * y;
            return y;
        }

    private:
        BiquadCoefficients coeffs;
        double z1 { 0.0 };
        double z2 { 0.0 };
    };

    /** The complete two-stage K-weighting filter for a single channel. */
    class KWeightingFilter
    {
    public:
        explicit KWeightingFilter (double sampleRate);

        void reset() noexcept;

        double process (double x) noexcept { return highPass.process (shelf.process (x)); }

    private:
        Biquad shelf;
        Biquad highPass;
    };
}
