#pragma once

#include "GlassStyle.h"

namespace qc
{
    /** Restyles JUCE's stock controls to match the glass surfaces.

        Without this the buttons, toggles and scrollbars keep their default grey
        chrome, which reads as pasted on top of the interface rather than part of it -
        the single biggest giveaway in a themed JUCE app.
    */
    class GlassLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        GlassLookAndFeel();

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
        juce::Font getLabelFont (juce::Label&) override;

        void drawButtonBackground (juce::Graphics& g,
                                   juce::Button& button,
                                   const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;

        void drawButtonText (juce::Graphics& g,
                             juce::TextButton& button,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

        void drawToggleButton (juce::Graphics& g,
                               juce::ToggleButton& button,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

        void drawScrollbar (juce::Graphics& g,
                            juce::ScrollBar& scrollbar,
                            int x, int y, int width, int height,
                            bool isScrollbarVertical,
                            int thumbStartPosition,
                            int thumbSize,
                            bool isMouseOver,
                            bool isMouseDown) override;

        void drawTableHeaderBackground (juce::Graphics& g, juce::TableHeaderComponent& header) override;

        void drawTableHeaderColumn (juce::Graphics& g,
                                    juce::TableHeaderComponent& header,
                                    const juce::String& columnName,
                                    int columnId,
                                    int width, int height,
                                    bool isMouseOver,
                                    bool isMouseDown,
                                    int columnFlags) override;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassLookAndFeel)
    };
}
