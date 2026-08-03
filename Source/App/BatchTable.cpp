#include "BatchTable.h"

#include "GlassStyle.h"
#include "VerdictPanel.h"

#include <algorithm>

namespace qc
{
    namespace
    {
        enum ColumnId
        {
            fileColumn = 1,
            statusColumn,
            integratedColumn,
            truePeakColumn,
            loudnessRangeColumn,
            durationColumn,

            /** Target columns start here; the target's position is added to this. */
            firstTargetColumn = 100
        };

        juce::String formatOrDash (double value, int decimals, const char* suffix)
        {
            if (! std::isfinite (value))
                return "--";

            return juce::String (value, decimals) + suffix;
        }

        juce::String formatDuration (double seconds)
        {
            if (! std::isfinite (seconds) || seconds <= 0.0)
                return "--";

            const auto total = static_cast<int> (seconds);
            return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
        }
    }

    BatchTable::BatchTable()
    {
        table.setModel (this);
        table.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        table.setRowHeight (28);
        table.getHeader().setStretchToFitActive (true);
        addAndMakeVisible (table);

        rebuildColumns();
    }

    void BatchTable::rebuildColumns()
    {
        auto& header = table.getHeader();
        header.removeAllColumns();

        header.addColumn ("File", fileColumn, 240, 120, -1, juce::TableHeaderComponent::defaultFlags);
        header.addColumn ("Result", statusColumn, 110, 80, -1, juce::TableHeaderComponent::defaultFlags);
        header.addColumn ("Integrated", integratedColumn, 100, 70, -1, juce::TableHeaderComponent::defaultFlags);
        header.addColumn ("True peak", truePeakColumn, 95, 70, -1, juce::TableHeaderComponent::defaultFlags);
        header.addColumn ("LRA", loudnessRangeColumn, 70, 50, -1, juce::TableHeaderComponent::defaultFlags);
        header.addColumn ("Length", durationColumn, 70, 50, -1, juce::TableHeaderComponent::defaultFlags);

        for (std::size_t i = 0; i < targets.size(); ++i)
            header.addColumn (targets[i].name,
                              firstTargetColumn + static_cast<int> (i),
                              110, 70, -1,
                              juce::TableHeaderComponent::defaultFlags);

        header.setSortColumnId (sortColumnId, sortForwards);
    }

    void BatchTable::setSource (const BatchAnalyser* analyser, std::vector<Target> targetsToShow)
    {
        source = analyser;
        targets = std::move (targetsToShow);

        rebuildColumns();
        resetRowOrder();
        table.updateContent();
        repaint();
    }

    void BatchTable::setTargets (std::vector<Target> targetsToShow)
    {
        const auto selected = table.getSelectedRow();

        targets = std::move (targetsToShow);
        rebuildColumns();

        if (selected >= 0)
            table.selectRow (selected, true, true);

        table.repaint();
    }

    void BatchTable::clear()
    {
        source = nullptr;
        rowOrder.clear();
        table.updateContent();
        repaint();
    }

    void BatchTable::resetRowOrder()
    {
        rowOrder.clear();

        if (source == nullptr)
            return;

        rowOrder.reserve (static_cast<std::size_t> (source->getNumEntries()));

        for (int i = 0; i < source->getNumEntries(); ++i)
            rowOrder.push_back (i);

        sortOrderChanged (sortColumnId, sortForwards);
    }

    void BatchTable::refreshRow (int)
    {
        // Repainting the whole table is cheap at these row counts, and it keeps a sorted
        // view consistent when a row's sort key changes on completion.
        table.repaint();
    }

    void BatchTable::refreshAll()
    {
        if (source != nullptr && static_cast<int> (rowOrder.size()) != source->getNumEntries())
            resetRowOrder();

        table.updateContent();
        table.repaint();
    }

    int BatchTable::getNumRows()
    {
        return static_cast<int> (rowOrder.size());
    }

