#include "PdfReport.h"

#include "PdfWriter.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace qc
{
    namespace
    {
        constexpr double kMargin = 40.0;
        constexpr double kContentRight = 555.0;

        constexpr double kGraphTop = 548.0;
        constexpr double kGraphBottom = 752.0;

        constexpr double kFooterBaseline = 800.0;

        struct Colour { double r, g, b; };

        const Colour kInk         { 0.10, 0.11, 0.13 };
        const Colour kMuted       { 0.42, 0.45, 0.50 };
        const Colour kRule        { 0.82, 0.84, 0.87 };
        const Colour kPassColour  { 0.13, 0.60, 0.33 };
        const Colour kWarnColour  { 0.72, 0.51, 0.10 };
        const Colour kFailColour  { 0.75, 0.20, 0.22 };
        const Colour kAbsentGrey  { 0.45, 0.47, 0.52 };
        const Colour kBandColour  { 0.87, 0.94, 0.89 };
        const Colour kTraceColour { 0.15, 0.35, 0.62 };

        Colour colourFor (Status status)
        {
            switch (status)
            {
                case Status::pass:        return kPassColour;
                case Status::warn:        return kWarnColour;
                case Status::fail:        return kFailColour;
                case Status::notMeasured: return kAbsentGrey;
            }

            return kAbsentGrey;
        }

        void setFill (PdfWriter& pdf, const Colour& colour) { pdf.setFillColour (colour.r, colour.g, colour.b); }
        void setStroke (PdfWriter& pdf, const Colour& colour) { pdf.setStrokeColour (colour.r, colour.g, colour.b); }

        std::string fixed (double value, int decimals)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision (decimals) << value;
            return stream.str();
        }

        std::string measurementOrDash (double value, int decimals, const std::string& suffix)
        {
            if (! std::isfinite (value))
                return "--";

            return fixed (value, decimals) + suffix;
        }

        std::string formatDuration (double seconds)
        {
            if (! std::isfinite (seconds) || seconds < 0.0)
                return "--";

            const auto total = static_cast<int> (seconds);
            const auto minutes = total / 60;
            const auto remainder = total % 60;

            std::ostringstream stream;
            stream << minutes << ":" << std::setw (2) << std::setfill ('0') << remainder;
            return stream.str();
        }

        std::string today()
        {
            const auto now = std::time (nullptr);
            std::tm parts {};

        #ifdef _WIN32
            localtime_s (&parts, &now);
        #else
            localtime_r (&now, &parts);
        #endif

            std::ostringstream stream;
            stream << std::put_time (&parts, "%Y-%m-%d");
            return stream.str();
        }

        std::vector<std::string> splitLines (const std::string& text)
        {
            std::vector<std::string> lines;
            std::istringstream stream (text);
            std::string line;

            while (std::getline (stream, line))
                lines.push_back (line);

            return lines;
        }

        /** Reduces a series to at most maxPoints, keeping the loudest value in each
            bucket. An hour of audio is 36,000 short-term values; drawn point for point
            that is a several-megabyte page for detail no printer can resolve. Taking the
            maximum rather than an average preserves the peaks, which is what a reader is
            looking for.
        */
        std::vector<double> decimate (const std::vector<double>& series, std::size_t maxPoints)
        {
            if (series.size() <= maxPoints || maxPoints == 0)
                return series;

            std::vector<double> result;
            result.reserve (maxPoints);

            const double bucketSize = static_cast<double> (series.size()) / static_cast<double> (maxPoints);

            for (std::size_t bucket = 0; bucket < maxPoints; ++bucket)
            {
                const auto start = static_cast<std::size_t> (static_cast<double> (bucket) * bucketSize);
                const auto end = std::min (series.size(),
                                           static_cast<std::size_t> (static_cast<double> (bucket + 1) * bucketSize));

                double loudest = -std::numeric_limits<double>::infinity();

                for (std::size_t i = start; i < end; ++i)
                    if (isMeasured (series[i]))
                        loudest = std::max (loudest, series[i]);

                result.push_back (loudest);
            }

            return result;
        }

        void drawHeader (PdfWriter& pdf, const PdfReportItem& item, const PdfReportOptions& options)
        {
            setFill (pdf, kInk);
            pdf.drawText (options.title, kMargin, 58.0, 17.0, PdfWriter::Font::bold);

            pdf.drawText (pdf.truncateToWidth (item.displayName, kContentRight - kMargin, 11.5),
                          kMargin, 78.0, 11.5);

            setFill (pdf, kMuted);

            const auto& source = item.result.source;
            std::string details;

            if (! item.errorMessage.empty())
            {
                details = "Not analysed";
            }
            else
            {
                details = source.formatName
                        + "  |  " + fixed (source.sampleRate / 1000.0, 1) + " kHz"
                        + "  |  " + std::to_string (source.bitDepth) + "-bit"
                        + "  |  " + (source.numChannels == 1 ? "mono" : "stereo")
                        + "  |  " + formatDuration (source.durationSeconds)
                        + "  |  true peak oversampled " + std::to_string (item.result.oversamplingFactor) + "x";
            }

            pdf.drawText (details, kMargin, 93.0, 8.5);
            pdf.drawText (options.generatorName + " " + options.generatorVersion + "  |  "
                              + (options.generatedOn.empty() ? today() : options.generatedOn),
                          kMargin, 105.0, 8.5);

            setStroke (pdf, kRule);
            pdf.setLineWidth (0.7);
            pdf.drawLine (kMargin, 116.0, kContentRight, 116.0);
        }

        void drawMeasurements (PdfWriter& pdf, const AnalysisResult& result)
        {
            struct Field { std::string label; std::string value; };

            const std::vector<Field> fields {
                { "Integrated",     measurementOrDash (result.loudness.integratedLufs, 1, " LUFS") },
                { "Loudness range", measurementOrDash (result.loudness.loudnessRangeLu, 1, " LU") },
                { "True peak",      measurementOrDash (result.truePeakDb, 1, " dBTP") },
                { "PLR",            measurementOrDash (result.peakToLoudnessRatioDb, 1, " LU") },
                { "Max short-term", measurementOrDash (result.loudness.maxShortTermLufs, 1, " LUFS") },
                { "Max momentary",  measurementOrDash (result.loudness.maxMomentaryLufs, 1, " LUFS") },
                { "Sample peak",    result.quality.samplePeakDb.empty()
                                        ? "--"
                                        : measurementOrDash (*std::max_element (result.quality.samplePeakDb.begin(),
                                                                                result.quality.samplePeakDb.end()),
                                                             1, " dBFS") },
                { "Correlation",    result.quality.isStereo
                                        ? measurementOrDash (result.quality.correlation, 2, "") : "--" }
            };

            const double columnWidth = (kContentRight - kMargin) / 4.0;

            for (std::size_t i = 0; i < fields.size(); ++i)
            {
                const double x = kMargin + static_cast<double> (i % 4) * columnWidth;
                const double y = 140.0 + static_cast<double> (i / 4) * 38.0;

                setFill (pdf, kMuted);
                pdf.drawText (fields[i].label, x, y, 8.0);

                setFill (pdf, kInk);
                pdf.drawText (fields[i].value, x, y + 15.0, 13.0, PdfWriter::Font::bold);
            }
        }

        /** Anything the loudness figures cannot show: clipping, phase, mono collapse. */
        double drawQualityNotes (PdfWriter& pdf, const AnalysisResult& result, double y)
        {
            std::vector<std::string> notes;

            if (! result.quality.clipEvents.empty())
                notes.push_back (std::to_string (result.quality.clipEvents.size())
                                 + " clipped run(s), first at "
                                 + fixed (result.quality.clipEvents.front().startSeconds, 2) + " s");

            if (result.quality.isStereo && result.quality.negativeCorrelationFraction > 0.05)
                notes.push_back (fixed (result.quality.negativeCorrelationFraction * 100.0, 1)
                                 + "% of the programme is out of phase");

            if (result.quality.isStereo && result.monoCompatibilityLossDb > 3.0)
                notes.push_back (std::isfinite (result.monoCompatibilityLossDb)
                                     ? "Mono sum loses " + fixed (result.monoCompatibilityLossDb, 1)
                                           + " dB against the stereo original"
                                     : "Mono sum cancels to silence - the channels are out of phase");

            if (result.hasDialogueGatedLoudness)
                notes.push_back ("Dialogue-gated loudness "
                                 + measurementOrDash (result.dialogueGatedLufs, 1, " LUFS"));

            if (notes.empty())
                return y;

            setFill (pdf, kMuted);
            pdf.drawText ("NOTES", kMargin, y, 8.0, PdfWriter::Font::bold);
            y += 13.0;

            setFill (pdf, kInk);

            for (const auto& note : notes)
            {
                pdf.drawText (note, kMargin, y, 9.0);
                y += 12.0;
            }

            return y + 6.0;
        }

        double drawVerdicts (PdfWriter& pdf, const PdfReportItem& item, double y)
        {
            setFill (pdf, kMuted);
            pdf.drawText ("TARGETS", kMargin, y, 8.0, PdfWriter::Font::bold);
            y += 14.0;

            if (item.verdicts.empty())
            {
                setFill (pdf, kInk);
                pdf.drawText ("No targets were selected, so nothing was judged.", kMargin, y, 9.5);
                return y + 16.0;
            }

            for (const auto& verdict : item.verdicts)
            {
                const auto colour = colourFor (verdict.status);

                setFill (pdf, colour);
                pdf.drawText (toString (verdict.status), kMargin, y, 9.0, PdfWriter::Font::bold);

                setFill (pdf, kInk);
                pdf.drawText (verdict.targetName, kMargin + 78.0, y, 10.0, PdfWriter::Font::bold);

                setFill (pdf, kMuted);

                std::string specification = fixed (verdict.targetIntegratedLufs, 1) + " LUFS, max "
                                          + fixed (verdict.targetMaxTruePeakDb, 1) + " dBTP";

                // A verdict against a spec nobody has checked is worth less than no
                // verdict, so the page says so rather than looking authoritative.
                specification += verdict.targetLastVerified.empty()
                               ? "  (spec not verified)"
                               : "  (spec verified " + verdict.targetLastVerified + ")";

                pdf.drawText (specification, kMargin + 200.0, y, 8.5);
                y += 13.0;

                for (const auto& check : verdict.checks)
                {
                    setFill (pdf, colourFor (check.status));
                    pdf.drawText ("-", kMargin + 12.0, y, 9.0);

                    setFill (pdf, kMuted);
                    pdf.drawText (check.name, kMargin + 22.0, y, 8.5);

                    setFill (pdf, kInk);
                    pdf.drawText (pdf.truncateToWidth (check.detail, kContentRight - kMargin - 170.0, 8.5),
                                  kMargin + 160.0, y, 8.5);
                    y += 11.0;
                }

                if (! verdict.fixHint.empty())
                {
                    for (const auto& line : splitLines (verdict.fixHint))
                    {
                        setFill (pdf, kWarnColour);
                        pdf.drawText (pdf.truncateToWidth (line, kContentRight - kMargin - 22.0, 9.0,
                                                           PdfWriter::Font::italic),
                                      kMargin + 22.0, y + 2.0, 9.0, PdfWriter::Font::italic);
                        y += 12.0;
                    }
                }

                y += 8.0;
            }

            return y;
        }

        void drawGraph (PdfWriter& pdf, const PdfReportItem& item)
        {
            const auto& loudness = item.result.loudness;
            const double duration = item.result.source.durationSeconds;

            setStroke (pdf, kRule);
            pdf.setLineWidth (0.7);
            pdf.strokeRect (kMargin, kGraphTop, kContentRight - kMargin, kGraphBottom - kGraphTop);

            setFill (pdf, kMuted);
            pdf.drawText ("LOUDNESS OVER TIME", kMargin, kGraphTop - 8.0, 8.0, PdfWriter::Font::bold);

            if (loudness.shortTermLufs.empty() || duration <= 0.0)
            {
                pdf.drawText ("Too short to plot", kMargin + 12.0, kGraphTop + 24.0, 9.0);
                return;
            }

            double lowest = -40.0;
            double highest = -10.0;

            for (double value : loudness.shortTermLufs)
            {
                if (! isMeasured (value))
                    continue;

                lowest = std::min (lowest, value);
                highest = std::max (highest, value);
            }

            if (! item.verdicts.empty())
            {
                lowest = std::min (lowest, item.verdicts.front().targetIntegratedLufs - 6.0);
                highest = std::max (highest, item.verdicts.front().targetIntegratedLufs + 6.0);
            }

            lowest -= 2.0;
            highest += 2.0;

            const double span = std::max (6.0, highest - lowest);
            const double plotLeft = kMargin + 34.0;
            const double plotRight = kContentRight - 8.0;
            const double plotTop = kGraphTop + 12.0;
            const double plotBottom = kGraphBottom - 18.0;

            const auto toY = [&] (double lufs)
            {
                const double proportion = (lufs - lowest) / span;
                return plotBottom - proportion * (plotBottom - plotTop);
            };

            // Target band behind everything else.
            if (! item.verdicts.empty())
            {
                const auto& verdict = item.verdicts.front();
                const double tolerance = 1.0;
                const double bandTop = toY (verdict.targetIntegratedLufs + tolerance);
                const double bandBottom = toY (verdict.targetIntegratedLufs - tolerance);

                setFill (pdf, kBandColour);
                pdf.fillRect (plotLeft, bandTop, plotRight - plotLeft, bandBottom - bandTop);
            }

            setStroke (pdf, kRule);
            pdf.setLineWidth (0.4);
            setFill (pdf, kMuted);

            for (double lufs = std::ceil (lowest / 6.0) * 6.0; lufs <= highest; lufs += 6.0)
            {
                const double y = toY (lufs);
                pdf.drawLine (plotLeft, y, plotRight, y);
                pdf.drawText (fixed (lufs, 0), kMargin, y + 3.0, 7.0);
            }

            const auto points = decimate (loudness.shortTermLufs, 900);
            std::vector<std::pair<double, double>> path;
            path.reserve (points.size());

            for (std::size_t i = 0; i < points.size(); ++i)
            {
                if (! isMeasured (points[i]))
                    continue;

                const double proportion = points.size() > 1
                                        ? static_cast<double> (i) / static_cast<double> (points.size() - 1)
                                        : 0.0;

                path.emplace_back (plotLeft + proportion * (plotRight - plotLeft),
                                   toY (std::max (lowest, points[i])));
            }

            setStroke (pdf, kTraceColour);
            pdf.setLineWidth (0.9);
            pdf.drawPolyline (path);

            if (isMeasured (loudness.integratedLufs))
            {
                setStroke (pdf, kInk);
                pdf.setLineWidth (0.6);
                const double y = toY (loudness.integratedLufs);
                pdf.drawLine (plotLeft, y, plotRight, y);

                setFill (pdf, kInk);
                pdf.drawText ("Integrated " + fixed (loudness.integratedLufs, 1),
                              plotLeft + 4.0, y - 3.0, 7.5);
            }

            setFill (pdf, kMuted);
            pdf.drawText ("0:00", plotLeft, plotBottom + 12.0, 7.0);
            pdf.drawText (formatDuration (duration), plotRight - 26.0, plotBottom + 12.0, 7.0);
            pdf.drawText ("Short-term loudness (3 s)", plotLeft + 40.0, plotBottom + 12.0, 7.0);
        }

        void drawFooter (PdfWriter& pdf, int pageNumber, int pageCount)
        {
            setStroke (pdf, kRule);
            pdf.setLineWidth (0.7);
            pdf.drawLine (kMargin, kFooterBaseline - 12.0, kContentRight, kFooterBaseline - 12.0);

            setFill (pdf, kMuted);
            pdf.drawText ("Measured to ITU-R BS.1770-4 and EBU Tech 3341-3342", kMargin, kFooterBaseline, 7.5);
            pdf.drawText ("Page " + std::to_string (pageNumber) + " of " + std::to_string (pageCount),
                          kContentRight - 60.0, kFooterBaseline, 7.5);
        }

        void drawFailurePage (PdfWriter& pdf, const PdfReportItem& item)
        {
            setFill (pdf, kFailColour);
            pdf.drawText ("COULD NOT BE ANALYSED", kMargin, 150.0, 12.0, PdfWriter::Font::bold);

            setFill (pdf, kInk);
            pdf.drawText (pdf.truncateToWidth (item.errorMessage, kContentRight - kMargin, 10.0),
                          kMargin, 170.0, 10.0);

            setFill (pdf, kMuted);
            pdf.drawText ("This file is listed so that the report does not overstate what was checked.",
                          kMargin, 190.0, 8.5);
        }
    }

    std::string writePdfReport (const std::vector<PdfReportItem>& items, const PdfReportOptions& options)
    {
        PdfWriter pdf;

        const auto pageCount = std::max (1, static_cast<int> (items.size()));

        if (items.empty())
        {
            pdf.beginPage();
            setFill (pdf, kInk);
            pdf.drawText (options.title, kMargin, 58.0, 17.0, PdfWriter::Font::bold);
            setFill (pdf, kMuted);
            pdf.drawText ("Nothing was analysed.", kMargin, 82.0, 10.0);
            drawFooter (pdf, 1, 1);
            pdf.endPage();
            return pdf.finish();
        }

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];

            pdf.beginPage();
            drawHeader (pdf, item, options);

            if (! item.errorMessage.empty())
            {
                drawFailurePage (pdf, item);
            }
            else
            {
                drawMeasurements (pdf, item.result);

                double y = drawVerdicts (pdf, item, 230.0);
                y = drawQualityNotes (pdf, item.result, y + 4.0);

                drawGraph (pdf, item);
            }

            drawFooter (pdf, static_cast<int> (i) + 1, pageCount);
            pdf.endPage();
        }

        return pdf.finish();
    }
}
