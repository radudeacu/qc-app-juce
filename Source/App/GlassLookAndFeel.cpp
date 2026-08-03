#include "GlassLookAndFeel.h"

namespace qc
{
    using namespace glass;

    GlassLookAndFeel::GlassLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, colour::backdropBase);
        setColour (juce::Label::textColourId, colour::text());
        setColour (juce::TextButton::textColourOnId, colour::text());
        setColour (juce::TextButton::textColourOffId, colour::text (0.86f));
        setColour (juce::ToggleButton::textColourId, colour::text (0.86f));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf01b2130));
        setColour (juce::TooltipWindow::textColourId, colour::text (0.88f));
        setColour (juce::TooltipWindow::outlineColourId, juce::Colours::white.withAlpha (0.12f));
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, juce::Colours::white.withAlpha (0.22f));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf0161c28));
        setColour (juce::PopupMenu::textColourId, colour::text (0.9f));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::accent.withAlpha (0.28f));
    }

    juce::Font GlassLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
    {
        return font (juce::jmin (15.0f, static_cast<float> (buttonHeight) * 0.45f));
    }

    juce::Font GlassLookAndFeel::getLabelFont (juce::Label& label)
    {
        return font (label.getFont().getHeight());
    }

    void GlassLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                 juce::Button& button,
                                                 const juce::Colour&,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto radius = metrics::controlRadius;

        if (! button.isEnabled())
        {
            // Disabled controls recede rather than greying out: on glass, a grey fill
            // reads as a different material instead of the same one, dimmed.
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRoundedRectangle (bounds, radius);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawRoundedRectangle (bounds, radius, 1.0f);
            return;
        }

        const auto depth = shouldDrawButtonAsDown ? Depth::recessed
                         : shouldDrawButtonAsHighlighted ? Depth::floating
                                                         : Depth::raised;

        paintPanel (g, bounds, depth, radius);

        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
        {
            g.setColour (colour::accent.withAlpha (shouldDrawButtonAsDown ? 0.22f : 0.14f));
            g.fillRoundedRectangle (bounds, radius);
        }
    }

    void GlassLookAndFeel::drawButtonText (juce::Graphics& g,
                                           juce::TextButton& button,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool)
    {
        const auto alpha = ! button.isEnabled() ? 0.28f
                         : shouldDrawButtonAsHighlighted ? 1.0f
                                                         : 0.86f;

        g.setColour (colour::text (alpha));
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
    }

    void GlassLookAndFeel::drawToggleButton (juce::Graphics& g,
                                             juce::ToggleButton& button,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool)
    {
        auto bounds = button.getLocalBounds().toFloat();
        const auto on = button.getToggleState();

        // A rounded square rather than JUCE's tick: it sits better against the panel
        // radii used everywhere else.
        const auto boxSize = juce::jmin (18.0f, bounds.getHeight() - 4.0f);
        auto box = juce::Rectangle<float> (bounds.getX() + 1.0f,
                                           bounds.getCentreY() - boxSize * 0.5f,
                                           boxSize, boxSize);

        if (on)
        {
            g.setColour (colour::accent.withAlpha (0.85f));
            g.fillRoundedRectangle (box, 5.0f);

            g.setColour (juce::Colours::white.withAlpha (0.95f));
            juce::Path tick;
            tick.startNewSubPath (box.getX() + boxSize * 0.26f, box.getCentreY());
            tick.lineTo (box.getX() + boxSize * 0.44f, box.getY() + boxSize * 0.68f);
            tick.lineTo (box.getX() + boxSize * 0.76f, box.getY() + boxSize * 0.32f);
            g.strokePath (tick, juce::PathStrokeType (1.9f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsHighlighted ? 0.10f : 0.05f));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsHighlighted ? 0.32f : 0.18f));
            g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
        }

        g.setColour (colour::text (on ? 0.94f : 0.62f));
        g.setFont (font (13.0f));
        g.drawText (button.getButtonText(),
                    bounds.withTrimmedLeft (boxSize + 10.0f),
                    juce::Justification::centredLeft, true);
    }

    void GlassLookAndFeel::drawScrollbar (juce::Graphics& g,
                                          juce::ScrollBar&,
                                          int x, int y, int width, int height,
                                          bool isScrollbarVertical,
                                          int thumbStartPosition,
                                          int thumbSize,
                                          bool isMouseOver,
                                          bool isMouseDown)
    {
        if (thumbSize <= 0)
            return;

        juce::Rectangle<float> thumb;

        if (isScrollbarVertical)
            thumb = { static_cast<float> (x) + static_cast<float> (width) * 0.35f,
                      static_cast<float> (thumbStartPosition),
                      static_cast<float> (width) * 0.3f,
                      static_cast<float> (thumbSize) };
        else
            thumb = { static_cast<float> (thumbStartPosition),
                      static_cast<float> (y) + static_cast<float> (height) * 0.35f,
                      static_cast<float> (thumbSize),
                      static_cast<float> (height) * 0.3f };

        const auto alpha = isMouseDown ? 0.42f : isMouseOver ? 0.32f : 0.18f;
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.fillRoundedRectangle (thumb, thumb.getWidth() * 0.5f);
    }

    void GlassLookAndFeel::drawTableHeaderBackground (juce::Graphics& g, juce::TableHeaderComponent& header)
    {
        const auto bounds = header.getLocalBounds().toFloat();

        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (bounds);

        paintSeparator (g, bounds.withY (bounds.getBottom() - 1.0f), 0.12f);
    }

    void GlassLookAndFeel::drawTableHeaderColumn (juce::Graphics& g,
                                                  juce::TableHeaderComponent&,
                                                  const juce::String& columnName,
                                                  int,
                                                  int width, int height,
                                                  bool isMouseOver,
                                                  bool,
                                                  int columnFlags)
    {
        const auto area = juce::Rectangle<int> (0, 0, width, height).reduced (8, 0);

        g.setColour (colour::text (isMouseOver ? 0.85f : 0.55f));
        g.setFont (font (11.0f, true));
        g.drawText (columnName.toUpperCase(), area, juce::Justification::centredLeft, true);

        // A sorted column earns a small caret rather than a whole different style.
        if ((columnFlags & (juce::TableHeaderComponent::sortedForwards
                            | juce::TableHeaderComponent::sortedBackwards)) != 0)
        {
            const auto ascending = (columnFlags & juce::TableHeaderComponent::sortedForwards) != 0;
            const auto centreX = static_cast<float> (width) - 12.0f;
            const auto centreY = static_cast<float> (height) * 0.5f;

            juce::Path caret;
            if (ascending)
            {
                caret.startNewSubPath (centreX - 3.5f, centreY + 1.8f);
                caret.lineTo (centreX, centreY - 2.2f);
                caret.lineTo (centreX + 3.5f, centreY + 1.8f);
            }
            else
            {
                caret.startNewSubPath (centreX - 3.5f, centreY - 2.2f);
                caret.lineTo (centreX, centreY + 1.8f);
                caret.lineTo (centreX + 3.5f, centreY - 2.2f);
            }

            g.setColour (colour::accent.withAlpha (0.9f));
            g.strokePath (caret, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (static_cast<float> (width) - 1.0f, 6.0f, 1.0f, static_cast<float> (height) - 12.0f);
    }
}
