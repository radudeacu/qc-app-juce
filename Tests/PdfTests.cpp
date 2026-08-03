#include "TestHarness.h"

#include "Report/PdfReport.h"
#include "Report/PdfWriter.h"

#include <algorithm>

using namespace qctest;

namespace
{
    bool contains (const std::string& haystack, const std::string& needle)
    {
        return haystack.find (needle) != std::string::npos;
    }

    int countOccurrences (const std::string& haystack, const std::string& needle)
    {
        int count = 0;
        std::size_t position = 0;

        while ((position = haystack.find (needle, position)) != std::string::npos)
        {
            ++count;
            position += needle.size();
        }

        return count;
    }

    /** Walks the cross-reference table and checks every entry points at the object it
        claims. A PDF with a wrong offset opens as a blank page or not at all, and no
        substring check would notice.
    */
    void checkCrossReferenceTable (const std::string& pdf)
    {
        const auto startxref = pdf.rfind ("startxref");
        check (startxref != std::string::npos, "the document must end with a startxref");

        const auto offsetText = pdf.substr (startxref + 10, 20);
        const auto xrefOffset = static_cast<std::size_t> (std::stoul (offsetText));

        check (xrefOffset < pdf.size(), "startxref must point inside the document");
        check (pdf.compare (xrefOffset, 4, "xref") == 0,
               "startxref must point at the cross-reference table");

        // Header line is "xref\n0 N\n", then N fixed-width 20-byte entries.
        auto position = pdf.find ('\n', xrefOffset) + 1;
        const auto countEnd = pdf.find ('\n', position);
        const auto header = pdf.substr (position, countEnd - position);

        const auto space = header.find (' ');
        check (space != std::string::npos, "the xref header should be '0 N'");

        const auto objectCount = static_cast<std::size_t> (std::stoul (header.substr (space + 1)));
        check (objectCount > 1, "a document should declare more than the free object");

        position = countEnd + 1;

        // Entry 0 is the free head; every other must land on "<n> 0 obj".
        for (std::size_t object = 1; object < objectCount; ++object)
        {
            const auto entry = pdf.substr (position + object * 20, 10);
            const auto objectOffset = static_cast<std::size_t> (std::stoul (entry));

            check (objectOffset > 0 && objectOffset < pdf.size(),
                   "object " + std::to_string (object) + " has an out-of-range offset");

            const auto expected = std::to_string (object) + " 0 obj";
            check (pdf.compare (objectOffset, expected.size(), expected) == 0,
                   "the xref entry for object " + std::to_string (object)
                       + " should point at its definition, but found: "
                       + pdf.substr (objectOffset, 20));
        }
    }

    qc::Target makeTarget()
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

    qc::PdfReportItem makeItem (const std::string& name, double integrated, double truePeak)
    {
        qc::PdfReportItem item;
        item.displayName = name;
        item.result.source.filePath = name;
        item.result.source.formatName = "WAV file";
        item.result.source.sampleRate = 48000.0;
        item.result.source.numChannels = 2;
        item.result.source.bitDepth = 24;
        item.result.source.durationSeconds = 30.0;
        item.result.loudness.integratedLufs = integrated;
        item.result.loudness.loudnessRangeLu = 6.0;
        item.result.loudness.maxShortTermLufs = integrated + 3.0;
        item.result.loudness.maxMomentaryLufs = integrated + 5.0;
        item.result.truePeakDb = truePeak;
        item.result.oversamplingFactor = 4;
        item.result.quality.isStereo = true;
        item.result.quality.samplePeakDb = { truePeak - 0.2, truePeak - 0.3 };

        for (int i = 0; i < 280; ++i)
            item.result.loudness.shortTermLufs.push_back (integrated + std::sin (i * 0.1) * 2.0);

        item.verdicts = qc::evaluate (item.result, std::vector<qc::Target> { makeTarget() });
        return item;
    }

    qc::PdfReportOptions fixedDateOptions()
    {
        qc::PdfReportOptions options;
        options.generatedOn = "2026-08-03";
        return options;
    }
}

QC_TEST (pdfWriterProducesAStructurallyValidDocument)
{
    qc::PdfWriter pdf;

    pdf.beginPage();
    pdf.setFillColour (0.0, 0.0, 0.0);
    pdf.drawText ("Hello", 40.0, 60.0, 12.0);
    pdf.endPage();

    const auto document = pdf.finish();

    check (document.rfind ("%PDF-1.4", 0) == 0, "must start with the PDF header");
    check (contains (document, "%%EOF"), "and end with the EOF marker");
    check (contains (document, "/Type /Catalog"), "a catalog is required");
    check (contains (document, "/Type /Pages"), "a page tree is required");
    check (countOccurrences (document, "/Type /Page\n") + countOccurrences (document, "/Type /Page ") == 1,
           "one page expected");

    checkCrossReferenceTable (document);
}

