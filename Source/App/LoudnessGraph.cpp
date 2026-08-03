#include "LoudnessGraph.h"

#include "GlassStyle.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace qc
{
    namespace
    {
        constexpr float kAxisWidth = 46.0f;
        constexpr float kBottomAxisHeight = 20.0f;
        constexpr double kHeadroomLu = 4.0;
        constexpr double kMinimumSpanLu = 12.0;
        constexpr float kHeaderHeight = 20.0f;

        // The traces are the point of this panel, so they are the brightest thing on
        // screen. Short-term carries the reading; momentary sits behind it as context.
        const juce::Colour kShortTermColour { 0xffd6e2ff };
        const juce::Colour kMomentaryColour { 0x8a5eead4 };
        const juce::Colour kBandColour { 0x4d6ee79b };
        const juce::Colour kOverColour = glass::colour::fail;
        const juce::Colour kGridColour { 0x26ffffff };
        const juce::Colour kTextColour = glass::colour::secondary (0.82f);

        constexpr float kShortTermThickness = 2.2f;
        constexpr float kMomentaryThickness = 1.3f;
        constexpr float kAxisFontHeight = 11.5f;
    }

    LoudnessGraph::LoudnessGraph()
    {
        setOpaque (false);
    }

    void LoudnessGraph::setResult (const AnalysisResult& result)
    {
        analysis = result;
        hasResult = true;
        durationSeconds = result.source.durationSeconds;

        // Scale to the material rather than to a fixed window, so a -40 LUFS podcast and
        // a -9 LUFS master are both legible and neither wastes half the plot.
        double lowest = std::numeric_limits<double>::max();
        double highest = std::numeric_limits<double>::lowest();

        for (double value : result.loudness.shortTermLufs)
        {
            if (! isMeasured (value))
                continue;

            lowest = std::min (lowest, value);
            highest = std::max (highest, value);
        }

        if (lowest > highest)
        {
            lowest = -40.0;
            highest = -10.0;
        }

        if (hasTarget)
        {
            lowest = std::min (lowest, target.integratedLufs - kHeadroomLu);
            highest = std::max (highest, target.integratedLufs + kHeadroomLu);
        }

        minimumLufs = lowest - 2.0;
        maximumLufs = highest + 2.0;

        if (maximumLufs - minimumLufs < kMinimumSpanLu)
        {
            const double centre = (maximumLufs + minimumLufs) * 0.5;
            minimumLufs = centre - kMinimumSpanLu * 0.5;
            maximumLufs = centre + kMinimumSpanLu * 0.5;
        }

        repaint();
    }

    void LoudnessGraph::setTarget (const Target* targetToUse)
    {
        hasTarget = targetToUse != nullptr;

        if (hasTarget)
            target = *targetToUse;

        if (hasResult)
            setResult (analysis);
        else
            repaint();
    }

    void LoudnessGraph::clearResult()
    {
        hasResult = false;
        analysis = {};
        repaint();
    }

    juce::Rectangle<float> LoudnessGraph::getPlotArea() const
    {
        return getLocalBounds().toFloat()
                              .reduced (8.0f)
                              .withTrimmedLeft (kAxisWidth)
                              .withTrimmedTop (kHeaderHeight)
                              .withTrimmedBottom (kBottomAxisHeight);
    }

    float LoudnessGraph::loudnessToY (double lufs, juce::Rectangle<float> plot) const
    {
        const double span = maximumLufs - minimumLufs;

        if (span <= 0.0)
            return plot.getCentreY();

        const double proportion = (lufs - minimumLufs) / span;
        return plot.getBottom() - static_cast<float> (proportion) * plot.getHeight();
    }

    float LoudnessGraph::timeToX (double seconds, juce::Rectangle<float> plot) const
    {
        if (durationSeconds <= 0.0)
            return plot.getX();

        return plot.getX() + static_cast<float> (seconds / durationSeconds) * plot.getWidth();
    }

    void LoudnessGraph::paintEmptyState (juce::Graphics& g)
    {
        g.setColour (kTextColour.withAlpha (0.7f));
        g.setFont (glass::font (13.5f));
        g.drawText ("Loudness over time appears here once a file is analysed",
                    getLocalBounds(), juce::Justification::centred);
    }

    void LoudnessGraph::paintGrid (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        g.setFont (glass::font (kAxisFontHeight));

        // A gridline every 6 LU keeps the labels readable at any zoom level.
        const double step = 6.0;
        const double firstLine = std::ceil (minimumLufs / step) * step;

        for (double lufs = firstLine; lufs <= maximumLufs; lufs += step)
        {
            const float y = loudnessToY (lufs, plot);

            g.setColour (kGridColour);
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

            g.setColour (kTextColour);
            g.drawText (juce::String (lufs, 0),
                        juce::Rectangle<float> (0.0f, y - 8.0f, kAxisWidth - 6.0f, 16.0f),
                        juce::Justification::centredRight);
        }

        g.setColour (kTextColour.withAlpha (0.7f));
        g.drawText ("LUFS", juce::Rectangle<float> (0.0f, plot.getBottom() + 4.0f, kAxisWidth - 8.0f, 14.0f),
                    juce::Justification::centredRight);

        if (durationSeconds <= 0.0)
            return;

        const int divisions = 6;

        for (int i = 0; i <= divisions; ++i)
        {
            const double seconds = durationSeconds * static_cast<double> (i) / divisions;
            const float x = timeToX (seconds, plot);

            if (i > 0 && i < divisions)
            {
                g.setColour (kGridColour);
                g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
            }

            const auto minutes = static_cast<int> (seconds) / 60;
            const auto remainder = seconds - minutes * 60.0;
            const auto label = juce::String (minutes) + ":"
                             + juce::String (remainder, 1).paddedLeft ('0', 4);

            // The end labels are aligned inward so the first cannot run back over the
            // axis caption and the last cannot run off the panel.
            auto justification = juce::Justification::centred;
            auto labelX = x - 30.0f;

            if (i == 0)
            {
                justification = juce::Justification::centredLeft;
                labelX = x;
            }
            else if (i == divisions)
            {
                justification = juce::Justification::centredRight;
                labelX = x - 60.0f;
            }

            g.setColour (kTextColour);
            g.drawText (label,
                        juce::Rectangle<float> (labelX, plot.getBottom() + 4.0f, 60.0f, 14.0f),
                        justification);
        }
    }

    void LoudnessGraph::paintTargetBand (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        if (! hasTarget)
            return;

        const double tolerance = target.toleranceLu.value_or (1.0);
        const float top = loudnessToY (target.integratedLufs + tolerance, plot);
        const float bottom = loudnessToY (target.integratedLufs - tolerance, plot);

        const float centre = loudnessToY (target.integratedLufs, plot);
        const float height = juce::jmax (3.0f, bottom - top);
        const float bandTop = juce::jmin (top, centre - height * 0.5f);

        g.setColour (kBandColour);
        g.fillRect (juce::Rectangle<float> (plot.getX(), bandTop, plot.getWidth(), height));

        g.setColour (glass::colour::pass.withAlpha (0.55f));
        g.drawHorizontalLine (juce::roundToInt (centre), plot.getX(), plot.getRight());

        g.setColour (glass::colour::pass.withAlpha (0.95f));
        g.setFont (glass::font (11.0f, true));
        g.drawText (juce::String (target.name) + " " + juce::String (target.integratedLufs, 1),
                    juce::Rectangle<float> (plot.getX() + 8.0f, centre + 3.0f, 220.0f, 13.0f),
                    juce::Justification::centredLeft);
    }

    void LoudnessGraph::paintSeries (juce::Graphics& g,
                                     juce::Rectangle<float> plot,
                                     const std::vector<double>& series,
                                     double windowSeconds,
                                     juce::Colour colour,
                                     float thickness)
    {
        if (series.empty())
            return;

        juce::Path path;
        bool started = false;

        for (std::size_t i = 0; i < series.size(); ++i)
        {
            const double value = series[i];

            if (! isMeasured (value) || value < minimumLufs)
            {
                started = false;
                continue;
            }

            // Each entry is the loudness of the window ending at its far edge; plotting
            // at the window centre stops the traces lagging the audio they describe.
            const double time = static_cast<double> (i) * LoudnessMeasurements::seriesStepSeconds
                              + windowSeconds * 0.5;

            const float x = timeToX (time, plot);
            const float y = loudnessToY (value, plot);

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness));
    }

    void LoudnessGraph::paintOverMarkers (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        if (! hasTarget)
            return;

        const auto overs = findOverEvents (analysis.truePeakEnvelope, target.maxTruePeakDb);

        if (overs.empty())
            return;

        g.setColour (kOverColour);

        for (const auto& over : overs)
        {
            const float x = timeToX (over.startSeconds, plot);
            const float width = juce::jmax (3.0f, timeToX (over.endSeconds, plot) - x);

            g.setColour (kOverColour);
            g.fillRect (juce::Rectangle<float> (x, plot.getY(), width, 7.0f));

            // A faint column down the plot so the position is findable against the
            // trace, not just marked at the very top edge.
            g.setColour (kOverColour.withAlpha (0.22f));
            g.fillRect (juce::Rectangle<float> (x, plot.getY(), width, plot.getHeight()));
        }

        g.setColour (kOverColour);

        g.setFont (glass::font (11.0f, true));
        g.drawText (juce::String (overs.size()) + (overs.size() == 1 ? " TRUE-PEAK OVER" : " TRUE-PEAK OVERS"),
                    juce::Rectangle<float> (plot.getRight() - 200.0f, plot.getY() - kHeaderHeight,
                                            200.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    void LoudnessGraph::paint (juce::Graphics& g)
    {
        const auto panel = getLocalBounds().toFloat();
        glass::paintPanel (g, panel, glass::Depth::recessed);

        if (! hasResult || durationSeconds <= 0.0)
        {
            paintEmptyState (g);
            return;
        }

        const auto plot = getPlotArea();

        if (plot.getWidth() <= 0.0f || plot.getHeight() <= 0.0f)
            return;

        glass::paintWell (g, plot.expanded (6.0f, 4.0f));

        g.setColour (kTextColour.withAlpha (0.9f));
        g.setFont (glass::font (11.0f, true));
        g.drawText ("LOUDNESS OVER TIME",
                    juce::Rectangle<float> (plot.getX(), plot.getY() - kHeaderHeight, 240.0f, 14.0f),
                    juce::Justification::centredLeft);

        paintTargetBand (g, plot);
        paintGrid (g, plot);

        paintSeries (g, plot, analysis.loudness.momentaryLufs, 0.4, kMomentaryColour, kMomentaryThickness);
        paintSeries (g, plot, analysis.loudness.shortTermLufs, 3.0, kShortTermColour, kShortTermThickness);

        paintOverMarkers (g, plot);

        if (isMeasured (analysis.loudness.integratedLufs))
        {
            const float y = loudnessToY (analysis.loudness.integratedLufs, plot);
            g.setColour (juce::Colours::white.withAlpha (0.9f));

            const float dashes[] = { 5.0f, 5.0f };
            g.drawDashedLine (juce::Line<float> (plot.getX(), y, plot.getRight(), y), dashes, 2, 1.6f);

            g.setFont (glass::font (11.0f, true));
            g.drawText ("Integrated " + juce::String (analysis.loudness.integratedLufs, 1),
                        juce::Rectangle<float> (plot.getRight() - 154.0f, y - 16.0f, 150.0f, 13.0f),
                        juce::Justification::centredRight);
        }
    }
}
