#pragma once

#include "../Verdict/VerdictEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace qc
{
    /** Scrollable list of per-target verdicts: a status chip, the checks behind it, and
        the fix hint when there is one.

        Height is derived from the content rather than fixed, because a failing target
        with three checks and a two-line hint needs more room than a passing one.
    */
    class VerdictPanel : public juce::Component
    {
    public:
        VerdictPanel();

        void setVerdicts (std::vector<TargetVerdict> newVerdicts);
        void clear();

        /** Total height needed to show everything, for the enclosing viewport. */
        int getRequiredHeight() const;

        void paint (juce::Graphics& g) override;

        static juce::Colour getStatusColour (Status status);

    private:
        int paintVerdict (juce::Graphics& g, const TargetVerdict& verdict, int y);

        std::vector<TargetVerdict> verdicts;
    };
}
