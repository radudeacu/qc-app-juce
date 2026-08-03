/** Draws the application icon and writes it out as a PNG.

    The icon is generated rather than committed as an opaque bitmap so it can be
    adjusted later without a graphics editor, and so the reasoning behind it stays next
    to it. Run it after changing anything here and commit the resulting PNG - the build
    consumes the file, not this tool.

    Usage: qc_makeicon <icon.png> [preview.png]
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
    constexpr int kCanvas = 1024;

    /** Level-meter bars crossed by a target line: the two things this app is about,
        in the fewest marks that still read at 16 pixels.

        Two bars sit under the line and two over it, which is the whole idea of the
        application - some material meets the target and some does not.
    */
    void drawIcon (juce::Graphics& g, float size)
    {
        const auto scale = size / static_cast<float> (kCanvas);
        const auto px = [scale] (float value) { return value * scale; };

        const juce::Rectangle<float> bounds (0.0f, 0.0f, size, size);
        const auto radius = px (232.0f);

        // Body. A diagonal gradient rather than a flat fill, so the icon has the same
        // depth as the interface it launches.
        juce::ColourGradient body (juce::Colour (0xff232d54), bounds.getX(), bounds.getY(),
                                   juce::Colour (0xff070a12), bounds.getRight(), bounds.getBottom(),
                                   false);
        body.addColour (0.55, juce::Colour (0xff121734));

        g.setGradientFill (body);
        g.fillRoundedRectangle (bounds, radius);

        // A hint of the interface's aurora, kept faint: at small sizes it should read as
        // depth rather than as a second colour competing with the bars.
        juce::ColourGradient glow (juce::Colour (0xff5b4bd6).withAlpha (0.42f),
                                   px (250.0f), px (170.0f),
                                   juce::Colour (0xff5b4bd6).withAlpha (0.0f),
                                   px (250.0f) + px (620.0f), px (170.0f),
                                   true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (bounds, radius);

        const float barWidth = px (116.0f);
        const float gap = px (60.0f);
        const float left = px (190.0f);
        const float baseline = px (768.0f);
        const float heights[] = { px (250.0f), px (430.0f), px (330.0f), px (520.0f) };

        for (int i = 0; i < 4; ++i)
        {
            const auto x = left + static_cast<float> (i) * (barWidth + gap);
            const juce::Rectangle<float> bar (x, baseline - heights[i], barWidth, heights[i]);

            juce::ColourGradient fill (juce::Colours::white.withAlpha (0.97f), x, bar.getY(),
                                       juce::Colours::white.withAlpha (0.78f), x, baseline,
                                       false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (bar, barWidth * 0.5f);
        }

        // The target line, drawn over the bars so the crossing is unmistakable. Its dark
        // underlay keeps it readable where it passes across a white bar.
        const float lineY = px (400.0f);
        const float lineThickness = px (34.0f);
        const juce::Rectangle<float> line (px (140.0f), lineY - lineThickness * 0.5f,
                                           size - px (280.0f), lineThickness);

        g.setColour (juce::Colour (0xff070a12).withAlpha (0.85f));
        g.fillRoundedRectangle (line.expanded (px (5.0f)), lineThickness);

        g.setColour (juce::Colour (0xff9db4ff));
        g.fillRoundedRectangle (line, lineThickness * 0.5f);

        // Hairline rim. Without it the icon dissolves into a dark taskbar.
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (bounds.reduced (px (3.0f)), radius, px (6.0f));
    }

    juce::Image renderIcon (int size)
    {
        juce::Image image (juce::Image::ARGB, size, size, true);
        juce::Graphics g (image);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        drawIcon (g, static_cast<float> (size));
        return image;
    }

    /** Side-by-side sizes on light and dark strips. An icon that works at 512 and
        disappears at 16 is a failed icon, and 16 is where it spends its life.
    */
    juce::Image renderPreview()
    {
        const int sizes[] = { 16, 24, 32, 48, 64, 128, 256 };
        const int padding = 16;

        int width = padding;
        for (int size : sizes)
            width += size + padding;

        const int stripHeight = 256 + padding * 2;
        juce::Image preview (juce::Image::ARGB, width, stripHeight * 2, true);
        juce::Graphics g (preview);

        g.setColour (juce::Colour (0xff11151d));
        g.fillRect (0, 0, width, stripHeight);
        g.setColour (juce::Colour (0xfff2f3f5));
        g.fillRect (0, stripHeight, width, stripHeight);

        int x = padding;

        for (int size : sizes)
        {
            const auto icon = renderIcon (size);
            g.drawImageAt (icon, x, padding + (256 - size) / 2);
            g.drawImageAt (icon, x, stripHeight + padding + (256 - size) / 2);
            x += size + padding;
        }

        return preview;
    }

    bool writePng (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream stream (file);

        if (! stream.openedOk())
            return false;

        juce::PNGImageFormat png;
        return png.writeImageToStream (image, stream);
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: qc_makeicon <icon.png> [preview.png]\n");
        return 1;
    }

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto iconFile = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);

    if (! writePng (renderIcon (kCanvas), iconFile))
    {
        std::fprintf (stderr, "could not write %s\n", iconFile.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("wrote %s (%d x %d)\n", iconFile.getFullPathName().toRawUTF8(), kCanvas, kCanvas);

    if (argc >= 3)
    {
        const auto previewFile = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);

        if (! writePng (renderPreview(), previewFile))
        {
            std::fprintf (stderr, "could not write the preview\n");
            return 1;
        }

        std::printf ("wrote %s\n", previewFile.getFullPathName().toRawUTF8());
    }

    return 0;
}