    const BatchEntry* BatchTable::getEntryForRow (int rowNumber) const
    {
        if (source == nullptr || rowNumber < 0 || rowNumber >= static_cast<int> (rowOrder.size()))
            return nullptr;

        const auto index = rowOrder[static_cast<std::size_t> (rowNumber)];

        if (index < 0 || index >= source->getNumEntries())
            return nullptr;

        return &source->getEntry (index);
    }

    int BatchTable::getSelectedEntryIndex() const
    {
        const auto row = table.getSelectedRow();

        if (row < 0 || row >= static_cast<int> (rowOrder.size()))
            return -1;

        return rowOrder[static_cast<std::size_t> (row)];
    }

    void BatchTable::selectEntry (int entryIndex)
    {
        for (std::size_t row = 0; row < rowOrder.size(); ++row)
        {
            if (rowOrder[row] == entryIndex)
            {
                table.selectRow (static_cast<int> (row));
                return;
            }
        }
    }

    bool BatchTable::isStatusColumn (int columnId) const
    {
        return columnId == statusColumn || columnId >= firstTargetColumn;
    }

    Status BatchTable::getStatusForColumn (const BatchEntry& entry, int columnId) const
    {
        if (columnId == statusColumn)
            return entry.overall;

        const auto targetIndex = static_cast<std::size_t> (columnId - firstTargetColumn);

        if (targetIndex >= targets.size())
            return Status::notMeasured;

        for (const auto& verdict : entry.verdicts)
            if (verdict.targetId == targets[targetIndex].id)
                return verdict.status;

        return Status::notMeasured;
    }

    juce::String BatchTable::getCellText (const BatchEntry& entry, int columnId) const
    {
        if (columnId == fileColumn)
            return entry.file.getFileName();

        if (entry.state == BatchEntry::State::pending)
            return columnId == statusColumn ? "waiting" : "";

        if (entry.state == BatchEntry::State::running)
            return columnId == statusColumn ? "analysing" : "";

        if (entry.state == BatchEntry::State::failed)
            return columnId == statusColumn ? "ERROR" : "";

        switch (columnId)
        {
            case integratedColumn:
                return isMeasured (entry.result.loudness.integratedLufs)
                     ? juce::String (entry.result.loudness.integratedLufs, 1) + " LUFS"
                     : "--";

            case truePeakColumn:
                return formatOrDash (entry.result.truePeakDb, 1, " dBTP");

            case loudnessRangeColumn:
                return formatOrDash (entry.result.loudness.loudnessRangeLu, 1, " LU");

            case durationColumn:
                return formatDuration (entry.result.source.durationSeconds);

            default:
                break;
        }

        if (isStatusColumn (columnId))
            return toString (getStatusForColumn (entry, columnId));

        return {};
    }

