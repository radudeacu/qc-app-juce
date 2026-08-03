#pragma once

#include "../Engine/AnalysisResult.h"
#include "../Verdict/VerdictEngine.h"

#include <string>
#include <vector>

namespace qc
{
    /** One file's worth of report. A file that could not be read still gets a page:
        a QC report has to record what was not checked, or it overstates what it covers.
    */
    struct PdfReportItem
    {
        std::string displayName;
        AnalysisResult result;
        std::vector<TargetVerdict> verdicts;

        /** Non-empty when the file failed to analyse; the page then states the reason
            instead of showing measurements.
        */
        std::string errorMessage;
    };

    struct PdfReportOptions
    {
        std::string title { "Loudness QC Report" };

        /** ISO date shown in the header. Empty means today, which is what the app wants
            and what a test cannot assert against.
        */
        std::string generatedOn;

        std::string generatorName { "QC App" };
        std::string generatorVersion { "0.1.0" };
    };

    /** A print-ready A4 report, one page per item.

        Everything is drawn from the analysis data, so the page is vector text and
        vector graphics rather than a screenshot - it stays sharp at any zoom and the
        numbers can be selected and copied out of it.
    */
    std::string writePdfReport (const std::vector<PdfReportItem>& items,
                                const PdfReportOptions& options = {});
}
