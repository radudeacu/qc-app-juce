#include "GlassStyle.h"

namespace qc::glass
{
    namespace
    {
        /** The backdrop is generated at 1/scale and resampled up. Small enough to be
            fast, large enough that the blobs keep their shape.
        */
        constexpr int kBackdropDownscale = 10;

        struct Blob
        {
            float x, y;        // proportion of width/height
            float radius;      // proportion of the larger dimension
            juce::Colour colour;
        };

        // Cool violets and teals with one warm accent, all low saturation once they are
        // spread this wide. Positioned so the brightest area sits behind the header,
        // where the least content overlaps it.
        const Blob kBlobs[] =
        {
            { 0.14f, 0.06f, 0.52f, juce::Colour (0xff5b4bd6) },
            { 0.82f, 0.02f, 0.44f, juce::Colour (0xff2f6fd0) },
            { 0.62f, 0.42f, 0.50f, juce::Colour (0xff1d7f8c) },
            { 0.06f, 0.72f, 0.46f, juce::Colour (0xff6d3f9e) },
            { 0.92f, 0.86f, 0.42f, juce::Colour (0xffb4508a) },
            { 0.38f, 0.96f, 0.38f, juce::Colour (0xff28527a) }
        };

        float depthFill (Depth depth)
        {
            switch (depth)
            {
                case Depth::recessed: return 0.035f;
                case Depth::raised:   return 0.070f;
                case Depth::floating: return 0.115f;
            }

            return 0.070f;
        }

        float depthBorder (Depth depth)
        {
            switch (depth)
            {
                case Depth::recessed: return 0.070f;
                case Depth::raised:   return 0.130f;
                case Depth::floating: return 0.200f;
            }

            return 0.130f;
        }

        void overlayGrain (juce::Image& image)
        {
            juce::Random random (0x9e3779b9);
            const juce::Image::BitmapData data (image, juce::Image::BitmapData::readWrite);

            for (int y = 0; y < data.height; ++y)
            {
                for (int x = 0; x < data.width; ++x)
                {
                    // +/-3 levels is invisible as texture but enough to break up the
                    // straight edges that wide gradients otherwise show as banding.
                    const auto noise = random.nextInt (7) - 3;
                    auto pixel = data.getPixelColour (x, y);

                    data.setPixelColour (x, y,
                                         juce::Colour::fromRGB (
                                             static_cast<juce::uint8> (juce::jlimit (0, 255, pixel.getRed() + noise)),
                                             static_cast<juce::uint8> (juce::jlimit (0, 255, pixel.getGreen() + noise)),
                                             static_cast<juce::uint8> (juce::jlimit (0, 255, pixel.getBlue() + noise))));
                }
            }
        }
    }

    juce::Image renderBackdrop (int width, int height)
    {
        width = juce::jmax (1, width);
        height = juce::jmax (1, height);

        const int smallWidth = juce::jmax (8, width / kBackdropDownscale);
        const int smallHeight = juce::jmax (8, height / kBackdropDownscale);

        juce::Image small (juce::Image::RGB, smallWidth, smallHeight, true);

        {
            juce::Graphics g (small);
            g.fillAll (colour::backdropBase);

            const auto longest = static_cast<float> (juce::jmax (smallWidth, smallHeight));

            for (const auto& blob : kBlobs)
            {
                const auto centreX = blob.x * static_cast<float> (smallWidth);
                const auto centreY = blob.y * static_cast<float> (smallHeight);
                const auto radius = blob.radius * longest;

                juce::ColourGradient gradient (blob.colour.withAlpha (0.55f), centreX, centreY,
                                               blob.colour.withAlpha (0.0f), centreX + radius, centreY,
                                               true);

                g.setGradientFill (gradient);
                g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
            }

            // Darken the lower half so content there keeps its contrast.
            juce::ColourGradient shade (juce::Colours::transparentBlack, 0.0f, 0.0f,
                                        juce::Colours::black.withAlpha (0.45f), 0.0f,
                                        static_cast<float> (smallHeight), false);
            g.setGradientFill (shade);
            g.fillAll();
        }

        auto backdrop = small.rescaled (width, height, juce::Graphics::highResamplingQuality);
        overlayGrain (backdrop);

        return backdrop;
    }

    void paintPanelShadow (juce::Graphics& g, juce::Rectangle<float> bounds, float radius, float strength)
    {
        // Concentric rounded rectangles rather than a real blur: at these radii the
        // result is indistinguishable and costs nothing per repaint.
        const int layers = 7;

        for (int i = layers; i >= 1; --i)
        {
            const auto spread = static_cast<float> (i) * 1.6f;
            const auto alpha = 0.045f * strength * (1.0f - static_cast<float> (i) / (layers + 1.0f));

            g.setColour (juce::Colours::black.withAlpha (alpha));
            g.fillRoundedRectangle (bounds.expanded (spread).translated (0.0f, spread * 0.45f),
                                    radius + spread);
        }
    }

    void paintPanel (juce::Graphics& g, juce::Rectangle<float> bounds, Depth depth, float radius)
    {
        // Vertical gradient rather than a flat wash: a real pane catches more light at
        // the top, and this is most of what separates glass from a grey rectangle.
        const auto fill = depthFill (depth);

        juce::ColourGradient body (juce::Colours::white.withAlpha (fill * 1.5f),
                                   bounds.getCentreX(), bounds.getY(),
                                   juce::Colours::white.withAlpha (fill * 0.6f),
                                   bounds.getCentreX(), bounds.getBottom(),
                                   false);

        g.setGradientFill (body);
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (juce::Colours::white.withAlpha (depthBorder (depth)));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, metrics::borderWidth);

        // The specular line along the top edge. Inset from the corners so it fades out
        // where the border curves away rather than stopping abruptly.
        const auto inset = radius * 0.75f;
        juce::ColourGradient highlight (juce::Colours::white.withAlpha (0.0f), bounds.getX() + inset, 0.0f,
                                        juce::Colours::white.withAlpha (0.0f), bounds.getRight() - inset, 0.0f,
                                        false);
        highlight.addColour (0.5, juce::Colours::white.withAlpha (depth == Depth::recessed ? 0.14f : 0.28f));

        g.setGradientFill (highlight);
        g.fillRect (bounds.getX() + inset, bounds.getY() + 1.0f, bounds.getWidth() - inset * 2.0f, 1.0f);
    }

    void paintStatusPill (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour tint,
                          const juce::String& text)
    {
        g.setColour (tint.withAlpha (0.16f));
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

        g.setColour (tint.withAlpha (0.40f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), bounds.getHeight() * 0.5f, 1.0f);

        g.setColour (tint.brighter (0.15f));
        g.setFont (font (bounds.getHeight() * 0.46f, true));
        g.drawText (text, bounds, juce::Justification::centred);
    }

    void paintSeparator (juce::Graphics& g, juce::Rectangle<float> bounds, float alpha)
    {
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.fillRect (bounds.withHeight (1.0f));
    }

    juce::Font font (float height, bool bold)
    {
        // Segoe UI Variable is the modern Windows face; the fallbacks keep this sane on
        // installs that predate it and on other platforms.
        static const juce::StringArray preferred { "Segoe UI Variable Text", "Segoe UI", "Inter", "Helvetica Neue" };
        static const juce::String chosen = [&]
        {
            const auto available = juce::Font::findAllTypefaceNames();

            for (const auto& name : preferred)
                if (available.contains (name))
                    return name;

            return juce::Font::getDefaultSansSerifFontName();
        }();

        auto options = juce::FontOptions (chosen, height, bold ? juce::Font::bold : juce::Font::plain);
        return juce::Font (options);
    }
}
