#pragma once

#include "BatchAnalyser.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace qc
{
    /** Sortable one-row-per-file view of a batch run.

        Columns for the selected targets are built on demand, so choosing a different set
        of specs re-columns the table rather than forcing the user to read a fixed set
        they did not ask for.
    */
    class BatchTable : public juce::Component,
                       private juce::TableListBoxModel
    {
    public:
        BatchTable();

        /** Points the table at a run and rebuilds its columns for these targets. */
        void setSource (const BatchAnalyser* analyser, std::vector<Target> targetsToShow);

        /** Re-columns for a new target selection while preserving the sort order and the
            selected row - changing which specs you care about should not lose your place.
        */
        void setTargets (std::vector<Target> targetsToShow);

        void refreshRow (int entryIndex);
        void refreshAll();
        void clear();

        /** Index into the analyser's entries, or -1 when nothing is selected. */
        int getSelectedEntryIndex() const;

        void resized() override;

        std::function<void (int entryIndex)> onSelectionChanged;

    private:
        int getNumRows() override;
        void paintRowBackground (juce::Graphics& g, int rowNumber, int width, int height,
                                 bool rowIsSelected) override;
        void paintCell (juce::Graphics& g, int rowNumber, int columnId, int width, int height,
                        bool rowIsSelected) override;
        void sortOrderChanged (int newSortColumnId, bool isForwards) override;
        void selectedRowsChanged (int lastRowSelected) override;

        const BatchEntry* getEntryForRow (int rowNumber) const;
        juce::String getCellText (const BatchEntry& entry, int columnId) const;
        bool isStatusColumn (int columnId) const;
        Status getStatusForColumn (const BatchEntry& entry, int columnId) const;

        void rebuildColumns();
        void resetRowOrder();

        juce::TableListBox table;
        const BatchAnalyser* source { nullptr };
        std::vector<Target> targets;

        /** Display row to entry index. Sorting permutes this rather than the entries,
            so a row that finishes while the table is sorted stays where the user sees it.
        */
        std::vector<int> rowOrder;

        int sortColumnId { 1 };
        bool sortForwards { true };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BatchTable)
    };
}
