#include "TestHarness.h"

#include "Verdict/VerdictEngine.h"

using namespace qctest;

namespace
{
    qc::Target makeR128()
    {
        qc::Target target;
        target.id = "ebu-r128";
        target.name = "EBU R128";
        target.integratedLufs = -23.0;
        target.toleranceLu = 0.5;
        target.maxTruePeakDb = -1.0;
        return target;
    }

    qc::Target makeSpotify()
    {
        qc::Target target;
        target.id = "spotify";
        target.name = "Spotify";
        target.integratedLufs = -14.0;
        target.maxTruePeakDb = -1.0;
        return target;
    }

    qc::Target makeNetflix()
    {
        qc::Target target;
        target.id = "netflix";
        target.name = "Netflix";
        target.integratedLufs = -27.0;
        target.toleranceLu = 2.0;
        target.maxTruePeakDb = -2.0;
        target.gating = qc::GatingMode::dialogue;
        return target;
    }

    qc::AnalysisResult makeResult (double integratedLufs, double truePeakDb, double lra = 6.0)
    {
        qc::AnalysisResult result;
        result.loudness.integratedLufs = integratedLufs;
        result.loudness.loudnessRangeLu = lra;
        result.truePeakDb = truePeakDb;
        result.quality.isStereo = true;
        return result;
    }

    bool contains (const std::string& haystack, const std::string& needle)
    {
        return haystack.find (needle) != std::string::npos;
    }

    const qc::CheckResult& findCheck (const qc::TargetVerdict& verdict, const std::string& name)
    {
        for (const auto& check : verdict.checks)
            if (check.name == name)
                return check;

        fail ("no check named '" + name + "' in the verdict");
        throw std::logic_error ("unreachable");
    }
}

QC_TEST (compliantFilePasses)
{
    const auto verdict = qc::evaluate (makeResult (-23.1, -1.4), makeR128());

    check (verdict.status == qc::Status::pass, "an on-spec file should pass");
    check (verdict.fixHint.empty(), "a passing file needs no fix hint");
}

QC_TEST (loudnessOutsideToleranceFails)
{
    const auto verdict = qc::evaluate (makeResult (-20.0, -4.5), makeR128());

    check (verdict.status == qc::Status::fail, "3 LU over target is a failure at R128");
    check (findCheck (verdict, "Integrated loudness").status == qc::Status::fail, "loudness check");
}

QC_TEST (fixHintGivesExactGainWhenHeadroomAllowsIt)
{
    // -20 LUFS needs -3.0 dB to reach -23. True peak -4.5 dBTP then lands at -7.5,
    // comfortably under the -1.0 ceiling.
    const auto verdict = qc::evaluate (makeResult (-20.0, -4.5), makeR128());

    check (contains (verdict.fixHint, "-3.0 dB gain"), "hint should state the exact gain: " + verdict.fixHint);
    check (contains (verdict.fixHint, "-7.5 dBTP"), "hint should state the resulting true peak: " + verdict.fixHint);
    check (contains (verdict.fixHint, "Passes."), "hint should confirm the fix is sufficient");
}

QC_TEST (fixHintWarnsWhenGainWouldBreachTheCeiling)
{
    // -30 LUFS needs +7.0 dB to reach -23, which would drive -0.5 dBTP up to +6.5.
    const auto verdict = qc::evaluate (makeResult (-30.0, -0.5), makeR128());

    check (contains (verdict.fixHint, "+7.0 dB gain"), "hint should state the gain: " + verdict.fixHint);
    check (contains (verdict.fixHint, "Needs limiting"), "hint should call for limiting: " + verdict.fixHint);
    check (contains (verdict.fixHint, "7.5 dB"), "hint should estimate the gain reduction: " + verdict.fixHint);
}

QC_TEST (truePeakOnlyFailureProducesItsOwnHint)
{
    const auto verdict = qc::evaluate (makeResult (-23.0, -0.3), makeR128());

    check (verdict.status == qc::Status::fail, "a true-peak over is a failure, not a warning");
    check (findCheck (verdict, "Integrated loudness").status == qc::Status::pass, "loudness is fine");
    check (contains (verdict.fixHint, "Loudness passes"), "hint should say so: " + verdict.fixHint);
    check (contains (verdict.fixHint, "0.7 dB"), "hint should state the reduction needed: " + verdict.fixHint);
}

QC_TEST (loudnessInToleranceWithTruePeakOverIsFailNotWarn)
{
    const auto verdict = qc::evaluate (makeResult (-23.2, -0.9), makeR128());
    check (verdict.status == qc::Status::fail,
           "the PRD is explicit: this combination is a failure");
}

QC_TEST (normalisedPlatformsDoNotFailOnLoudness)
{
    // Spotify normalises on playback, so being 6 LU quiet is information, not rejection.
    const auto verdict = qc::evaluate (makeResult (-20.0, -1.5), makeSpotify());

    check (verdict.status == qc::Status::pass, "off-target loudness is not a failure here");

    const auto& loudness = findCheck (verdict, "Integrated loudness");
    check (contains (loudness.detail, "normalises"), "the detail should explain why: " + loudness.detail);
    // Measured minus target: the file sits 6 LU below where the platform wants it.
    check (contains (loudness.detail, "-6.0 LU"), "the deviation should still be reported: " + loudness.detail);
}