QC_TEST (pdfWriterEscapesTextThatWouldBreakTheSyntax)
{
    qc::PdfWriter pdf;
    pdf.beginPage();
    pdf.drawText (R"(mix (final) 50% \ done)", 40.0, 60.0, 10.0);
    pdf.endPage();

    const auto document = pdf.finish();

    // An unescaped parenthesis terminates the string early and corrupts everything
    // after it, which is exactly the sort of break a structural check must catch.
    check (contains (document, R"(\(final\))"), "parentheses must be escaped");
    check (contains (document, R"(\\ done)"), "backslashes must be escaped");

    checkCrossReferenceTable (document);
}

QC_TEST (pdfWriterRejectsUnbalancedPages)
{
    qc::PdfWriter pdf;
    pdf.beginPage();

    bool threw = false;

    try
    {
        // Drawing is fine; finishing with a page still open is a programming error and
        // would silently drop the page's contents.
        pdf.finish();
    }
    catch (const std::logic_error&)
    {
        threw = true;
    }

    check (threw, "finishing with a page open should be rejected");
}

QC_TEST (pdfWriterRefusesDrawingOutsideAPage)
{
    qc::PdfWriter pdf;
    bool threw = false;

    try
    {
        pdf.drawText ("stray", 10.0, 10.0, 10.0);
    }
    catch (const std::logic_error&)
    {
        threw = true;
    }

    check (threw, "drawing with no page open should be rejected rather than lost");
}

QC_TEST (reportProducesOnePagePerFile)
{
    const std::vector<qc::PdfReportItem> items {
        makeItem ("one.wav", -23.1, -1.5),
        makeItem ("two.wav", -18.0, -0.3),
        makeItem ("three.wav", -30.0, -6.0)
    };

    const auto document = qc::writePdfReport (items, fixedDateOptions());

    check (contains (document, "/Count 3"), "the page tree should declare three pages");
    check (contains (document, "Page 1 of 3"), "footers should be numbered");
    check (contains (document, "Page 3 of 3"), "including the last");

    checkCrossReferenceTable (document);
}

QC_TEST (reportStatesTheVerdictAndTheFixHint)
{
    const auto document = qc::writePdfReport ({ makeItem ("loud.wav", -18.0, -0.3) }, fixedDateOptions());

    check (contains (document, "FAIL"), "a failing file should say so");
    check (contains (document, "EBU R128"), "the target should be named");

    // -18 needs -5.0 dB to reach -23, and -0.3 dBTP then lands at -5.3.
    check (contains (document, "-5.0 dB gain"), "the fix hint should carry through to the page");
    check (contains (document, "spec verified 2026-08-03"), "and the spec's provenance");

    checkCrossReferenceTable (document);
}

QC_TEST (reportFlagsAnUnverifiedSpecification)
{
    auto item = makeItem ("mix.wav", -23.0, -2.0);

    auto target = makeTarget();
    target.lastVerified.clear();
    item.verdicts = qc::evaluate (item.result, std::vector<qc::Target> { target });

    const auto document = qc::writePdfReport ({ item }, fixedDateOptions());

    check (contains (document, "spec not verified"),
           "a report must not present a verdict against an unchecked spec as authoritative");
}

QC_TEST (unreadableFilesStillGetAPage)
{
    qc::PdfReportItem broken;
    broken.displayName = "broken.wav";
    broken.errorMessage = "Cannot decode .wav on this machine.";

    const auto document = qc::writePdfReport ({ makeItem ("good.wav", -23.0, -2.0), broken },
                                              fixedDateOptions());

    check (contains (document, "/Count 2"), "the failed file should still get a page");
    check (contains (document, "COULD NOT BE ANALYSED"), "and should say so plainly");
    check (contains (document, "Cannot decode"), "carrying the reason");

    checkCrossReferenceTable (document);
}

QC_TEST (emptyReportIsStillAValidDocument)
{
    const auto document = qc::writePdfReport ({}, fixedDateOptions());

    check (contains (document, "Nothing was analysed"), "it should say what happened");
    checkCrossReferenceTable (document);
}

QC_TEST (longSeriesIsDecimatedRatherThanDrawnPointForPoint)
{
    auto item = makeItem ("long.wav", -23.0, -2.0);

    // An hour of short-term values. Drawn point for point this would be megabytes.
    item.result.loudness.shortTermLufs.clear();
    for (int i = 0; i < 36000; ++i)
        item.result.loudness.shortTermLufs.push_back (-23.0 + std::sin (i * 0.001) * 3.0);

    item.result.source.durationSeconds = 3600.0;

    const auto document = qc::writePdfReport ({ item }, fixedDateOptions());

    check (document.size() < 200000,
           "an hour-long file should not produce a huge page, got "
               + std::to_string (document.size()) + " bytes");

    checkCrossReferenceTable (document);
}

QC_TEST (silentFileReportsDashesRatherThanInfinity)
{
    qc::PdfReportItem item;
    item.displayName = "silence.wav";
    item.result.source.formatName = "WAV file";
    item.result.source.sampleRate = 48000.0;
    item.result.source.numChannels = 2;
    item.result.source.durationSeconds = 10.0;

    const auto document = qc::writePdfReport ({ item }, fixedDateOptions());

    check (! contains (document, "inf"), "infinity must never reach the page");
    check (contains (document, "--"), "unmeasured values should read as dashes");

    checkCrossReferenceTable (document);
}
