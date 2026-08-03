#pragma once

#include "../Engine/AnalysisResult.h"
#include "Target.h"

#include <string>
#include <vector>

namespace qc
{
    enum class Status
    {
        pass,
        warn,
        fail,

        /** The measurement this check needs was not available — a dialogue-gated target
            with no stem loaded, or a file too short or too quiet to measure. Must never
            be rendered as either a pass or a failure.
        */
        notMeasured
    };

    std::string toString (Status status);

    struct CheckResult
    {
        std::string name;
        Status status { Status::pass };
        std::string detail;
    };

    struct TargetVerdict
    {
        std::string targetId;
        std::string targetName;
        Status status { Status::pass };
        std::vector<CheckResult> checks;

        /** The specification this was judged against, carried through so a report can
            state the numbers rather than making the reader look them up - and so that
            lastVerified travels with the verdict. A verdict against an unverified spec
            is worth less than no verdict, and the report must be able to say so.
        */
        double targetIntegratedLufs { 0.0 };
        double targetMaxTruePeakDb { 0.0 };
        std::string targetLastVerified;

        /** Empty when the target passes. Otherwise one line per problem, describing the
            change that would fix it.
        */
        std::string fixHint;
    };

    /** Judges one analysis against one specification.

        Loudness and true peak are pass-or-fail only. Warnings are reserved for the
        sample-domain checks and for loudness-range guidance, so that a green result
        always means "this is deliverable" and never "this is deliverable apart from the
        bit that isn't".
    */
    TargetVerdict evaluate (const AnalysisResult& result, const Target& target);

    std::vector<TargetVerdict> evaluate (const AnalysisResult& result,
                                         const std::vector<Target>& targets);

    /** Worst status across a set of verdicts, for the one-line summary in a batch row. */
    Status overallStatus (const std::vector<TargetVerdict>& verdicts);
}
