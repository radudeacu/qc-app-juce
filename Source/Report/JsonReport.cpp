#include "JsonReport.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace qc
{
    namespace
    {
        std::string indent (int depth)
        {
            return std::string (static_cast<std::size_t> (depth) * 2, ' ');
        }

        std::string escape (const std::string& text)
        {
            std::string result;
            result.reserve (text.size() + 8);

            for (char character : text)
            {
                switch (character)
                {
                    case '"':  result += "\\\""; break;
                    case '\\': result += "\\\\"; break;  // Windows paths are full of these
                    case '\b': result += "\\b";  break;
                    case '\f': result += "\\f";  break;
                    case '\n': result += "\\n";  break;
                    case '\r': result += "\\r";  break;
                    case '\t': result += "\\t";  break;

                    default:
                        if (static_cast<unsigned char> (character) < 0x20)
                        {
                            std::ostringstream stream;
                            stream << "\\u" << std::hex << std::setw (4) << std::setfill ('0')
                                   << static_cast<int> (static_cast<unsigned char> (character));
                            result += stream.str();
                        }
                        else
                        {
                            result += character;
                        }
                        break;
                }
            }

            return result;
        }

        std::string quoted (const std::string& text)
        {
            return "\"" + escape (text) + "\"";
        }

        /** A finite number, or null. Anything infinite or NaN is "not measured", and
            writing it as a number would invite a consumer to treat it as a level.
        */
        std::string numberOrNull (double value, int decimals = 2)
        {
            if (! std::isfinite (value))
                return "null";

            std::ostringstream stream;
            stream << std::fixed << std::setprecision (decimals) << value;
            return stream.str();
        }

        std::string boolean (bool value)
        {
            return value ? "true" : "false";
        }

        /** Every stretch that exceeded this target's ceiling, with timestamps - the list
            an engineer needs to go and fix the file, rather than just a count.
        */
        void writeOverEvents (std::ostringstream& out,
                              const AnalysisResult& result,
                              double ceilingDb,
                              const JsonReportOptions& options)
        {
            auto overs = findOverEvents (result.truePeakEnvelope, ceilingDb);

            // The count is the real total even when the list below is capped, so a
            // consumer is never misled about how bad the file is.
            const auto totalOvers = overs.size();
            const bool truncated = totalOvers > options.maximumOverEventsPerTarget;

            if (truncated)
                overs.resize (options.maximumOverEventsPerTarget);

            out << indent (3) << "\"truePeakOverCount\": " << totalOvers << ",\n";
            out << indent (3) << "\"truePeakOversTruncated\": " << (truncated ? "true" : "false") << ",\n";
            out << indent (3) << "\"truePeakOvers\": [";

            for (std::size_t i = 0; i < overs.size(); ++i)
            {
                const auto& over = overs[i];
                out << (i > 0 ? "," : "") << "\n" << indent (4)
                    << "{ \"startSeconds\": " << numberOrNull (over.startSeconds, 3)
                    << ", \"endSeconds\": " << numberOrNull (over.endSeconds, 3)
                    << ", \"peakDbtp\": " << numberOrNull (over.peakDb)
                    << ", \"channel\": " << over.channel << " }";
            }

            if (! overs.empty())
                out << "\n" << indent (3);

            out << "],\n";
        }

        void writeSeries (std::ostringstream& out, const std::vector<double>& series, int depth)
        {
            out << "[";

            for (std::size_t i = 0; i < series.size(); ++i)
            {
                if (i > 0)
                    out << ", ";

                // Wrapped so a long file's series stays readable in a diff or an editor.
                if (i > 0 && i % 20 == 0)
                    out << "\n" << indent (depth + 1);

                out << numberOrNull (series[i], 1);
            }

            out << "]";
        }
    }

    std::string toJsonToken (Status status)
    {
        switch (status)
        {
            case Status::pass:        return "pass";
            case Status::warn:        return "warn";
            case Status::fail:        return "fail";
            case Status::notMeasured: return "notMeasured";
        }

        return "notMeasured";
    }

    std::string writeJsonReport (const AnalysisResult& result,
                                 const std::vector<TargetVerdict>& verdicts,
                                 const JsonReportOptions& options)
    {
        std::ostringstream out;
        out << "{\n";

        out << indent (1) << "\"schemaVersion\": 1,\n";
        out << indent (1) << "\"generator\": {\n";
        out << indent (2) << "\"name\": " << quoted (options.generatorName) << ",\n";
        out << indent (2) << "\"version\": " << quoted (options.generatorVersion) << "\n";
        out << indent (1) << "},\n";

        out << indent (1) << "\"source\": {\n";
        out << indent (2) << "\"path\": " << quoted (result.source.filePath) << ",\n";
        out << indent (2) << "\"format\": " << quoted (result.source.formatName) << ",\n";
        out << indent (2) << "\"sampleRate\": " << numberOrNull (result.source.sampleRate, 0) << ",\n";
        out << indent (2) << "\"channels\": " << result.source.numChannels << ",\n";
        out << indent (2) << "\"bitDepth\": " << result.source.bitDepth << ",\n";
        out << indent (2) << "\"durationSeconds\": " << numberOrNull (result.source.durationSeconds, 3) << "\n";
        out << indent (1) << "},\n";

        out << indent (1) << "\"loudness\": {\n";
        out << indent (2) << "\"integratedLufs\": " << numberOrNull (result.loudness.integratedLufs) << ",\n";
        out << indent (2) << "\"loudnessRangeLu\": " << numberOrNull (result.loudness.loudnessRangeLu) << ",\n";
        out << indent (2) << "\"maxShortTermLufs\": " << numberOrNull (result.loudness.maxShortTermLufs) << ",\n";
        out << indent (2) << "\"maxMomentaryLufs\": " << numberOrNull (result.loudness.maxMomentaryLufs) << ",\n";
        out << indent (2) << "\"dialogueGatedLufs\": "
            << (result.hasDialogueGatedLoudness ? numberOrNull (result.dialogueGatedLufs) : "null") << "\n";
        out << indent (1) << "},\n";

        out << indent (1) << "\"truePeak\": {\n";
        out << indent (2) << "\"maxDbtp\": " << numberOrNull (result.truePeakDb) << ",\n";
        out << indent (2) << "\"oversamplingFactor\": " << result.oversamplingFactor << ",\n";
        out << indent (2) << "\"peakToLoudnessRatioDb\": " << numberOrNull (result.peakToLoudnessRatioDb) << "\n";
        out << indent (1) << "},\n";

        out << indent (1) << "\"quality\": {\n";
        out << indent (2) << "\"samplePeakDb\": [";

        for (std::size_t i = 0; i < result.quality.samplePeakDb.size(); ++i)
            out << (i > 0 ? ", " : "") << numberOrNull (result.quality.samplePeakDb[i]);

        out << "],\n";
        out << indent (2) << "\"isStereo\": " << boolean (result.quality.isStereo) << ",\n";
        out << indent (2) << "\"correlation\": "
            << (result.quality.isStereo ? numberOrNull (result.quality.correlation) : "null") << ",\n";
        out << indent (2) << "\"negativeCorrelationFraction\": "
            << (result.quality.isStereo ? numberOrNull (result.quality.negativeCorrelationFraction, 4) : "null") << ",\n";
        out << indent (2) << "\"monoCompatibilityLossDb\": "
            << numberOrNull (result.monoCompatibilityLossDb) << ",\n";
        out << indent (2) << "\"clippedSampleCount\": " << result.quality.clippedSampleCount << ",\n";
        out << indent (2) << "\"clipEvents\": [";

        for (std::size_t i = 0; i < result.quality.clipEvents.size(); ++i)
        {
            const auto& event = result.quality.clipEvents[i];
            out << (i > 0 ? "," : "") << "\n" << indent (3)
                << "{ \"startSeconds\": " << numberOrNull (event.startSeconds, 3)
                << ", \"lengthInSamples\": " << event.lengthInSamples
                << ", \"channel\": " << event.channel << " }";
        }

        if (! result.quality.clipEvents.empty())
            out << "\n" << indent (2);

        out << "]\n";
        out << indent (1) << "},\n";

        out << indent (1) << "\"verdicts\": [";

        for (std::size_t i = 0; i < verdicts.size(); ++i)
        {
            const auto& verdict = verdicts[i];

            out << (i > 0 ? "," : "") << "\n" << indent (2) << "{\n";
            out << indent (3) << "\"targetId\": " << quoted (verdict.targetId) << ",\n";
            out << indent (3) << "\"targetName\": " << quoted (verdict.targetName) << ",\n";
            out << indent (3) << "\"status\": " << quoted (toJsonToken (verdict.status)) << ",\n";
            out << indent (3) << "\"fixHint\": " << quoted (verdict.fixHint) << ",\n";

            out << indent (3) << "\"specification\": { \"integratedLufs\": "
                << numberOrNull (verdict.targetIntegratedLufs)
                << ", \"maxTruePeakDb\": " << numberOrNull (verdict.targetMaxTruePeakDb)
                << ", \"lastVerified\": "
                << (verdict.targetLastVerified.empty() ? "null" : quoted (verdict.targetLastVerified))
                << " },\n";

            writeOverEvents (out, result, verdict.targetMaxTruePeakDb, options);

            out << indent (3) << "\"checks\": [";

            for (std::size_t c = 0; c < verdict.checks.size(); ++c)
            {
                const auto& check = verdict.checks[c];
                out << (c > 0 ? "," : "") << "\n" << indent (4)
                    << "{ \"name\": " << quoted (check.name)
                    << ", \"status\": " << quoted (toJsonToken (check.status))
                    << ", \"detail\": " << quoted (check.detail) << " }";
            }

            if (! verdict.checks.empty())
                out << "\n" << indent (3);

            out << "]\n";
            out << indent (2) << "}";
        }

        if (! verdicts.empty())
            out << "\n" << indent (1);

        out << "],\n";

        out << indent (1) << "\"timeSeries\": ";

        if (options.includeTimeSeries)
        {
            out << "{\n";
            out << indent (2) << "\"stepSeconds\": " << LoudnessMeasurements::seriesStepSeconds << ",\n";
            out << indent (2) << "\"momentaryWindowSeconds\": 0.4,\n";
            out << indent (2) << "\"shortTermWindowSeconds\": 3.0,\n";
            out << indent (2) << "\"momentaryLufs\": ";
            writeSeries (out, result.loudness.momentaryLufs, 2);
            out << ",\n";
            out << indent (2) << "\"shortTermLufs\": ";
            writeSeries (out, result.loudness.shortTermLufs, 2);
            out << "\n";
            out << indent (1) << "}\n";
        }
        else
        {
            out << "null\n";
        }

        out << "}\n";
        return out.str();
    }
}
