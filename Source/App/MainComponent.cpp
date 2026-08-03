#include "MainComponent.h"

#include "../Verdict/VerdictEngine.h"

namespace qc
{
    namespace
    {
        constexpr int kTopBarHeight = 46;
        constexpr int kTargetColumnWidth = 210;
        constexpr int kSummaryHeight = 84;
        constexpr int kGraphHeight = 230;
        constexpr int kGap = 10;

        const juce::Colour kBackground { 0xff0f1319 };
        const juce::Colour kPanel { 0xff171c24 };
        const juce::Colour kMutedText { 0xff8892a4 };

        juce::String formatLoudness (double lufs, const char* suffix = " LUFS")
        {
            if (! isMeasured (lufs))
                return "--";

            return juce::String (lufs, 1) + suffix;
        }

        juce::String formatDecibels (double db)
        {
            if (! std::isfinite (db))
                return "--";

            return juce::String (db, 1) + " dB";
        }

        juce::String formatDuration (double seconds)
        {
            const auto total = static_cast<int> (seconds);
            return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
        }
    }

    MainComponent::MainComponent()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "QC App";
        options.filenameSuffix = ".settings";
        options.folderName = "QC App";
        options.osxLibrarySubFolder = "Application Support";
        applicationProperties.setStorageParameters (options);

        targetLibrary = TargetLibrary::load();

        addAndMakeVisible (openButton);
        openButton.onClick = [this] { chooseProgrammeFile(); };

        addAndMakeVisible (stemButton);
        stemButton.onClick = [this] { chooseDialogueStem(); };

        addAndMakeVisible (clearStemButton);
        clearStemButton.onClick = [this] { clearDialogueStem(); };
        clearStemButton.setEnabled (false);

        fileLabel.setText ("Drop an audio file anywhere in this window", juce::dontSendNotification);
        fileLabel.setColour (juce::Label::textColourId, kMutedText);
        addAndMakeVisible (fileLabel);

        stemLabel.setText ("No dialogue stem", juce::dontSendNotification);
        stemLabel.setColour (juce::Label::textColourId, kMutedText);
        stemLabel.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (stemLabel);

        statusLabel.setColour (juce::Label::textColourId, kMutedText);
        statusLabel.setFont (juce::FontOptions (12.0f));
        statusLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (statusLabel);

        targetViewport.setViewedComponent (&targetHolder, false);
        targetViewport.setScrollBarsShown (true, false);
        addAndMakeVisible (targetViewport);

        verdictViewport.setViewedComponent (&verdictPanel, false);
        verdictViewport.setScrollBarsShown (true, false);
        addAndMakeVisible (verdictViewport);

        addAndMakeVisible (graph);

        buildTargetToggles();
        restoreSelection();

        if (! targetLibrary.problems.isEmpty())
            statusLabel.setText (targetLibrary.problems[0], juce::dontSendNotification);

