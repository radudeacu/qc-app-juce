#pragma once

#include "../Engine/AnalysisResult.h"
#include "../Verdict/VerdictEngine.h"

#include <string>
#include <vector>

namespace qc
{
    struct JsonReportOptions
    {
        /** The momentary and short-term series. Roughly 20 kB per minute of audio, so
            worth turning off for a batch summary and leaving on for a single file.
        */
        bool includeTimeSeries { true };

        /** True-peak overs listed per target. Capped so that a file clipped end to end
            cannot produce a hundred-megabyte report.
        */
        std::size_t maximumOverEventsPerTarget { 200 };

        std::string generatorName { "QC App" };
        std::string generatorVersion { "0.1.0" };
    };

    /** Serialises a complete analysis, including every verdict, as JSON.

        Values that were never measured - the loudness of a silent file, a dialogue-gated
        figure with no stem loaded - are written as `null`, never as a number and never
        as a placeholder like -999. JSON has no representation for infinity, and a
        consumer that reads -inf as a level would draw exactly the wrong conclusion.
    */
    std::string writeJsonReport (const AnalysisResult& result,
                                 const std::vector<TargetVerdict>& verdicts,
                                 const JsonReportOptions& options = {});

    /** Machine-readable status token: "pass", "warn", "fail", "notMeasured".

        Deliberately separate from toString(), which produces the label shown in the UI.
        Renaming a label on screen must not silently change a wire format that another
        tool is parsing.
    */
    std::string toJsonToken (Status status);
}
