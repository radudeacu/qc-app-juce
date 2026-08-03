#include "PdfWriter.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace qc
{
    namespace
    {
        /** Object numbers are fixed for everything that exists once per document; pages
            and their content streams are numbered after these.
        */
        constexpr int kCatalogObject = 1;
        constexpr int kPagesObject = 2;
        constexpr int kFirstFontObject = 3;
        constexpr int kNumFonts = 3;
        constexpr int kFirstPageObject = kFirstFontObject + kNumFonts;

        std::string number (double value)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision (3) << value;

            auto text = stream.str();

            // Trailing zeros are pure noise in a content stream, and there are a lot of
            // numbers in a graph.
            const auto lastNonZero = text.find_last_not_of ('0');

            if (lastNonZero != std::string::npos && text[lastNonZero] == '.')
                text.erase (lastNonZero);
            else if (lastNonZero != std::string::npos)
                text.erase (lastNonZero + 1);

            return text;
        }

        /** PDF string literals are parenthesised, so those and the escape character
            itself have to be escaped or the file will not parse.
        */
        std::string escapeText (const std::string& text)
        {
            std::string result;
            result.reserve (text.size() + 8);

            for (unsigned char character : text)
            {
                switch (character)
                {
                    case '(':  result += "\\("; break;
                    case ')':  result += "\\)"; break;
                    case '\\': result += "\\\\"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;

                    default:
                        if (character < 0x20)
                            break; // Control characters have no place in a report.
                        else if (character > 0x7e)
                            result += '?'; // WinAnsi beyond ASCII is not worth the table.
                        else
                            result += static_cast<char> (character);
                        break;
                }
            }

            return result;
        }

        const char* fontResourceName (PdfWriter::Font font)
        {
            switch (font)
            {
                case PdfWriter::Font::regular: return "/F1";
                case PdfWriter::Font::bold:    return "/F2";
                case PdfWriter::Font::italic:  return "/F3";
            }

            return "/F1";
        }

        const char* fontBaseName (int index)
        {
            switch (index)
            {
                case 0:  return "/Helvetica";
                case 1:  return "/Helvetica-Bold";
                default: return "/Helvetica-Oblique";
            }
        }

        /** Average glyph width as a fraction of point size. Helvetica's lowercase
            averages near 0.5; bold is a little wider.
        */
        double averageWidthFactor (PdfWriter::Font font)
        {
            return font == PdfWriter::Font::bold ? 0.55 : 0.5;
        }
    }

    PdfWriter::PdfWriter (double pageWidthPoints, double pageHeightPoints)
        : pageWidth (pageWidthPoints), pageHeight (pageHeightPoints)
    {
        if (! (pageWidth > 0.0) || ! (pageHeight > 0.0))
            throw std::invalid_argument ("PdfWriter requires positive page dimensions");
    }

    void PdfWriter::append (const std::string& text)
    {
        if (! pageOpen)
            throw std::logic_error ("Drawing outside beginPage()/endPage()");

        currentPage += text;
    }

    void PdfWriter::beginPage()
    {
        if (pageOpen)
            throw std::logic_error ("beginPage() called with a page already open");

        currentPage.clear();
        pageOpen = true;
    }

    void PdfWriter::endPage()
    {
        if (! pageOpen)
            throw std::logic_error ("endPage() called with no page open");

        pages.push_back (currentPage);
        currentPage.clear();
        pageOpen = false;
    }

    void PdfWriter::setFillColour (double red, double green, double blue)
    {
        append (number (red) + " " + number (green) + " " + number (blue) + " rg\n");
    }

    void PdfWriter::setStrokeColour (double red, double green, double blue)
    {
        append (number (red) + " " + number (green) + " " + number (blue) + " RG\n");
    }

    void PdfWriter::setLineWidth (double width)
    {
        append (number (width) + " w\n");
    }

    void PdfWriter::fillRect (double x, double y, double width, double height)
    {
        append (number (x) + " " + number (flipY (y + height)) + " "
                + number (width) + " " + number (height) + " re f\n");
    }

    void PdfWriter::strokeRect (double x, double y, double width, double height)
    {
        append (number (x) + " " + number (flipY (y + height)) + " "
                + number (width) + " " + number (height) + " re S\n");
    }

    void PdfWriter::drawLine (double x1, double y1, double x2, double y2)
    {
        append (number (x1) + " " + number (flipY (y1)) + " m "
                + number (x2) + " " + number (flipY (y2)) + " l S\n");
    }

    void PdfWriter::drawPolyline (const std::vector<std::pair<double, double>>& points)
    {
        if (points.size() < 2)
            return;

        append (number (points.front().first) + " " + number (flipY (points.front().second)) + " m\n");

        for (std::size_t i = 1; i < points.size(); ++i)
            append (number (points[i].first) + " " + number (flipY (points[i].second)) + " l\n");

        append ("S\n");
    }

    void PdfWriter::drawText (const std::string& text, double x, double y, double size, Font font)
    {
        if (text.empty())
            return;

        append (std::string ("BT ") + fontResourceName (font) + " " + number (size) + " Tf "
                + number (x) + " " + number (flipY (y)) + " Td ("
                + escapeText (text) + ") Tj ET\n");
    }

    double PdfWriter::approximateTextWidth (const std::string& text, double size, Font font) const
    {
        return static_cast<double> (text.size()) * size * averageWidthFactor (font);
    }

    std::string PdfWriter::truncateToWidth (const std::string& text, double maxWidth, double size,
                                            Font font) const
    {
        if (approximateTextWidth (text, size, font) <= maxWidth || text.empty())
            return text;

        const auto perCharacter = size * averageWidthFactor (font);

        if (! (perCharacter > 0.0))
            return text;

        const auto fitting = static_cast<std::size_t> (maxWidth / perCharacter);

        if (fitting <= 3)
            return "...";

        return text.substr (0, fitting - 3) + "...";
    }

    std::string PdfWriter::finish() const
    {
        if (pageOpen)
            throw std::logic_error ("finish() called with a page still open");

        const auto pageCount = static_cast<int> (pages.size());

        // Every page contributes two objects: the page and its content stream.
        const int totalObjects = kFirstPageObject - 1 + pageCount * 2;

        std::string document = "%PDF-1.4\n";
        std::vector<std::size_t> offsets (static_cast<std::size_t> (totalObjects) + 1, 0);

        const auto beginObject = [&] (int objectNumber)
        {
            offsets[static_cast<std::size_t> (objectNumber)] = document.size();
            document += std::to_string (objectNumber) + " 0 obj\n";
        };

        beginObject (kCatalogObject);
        document += "<< /Type /Catalog /Pages " + std::to_string (kPagesObject) + " 0 R >>\nendobj\n";

        beginObject (kPagesObject);
        document += "<< /Type /Pages /Count " + std::to_string (pageCount) + " /Kids [";

        for (int i = 0; i < pageCount; ++i)
            document += (i > 0 ? " " : "") + std::to_string (kFirstPageObject + i * 2) + " 0 R";

        document += "] >>\nendobj\n";

        for (int i = 0; i < kNumFonts; ++i)
        {
            beginObject (kFirstFontObject + i);
            document += std::string ("<< /Type /Font /Subtype /Type1 /BaseFont ")
                      + fontBaseName (i) + " /Encoding /WinAnsiEncoding >>\nendobj\n";
        }

        const auto mediaBox = "[0 0 " + number (pageWidth) + " " + number (pageHeight) + "]";

        for (int i = 0; i < pageCount; ++i)
        {
            const int pageObject = kFirstPageObject + i * 2;
            const int contentObject = pageObject + 1;

            beginObject (pageObject);
            document += "<< /Type /Page /Parent " + std::to_string (kPagesObject) + " 0 R"
                      + " /MediaBox " + mediaBox
                      + " /Resources << /Font << /F1 " + std::to_string (kFirstFontObject) + " 0 R"
                      + " /F2 " + std::to_string (kFirstFontObject + 1) + " 0 R"
                      + " /F3 " + std::to_string (kFirstFontObject + 2) + " 0 R >> >>"
                      + " /Contents " + std::to_string (contentObject) + " 0 R >>\nendobj\n";

            const auto& stream = pages[static_cast<std::size_t> (i)];

            beginObject (contentObject);
            document += "<< /Length " + std::to_string (stream.size()) + " >>\nstream\n";
            document += stream;
            document += "endstream\nendobj\n";
        }

        const auto xrefOffset = document.size();

        document += "xref\n0 " + std::to_string (totalObjects + 1) + "\n";
        document += "0000000000 65535 f \n";

        for (int i = 1; i <= totalObjects; ++i)
        {
            std::ostringstream entry;
            entry << std::setw (10) << std::setfill ('0') << offsets[static_cast<std::size_t> (i)]
                  << " 00000 n \n";
            document += entry.str();
        }

        document += "trailer\n<< /Size " + std::to_string (totalObjects + 1)
                  + " /Root " + std::to_string (kCatalogObject) + " 0 R >>\n";
        document += "startxref\n" + std::to_string (xrefOffset) + "\n%%EOF\n";

        return document;
    }
}
