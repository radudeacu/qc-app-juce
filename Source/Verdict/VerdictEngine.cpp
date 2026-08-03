#include "VerdictEngine.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace qc
{
    namespace
    {
        // Sample-domain warning thresholds. These are house limits, not published specs,
        // which is why they live here rather than in targets.json.
        constexpr double kNegativeCorrelationWarnFraction = 0.05;
        constexpr double kMonoCompatibilityWarnDb = 3.0;

        /** Suppresses "-0.0" and rounds to the resolution actually shown. */
        double tidy (double value) noexcept
        {
            return std::abs (value) < 0.05 ? 0.0 : value;
        }

        std::string format (double value, int decimals = 1)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision (decimals) << tidy (value);
            return stream.str();
        }

        std::string formatSigned (double value, int decimals = 1)
        {
            const double tidied = tidy (value);
            std::ostringstream stream;
            stream << std::fixed << std::setprecision (decimals);

            if (tidied > 0.0)
                stream << "+";

            stream << tidied;
            return stream.str();
        }

        void appendLine (std::string& text, const std::string& line)
        {
            if (! text.empty())
                text += "\n";

            text += line;
        }

        Status worse (Status a, Status b) noexcept
        {
            const auto rank = [] (Status status)
            {
                switch (status)
                {
                    case Status::pass:        return 0;
                    case Status::warn:        return 1;
                    case Status::notMeasured: return 2;
                    case Status::fail:        return 3;
                }

                return 0;
            };

            return rank (a) >= rank (b) ? a : b;
        }
    }

    std::string toString (Status status)
    {
        switch (status)
        {
            case Status::pass:        return "PASS";
            case Status::warn:        return "WARN";
            case Status::fail:        return "FAIL";
            case Status::notMeasured: return "NOT MEASURED";
        }

        return "NOT MEASURED";
    }

    TargetVerdict evaluate (const AnalysisResult& result, const Target& target)
    {
        TargetVerdict verdict;
        verdict.targetId = target.id;
        verdict.targetName = target.name;

        // Which loudness figure applies depends on how the target gates. A dialogue-gated
        // spec judged against programme loudness would be a different measurement wearing
        // the same name, so it reports NOT MEASURED instead.
        const bool needsDialogueGate = target.gating == GatingMode::dialogue;
        const double measuredLufs = needsDialogueGate ? result.dialogueGatedLufs
                                                      : result.loudness.integratedLufs;
        const bool loudnessAvailable = needsDialogueGate
                                     ? (result.hasDialogueGatedLoudness && isMeasured (measuredLufs))
                                     : isMeasured (measuredLufs);

        CheckResult loudnessCheck;
        loudnessCheck.name = needsDialogueGate ? "Integrated loudness (dialogue-gated)"
                                               : "Integrated loudness";

        if (! loudnessAvailable)
        {
            loudnessCheck.status = Status::notMeasured;
            loudnessCheck.detail = needsDialogueGate
                                 ? "Dialogue stem required — load one to judge this target"
                                 : "File is silent or falls below the -70 LUFS gate";
        }
        else
        {
            const double deviation = measuredLufs - target.integratedLufs;

            if (target.toleranceLu.has_value())
            {
                const bool withinTolerance = std::abs (deviation) <= *target.toleranceLu;
                loudnessCheck.status = withinTolerance ? Status::pass : Status::fail;
                loudnessCheck.detail = format (measuredLufs) + " LUFS ("
                                     + formatSigned (deviation) + " LU vs target "
                                     + format (target.integratedLufs) + " +/-"
                                     + format (*target.toleranceLu) + ")";
            }
            else
            {
                // Platform normalises on playback, so being off target is information
                // rather than a rejection.
                loudnessCheck.status = Status::pass;
                loudnessCheck.detail = format (measuredLufs) + " LUFS ("
                                     + formatSigned (deviation) + " LU vs target "
                                     + format (target.integratedLufs)
                                     + "; platform normalises on playback)";
            }
        }

        verdict.checks.push_back (loudnessCheck);

        CheckResult truePeakCheck;
        truePeakCheck.name = "True peak";

        if (! isMeasured (result.truePeakDb))
        {
            truePeakCheck.status = Status::notMeasured;
            truePeakCheck.detail = "No signal";
        }
        else
        {
            const bool exceedsCeiling = result.truePeakDb > target.maxTruePeakDb;
            truePeakCheck.status = exceedsCeiling ? Status::fail : Status::pass;
            truePeakCheck.detail = format (result.truePeakDb) + " dBTP (ceiling "
                                 + format (target.maxTruePeakDb) + " dBTP)";

            if (exceedsCeiling)
            {
                const auto overs = findOverEvents (result.truePeakEnvelope, target.maxTruePeakDb);
                truePeakCheck.detail += ", " + std::to_string (overs.size())
                                      + (overs.size() == 1 ? " over" : " overs");
            }
        }

        verdict.checks.push_back (truePeakCheck);

        CheckResult loudnessRangeCheck;
        loudnessRangeCheck.name = "Loudness range";
        loudnessRangeCheck.detail = format (result.loudness.loudnessRangeLu) + " LU";

        if (target.maxLoudnessRangeLu.has_value()
            && result.loudness.loudnessRangeLu > *target.maxLoudnessRangeLu)
        {
            loudnessRangeCheck.status = Status::warn;
            loudnessRangeCheck.detail += " (guidance " + format (*target.maxLoudnessRangeLu) + " LU)";
        }

        verdict.checks.push_back (loudnessRangeCheck);

        if (! result.quality.clipEvents.empty())
        {
            CheckResult clipping;
            clipping.name = "Clipping";
            clipping.status = Status::warn;
            clipping.detail = std::to_string (result.quality.clipEvents.size())
                            + " clipped run(s), first at " + format (result.quality.clipEvents.front().startSeconds, 2) + " s";
            verdict.checks.push_back (clipping);
        }

        if (result.quality.isStereo)
        {
            if (result.quality.negativeCorrelationFraction > kNegativeCorrelationWarnFraction)
            {
                CheckResult correlation;
                correlation.name = "Stereo correlation";
                correlation.status = Status::warn;
                correlation.detail = format (result.quality.negativeCorrelationFraction * 100.0)
                                   + "% of the programme is out of phase (correlation "
                                   + format (result.quality.correlation, 2) + ")";
                verdict.checks.push_back (correlation);
            }

            if (result.monoCompatibilityLossDb > kMonoCompatibilityWarnDb)
            {
                CheckResult monoCompatibility;
                monoCompatibility.name = "Mono compatibility";
                monoCompatibility.status = Status::warn;
                monoCompatibility.detail = std::isfinite (result.monoCompatibilityLossDb)
                                         ? "Mono sum loses " + format (result.monoCompatibilityLossDb)
                                               + " dB against the stereo original"
                                         : "Mono sum cancels to silence - the channels are out of phase";
                verdict.checks.push_back (monoCompatibility);
            }
        }

        for (const auto& check : verdict.checks)
            verdict.status = worse (verdict.status, check.status);

        // Fix hints. Everything below is arithmetic on measured values: the gain figure
        // and the resulting true peak are exact, because gain is linear. Only the amount
        // of limiting is an estimate, and it is worded as one.
        if (loudnessAvailable && isMeasured (result.truePeakDb))
        {
            const double gainNeeded = target.integratedLufs - measuredLufs;
            const double truePeakAfterGain = result.truePeakDb + gainNeeded;

            if (loudnessCheck.status == Status::fail)
            {
                if (truePeakAfterGain <= target.maxTruePeakDb)
                {
                    appendLine (verdict.fixHint,
                                "Apply " + formatSigned (gainNeeded) + " dB gain -> "
                                    + format (target.integratedLufs) + " LUFS, true peak lands at "
                                    + format (truePeakAfterGain) + " dBTP. Passes.");
                }
                else
                {
                    appendLine (verdict.fixHint,
                                "Apply " + formatSigned (gainNeeded) + " dB gain -> "
                                    + format (target.integratedLufs) + " LUFS, but true peak would hit "
                                    + format (truePeakAfterGain) + " dBTP (ceiling "
                                    + format (target.maxTruePeakDb) + "). Needs limiting, around "
                                    + format (truePeakAfterGain - target.maxTruePeakDb)
                                    + " dB of gain reduction.");
                }
            }
            else if (truePeakCheck.status == Status::fail)
            {
                appendLine (verdict.fixHint,
                            "Loudness passes. Reduce true peak by "
                                + format (result.truePeakDb - target.maxTruePeakDb)
                                + " dB - limiter ceiling at " + format (target.maxTruePeakDb) + " dBTP.");
            }
        }

        if (loudnessRangeCheck.status == Status::warn)
            appendLine (verdict.fixHint,
                        "LRA " + format (result.loudness.loudnessRangeLu)
                            + " LU is wide for this target - compression or level-riding needed.");

        return verdict;
    }

    std::vector<TargetVerdict> evaluate (const AnalysisResult& result,
                                         const std::vector<Target>& targets)
    {
        std::vector<TargetVerdict> verdicts;
        verdicts.reserve (targets.size());

        for (const auto& target : targets)
            verdicts.push_back (evaluate (result, target));

        return verdicts;
    }

    Status overallStatus (const std::vector<TargetVerdict>& verdicts)
    {
        Status status = Status::pass;

        for (const auto& verdict : verdicts)
            status = worse (status, verdict.status);

        return status;
    }
}
