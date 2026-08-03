#include "BatchAnalyser.h"

#include <algorithm>

namespace qc
{
    namespace
    {
        /** Analysis spends most of its time streaming samples through filters, so it
            saturates memory bandwidth long before it saturates the CPU. Four workers is
            where the returns flatten, and it leaves the machine usable meanwhile.
        */
        constexpr int kMaximumWorkers = 4;

        int chooseWorkerCount()
        {
            return juce::jlimit (1, kMaximumWorkers, juce::SystemStats::getNumCpus() - 1);
        }
    }

    BatchAnalyser::BatchAnalyser() = default;

    BatchAnalyser::~BatchAnalyser()
    {
        cancel();
    }

    void BatchAnalyser::cancel()
    {
        cancelRequested = true;

        if (pool != nullptr)
        {
            pool->removeAllJobs (true, 5000);
            pool.reset();
        }

        running = false;
    }

    void BatchAnalyser::start (std::vector<juce::File> files,
                               std::vector<Target> targets,
                               juce::File dialogueStem)
    {
        cancel();

        entries.clear();
        entries.reserve (files.size());

        for (auto& file : files)
        {
            BatchEntry entry;
            entry.file = file;
            entries.push_back (std::move (entry));
        }

        currentTargets = std::move (targets);
        completedCount = 0;
        failedCount = 0;
        cancelRequested = false;

        if (entries.empty())
        {
            if (onFinished)
                onFinished();

            return;
        }

        running = true;
        pool = std::make_unique<juce::ThreadPool> (juce::ThreadPool::Options {}
                                                       .withNumberOfThreads (chooseWorkerCount())
                                                       .withThreadName ("QC batch"));

        for (int index = 0; index < static_cast<int> (entries.size()); ++index)
        {
            const auto file = entries[static_cast<std::size_t> (index)].file;

            pool->addJob ([this, index, file, dialogueStem]
            {
                if (cancelRequested)
                    return;

                auto outcome = analyseFile (file, dialogueStem,
                                            [this] { return cancelRequested.load(); },
                                            nullptr);

                juce::MessageManager::callAsync ([this, index, outcome = std::move (outcome)]() mutable
                {
                    handleOutcome (index, std::move (outcome));
                });
            });
        }
    }

    void BatchAnalyser::handleOutcome (int index, FileAnalysisOutcome outcome)
    {
        // A run can be replaced while callbacks are still in flight from the previous
        // one; those must not write into the new set of entries.
        if (cancelRequested || index < 0 || index >= static_cast<int> (entries.size()))
            return;

        auto& entry = entries[static_cast<std::size_t> (index)];

        if (outcome.succeeded)
        {
            entry.state = BatchEntry::State::completed;
            entry.result = std::move (outcome.result);
            entry.verdicts = evaluate (entry.result, currentTargets);
            entry.overall = overallStatus (entry.verdicts);
        }
        else
        {
            entry.state = BatchEntry::State::failed;
            entry.errorMessage = outcome.errorMessage;
            entry.overall = Status::notMeasured;
            ++failedCount;
        }

        ++completedCount;

        if (onEntryChanged)
            onEntryChanged (index);

        if (completedCount >= static_cast<int> (entries.size()))
        {
            running = false;

            if (onFinished)
                onFinished();
        }
    }

    double BatchAnalyser::getProgress() const noexcept
    {
        if (entries.empty())
            return 1.0;

        return static_cast<double> (completedCount) / static_cast<double> (entries.size());
    }

    void BatchAnalyser::reevaluate (const std::vector<Target>& targets)
    {
        currentTargets = targets;

        for (int index = 0; index < static_cast<int> (entries.size()); ++index)
        {
            auto& entry = entries[static_cast<std::size_t> (index)];

            if (entry.state != BatchEntry::State::completed)
                continue;

            entry.verdicts = evaluate (entry.result, currentTargets);
            entry.overall = overallStatus (entry.verdicts);

            if (onEntryChanged)
                onEntryChanged (index);
        }
    }
}
