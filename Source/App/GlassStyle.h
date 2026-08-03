#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace qc::glass
{
    /** The palette. Everything on screen comes from here so the interface stays one
        material rather than a collection of separately-tinted panels.

        Text is expressed as alpha over the backdrop rather than as fixed greys: on
        glass, opacity is what reads as hierarchy, and a fixed grey would go muddy
        wherever the aurora behind it is bright.
    */
    namespace colour
    {
        const juce::Colour backdropBase   { 0xff0a0d14 };

        const juce::Colour accent         { 0xff7c9cff };
        const juce::Colour accentSoft     { 0x807c9cff };

        const juce::Colour pass           { 0xff4ade80 };
        const juce::Colour warn           { 0xfffbbf24 };
        const juce::Colour fail           { 0xfffb7185 };
        const juce::Colour absent         { 0xff94a3b8 };

        inline juce::Colour text (float alpha = 0.92f)      { return juce::Colours::white.withAlpha (alpha); }
        inline juce::Colour secondary (float alpha = 0.58f) { return juce::Colours::white.withAlpha (alpha); }
        inline juce::Colour faint (float alpha = 0.30f)     { return juce::Colours::white.withAlpha (alpha); }
    }

    namespace metrics
    {
        constexpr float panelRadius = 18.0f;
        constexpr float controlRadius = 9.0f;
        constexpr float borderWidth = 1.0f;
    }

    /** How much a surface stands out from the backdrop. */
    enum class Depth
    {
        /** Large background surfaces: the target column, the graph well. */
        recessed,

        /** The default sheet of glass. */
        raised,

        /** Foreground elements that should read as sitting on top: selected rows,
            hovered controls.
        */
        floating
    };

    /** The aurora that everything else sits on.

        Drawn at a fraction of the final size and scaled up, which produces genuinely
        smooth falloff far more cheaply than a large-radius blur, and gives the soft
        colour fields that make translucent panels read as glass rather than as flat
        grey rectangles. A faint grain is laid over the top: without it, wide smooth
        gradients band visibly on 8-bit displays.
    */
    juce::Image renderBackdrop (int width, int height);

    /** A frosted panel: translucent fill, a hairline border, and a highlight along the
        top edge where light would catch a real pane.
    */
    void paintPanel (juce::Graphics& g,
                     juce::Rectangle<float> bounds,
                     Depth depth = Depth::raised,
                     float radius = metrics::panelRadius);

    /** Soft shadow beneath a panel. Drawn separately so a caller can skip it where
        panels butt against each other and the shadows would stack into a dark seam.
    */
    void paintPanelShadow (juce::Graphics& g,
                           juce::Rectangle<float> bounds,
                           float radius = metrics::panelRadius,
                           float strength = 1.0f);

    /** A pill carrying a status word, tinted by its colour. */
    void paintStatusPill (juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          juce::Colour tint,
                          const juce::String& text);

    /** Hairline rule, for separating rows without drawing a hard line. */
    void paintSeparator (juce::Graphics& g, juce::Rectangle<float> bounds, float alpha = 0.08f);

    /** The interface font. Falls back cleanly when the preferred face is missing. */
    juce::Font font (float height, bool bold = false);
}
