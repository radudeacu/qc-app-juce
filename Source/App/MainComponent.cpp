#include "MainComponent.h"

#include "../Report/JsonReport.h"
#include "../Verdict/VerdictEngine.h"

namespace qc
{
    namespace
    {
        constexpr int kTopBarHeight = 62;
        constexpr int kTargetColumnWidth = 236;
        constexpr int kSummaryHeight = 104;
        constexpr int kMinimumGraphHeight = 230;
        constexpr int kMinimumVerdictHeight = 132;
        constexpr int kGap = 14;
        constexpr int kEdge = 16;

        const juce::Colour kMutedText = glass::colour::secondary();

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

        addAndMakeVisible (exportJsonButton);
        exportJsonButton.onClick = [this] { exportJson(); };
        exportJsonButton.setEnabled (false);

        addAndMakeVisible (exportPdfButton);
        exportPdfButton.onClick = [this] { exportPdf(); };
        exportPdfButton.setEnabled (false);

        fileLabel.setText ("Drop audio or a folder here", juce::dontSendNotification);
        fileLabel.setColour (juce::Label::textColourId, kMutedText);
        fileLabel.setFont (glass::font (13.0f));
        addAndMakeVisible (fileLabel);

        stemLabel.setText ("No dialogue stem", juce::dontSendNotification);
        stemLabel.setColour (juce::Label::textColourId, glass::colour::faint (0.6f));
        stemLabel.setFont (glass::font (11.5f));
        addAndMakeVisible (stemLabel);

        statusLabel.setColour (juce::Label::textColourId, kMutedText);
        statusLabel.setFont (glass::font (12.0f));
        statusLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (statusLabel);

        targetViewport.setViewedComponent (&targetHolder, false);
        targetViewport.setScrollBarsShown (true, false);
        addAndMakeVisible (targetViewport);

        verdictViewport.setViewedComponent (&verdictPanel, false);
        verdictViewport.setScrollBarsShown (true, false);
        addAndMakeVisible (verdictViewport);

        addAndMakeVisible (graph);

        addAndMakeVisible (recurseToggle);
        recurseToggle.setColour (juce::ToggleButton::textColourId, glass::colour::secondary (0.82f));
        recurseToggle.setTooltip ("Off by default: dropping a project folder should not pull in "
                                  "every bounce and stem nested underneath it");

        addChildComponent (batchTable);
        batchTable.onSelectionChanged = [this] (int entryIndex) { showBatchEntry (entryIndex); };

        batchAnalyser.onEntryChanged = [this] (int index)
        {
            batchTable.refreshRow (index);
            updateBatchStatus();
            updateExportButtons();

            // Show the first file that finishes rather than leaving the summary, graph
            // and verdict panel blank while a long folder works through.
            const auto& entry = batchAnalyser.getEntry (index);

            if (batchTable.getSelectedEntryIndex() < 0
                && entry.state == BatchEntry::State::completed)
                batchTable.selectEntry (index);
        };

        batchAnalyser.onFinished = [this]
        {
            batchTable.refreshAll();
            updateBatchStatus();
        };

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

        if (batchMode)
        {
            // Judging against a different spec does not require re-reading anything: the
            // measurements are unchanged, only what they are compared with.
            batchAnalyser.reevaluate (getSelectedTargets());
            batchTable.setTargets (getSelectedTargets());
        }

        if (! hasResult)
        {
            verdictPanel.clear();
            return;
        }

        verdictPanel.setVerdicts (evaluate (currentResult, getSelectedTargets()));
        verdictPanel.setSize (verdictViewport.getWidth() - 8, verdictPanel.getRequiredHeight());
        repaint();
    }

    void MainComponent::updateExportButtons()
    {
        // JSON covers the file on screen; PDF covers everything in the current view, so
        // a batch can produce a report before every file has finished.
        exportJsonButton.setEnabled (hasResult);
        exportPdfButton.setEnabled (hasResult || (batchMode && batchAnalyser.getCompletedCount() > 0));
    }

