#pragma once

#include "FileAnalysisJob.h"
#include "LoudnessGraph.h"
#include "TargetLibrary.h"
#include "VerdictPanel.h"

#include <atomic>
#include <juce_gui_extra/juce_gui_extra.h>

namespace qc
{
    /** The application window's single view: drop a file, pick targets, read the verdict.

        Analysis runs on a background thread and reports back through the message thread.
        No audio device is ever opened - the app measures files and nothing else - so
        there is no real-time path here to protect.
    */
    class MainComponent : public juce::Component,
                          public juce::FileDragAndDropTarget
    {
    public:
        MainComponent();
        ~MainComponent() override;

        void paint (juce::Graphics& g) override;
        void resized() override;

        /** Analyses a file chosen outside the UI - a command-line argument, or the
            shell's "open with".
        */
        void openFile (const juce::File& file);

        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragEnter (const juce::StringArray& files, int x, int y) override;
        void fileDragExit (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;

    private:
        void buildTargetToggles();
        void rebuildVerdicts();
        void startAnalysis (const juce::File& file);
        void finishAnalysis (FileAnalysisOutcome outcome);
        void chooseProgrammeFile();
        void chooseDialogueStem();
        void clearDialogueStem();

        std::vector<Target> getSelectedTargets() const;
        const Target* getPrimaryTarget() const;

        void saveSelection();
        void restoreSelection();

        void paintSummary (juce::Graphics& g, juce::Rectangle<int> area);
        void paintDropHighlight (juce::Graphics& g);

        juce::ApplicationProperties applicationProperties;

        TargetLoadResult targetLibrary;

        juce::TextButton openButton { "Open file..." };
        juce::TextButton stemButton { "Dialogue stem..." };
        juce::TextButton clearStemButton { "Clear" };
        juce::Label fileLabel;
        juce::Label stemLabel;
        juce::Label statusLabel;

        juce::Component targetHolder;
        juce::Viewport targetViewport;
        juce::OwnedArray<juce::ToggleButton> targetToggles;

        VerdictPanel verdictPanel;
        juce::Viewport verdictViewport;
        LoudnessGraph graph;

        std::unique_ptr<juce::FileChooser> fileChooser;

        juce::File currentFile;
        juce::File dialogueStem;

        AnalysisResult currentResult;
        bool hasResult { false };

        juce::ThreadPool analysisPool { 1 };
        std::atomic<bool> cancelRequested { false };
        std::atomic<bool> analysisRunning { false };
        std::atomic<double> analysisProgress { 0.0 };

        bool dragHighlight { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
}
