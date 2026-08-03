#pragma once

#include <string>
#include <vector>

namespace qc
{
    /** Minimal PDF 1.4 document builder.

        JUCE provides no PDF or printing API on Windows, so rather than shell out to a
        renderer or ship a library for one file format, the report emits PDF operators
        directly. Only what a QC report needs is here: filled and stroked rectangles,
        polylines, and text in the standard Helvetica faces.

        Using the standard fonts means nothing has to be embedded, which keeps a report
        small and openable anywhere.

        Coordinates are top-left origin with y increasing downwards, which is how the
        layout code thinks. The flip to PDF's bottom-left origin happens here.
    */
    class PdfWriter
    {
    public:
        enum class Font
        {
            regular,
            bold,
            italic
        };

        /** Defaults to A4 in points. */
        explicit PdfWriter (double pageWidthPoints = 595.28, double pageHeightPoints = 841.89);

        void beginPage();
        void endPage();

        void setFillColour (double red, double green, double blue);
        void setStrokeColour (double red, double green, double blue);
        void setLineWidth (double width);

        void fillRect (double x, double y, double width, double height);
        void strokeRect (double x, double y, double width, double height);
        void drawLine (double x1, double y1, double x2, double y2);

        /** Stroked polyline. Fewer than two points draws nothing. */
        void drawPolyline (const std::vector<std::pair<double, double>>& points);

        /** @param y  Baseline of the text, measured from the top of the page. */
        void drawText (const std::string& text, double x, double y, double size,
                       Font font = Font::regular);

        /** Approximate width of a string, for truncation only.

            Deliberately approximate: exact widths would mean shipping the Helvetica
            metrics for three faces, and nothing in this report is centred or
            right-aligned against a measured width. Used to decide where to cut a long
            file path, where being a few points out is invisible.
        */
        double approximateTextWidth (const std::string& text, double size, Font font = Font::regular) const;

        /** Truncates with an ellipsis so it fits within maxWidth. */
        std::string truncateToWidth (const std::string& text, double maxWidth, double size,
                                     Font font = Font::regular) const;

        double getPageWidth() const noexcept { return pageWidth; }
        double getPageHeight() const noexcept { return pageHeight; }
        int getPageCount() const noexcept { return static_cast<int> (pages.size()); }

        /** The complete document. Valid to call once every page has been ended. */
        std::string finish() const;

    private:
        void append (const std::string& text);
        double flipY (double y) const noexcept { return pageHeight - y; }

        double pageWidth;
        double pageHeight;

        std::vector<std::string> pages;
        std::string currentPage;
        bool pageOpen { false };
    };
}
