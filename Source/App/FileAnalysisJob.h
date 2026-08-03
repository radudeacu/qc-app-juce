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

    /** The shared format manager. Exposed so playback opens exactly the formats
        analysis can - a file that measures but will not play, or the reverse, would be
        a confusing thing to explain.
    */
    juce::AudioFormatManager& getFormatManager();

    /** Extensions the format manager can actually open on this machine, for error
        messages and for the file chooser's filter.
    */
    juce::String getSupportedFormatWildcard();

    /** True if this file's extension is one the format manager can open. Used to filter
        a dropped folder so that artwork and text files are skipped silently rather than
        filling the results table with errors the user cannot act on.
    */
    bool isSupportedAudioFile (const juce::File& file);

    /** Audio files from a set of dropped paths. Directories contribute their contents;
        anything unreadable is left out rather than reported.

        @param recurseIntoSubfolders  Off by default in the UI: dropping a project folder
                                      should not silently pull in every bounce and stem
                                      nested underneath it.
    */
    std::vector<juce::File> collectAudioFiles (const juce::StringArray& paths,
                                               bool recurseIntoSubfolders);
}
