#pragma once

#include "../Verdict/Target.h"

#include <juce_core/juce_core.h>

namespace qc
{
    struct TargetLoadResult
    {
        std::vector<Target> targets;

        /** Entries that could not be understood, and why. Shown to the user rather than
            swallowed: a target silently missing from the list would look like a pass.
        */
        juce::StringArray problems;

        /** Targets whose numbers have never been checked against the published spec. */
        juce::StringArray unverified;

        /** Where the definitions came from, for display in the report. */
        juce::String sourceDescription;
    };

    /** Loads delivery specifications from JSON.

        The factory copy is compiled into the binary. On first run it is written to the
        user's application data directory, and that copy wins from then on — so when a
        platform changes its numbers the fix is a text edit, not a rebuild, and an app
        update never silently overwrites a correction the user made.
    */
    class TargetLibrary
    {
    public:
        static juce::File getUserTargetsFile();

        /** Parses JSON text. Malformed entries are skipped and reported in `problems`. */
        static TargetLoadResult parse (const juce::String& jsonText, const juce::String& sourceDescription);

        /** User copy if it exists, otherwise the built-in default (which is then written
            out for editing).
        */
        static TargetLoadResult load();

        /** The factory definitions, ignoring any user copy. */
        static juce::String getBuiltInJson();
    };
}
