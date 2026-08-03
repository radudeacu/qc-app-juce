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

        /** How much the panel darkens what is behind it. */
        float depthShade (Depth depth)
        {
            switch (depth)
            {
                case Depth::recessed: return 0.52f;
                case Depth::raised:   return 0.40f;
                case Depth::floating: return 0.26f;
            }

            return 0.40f;
        }

        /** The white lift laid over the shade, which is what still reads as glass. */
        float depthSheen (Depth depth)
        {
            switch (depth)
            {
                case Depth::recessed: return 0.020f;
                case Depth::raised:   return 0.045f;
                case Depth::floating: return 0.090f;
            }

            return 0.045f;
        }

        float depthBorder (Depth depth)
        {
            switch (depth)
            {
                case Depth::recessed: return 0.10f;
                case Depth::raised:   return 0.17f;
                case Depth::floating: return 0.26f;
            }

            return 0.17f;
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

                juce::ColourGradient gradient (blob.colour.withAlpha (0.46f), centreX, centreY,
                                               blob.colour.withAlpha (0.0f), centreX + radius, centreY,
                                               true);

                g.setGradientFill (gradient);
                g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
            }

            // Darken towards the bottom, where the densest content sits.
            juce::ColourGradient shade (juce::Colours::black.withAlpha (0.10f), 0.0f, 0.0f,
                                        juce::Colours::black.withAlpha (0.60f), 0.0f,
                                        static_cast<float> (smallHeight), false);
            g.setGradientFill (shade);
            g.fillAll();

            // A flat veil over everything. The aurora is meant to be felt rather than
            // looked at, and every panel sits on top of it.
            g.setColour (juce::Colours::black.withAlpha (0.10f));
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
        g.setColour (juce::Colours::black.withAlpha (depthShade (depth)));
        g.fillRoundedRectangle (bounds, radius);

        // Vertical gradient rather than a flat wash: a real pane catches more light at
        // the top, and this is most of what separates glass from a grey rectangle.
        const auto sheen = depthSheen (depth);

        juce::ColourGradient body (juce::Colours::white.withAlpha (sheen * 1.6f),
                                   bounds.getCentreX(), bounds.getY(),
                                   juce::Colours::white.withAlpha (sheen * 0.4f),
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
        highlight.addColour (0.5, juce::Colours::white.withAlpha (depth == Depth::recessed ? 0.18f : 0.34f));

        g.setGradientFill (highlight);
        g.fillRect (bounds.getX() + inset, bounds.getY() + 1.0f, bounds.getWidth() - inset * 2.0f, 1.0f);
    }

    void paintStatusPill (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour tint,
                          const juce::String& text)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

        g.setColour (tint.withAlpha (0.26f));
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

        g.setColour (tint.withAlpha (0.75f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), bounds.getHeight() * 0.5f, 1.2f);

        g.setColour (tint);
        g.setFont (font (bounds.getHeight() * 0.5f, true));
        g.drawText (text, bounds, juce::Justification::centred);
    }

    void paintWell (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
    {
        g.setColour (juce::Colours::black.withAlpha (0.34f));
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
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
