#pragma once

#include "../Engine/AnalysisResult.h"
#include "../Verdict/Target.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace qc
{
    /** Short-term and momentary loudness across the file, with the selected target's
        tolerance band shaded and true-peak overs marked.

        The point of the graph is to make "where did it go wrong" answerable at a glance,
        so the band is drawn behind the traces and overs are marked on the time axis
        rather than hidden in a list.
    */
    class LoudnessGraph : public juce::Component
    {
    public:
        LoudnessGraph();

        void setResult (const AnalysisResult& result);
        void setTarget (const Target* target);
        void clearResult();

        void paint (juce::Graphics& g) override;

    private:
        juce::Rectangle<float> getPlotArea() const;
        float loudnessToY (double lufs, juce::Rectangle<float> plot) const;
        float timeToX (double seconds, juce::Rectangle<float> plot) const;

        void paintEmptyState (juce::Graphics& g);
        void paintGrid (juce::Graphics& g, juce::Rectangle<float> plot);
        void paintTargetBand (juce::Graphics& g, juce::Rectangle<float> plot);
        void paintSeries (juce::Graphics& g,
                          juce::Rectangle<float> plot,
                          const std::vector<double>& series,
                          double windowSeconds,
                          juce::Colour colour,
                          float thickness);
        void paintOverMarkers (juce::Graphics& g, juce::Rectangle<float> plot);

        AnalysisResult analysis;
        bool hasResult { false };

        Target target;
        bool hasTarget { false };

        double durationSeconds { 0.0 };
        double minimumLufs { -60.0 };
        double maximumLufs { 0.0 };
    };
}
