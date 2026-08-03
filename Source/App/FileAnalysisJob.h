#pragma once

#include "../Engine/AnalysisResult.h"

#include <functional>
#include <juce_audio_formats/juce_audio_formats.h>

namespace qc
{
    struct FileAnalysisOutcome
    {
        bool succeeded { false };

        /** Why the file could not be analysed, in terms a user can act on. Never a bare
            "failed to open" - it names the format and what is missing.
        */
        juce::String errorMessage;

        AnalysisResult result;
    };

    /** Reads a file from disk and runs it through the measurement engine.

        Audio is streamed in blocks rather than loaded whole, so a two-hour broadcast
        master costs the same memory as a thirty-second spot.

        @param file           Programme audio.
        @param dialogueStem   Optional stem for dialogue-gated targets. When empty, the
                              result simply carries no dialogue-gated figure and those
                              targets report NOT MEASURED.
        @param shouldCancel   Polled between blocks; return true to abandon the analysis.
        @param onProgress     Called with 0.0-1.0 between blocks. May be null.
    */
    FileAnalysisOutcome analyseFile (const juce::File& file,
                                     const juce::File& dialogueStem = {},
                                     std::function<bool()> shouldCancel = {},
                                     std::function<void (double)> onProgress = {});

    /** Extensions the format manager can actually open on this machine, for error
        messages and for the file chooser's filter.
    */
    juce::String getSupportedFormatWildcard();
}
