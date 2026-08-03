#include "TestHarness.h"

#include "Report/JsonReport.h"

#include <limits>

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
        target.lastVerified = "2026-08-03";
        return target;
    }

    qc::AnalysisResult makeResult()
    {
        qc::AnalysisResult result;
        result.source.filePath = R"(C:\Mixes\episode "01".wav)";
        result.source.formatName = "WAV file";
        result.source.sampleRate = 48000.0;
        result.source.numChannels = 2;
        result.source.bitDepth = 24;
        result.source.durationSeconds = 12.5;

        result.loudness.integratedLufs = -18.4;
        result.loudness.loudnessRangeLu = 7.2;
        result.loudness.maxShortTermLufs = -15.1;
        result.loudness.maxMomentaryLufs = -13.8;
        result.loudness.momentaryLufs = { -20.0, -19.5, -18.9 };
        result.loudness.shortTermLufs = { -19.2, -18.8 };

        result.truePeakDb = -0.4;
        result.oversamplingFactor = 4;
        result.peakToLoudnessRatioDb = 18.0;
        result.quality.isStereo = true;
        result.quality.samplePeakDb = { -0.8, -0.9 };
        result.quality.correlation = 0.82;

        return result;
    }

    bool contains (const std::string& haystack, const std::string& needle)
    {
        return haystack.find (needle) != std::string::npos;
    }
}

QC_TEST (jsonEscapesWindowsPaths)
{
    const auto json = qc::writeJsonReport (makeResult(), {});

    // A raw backslash or quote here would produce a file no parser can read - and
    // every path on this platform is full of backslashes.
    check (contains (json, R"("C:\\Mixes\\episode \"01\".wav")"),
           "the path should be escaped, got: " + json.substr (0, 400));

    check (! contains (json, R"(C:\Mixes)"), "no unescaped backslash should survive");
}

QC_TEST (unmeasuredValuesAreNullNotNumbers)
{
    qc::AnalysisResult silent;
    silent.source.numChannels = 2;

    const auto json = qc::writeJsonReport (silent, {});

    check (contains (json, "\"integratedLufs\": null"),
           "a silent file's loudness must be null, not a number");
    check (contains (json, "\"maxDbtp\": null"), "and so must its true peak");

    // Infinity has no JSON representation; writing it would produce either invalid
    // JSON or a number a consumer would read as a real level.
    check (! contains (json, "inf"), "no infinity should reach the output: " + json.substr (0, 300));
    check (! contains (json, "nan"), "and no NaN either");
}

QC_TEST (dialogueGatedLoudnessIsNullWithoutAStem)
{
    const auto json = qc::writeJsonReport (makeResult(), {});
    check (contains (json, "\"dialogueGatedLufs\": null"),
           "without a stem this must be null rather than the programme figure");
}

QC_TEST (dialogueGatedLoudnessIsWrittenWhenPresent)
{
    auto result = makeResult();
    result.hasDialogueGatedLoudness = true;
    result.dialogueGatedLufs = -27.3;

    const auto json = qc::writeJsonReport (result, {});
    check (contains (json, "\"dialogueGatedLufs\": -27.30"), "the gated figure should be written");
}

QC_TEST (infiniteMonoLossIsNull)
{
    auto result = makeResult();
    result.monoCompatibilityLossDb = std::numeric_limits<double>::infinity();

    const auto json = qc::writeJsonReport (result, {});
    check (contains (json, "\"monoCompatibilityLossDb\": null"),
           "total cancellation is not a number a consumer can use");
}

QC_TEST (verdictsCarryTheSpecificationTheyWereJudgedAgainst)
{
    const auto result = makeResult();
    const auto verdicts = qc::evaluate (result, std::vector<qc::Target> { makeR128() });
    const auto json = qc::writeJsonReport (result, verdicts);

    check (contains (json, "\"targetId\": \"ebu-r128\""), "target id");
    check (contains (json, "\"status\": \"fail\""), "machine token, not the UI label");
    check (contains (json, "\"integratedLufs\": -23.00"), "the spec's own number");
    check (contains (json, "\"lastVerified\": \"2026-08-03\""), "and when it was last checked");
}

QC_TEST (unverifiedSpecificationsAreNull)
{
    auto target = makeR128();
    target.lastVerified.clear();

    const auto result = makeResult();
    const auto json = qc::writeJsonReport (result, qc::evaluate (result, std::vector<qc::Target> { target }));

    check (contains (json, "\"lastVerified\": null"),
           "an unverified spec must say so rather than emit an empty string");
}

QC_TEST (statusTokensAreStableAndDistinctFromUiLabels)
{
    check (qc::toJsonToken (qc::Status::pass) == "pass", "pass");
    check (qc::toJsonToken (qc::Status::warn) == "warn", "warn");
    check (qc::toJsonToken (qc::Status::fail) == "fail", "fail");

    // The UI says "NOT MEASURED"; the wire format must not inherit a display decision.
    check (qc::toJsonToken (qc::Status::notMeasured) == "notMeasured", "notMeasured");
    check (qc::toString (qc::Status::notMeasured) == "NOT MEASURED", "the UI label is separate");
}

QC_TEST (timeSeriesCanBeOmitted)
{
    qc::JsonReportOptions options;
    options.includeTimeSeries = false;

    const auto json = qc::writeJsonReport (makeResult(), {}, options);

    check (contains (json, "\"timeSeries\": null"), "omitted series should be explicit null");
    check (! contains (json, "momentaryLufs"), "and the arrays should be gone");
}

QC_TEST (timeSeriesIsIncludedByDefault)
{
    const auto json = qc::writeJsonReport (makeResult(), {});

    check (contains (json, "\"stepSeconds\": 0.1"), "the series needs its own step to be usable");
    check (contains (json, "\"momentaryLufs\": [-20.0, -19.5, -18.9]"), "momentary series");
    check (contains (json, "\"shortTermLufs\": [-19.2, -18.8]"), "short-term series");
}

QC_TEST (clipEventsAreListedWithTimestamps)
{
    auto result = makeResult();
    result.quality.clipEvents.push_back ({ 3.25, 12, 1 });
    result.quality.clippedSampleCount = 12;

    const auto json = qc::writeJsonReport (result, {});

    check (contains (json, "\"startSeconds\": 3.250"), "clip timestamp");
    check (contains (json, "\"lengthInSamples\": 12"), "clip length");
    check (contains (json, "\"channel\": 1"), "which channel");
}
