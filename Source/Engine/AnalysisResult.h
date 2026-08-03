#pragma once

#include "LoudnessMeter.h"
#include "QualityChecks.h"
#include "TruePeakDetector.h"

#include <string>

namespace qc
{
    struct SourceInfo
    {
        std::string filePath;
        std::string formatName;
        double sampleRate { 0.0 };
        int numChannels { 0 };
        int bitDepth { 0 };
        double durationSeconds { 0.0 };
    };

    /** Everything measured for one file. Deliberately plain data: the verdict engine,
        the graph, the PDF and the JSON writer all consume this and nothing else, so a
        result can be produced in a test without a window, a file, or a JUCE dependency.
    */
    struct AnalysisResult
    {
        SourceInfo source;
        LoudnessMeasurements loudness;
        QualityMeasurements quality;

        double truePeakDb { -std::numeric_limits<double>::infinity() };
        int oversamplingFactor { 1 };
        TruePeakEnvelope truePeakEnvelope;

        /** True peak minus integrated loudness. Meaningless without a loudness reading,
            so check isMeasured (loudness.integratedLufs) first.
        */
        double peakToLoudnessRatioDb { 0.0 };

        /** How much integrated loudness the mono sum loses against the stereo original.
            Zero for mono sources. Above roughly 3 dB the material is wide enough that a
            mono listener hears a different mix.
        */
        double monoCompatibilityLossDb { 0.0 };

        /** Set only when a dialogue stem was supplied. Without it the dialogue-gated
            targets must report NOT MEASURED rather than a pass or a fail.
        */
        bool hasDialogueGatedLoudness { false };
        double dialogueGatedLufs { kNoLoudness };
    };
}
