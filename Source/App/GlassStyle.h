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
        const juce::Colour backdropBase   { 0xff05070c };

        const juce::Colour accent         { 0xff9db4ff };
        const juce::Colour accentSoft     { 0x809db4ff };

        // Lifted toward white from the original set: at the sizes used for a status
        // word these needed to read at a glance, not merely be distinguishable.
        const juce::Colour pass           { 0xff6ee79b };
        const juce::Colour warn           { 0xffffc949 };
        const juce::Colour fail           { 0xffff8a9b };
        const juce::Colour absent         { 0xffb2bccb };

        inline juce::Colour text (float alpha = 1.0f)       { return juce::Colours::white.withAlpha (alpha); }
        inline juce::Colour secondary (float alpha = 0.78f) { return juce::Colours::white.withAlpha (alpha); }
        inline juce::Colour faint (float alpha = 0.52f)     { return juce::Colours::white.withAlpha (alpha); }
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

        The fill darkens what is behind it rather than lightening it. Tinting glass
        white looks convincing over a photograph but it is the wrong choice here: light
        text on a lightened panel loses most of its contrast, and the interface is
        almost entirely light text.
    */
    void paintPanel (juce::Graphics& g,
                     juce::Rectangle<float> bounds,
                     Depth depth = Depth::raised,
                     float radius = metrics::panelRadius);

    /** An inner well, darker still, for plot areas where thin traces and small axis
        labels have to stay legible against whatever the backdrop is doing.
    */
    void paintWell (juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 10.0f);

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
