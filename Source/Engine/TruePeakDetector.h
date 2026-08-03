#pragma once

#include <cstdint>
#include <vector>

namespace qc
{
    /** Contiguous stretch of audio whose true peak exceeded a ceiling, collapsed into
        one reportable row so a sustained loud passage does not produce thousands.
    */
    struct OverEvent
    {
        double startSeconds { 0.0 };
        double endSeconds { 0.0 };
        double peakDb { 0.0 };
        int channel { 0 };
    };

    /** Per-channel envelope of the highest interpolated magnitude in each time slice,
        in linear units. Kept in the analysis result so over events can be recomputed
        for any ceiling — a different target selection re-reports overs without touching
        the file again.
    */
    struct TruePeakEnvelope
    {
        double sliceSeconds { 0.01 };
        double groupDelaySeconds { 0.0 };
        std::vector<std::vector<float>> channels;
    };

    /** Stretches of the envelope exceeding ceilingDb, ordered by time. Resolution is one
        slice, so a start time is accurate to within sliceSeconds.
    */
    std::vector<OverEvent> findOverEvents (const TruePeakEnvelope& envelope, double ceilingDb);

    /** Number of slices above the ceiling, summed across channels. */
    int countOverSlices (const TruePeakEnvelope& envelope, double ceilingDb) noexcept;

    /** True-peak detector per ITU-R BS.1770-4 Annex 2.

        The signal is oversampled with a Kaiser-windowed sinc polyphase interpolator
        rather than the coefficient table printed in the annex: that table is specified
        for 48 kHz 4x only, and generating the filter keeps the same code correct at
        88.2/96 kHz where 2x is sufficient. Each polyphase branch is normalised to unit
        DC gain, so full-scale DC reads 0 dBTP.

        Alongside the overall maximum the detector keeps a per-channel envelope of the
        highest interpolated magnitude in each 10 ms slice. Over events for any ceiling
        are derived from that envelope, so changing the selected target re-reports overs
        without re-reading the file, and memory stays bounded by duration rather than by
        how loud the material is.
    */
    class TruePeakDetector
    {
    public:
        /** @throws std::invalid_argument for a non-positive sample rate or channel count. */
        TruePeakDetector (double sampleRate, int numChannels);

        void process (const float* const* channelData, int numSamples);

        /** Highest interpolated magnitude seen, in dBTP. Returns -infinity for silence. */
        double getTruePeakDb() const noexcept;

        double getTruePeakLinear() const noexcept { return highestMagnitude; }

        int getOversamplingFactor() const noexcept { return oversamplingFactor; }

        /** Snapshot of the envelope accumulated so far, for storage in the result. */
        TruePeakEnvelope getEnvelope() const;

        /** Convenience for callers holding the live detector. */
        std::vector<OverEvent> getOverEvents (double ceilingDb) const;

        static constexpr double kEnvelopeSeconds = 0.01;

    private:
        void designPolyphaseFilter();
        void closeEnvelopeSlice();

        double sampleRate;
        int numChannels;
        int oversamplingFactor;
        int tapsPerPhase;
        int envelopeSamples;

        std::vector<std::vector<double>> phaseCoefficients; // [phase][tap]
        std::vector<std::vector<double>> history;           // [channel][tap], index 0 newest
        std::vector<std::vector<float>> envelope;           // [channel][slice], linear magnitude
        std::vector<double> currentSlicePeak;               // per channel

        double highestMagnitude { 0.0 };
        double groupDelaySeconds;
        int samplesIntoSlice { 0 };
    };
}
