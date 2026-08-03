#pragma once

#include <cstdint>
#include <limits>
#include <vector>

namespace qc
{
    /** A run of consecutive samples sitting at or above the clip threshold. Distinct
        from a true-peak over: this is digital flat-topping already baked into the file,
        not an inter-sample excursion that appears after reconstruction.
    */
    struct ClipEvent
    {
        double startSeconds { 0.0 };
        int lengthInSamples { 0 };
        int channel { 0 };
    };

    struct QualityMeasurements
    {
        /** Per channel, in dBFS. -infinity for a silent channel. */
        std::vector<double> samplePeakDb;

        std::vector<ClipEvent> clipEvents;
        std::int64_t clippedSampleCount { 0 };

        /** Whole-file Pearson correlation between left and right, -1 to +1.
            Only meaningful for stereo; stays at 1.0 for mono.
        */
        double correlation { 1.0 };

        /** Fraction of the duration, 0 to 1, spent at negative correlation. A brief dip
            is normal; a sustained one is the mono-compatibility problem worth flagging.
        */
        double negativeCorrelationFraction { 0.0 };

        bool isStereo { false };
    };

    /** Sample-domain checks that a loudness number will never catch: hard clipping,
        polarity problems, and the peak levels an engineer needs to see.

        Correlation is evaluated in 100 ms blocks so that the reported fraction means
        "this much of the programme was out of phase", not "the whole file averaged out
        to roughly zero" — which is what a single global figure gives you on material
        that swings between wide and mono.
    */
    class QualityAnalyser
    {
    public:
        /** @param clipThresholdDb   Level at or above which a sample counts toward a clip run.
            @param minimumClipRun    Consecutive samples required before a run is reported.
            @throws std::invalid_argument on a non-positive sample rate or channel count.
        */
        QualityAnalyser (double sampleRate,
                         int numChannels,
                         double clipThresholdDb = -0.1,
                         int minimumClipRun = 3);

        void process (const float* const* channelData, int numSamples);

        QualityMeasurements getMeasurements() const;

    private:
        void closeCorrelationBlock();

        double sampleRate;
        int numChannels;
        double clipThresholdLinear;
        int minimumClipRun;
        int correlationBlockSamples;

        std::vector<double> peakMagnitude;      // per channel
        std::vector<int> currentClipRun;        // per channel
        std::vector<ClipEvent> clipEvents;
        std::int64_t clippedSampleCount { 0 };

        // Running products for the current correlation block, plus whole-file totals.
        double blockSumLR { 0.0 }, blockSumLL { 0.0 }, blockSumRR { 0.0 };
        double totalSumLR { 0.0 }, totalSumLL { 0.0 }, totalSumRR { 0.0 };
        int samplesIntoBlock { 0 };
        int correlationBlocks { 0 };
        int negativeCorrelationBlocks { 0 };

        std::int64_t samplesProcessed { 0 };
    };
}