    std::vector<PdfReportItem> MainComponent::gatherReportItems() const
    {
        std::vector<PdfReportItem> items;

        if (batchMode)
        {
            for (int i = 0; i < batchAnalyser.getNumEntries(); ++i)
            {
                const auto& entry = batchAnalyser.getEntry (i);

                if (entry.state == BatchEntry::State::pending
                    || entry.state == BatchEntry::State::running)
                    continue;

                PdfReportItem item;
                item.displayName = entry.file.getFullPathName().toStdString();

                if (entry.state == BatchEntry::State::failed)
                    item.errorMessage = entry.errorMessage.toStdString();
                else
                {
                    item.result = entry.result;
                    item.verdicts = entry.verdicts;
                }

                items.push_back (std::move (item));
            }

            return items;
        }

        if (hasResult)
        {
            PdfReportItem item;
            item.displayName = currentFile.getFullPathName().toStdString();
            item.result = currentResult;
            item.verdicts = evaluate (currentResult, getSelectedTargets());
            items.push_back (std::move (item));
        }

        return items;
    }

    void MainComponent::exportPdf()
    {
        auto items = gatherReportItems();

        if (items.empty())
            return;

        const auto suggested = batchMode
                             ? currentFile.getParentDirectory()
                                          .getChildFile (currentFile.getParentDirectory().getFileName()
                                                         + " QC report.pdf")
                             : currentFile.getParentDirectory()
                                          .getChildFile (currentFile.getFileNameWithoutExtension()
                                                         + " QC report.pdf");

        fileChooser = std::make_unique<juce::FileChooser> ("Save the QC report", suggested, "*.pdf");

        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this, items = std::move (items)] (const juce::FileChooser& chooser)
                                  {
                                      const auto destination = chooser.getResult();

                                      if (destination == juce::File())
                                          return;

                                      const auto pdf = writePdfReport (items);

                                      // PDF is binary: writing it as text would corrupt it
                                      // through line-ending translation.
                                      juce::TemporaryFile temporary (destination);

                                      {
                                          juce::FileOutputStream stream (temporary.getFile());

                                          if (! stream.openedOk()
                                              || ! stream.write (pdf.data(), pdf.size()))
                                          {
                                              statusLabel.setText ("Could not write "
                                                                       + destination.getFullPathName(),
                                                                   juce::dontSendNotification);
                                              return;
                                          }
                                      }

                                      if (temporary.overwriteTargetFileWithTemporary())
                                          statusLabel.setText ("Saved " + destination.getFileName()
                                                                   + " (" + juce::String (items.size())
                                                                   + " page(s))",
                                                               juce::dontSendNotification);
                                      else
                                          statusLabel.setText ("Could not replace "
                                                                   + destination.getFullPathName(),
                                                               juce::dontSendNotification);
                                  });
    }

    void MainComponent::exportJson()
    {
        if (! hasResult)
            return;

        const auto suggested = currentFile.getParentDirectory()
                                          .getChildFile (currentFile.getFileNameWithoutExtension() + ".json");

        fileChooser = std::make_unique<juce::FileChooser> ("Save the analysis as JSON", suggested, "*.json");

        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& chooser)
                                  {
                                      const auto destination = chooser.getResult();

                                      if (destination == juce::File())
                                          return;

                                      const auto json = writeJsonReport (currentResult,
                                                                         evaluate (currentResult, getSelectedTargets()));

                                      // A failed write is silent otherwise, and the user
                                      // would walk away believing the report exists.
                                      if (destination.replaceWithText (json))
                                          statusLabel.setText ("Saved " + destination.getFileName(),
                                                               juce::dontSendNotification);
                                      else
                                          statusLabel.setText ("Could not write " + destination.getFullPathName(),
                                                               juce::dontSendNotification);
                                  });
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
                                      if (batchMode && ! batchFiles.empty())
                                          startBatch (batchFiles);
                                      else if (currentFile != juce::File())
                                          startAnalysis (currentFile);
                                  });
    }

    void MainComponent::clearDialogueStem()
    {
        dialogueStem = juce::File();
        stemLabel.setText ("No dialogue stem", juce::dontSendNotification);
        clearStemButton.setEnabled (false);

        if (batchMode && ! batchFiles.empty())
            startBatch (batchFiles);
        else if (currentFile != juce::File())
            startAnalysis (currentFile);
    }

    void MainComponent::startAnalysis (const juce::File& file)
    {
        setBatchMode (false);

        if (analysisRunning)
        {
            cancelRequested = true;
            analysisPool.removeAllJobs (true, 4000);
        }

        currentFile = file;
        fileLabel.setText (file.getFileName(), juce::dontSendNotification);
        fileLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.9f));

        hasResult = false;
        updateExportButtons();
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
        updateExportButtons();

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

    void MainComponent::setBatchMode (bool shouldBeBatch)
    {
        if (batchMode == shouldBeBatch)
            return;

        batchMode = shouldBeBatch;
        batchTable.setVisible (batchMode);
        resized();
        repaint();
    }

    void MainComponent::startBatch (std::vector<juce::File> files)
    {
        cancelRequested = true;
        analysisPool.removeAllJobs (true, 4000);

        setBatchMode (true);

        hasResult = false;
        exportJsonButton.setEnabled (false);
        verdictPanel.clear();
        graph.clearResult();

        fileLabel.setText (juce::String (static_cast<int> (files.size())) + " files",
                           juce::dontSendNotification);
        fileLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.9f));

        batchFiles = files;
        batchAnalyser.start (std::move (files), getSelectedTargets(), dialogueStem);
        batchTable.setSource (&batchAnalyser, getSelectedTargets());
        updateBatchStatus();
    }

    void MainComponent::updateBatchStatus()
    {
        const auto total = batchAnalyser.getNumEntries();
        const auto done = batchAnalyser.getCompletedCount();
        const auto failed = batchAnalyser.getFailedCount();

        juce::String status;

        if (batchAnalyser.isRunning())
            status = "Analysing " + juce::String (done) + " of " + juce::String (total) + "...";
        else
            status = juce::String (total) + " files analysed";

        if (failed > 0)
            status += "  |  " + juce::String (failed) + " could not be read";

        statusLabel.setText (status, juce::dontSendNotification);
    }

    void MainComponent::showBatchEntry (int entryIndex)
    {
        if (entryIndex < 0 || entryIndex >= batchAnalyser.getNumEntries())
            return;

        const auto& entry = batchAnalyser.getEntry (entryIndex);

        if (entry.state != BatchEntry::State::completed)
        {
            hasResult = false;
            updateExportButtons();
            verdictPanel.clear();
            graph.clearResult();

            if (entry.state == BatchEntry::State::failed)
                statusLabel.setText (entry.file.getFileName() + ": " + entry.errorMessage,
                                     juce::dontSendNotification);

            repaint();
            return;
        }

        currentFile = entry.file;
        currentResult = entry.result;
        hasResult = true;
        updateExportButtons();

        graph.setTarget (getPrimaryTarget());
        graph.setResult (currentResult);

        verdictPanel.setVerdicts (entry.verdicts);
        verdictPanel.setSize (juce::jmax (10, verdictViewport.getWidth() - 8),
                              verdictPanel.getRequiredHeight());

        repaint();
    }

    bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
    {
        return ! files.isEmpty();
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
        openPaths (files);
    }

    void MainComponent::openPaths (const juce::StringArray& files)
    {
        auto audioFiles = collectAudioFiles (files, recurseToggle.getToggleState());

        if (audioFiles.empty())
        {
            // Say what was wrong rather than appearing to ignore the drop.
            statusLabel.setText (files.size() == 1
                                     ? "Nothing here this build can decode. Supported: "
                                           + getSupportedFormatWildcard()
                                     : "No decodable audio found in what you dropped.",
                                 juce::dontSendNotification);
            return;
        }

        if (audioFiles.size() == 1)
        {
            setBatchMode (false);
            startAnalysis (audioFiles.front());
            return;
        }

        startBatch (std::move (audioFiles));
    }

    void MainComponent::paintSummary (juce::Graphics& g, juce::Rectangle<int> area)
    {
        const auto panel = area.toFloat();
        glass::paintPanelShadow (g, panel);
        glass::paintPanel (g, panel, glass::Depth::raised);

        if (! hasResult)
        {
            g.setColour (glass::colour::secondary (0.7f));
            g.setFont (glass::font (13.5f));
            g.drawText (analysisRunning ? "Analysing..." : "Nothing analysed yet",
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

        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            const auto& field = fields[i];
            const juce::Rectangle<int> cell (x, area.getY(), columnWidth, area.getHeight());

            // Hairlines between figures rather than boxes around them: the panel already
            // groups them, so anything heavier is noise.
            if (i > 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.11f));
                g.fillRect (static_cast<float> (x), static_cast<float> (area.getY() + 18),
                            1.0f, static_cast<float> (area.getHeight() - 36));
            }

            g.setColour (glass::colour::secondary (0.72f));
            g.setFont (glass::font (11.0f, true));
            g.drawText (juce::String (field.name).toUpperCase(),
                        cell.reduced (16, 0).withTrimmedBottom (cell.getHeight() / 2),
                        juce::Justification::bottomLeft);

            g.setColour (glass::colour::text());
            g.setFont (glass::font (23.0f, true));
            g.drawText (field.value,
                        cell.reduced (16, 0).withTrimmedTop (cell.getHeight() / 2 - 6),
                        juce::Justification::topLeft);

            x += columnWidth;
        }

        if (currentResult.hasDialogueGatedLoudness)
        {
            g.setColour (glass::colour::secondary());
            g.setFont (glass::font (11.0f));
            g.drawText ("Dialogue-gated " + formatLoudness (currentResult.dialogueGatedLufs),
                        area.reduced (16, 8), juce::Justification::bottomRight);
        }
    }

    void MainComponent::paintDropHighlight (juce::Graphics& g)
    {
        if (! dragHighlight)
            return;

        const auto bounds = getLocalBounds().toFloat().reduced (6.0f);

        g.setColour (glass::colour::accent.withAlpha (0.10f));
        g.fillRoundedRectangle (bounds, glass::metrics::panelRadius + 6.0f);

        g.setColour (glass::colour::accent.withAlpha (0.75f));
        g.drawRoundedRectangle (bounds, glass::metrics::panelRadius + 6.0f, 1.6f);
    }

    void MainComponent::paintHeader (juce::Graphics& g)
    {
        auto bar = getLocalBounds().reduced (kEdge).removeFromTop (kTopBarHeight).toFloat();

        glass::paintPanelShadow (g, bar);
        glass::paintPanel (g, bar, glass::Depth::raised, 16.0f);

        // Wordmark. Two weights in one line reads as a mark rather than as a heading.
        const auto textArea = bar.withTrimmedLeft (20.0f);

        g.setColour (glass::colour::text (0.95f));
        g.setFont (glass::font (16.0f, true));
        g.drawText ("Loudness", textArea.withWidth (72.0f), juce::Justification::centredLeft);

        g.setColour (glass::colour::accent.withAlpha (0.95f));
        g.setFont (glass::font (16.0f));
        g.drawText ("QC", textArea.withTrimmedLeft (74.0f).withWidth (40.0f),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.fillRect (bar.getX() + 128.0f, bar.getY() + 16.0f, 1.0f, bar.getHeight() - 32.0f);
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        if (backdrop.isNull() || backdrop.getWidth() != getWidth() || backdrop.getHeight() != getHeight())
            backdrop = glass::renderBackdrop (getWidth(), getHeight());

        g.drawImageAt (backdrop, 0, 0);

        paintHeader (g);

        auto bounds = getLocalBounds().reduced (kEdge);
        bounds.removeFromTop (kTopBarHeight + kGap);

        auto right = bounds.withTrimmedLeft (kTargetColumnWidth + kGap);
        paintSummary (g, right.removeFromTop (kSummaryHeight));

        const auto targetPanel = bounds.removeFromLeft (kTargetColumnWidth).toFloat();
        glass::paintPanelShadow (g, targetPanel, glass::metrics::panelRadius, 0.7f);
        glass::paintPanel (g, targetPanel, glass::Depth::recessed);

        g.setColour (glass::colour::secondary (0.72f));
        g.setFont (glass::font (11.0f, true));
        g.drawText ("TARGETS",
                    targetPanel.toNearestInt().reduced (18, 0).withHeight (34),
                    juce::Justification::centredLeft);

        // The well sits behind whatever the verdict viewport actually occupies rather
        // than behind a separately computed rectangle, so the two cannot drift apart.
        if (! verdictViewport.getBounds().isEmpty())
            glass::paintPanel (g, verdictViewport.getBounds().expanded (2).toFloat(),
                               glass::Depth::recessed);

        paintDropHighlight (g);
    }

    void MainComponent::resized()
    {
        auto bounds = getLocalBounds().reduced (kEdge);

        auto topBar = bounds.removeFromTop (kTopBarHeight);
        topBar.removeFromLeft (146); // Wordmark and its divider.

        const auto buttonRow = [&topBar] (int width)
        {
            auto slot = topBar.removeFromLeft (width).reduced (0, 15);
            topBar.removeFromLeft (6);
            return slot;
        };

        openButton.setBounds (buttonRow (86));
        stemButton.setBounds (buttonRow (84));
        clearStemButton.setBounds (buttonRow (58));
        topBar.removeFromLeft (8);
        exportJsonButton.setBounds (buttonRow (74));
        exportPdfButton.setBounds (buttonRow (68));
        topBar.removeFromLeft (8);
        recurseToggle.setBounds (topBar.removeFromLeft (152).reduced (0, 15));
        topBar.removeFromLeft (kGap);

        auto labels = topBar.removeFromLeft (juce::jmax (150, topBar.getWidth() / 3));
        fileLabel.setBounds (labels.removeFromTop (labels.getHeight() / 2).reduced (0, 2));
        stemLabel.setBounds (labels.reduced (0, 2));

        statusLabel.setBounds (topBar.withTrimmedRight (18));

        bounds.removeFromTop (kGap);

        auto targetColumn = bounds.removeFromLeft (kTargetColumnWidth);
        targetColumn.removeFromTop (34);
        targetViewport.setBounds (targetColumn.reduced (12, 8));

        int y = 2;
        for (auto* toggle : targetToggles)
        {
            toggle->setBounds (6, y, targetHolder.getWidth() - 12, 28);
            y += 30;
        }
        targetHolder.setSize (juce::jmax (10, targetViewport.getWidth() - 10), y + 4);

        bounds.removeFromLeft (kGap);

        bounds.removeFromTop (kSummaryHeight + kGap);

        if (batchMode)
        {
            // The table is the primary view in a batch; the detail below answers "why
            // did that one fail".
            batchTable.setBounds (bounds.removeFromTop (bounds.getHeight() * 42 / 100));
            bounds.removeFromTop (kGap);
        }

        // The verdict list takes what its content needs and the graph takes the rest,
        // rather than the graph taking a fixed slice and leaving a dead band in the
        // middle whenever only one or two targets are selected.
        verdictPanel.setSize (juce::jmax (10, bounds.getWidth() - 12), verdictPanel.getRequiredHeight());

        const int wanted = verdictPanel.getRequiredHeight() + 10;
        const int available = bounds.getHeight() - kMinimumGraphHeight - kGap;
        const int verdictHeight = juce::jlimit (kMinimumVerdictHeight,
                                                juce::jmax (kMinimumVerdictHeight, available),
                                                wanted);

        verdictViewport.setBounds (bounds.removeFromTop (verdictHeight).reduced (2));
        bounds.removeFromTop (kGap);
        graph.setBounds (bounds);
        verdictPanel.setSize (juce::jmax (10, verdictViewport.getWidth() - 8),
                              verdictPanel.getRequiredHeight());
    }
}