        setSize (1120, 760);
    }

    MainComponent::~MainComponent()
    {
        cancelRequested = true;
        analysisPool.removeAllJobs (true, 4000);
    }

    void MainComponent::buildTargetToggles()
    {
        targetToggles.clear();

        for (const auto& target : targetLibrary.targets)
        {
            auto* toggle = new juce::ToggleButton (target.name);
            toggle->setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.85f));

            // A target whose numbers were never checked against the published spec says so
            // on the control itself, not buried in a report footnote.
            if (target.lastVerified.empty())
                toggle->setTooltip ("Numbers not yet verified against the published specification");
            else
                toggle->setTooltip ("Last verified " + juce::String (target.lastVerified));

            toggle->onClick = [this]
            {
                saveSelection();
                rebuildVerdicts();
            };

            targetHolder.addAndMakeVisible (toggle);
            targetToggles.add (toggle);
        }

        targetHolder.setSize (kTargetColumnWidth - 20, static_cast<int> (targetToggles.size()) * 26 + 8);
    }

    std::vector<Target> MainComponent::getSelectedTargets() const
    {
        std::vector<Target> selected;

        for (int i = 0; i < targetToggles.size(); ++i)
            if (targetToggles[i]->getToggleState()
                && i < static_cast<int> (targetLibrary.targets.size()))
                selected.push_back (targetLibrary.targets[static_cast<std::size_t> (i)]);

        return selected;
    }

    const Target* MainComponent::getPrimaryTarget() const
    {
        // The graph can only shade one tolerance band, so it follows the first selection.
        for (int i = 0; i < targetToggles.size(); ++i)
            if (targetToggles[i]->getToggleState()
                && i < static_cast<int> (targetLibrary.targets.size()))
                return &targetLibrary.targets[static_cast<std::size_t> (i)];

        return nullptr;
    }

    void MainComponent::saveSelection()
    {
        juce::StringArray ids;

        for (int i = 0; i < targetToggles.size(); ++i)
            if (targetToggles[i]->getToggleState()
                && i < static_cast<int> (targetLibrary.targets.size()))
                ids.add (targetLibrary.targets[static_cast<std::size_t> (i)].id);

        if (auto* settings = applicationProperties.getUserSettings())
            settings->setValue ("selectedTargets", ids.joinIntoString (","));
    }

    void MainComponent::restoreSelection()
    {
        auto* settings = applicationProperties.getUserSettings();

        if (settings == nullptr)
            return;

        const auto stored = settings->getValue ("selectedTargets");

        if (stored.isEmpty())
        {
            // First run: something has to be selected or the app looks broken.
            if (! targetToggles.isEmpty())
                targetToggles[0]->setToggleState (true, juce::dontSendNotification);

            return;
        }

        juce::StringArray ids;
        ids.addTokens (stored, ",", "");

        for (int i = 0; i < targetToggles.size(); ++i)
            if (i < static_cast<int> (targetLibrary.targets.size()))
                targetToggles[i]->setToggleState (ids.contains (targetLibrary.targets[static_cast<std::size_t> (i)].id),
                                                  juce::dontSendNotification);
    }

    void MainComponent::rebuildVerdicts()
    {
        const auto* primary = getPrimaryTarget();
        graph.setTarget (primary);

        if (! hasResult)
        {
            verdictPanel.clear();
            return;
        }

        verdictPanel.setVerdicts (evaluate (currentResult, getSelectedTargets()));
        verdictPanel.setSize (verdictViewport.getWidth() - 8, verdictPanel.getRequiredHeight());
        repaint();
    }

    void MainComponent::openFile (const juce::File& file)
    {
        startAnalysis (file);
    }

    void MainComponent::chooseProgrammeFile()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Choose an audio file to analyse",
                                                           juce::File(),
                                                           getSupportedFormatWildcard());

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();

                                      if (file != juce::File())
                                          startAnalysis (file);
                                  });
    }

    void MainComponent::chooseDialogueStem()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Choose the dialogue stem",
                                                           juce::File(),
                                                           getSupportedFormatWildcard());

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();

                                      if (file == juce::File())
                                          return;

                                      dialogueStem = file;
                                      stemLabel.setText ("Stem: " + file.getFileName(), juce::dontSendNotification);
                                      clearStemButton.setEnabled (true);

                                      // The stem changes what Netflix-style targets measure,
                                      // so anything already on screen is now out of date.
                                      if (currentFile != juce::File())
                                          startAnalysis (currentFile);
                                  });
    }

    void MainComponent::clearDialogueStem()
    {
        dialogueStem = juce::File();
        stemLabel.setText ("No dialogue stem", juce::dontSendNotification);
        clearStemButton.setEnabled (false);

        if (currentFile != juce::File())
            startAnalysis (currentFile);
    }

    void MainComponent::startAnalysis (const juce::File& file)
    {
        if (analysisRunning)
        {
            cancelRequested = true;
            analysisPool.removeAllJobs (true, 4000);
        }

        currentFile = file;
        fileLabel.setText (file.getFileName(), juce::dontSendNotification);
        fileLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.9f));

        hasResult = false;
        verdictPanel.clear();
        graph.clearResult();

        cancelRequested = false;
        analysisRunning = true;
        analysisProgress = 0.0;
        statusLabel.setText ("Analysing...", juce::dontSendNotification);

        const auto stem = dialogueStem;
        juce::Component::SafePointer<MainComponent> safeThis (this);

        analysisPool.addJob ([this, safeThis, file, stem]
        {
            auto outcome = analyseFile (file,
                                        stem,
                                        [this] { return cancelRequested.load(); },
                                        [this] (double progress) { analysisProgress = progress; });

            juce::MessageManager::callAsync ([safeThis, outcome = std::move (outcome)]() mutable
            {
                if (safeThis != nullptr)
                    safeThis->finishAnalysis (std::move (outcome));
            });
        });
    }

    void MainComponent::finishAnalysis (FileAnalysisOutcome outcome)
    {
        analysisRunning = false;

        if (! outcome.succeeded)
        {
            hasResult = false;
            statusLabel.setText (outcome.errorMessage, juce::dontSendNotification);
            verdictPanel.clear();
            graph.clearResult();
            repaint();
            return;
        }

        currentResult = std::move (outcome.result);
        hasResult = true;

        juce::String status = juce::String (currentResult.source.formatName) + " - "
                            + juce::String (currentResult.source.sampleRate / 1000.0, 1) + " kHz - "
                            + juce::String (currentResult.source.bitDepth) + "-bit - "
                            + (currentResult.source.numChannels == 1 ? "mono" : "stereo")
                            + " - true peak oversampled " + juce::String (currentResult.oversamplingFactor) + "x";

        if (! targetLibrary.unverified.isEmpty())
            status += "  |  " + juce::String (targetLibrary.unverified.size())
                    + " target(s) not yet verified against their published spec";

        statusLabel.setText (status, juce::dontSendNotification);

        graph.setTarget (getPrimaryTarget());
        graph.setResult (currentResult);

        rebuildVerdicts();
        repaint();
    }

    bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
    {
        return files.size() == 1;
    }

    void MainComponent::fileDragEnter (const juce::StringArray&, int, int)
    {
        dragHighlight = true;
        repaint();
    }

    void MainComponent::fileDragExit (const juce::StringArray&)
    {
        dragHighlight = false;
        repaint();
    }

    void MainComponent::filesDropped (const juce::StringArray& files, int, int)
    {
        dragHighlight = false;
        repaint();

        if (! files.isEmpty())
            startAnalysis (juce::File (files[0]));
    }

    void MainComponent::paintSummary (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (area.toFloat(), 6.0f);

        if (! hasResult)
        {
            g.setColour (kMutedText);
            g.setFont (juce::FontOptions (13.0f));
            g.drawText (analysisRunning ? "Analysing..." : "No file analysed yet",
                        area, juce::Justification::centred);
            return;
        }

        struct Field { const char* name; juce::String value; };

        const std::vector<Field> fields {
            { "Integrated",   formatLoudness (currentResult.loudness.integratedLufs) },
            { "Short-term max", formatLoudness (currentResult.loudness.maxShortTermLufs) },
            { "Momentary max",  formatLoudness (currentResult.loudness.maxMomentaryLufs) },
            { "LRA",          juce::String (currentResult.loudness.loudnessRangeLu, 1) + " LU" },
            { "True peak",    isMeasured (currentResult.truePeakDb)
                                  ? juce::String (currentResult.truePeakDb, 1) + " dBTP" : "--" },
            { "Sample peak",  currentResult.quality.samplePeakDb.empty()
                                  ? "--"
                                  : formatDecibels (*std::max_element (currentResult.quality.samplePeakDb.begin(),
                                                                       currentResult.quality.samplePeakDb.end())) },
            { "PLR",          juce::String (currentResult.peakToLoudnessRatioDb, 1) + " LU" },
            { "Duration",     formatDuration (currentResult.source.durationSeconds) }
        };

        const int columnWidth = area.getWidth() / static_cast<int> (fields.size());
        int x = area.getX();

        for (const auto& field : fields)
        {
            const juce::Rectangle<int> cell (x, area.getY(), columnWidth, area.getHeight());

            g.setColour (kMutedText);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (field.name, cell.reduced (10, 0).withTrimmedBottom (cell.getHeight() / 2),
                        juce::Justification::bottomLeft);

            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
            g.drawText (field.value, cell.reduced (10, 0).withTrimmedTop (cell.getHeight() / 2 - 4),
                        juce::Justification::topLeft);

            x += columnWidth;
        }

        if (currentResult.hasDialogueGatedLoudness)
        {
            g.setColour (kMutedText);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("Dialogue-gated " + formatLoudness (currentResult.dialogueGatedLufs),
                        area.reduced (10, 4), juce::Justification::bottomRight);
        }
    }

    void MainComponent::paintDropHighlight (juce::Graphics& g)
    {
        if (! dragHighlight)
            return;

        g.setColour (juce::Colour (0xff4fa3ff).withAlpha (0.7f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 8.0f, 2.0f);
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        g.fillAll (kBackground);

        auto bounds = getLocalBounds().reduced (kGap);
        bounds.removeFromTop (kTopBarHeight + kGap);

        auto right = bounds.withTrimmedLeft (kTargetColumnWidth + kGap);
        paintSummary (g, right.removeFromTop (kSummaryHeight));

        g.setColour (kPanel);
        g.fillRoundedRectangle (bounds.removeFromLeft (kTargetColumnWidth).toFloat(), 6.0f);

        g.setColour (kMutedText);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("TARGETS",
                    juce::Rectangle<int> (kGap + 12, kTopBarHeight + kGap * 2, 160, 18),
                    juce::Justification::centredLeft);

        paintDropHighlight (g);
    }

    void MainComponent::resized()
    {
        auto bounds = getLocalBounds().reduced (kGap);

        auto topBar = bounds.removeFromTop (kTopBarHeight);
        openButton.setBounds (topBar.removeFromLeft (110).reduced (0, 8));
        topBar.removeFromLeft (kGap);
        stemButton.setBounds (topBar.removeFromLeft (120).reduced (0, 8));
        topBar.removeFromLeft (4);
        clearStemButton.setBounds (topBar.removeFromLeft (60).reduced (0, 8));
        topBar.removeFromLeft (kGap);

        auto labels = topBar.removeFromLeft (juce::jmax (200, topBar.getWidth() / 2));
        fileLabel.setBounds (labels.removeFromTop (labels.getHeight() / 2));
        stemLabel.setBounds (labels);

        statusLabel.setBounds (topBar);

        bounds.removeFromTop (kGap);

        auto targetColumn = bounds.removeFromLeft (kTargetColumnWidth);
        targetColumn.removeFromTop (28);
        targetViewport.setBounds (targetColumn.reduced (8, 4));

        int y = 4;
        for (auto* toggle : targetToggles)
        {
            toggle->setBounds (4, y, targetHolder.getWidth() - 8, 24);
            y += 26;
        }
        targetHolder.setSize (juce::jmax (10, targetViewport.getWidth() - 10), y + 4);

        bounds.removeFromLeft (kGap);

        bounds.removeFromTop (kSummaryHeight + kGap);
        graph.setBounds (bounds.removeFromBottom (kGraphHeight));
        bounds.removeFromBottom (kGap);

        verdictViewport.setBounds (bounds);
        verdictPanel.setSize (juce::jmax (10, verdictViewport.getWidth() - 8),
                              verdictPanel.getRequiredHeight());
    }
}
