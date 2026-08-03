#pragma once

#include <optional>
#include <string>
#include <vector>

namespace qc
{
    enum class GatingMode
    {
        /** Standard BS.1770-4 gating over the whole programme. */
        program,

        /** Loudness measured only over the intervals where dialogue is present.
            Requires a dialogue stem; without one the target cannot be judged.
        */
        dialogue
    };

    /** One delivery specification.

        Never construct these from literals in application code — they are loaded from
        targets.json so that a platform changing its numbers is a data edit. The
        lastVerified date travels with the target and is shown in the report, because a
        verdict against an unverified spec is worth less than no verdict at all.
    */
    struct Target
    {
        std::string id;
        std::string name;

        double integratedLufs { -23.0 };

        /** Absent for platforms that normalise on playback rather than reject on
            delivery. For those, loudness is reported and a matching gain is suggested,
            but being off target is not a failure.
        */
        std::optional<double> toleranceLu;

        double maxTruePeakDb { -1.0 };

        GatingMode gating { GatingMode::program };

        /** Guidance only — exceeding it produces a warning, never a failure. */
        std::optional<double> maxLoudnessRangeLu;

        /** ISO date, e.g. "2026-08-03". Empty means never verified. */
        std::string lastVerified;
    };
}
