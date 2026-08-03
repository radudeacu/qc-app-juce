#include "VerdictPanel.h"

#include "GlassStyle.h"

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

        /** With no verdicts the panel still has something to say, and sizing it to its
            (zero) content would clip the message out of existence.
        */
        constexpr int kEmptyHeight = 132;

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
            case Status::pass:        return glass::colour::pass;
            case Status::warn:        return glass::colour::warn;
            case Status::fail:        return glass::colour::fail;
            case Status::notMeasured: return glass::colour::absent;
        }

        return glass::colour::absent;
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
        if (verdicts.empty())
            return kEmptyHeight;

        int height = kPadding;

        for (const auto& verdict : verdicts)
            height += heightOf (verdict);

        return height + kPadding;
    }

    int VerdictPanel::paintVerdict (juce::Graphics& g, const TargetVerdict& verdict, int y)
    {
        const int width = getWidth();
        const auto colour = getStatusColour (verdict.status);

        const juce::Rectangle<int> chip (kPadding, y + 5, kChipWidth, kHeaderHeight - 12);
        glass::paintStatusPill (g, chip.toFloat(), colour, toString (verdict.status));

        g.setColour (glass::colour::text (0.94f));
        g.setFont (glass::font (15.0f, true));
        g.drawText (verdict.targetName,
                    juce::Rectangle<int> (chip.getRight() + 12, y, width - chip.getRight() - 24, kHeaderHeight),
                    juce::Justification::centredLeft);

        y += kHeaderHeight;

        for (const auto& check : verdict.checks)
        {
            const auto checkColour = getStatusColour (check.status);

            g.setColour (checkColour.withAlpha (check.status == Status::pass ? 0.45f : 0.9f));
            g.fillEllipse (static_cast<float> (kPadding + 6), static_cast<float> (y + 7), 5.0f, 5.0f);

            g.setColour (glass::colour::secondary (0.6f));
            g.setFont (glass::font (12.5f));
            g.drawText (check.name,
                        juce::Rectangle<int> (kPadding + 20, y, 210, kCheckHeight),
                        juce::Justification::centredLeft);

            g.setColour (glass::colour::text (0.86f));
            g.drawText (check.detail,
                        juce::Rectangle<int> (kPadding + 236, y, width - kPadding - 248, kCheckHeight),
                        juce::Justification::centredLeft);

            y += kCheckHeight;
        }

        if (! verdict.fixHint.empty())
        {
            juce::StringArray lines;
            lines.addLines (verdict.fixHint);

            g.setFont (glass::font (12.5f));

            for (const auto& line : lines)
            {
                g.setColour (glass::colour::warn.withAlpha (0.92f));
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
            g.setColour (glass::colour::secondary (0.45f));
            g.setFont (glass::font (13.0f));
            g.drawText ("Select one or more targets, then drop a file to see whether it passes.",
                        getLocalBounds().reduced (kPadding), juce::Justification::centred);
            return;
        }

        int y = kPadding;

        for (const auto& verdict : verdicts)
            y = paintVerdict (g, verdict, y);
    }
}