    void BatchTable::paintRowBackground (juce::Graphics& g, int rowNumber, int width, int height,
                                         bool rowIsSelected)
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width),
                                                    static_cast<float> (height));

        if (rowIsSelected)
        {
            // The selected row is the one piece of foreground glass in the table.
            glass::paintPanel (g, bounds.reduced (3.0f, 1.5f), glass::Depth::floating, 8.0f);
            g.setColour (glass::colour::accent.withAlpha (0.16f));
            g.fillRoundedRectangle (bounds.reduced (3.0f, 1.5f), 8.0f);
        }
        else if (rowNumber % 2 != 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            g.fillRoundedRectangle (bounds.reduced (3.0f, 1.5f), 8.0f);
        }
    }

    void BatchTable::paintCell (juce::Graphics& g, int rowNumber, int columnId, int width, int height,
                                bool rowIsSelected)
    {
        juce::ignoreUnused (rowIsSelected);

        const auto* entry = getEntryForRow (rowNumber);

        if (entry == nullptr)
            return;

        const auto text = getCellText (*entry, columnId);
        auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);

        if (entry->state == BatchEntry::State::failed && columnId == fileColumn)
        {
            // The reason belongs on the row, not in a tooltip: a folder of ninety files
            // with three failures should say why on screen.
            g.setColour (glass::colour::text (0.88f));
            g.setFont (glass::font (12.5f));
            g.drawText (entry->file.getFileName(), area.removeFromLeft (area.getWidth() / 2),
                        juce::Justification::centredLeft, true);

            g.setColour (VerdictPanel::getStatusColour (Status::fail).withAlpha (0.85f));
            g.setFont (glass::font (11.0f));
            g.drawText (entry->errorMessage, area, juce::Justification::centredLeft, true);
            return;
        }

        if (isStatusColumn (columnId) && entry->state == BatchEntry::State::completed)
        {
            const auto status = getStatusForColumn (*entry, columnId);
            g.setColour (VerdictPanel::getStatusColour (status));
            g.setFont (glass::font (11.5f, true));
            g.drawText (text, area, juce::Justification::centredLeft, true);
            return;
        }

        if (columnId == statusColumn && entry->state == BatchEntry::State::failed)
        {
            g.setColour (VerdictPanel::getStatusColour (Status::fail));
            g.setFont (glass::font (11.5f, true));
            g.drawText (text, area, juce::Justification::centredLeft, true);
            return;
        }

        g.setColour (glass::colour::text (columnId == fileColumn ? 0.9f : 0.7f));
        g.setFont (glass::font (12.5f));
        g.drawText (text, area, juce::Justification::centredLeft, true);
    }

    void BatchTable::sortOrderChanged (int newSortColumnId, bool isForwards)
    {
        sortColumnId = newSortColumnId;
        sortForwards = isForwards;

        if (source == nullptr)
            return;

        const auto* analyser = source;
        const auto columnId = sortColumnId;
        const auto* targetList = &targets;

        const auto keyFor = [analyser, columnId, targetList] (int index) -> double
        {
            const auto& entry = analyser->getEntry (index);

            if (entry.state != BatchEntry::State::completed)
                return std::numeric_limits<double>::lowest();

            switch (columnId)
            {
                case integratedColumn:      return entry.result.loudness.integratedLufs;
                case truePeakColumn:        return entry.result.truePeakDb;
                case loudnessRangeColumn:   return entry.result.loudness.loudnessRangeLu;
                case durationColumn:        return entry.result.source.durationSeconds;
                default:                    break;
            }

            if (columnId == statusColumn)
                return static_cast<double> (entry.overall);

            const auto targetIndex = static_cast<std::size_t> (columnId - firstTargetColumn);

            if (targetIndex < targetList->size())
                for (const auto& verdict : entry.verdicts)
                    if (verdict.targetId == (*targetList)[targetIndex].id)
                        return static_cast<double> (verdict.status);

            return std::numeric_limits<double>::lowest();
        };

        std::stable_sort (rowOrder.begin(), rowOrder.end(),
                          [this, analyser, keyFor] (int a, int b)
                          {
                              if (sortColumnId == fileColumn)
                              {
                                  const auto comparison = analyser->getEntry (a).file.getFileName()
                                                              .compareNatural (analyser->getEntry (b).file.getFileName());
                                  return sortForwards ? comparison < 0 : comparison > 0;
                              }

                              const auto keyA = keyFor (a);
                              const auto keyB = keyFor (b);

                              if (keyA == keyB)
                                  return a < b;

                              return sortForwards ? keyA < keyB : keyA > keyB;
                          });

        table.updateContent();
        table.repaint();
    }

    void BatchTable::selectedRowsChanged (int)
    {
        if (onSelectionChanged)
            onSelectionChanged (getSelectedEntryIndex());
    }

    void BatchTable::paint (juce::Graphics& g)
    {
        glass::paintPanel (g, getLocalBounds().toFloat(), glass::Depth::raised, 14.0f);
    }

    void BatchTable::resized()
    {
        table.setBounds (getLocalBounds().reduced (6, 8));
    }
}
