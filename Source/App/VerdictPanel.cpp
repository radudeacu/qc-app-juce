#include "VerdictPanel.h"

#include <algorithm>

namespace qc
{
    namespace
    {
        constexpr int kHeaderHeight = 30;
        constexpr int kCheckHeight = 19;
        constexpr int kHintLineHeight = 17;
        constexpr int kVerdictSpacing = 14;
        constexpr int kPadding = 12;
        constexpr int kChipWidth = 104;

        int countLines (const std::string& text)
        {
            if (text.empty())
                return 0;

            return 1 + static_cast<int> (std::count (text.begin(), text.end(), '\n'));
        }

        int heightOf (const TargetVerdict& verdict)
        {
            return kHeaderHeight
                 + static_cast<int> (verdict.checks.size()) * kCheckHeight
                 + countLines (verdict.fixHint) * kHintLineHeight
                 + kVerdictSpacing;
        }
    }

    VerdictPanel::VerdictPanel()
    {
        setOpaque (false);
    }

    juce::Colour VerdictPanel::getStatusColour (Status status)
    {
        switch (status)
        {
            case Status::pass:        return juce::Colour (0xff48c774);
            case Status::warn:        return juce::Colour (0xffe6a23c);
            case Status::fail:        return juce::Colour (0xffe0575b);
            case Status::notMeasured: return juce::Colour (0xff8892a4);
        }

        return juce::Colour (0xff8892a4);
    }

    void VerdictPanel::setVerdicts (std::vector<TargetVerdict> newVerdicts)
    {
        verdicts = std::move (newVerdicts);
        repaint();
    }

    void VerdictPanel::clear()
    {
        verdicts.clear();
        repaint();
    }

    int VerdictPanel::getRequiredHeight() const
    {
        int height = kPadding;

        for (const auto& verdict : verdicts)
            height += heightOf (verdict);

        return height + kPadding;
    }

    int VerdictPanel::paintVerdict (juce::Graphics& g, const TargetVerdict& verdict, int y)
    {
        const int width = getWidth();
        const auto colour = getStatusColour (verdict.status);

        const juce::Rectangle<int> chip (kPadding, y + 4, kChipWidth, kHeaderHeight - 10);

        g.setColour (colour.withAlpha (0.18f));
        g.fillRoundedRectangle (chip.toFloat(), 4.0f);

        g.setColour (colour);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (toString (verdict.status), chip, juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (verdict.targetName,
                    juce::Rectangle<int> (chip.getRight() + 12, y, width - chip.getRight() - 24, kHeaderHeight),
                    juce::Justification::centredLeft);

        y += kHeaderHeight;

        for (const auto& check : verdict.checks)
        {
            const auto checkColour = getStatusColour (check.status);

            g.setColour (checkColour.withAlpha (check.status == Status::pass ? 0.45f : 0.9f));
            g.fillEllipse (static_cast<float> (kPadding + 6), static_cast<float> (y + 7), 5.0f, 5.0f);

            g.setColour (juce::Colours::white.withAlpha (0.66f));
            g.setFont (juce::FontOptions (12.5f));
            g.drawText (check.name,
                        juce::Rectangle<int> (kPadding + 20, y, 210, kCheckHeight),
                        juce::Justification::centredLeft);

            g.setColour (juce::Colours::white.withAlpha (0.82f));
            g.drawText (check.detail,
                        juce::Rectangle<int> (kPadding + 236, y, width - kPadding - 248, kCheckHeight),
                        juce::Justification::centredLeft);

            y += kCheckHeight;
        }

        if (! verdict.fixHint.empty())
        {
            juce::StringArray lines;
            lines.addLines (verdict.fixHint);

            g.setFont (juce::FontOptions (12.5f, juce::Font::italic));

            for (const auto& line : lines)
            {
                g.setColour (juce::Colour (0xffffd479));
                g.drawText (line,
                            juce::Rectangle<int> (kPadding + 20, y, width - kPadding - 32, kHintLineHeight),
                            juce::Justification::centredLeft);

                y += kHintLineHeight;
            }
        }

        return y + kVerdictSpacing;
    }

    void VerdictPanel::paint (juce::Graphics& g)
    {
        if (verdicts.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (juce::FontOptions (13.0f));
            g.drawText ("Select one or more targets, then drop a file to see whether it passes.",
                        getLocalBounds().reduced (kPadding), juce::Justification::centredTop);
            return;
        }

        int y = kPadding;

        for (const auto& verdict : verdicts)
            y = paintVerdict (g, verdict, y);
    }
}
