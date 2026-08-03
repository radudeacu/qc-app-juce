#pragma once

#include "FileAnalysisJob.h"

#include "../Verdict/VerdictEngine.h"

#include <atomic>
#include <functional>
#include <juce_events/juce_events.h>
#include <memory>
#include <vector>

namespace qc
{
    struct BatchEntry
    {
        enum class State
        {
            pending,
            running,
            completed,
            failed
        };

        juce::File file;
        State state { State::pending };

        /** Populated when state is failed. The run continues regardless: one unreadable
            file in a delivery folder must not cost the results for the other ninety.
        */
        juce::String errorMessage;

        AnalysisResult result;
        std::vector<TargetVerdict> verdicts;
        Status overall { Status::notMeasured };
    };

    /** Runs a set of files through the analyser in parallel and reports back on the
        message thread.

        Worker count is capped well below the core count: analysis is memory-bandwidth
        bound rather than compute bound, so saturating every core buys little and makes
        the machine unusable while a folder is being checked.

        Entries are only ever mutated on the message thread. Worker threads produce an
        outcome and hand it over, which is why there is no lock here.
    */
    class BatchAnalyser
    {
    public:
        BatchAnalyser();
        ~BatchAnalyser();

        /** Begins a run, replacing any previous one. Safe to call while a run is in
            progress: the old run is cancelled first.
        */
        void start (std::vector<juce::File> files,
                    std::vector<Target> targets,
                    juce::File dialogueStem);

        void cancel();

        bool isRunning() const noexcept { return running; }

        int getNumEntries() const noexcept { return static_cast<int> (entries.size()); }
        const BatchEntry& getEntry (int index) const { return entries[static_cast<std::size_t> (index)]; }

        /** Proportion of the run finished, 0 to 1. */
        double getProgress() const noexcept;

        int getCompletedCount() const noexcept { return completedCount; }
        int getFailedCount() const noexcept { return failedCount; }

        /** Re-judges every completed entry against a new target selection, without
            re-reading a single file - the measurements do not change, only the specs
            they are compared with.
        */
        void reevaluate (const std::vector<Target>& targets);

        std::function<void (int index)> onEntryChanged;
        std::function<void()> onFinished;

    private:
        void handleOutcome (int index, FileAnalysisOutcome outcome);

        std::vector<BatchEntry> entries;
        std::vector<Target> currentTargets;

        std::unique_ptr<juce::ThreadPool> pool;
        std::atomic<bool> cancelRequested { false };
        bool running { false };
        int completedCount { 0 };
        int failedCount { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BatchAnalyser)
    };
}