QC_TEST (dialogueGatedTargetWithoutStemIsNotMeasured)
{
    const auto verdict = qc::evaluate (makeResult (-27.0, -3.0), makeNetflix());

    check (verdict.status == qc::Status::notMeasured,
           "Netflix cannot be judged without a dialogue stem");
    check (verdict.status != qc::Status::pass, "and must never be rendered as a pass");

    const auto& loudness = findCheck (verdict, "Integrated loudness (dialogue-gated)");
    check (loudness.status == qc::Status::notMeasured, "loudness check status");
    check (contains (loudness.detail, "Dialogue stem required"), "detail: " + loudness.detail);
}

QC_TEST (dialogueGatedTargetIsJudgedOnceAStemIsLoaded)
{
    auto result = makeResult (-20.0, -3.0);
    result.hasDialogueGatedLoudness = true;
    result.dialogueGatedLufs = -27.4;

    const auto verdict = qc::evaluate (result, makeNetflix());

    check (verdict.status == qc::Status::pass, "-27.4 is inside the +/-2 LU tolerance");

    const auto& loudness = findCheck (verdict, "Integrated loudness (dialogue-gated)");
    check (contains (loudness.detail, "-27.4"),
           "the dialogue-gated figure must be the one judged, not the programme figure: "
               + loudness.detail);
}

QC_TEST (silentFileIsNotMeasuredRatherThanFailed)
{
    qc::AnalysisResult silent;
    const auto verdict = qc::evaluate (silent, makeR128());

    check (verdict.status == qc::Status::notMeasured, "a silent file cannot pass or fail");
    check (verdict.fixHint.empty(), "and there is nothing to suggest");
}

QC_TEST (wideLoudnessRangeWarnsButDoesNotFail)
{
    auto target = makeR128();
    target.maxLoudnessRangeLu = 15.0;

    const auto verdict = qc::evaluate (makeResult (-23.0, -2.0, 21.4), target);

    check (verdict.status == qc::Status::warn, "LRA guidance is a warning, never a failure");
    check (contains (verdict.fixHint, "21.4 LU is wide"), "hint: " + verdict.fixHint);
    check (contains (verdict.fixHint, "compression or level-riding"), "hint: " + verdict.fixHint);
}

QC_TEST (clippingRaisesAWarningOnAnOtherwiseCompliantFile)
{
    auto result = makeResult (-23.0, -2.0);
    result.quality.clipEvents.push_back ({ 12.5, 40, 0 });

    const auto verdict = qc::evaluate (result, makeR128());

    check (verdict.status == qc::Status::warn, "clipping warns");
    check (contains (findCheck (verdict, "Clipping").detail, "12.50 s"), "with a timestamp");
}

QC_TEST (outOfPhaseMaterialWarns)
{
    auto result = makeResult (-23.0, -2.0);
    result.quality.negativeCorrelationFraction = 0.42;
    result.quality.correlation = -0.31;

    const auto verdict = qc::evaluate (result, makeR128());

    check (verdict.status == qc::Status::warn, "sustained negative correlation warns");
    check (contains (findCheck (verdict, "Stereo correlation").detail, "42.0%"), "with the proportion");
}

QC_TEST (briefPhaseDipsDoNotWarn)
{
    auto result = makeResult (-23.0, -2.0);
    result.quality.negativeCorrelationFraction = 0.02;

    check (qc::evaluate (result, makeR128()).status == qc::Status::pass,
           "2% out of phase is normal programme material");
}

QC_TEST (cancellingChannelsAreDescribedNotNumbered)
{
    auto result = makeResult (-23.0, -2.0);
    result.monoCompatibilityLossDb = std::numeric_limits<double>::infinity();

    const auto verdict = qc::evaluate (result, makeR128());

    check (verdict.status == qc::Status::warn, "total cancellation warns");
    check (contains (findCheck (verdict, "Mono compatibility").detail, "cancels to silence"),
           "an infinite loss must read as words, not as 'inf dB'");
}

QC_TEST (overallStatusTakesTheWorstResult)
{
    const auto result = makeResult (-20.0, -0.2);
    const std::vector<qc::Target> targets { makeR128(), makeSpotify() };

    const auto verdicts = qc::evaluate (result, targets);
    check (verdicts.size() == 2, "one verdict per target");
    check (qc::overallStatus (verdicts) == qc::Status::fail, "any failure dominates the summary");
}

QC_TEST (notMeasuredOutranksWarnInASummary)
{
    auto result = makeResult (-23.0, -3.0);
    result.quality.clipEvents.push_back ({ 1.0, 10, 0 });

    const std::vector<qc::Target> targets { makeR128(), makeNetflix() };
    const auto verdicts = qc::evaluate (result, targets);

    check (qc::overallStatus (verdicts) == qc::Status::notMeasured,
           "an unjudgeable target must not be hidden behind a warning");
}
