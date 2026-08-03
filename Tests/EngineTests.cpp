#include "TestHarness.h"
#include "SignalUtils.h"

#include "Engine/AudioAnalyser.h"
#include "Engine/KWeighting.h"
#include "Engine/LoudnessMeter.h"
#include "Engine/QualityChecks.h"
#include "Engine/TruePeakDetector.h"

#include <algorithm>
#include <cmath>

using namespace qctest;

namespace
{
    constexpr double kRate = 48000.0;

    double measureIntegrated (const Signal& signal, double sampleRate = kRate)
    {
        qc::LoudnessMeter meter (sampleRate, signal.numChannels());
        meter.process (signal.data(), signal.numSamples());
        return meter.getMeasurements().integratedLufs;
    }
}

// ---------------------------------------------------------------------------
// K-weighting
// ---------------------------------------------------------------------------

QC_TEST (kWeightingMatchesPublished48kTable)
{
    // ITU-R BS.1770-4 Tables 1 and 2. If the re-derivation drifts from these, every
    // measurement at every sample rate is wrong.
    const auto shelf = qc::designKWeightingShelf (48000.0);
    checkClose (shelf.b0,  1.53512485958697, 1.0e-9, "shelf b0");
    checkClose (shelf.b1, -2.69169618940638, 1.0e-9, "shelf b1");
    checkClose (shelf.b2,  1.19839281085285, 1.0e-9, "shelf b2");
    checkClose (shelf.a1, -1.69065929318241, 1.0e-9, "shelf a1");
    checkClose (shelf.a2,  0.73248077421585, 1.0e-9, "shelf a2");

    const auto highPass = qc::designKWeightingHighPass (48000.0);
    checkClose (highPass.b0,  1.0, 1.0e-12, "high-pass b0");
    checkClose (highPass.b1, -2.0, 1.0e-12, "high-pass b1");
    checkClose (highPass.b2,  1.0, 1.0e-12, "high-pass b2");
    checkClose (highPass.a1, -1.99004745483398, 1.0e-9, "high-pass a1");
    checkClose (highPass.a2,  0.99007225036621, 1.0e-9, "high-pass a2");
}

QC_TEST (kWeightingIsReDerivedForOtherSampleRates)
{
    // The shelf's corner must stay at the same frequency, not slide with the rate.
    const double gainAt48k = kWeightedGain (1000.0, 48000.0);
    const double gainAt44k = kWeightedGain (1000.0, 44100.0);
    const double gainAt96k = kWeightedGain (1000.0, 96000.0);

    checkClose (20.0 * std::log10 (gainAt44k / gainAt48k), 0.0, 0.05, "44.1 kHz response vs 48 kHz");
    checkClose (20.0 * std::log10 (gainAt96k / gainAt48k), 0.0, 0.05, "96 kHz response vs 48 kHz");

    // A meter that reused the 48 kHz coefficients at 44.1 kHz would read differently.
    const auto sine44 = makeSine (44100.0, 1000.0, 0.5, 5.0, 2);
    checkClose (measureIntegrated (sine44, 44100.0),
                predictedSineLoudness (1000.0, 0.5, 44100.0, 2),
                0.05, "integrated loudness at 44.1 kHz");
}

QC_TEST (rejectsUnusableSampleRates)
{
    bool threw = false;

    try
    {
        qc::LoudnessMeter meter (0.0, 2);
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    check (threw, "a zero sample rate should be rejected at construction");
}

// ---------------------------------------------------------------------------
// Integrated loudness
// ---------------------------------------------------------------------------

QC_TEST (integratedLoudnessMatchesTheoreticalSineLevel)
{
    const auto signal = makeSine (kRate, 1000.0, 0.5, 5.0, 2);

    checkClose (measureIntegrated (signal),
                predictedSineLoudness (1000.0, 0.5, kRate, 2),
                0.05, "integrated loudness of a 1 kHz stereo sine");
}

QC_TEST (integratedLoudnessIsLinearInGain)
{
    const auto loud = makeSine (kRate, 1000.0, 0.5, 5.0, 1);
    const auto quiet = makeSine (kRate, 1000.0, 0.25, 5.0, 1);

    checkClose (measureIntegrated (loud) - measureIntegrated (quiet),
                6.0206, 0.01, "halving amplitude should cost 6.02 dB");
}

QC_TEST (stereoSumsThreeDecibelsAboveMono)
{
    const auto mono = makeSine (kRate, 1000.0, 0.5, 5.0, 1);
    const auto stereo = makeSine (kRate, 1000.0, 0.5, 5.0, 2);

    checkClose (measureIntegrated (stereo) - measureIntegrated (mono),
                3.0103, 0.01, "two identical channels should sum to +3.01 dB");
}

QC_TEST (absoluteGateExcludesSilence)
{
    // Ten seconds of tone followed by ten of digital silence must read the same as the
    // tone alone: the -70 LUFS absolute gate discards the silent blocks entirely.
    const auto toneOnly = makeSine (kRate, 1000.0, 0.25, 10.0, 2);

    Signal withSilence (2, static_cast<int> (kRate * 20.0));
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < toneOnly.numSamples(); ++i)
            withSilence[channel][static_cast<std::size_t> (i)] =
                toneOnly[channel][static_cast<std::size_t> (i)];

    checkClose (measureIntegrated (withSilence), measureIntegrated (toneOnly),
                0.1, "silence should not drag the integrated reading down");
}

QC_TEST (relativeGateExcludesQuietPassages)
{
    const double loudAmplitude = 0.5;
    const double quietAmplitude = loudAmplitude * std::pow (10.0, -30.0 / 20.0);

    const auto loudOnly = makeSine (kRate, 1000.0, loudAmplitude, 10.0, 2);
    const auto quietPart = makeSine (kRate, 1000.0, quietAmplitude, 10.0, 2);

    Signal combined (2, static_cast<int> (kRate * 20.0));
    const int half = loudOnly.numSamples();

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int i = 0; i < half; ++i)
        {
            combined[channel][static_cast<std::size_t> (i)] =
                loudOnly[channel][static_cast<std::size_t> (i)];
            combined[channel][static_cast<std::size_t> (half + i)] =
                quietPart[channel][static_cast<std::size_t> (i)];
        }
    }

    // 30 LU below the loud section is well under the -10 LU relative gate, so the quiet
    // half must not pull the integrated figure down.
    checkClose (measureIntegrated (combined), measureIntegrated (loudOnly),
                0.15, "a passage 30 LU down should fall outside the relative gate");
}

QC_TEST (silenceProducesNoLoudnessRatherThanZero)
{
    Signal silence (2, static_cast<int> (kRate * 2.0));
    check (! qc::isMeasured (measureIntegrated (silence)),
           "a silent file must report no measurement, not a number");
}

QC_TEST (fileShorterThanOneBlockIsNotMeasured)
{
    const auto tooShort = makeSine (kRate, 1000.0, 0.5, 0.05, 2);
    check (! qc::isMeasured (measureIntegrated (tooShort)),
           "a file shorter than one 400 ms gating block cannot be integrated");
}

// ---------------------------------------------------------------------------
// Loudness range
// ---------------------------------------------------------------------------

QC_TEST (loudnessRangeTracksProgrammeDynamics)
{
    const double loudAmplitude = 0.5;
    const double quietAmplitude = loudAmplitude * std::pow (10.0, -10.0 / 20.0);

    const int segmentSamples = static_cast<int> (kRate * 10.0);
    Signal alternating (2, segmentSamples * 6);

    for (int segment = 0; segment < 6; ++segment)
    {
        const double amplitude = (segment % 2 == 0) ? loudAmplitude : quietAmplitude;

        for (int i = 0; i < segmentSamples; ++i)
        {
            const auto value = static_cast<float> (amplitude
                * std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / kRate));

            for (int channel = 0; channel < 2; ++channel)
                alternating[channel][static_cast<std::size_t> (segment * segmentSamples + i)] = value;
        }
    }

    qc::LoudnessMeter meter (kRate, 2);
    meter.process (alternating.data(), alternating.numSamples());

    checkClose (meter.getMeasurements().loudnessRangeLu, 10.0, 1.5,
                "alternating 10 LU segments should give an LRA near 10 LU");
}

QC_TEST (loudnessRangeOfSteadyToneIsNearZero)
{
    const auto steady = makeSine (kRate, 1000.0, 0.5, 30.0, 2);

    qc::LoudnessMeter meter (kRate, 2);
    meter.process (steady.data(), steady.numSamples());

    checkClose (meter.getMeasurements().loudnessRangeLu, 0.0, 0.2,
                "a steady tone has no loudness range");
}

// ---------------------------------------------------------------------------
// True peak
// ---------------------------------------------------------------------------

QC_TEST (truePeakFindsInterSamplePeaksThatSamplePeakMisses)
{
    // A sine at fs/4 offset by a quarter cycle lands every sample at +/-0.7071 while the
    // waveform itself reaches full scale exactly halfway between samples. Sample peak
    // says -3 dBFS; the true peak is 0 dBFS. This is the case the whole detector exists
    // for.
    const auto signal = makeSine (kRate, kRate / 4.0, 1.0, 1.0, 1, kPi / 4.0);

    qc::TruePeakDetector detector (kRate, 1);
    detector.process (signal.data(), signal.numSamples());

    qc::QualityAnalyser quality (kRate, 1);
    quality.process (signal.data(), signal.numSamples());

    const double samplePeakDb = quality.getMeasurements().samplePeakDb.front();

    checkClose (samplePeakDb, -3.01, 0.05, "sample peak of the offset fs/4 sine");
    checkClose (detector.getTruePeakDb(), 0.0, 0.15, "true peak of the offset fs/4 sine");
    check (detector.getOversamplingFactor() == 4, "48 kHz material should be oversampled 4x");
}

QC_TEST (truePeakIsNeverBelowSamplePeak)
{
    const auto signal = makeSine (kRate, 997.0, 0.9, 1.0, 2);

    qc::TruePeakDetector detector (kRate, 2);
    detector.process (signal.data(), signal.numSamples());

    qc::QualityAnalyser quality (kRate, 2);
    quality.process (signal.data(), signal.numSamples());

    const double samplePeakDb = quality.getMeasurements().samplePeakDb.front();
    check (detector.getTruePeakDb() >= samplePeakDb - 1.0e-9,
           "true peak must bound sample peak from above");
}

QC_TEST (truePeakOversampleFactorFollowsSampleRate)
{
    check (qc::TruePeakDetector (44100.0, 1).getOversamplingFactor() == 4, "44.1 kHz -> 4x");
    check (qc::TruePeakDetector (96000.0, 1).getOversamplingFactor() == 2, "96 kHz -> 2x");
    check (qc::TruePeakDetector (192000.0, 1).getOversamplingFactor() == 1, "192 kHz -> 1x");
}

QC_TEST (overEventsAreReportedWithTimestamps)
{
    const int numSamples = static_cast<int> (kRate * 2.0);
    Signal signal (1, numSamples);

    // Quiet throughout except for a loud burst one second in.
    const int burstStart = static_cast<int> (kRate * 1.0);
    const int burstLength = static_cast<int> (kRate * 0.1);

    for (int i = 0; i < numSamples; ++i)
    {
        const double amplitude = (i >= burstStart && i < burstStart + burstLength) ? 0.99 : 0.05;
        signal[0][static_cast<std::size_t> (i)] =
            static_cast<float> (amplitude * std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / kRate));
    }

    qc::TruePeakDetector detector (kRate, 1);
    detector.process (signal.data(), signal.numSamples());

    const auto overs = detector.getOverEvents (-1.0);

    check (! overs.empty(), "the burst should be reported as an over");
    checkClose (overs.front().startSeconds, 1.0, 0.05, "reported time of the over");
    check (overs.front().peakDb > -1.0, "an over must exceed the ceiling it was found against");

    check (detector.getOverEvents (0.5).empty(),
           "nothing should exceed a ceiling above the signal's true peak");
}

// ---------------------------------------------------------------------------
// Sample-domain checks
// ---------------------------------------------------------------------------

QC_TEST (clippingRunsAreDetectedAndTimed)
{
    const int numSamples = static_cast<int> (kRate * 1.0);
    Signal signal (1, numSamples);

    const int clipStart = static_cast<int> (kRate * 0.5);
    for (int i = 0; i < 10; ++i)
        signal[0][static_cast<std::size_t> (clipStart + i)] = 1.0f;

    qc::QualityAnalyser analyser (kRate, 1);
    analyser.process (signal.data(), signal.numSamples());

    const auto measurements = analyser.getMeasurements();
    check (measurements.clipEvents.size() == 1, "one clipped run expected");
    check (measurements.clipEvents.front().lengthInSamples == 10, "run length should be 10 samples");
    checkClose (measurements.clipEvents.front().startSeconds, 0.5, 0.001, "clip start time");
}

QC_TEST (runsShorterThanTheMinimumAreNotClips)
{
    const int numSamples = static_cast<int> (kRate * 1.0);
    Signal signal (1, numSamples);

    // Two consecutive full-scale samples is a peak, not flat-topping.
    signal[0][1000] = 1.0f;
    signal[0][1001] = 1.0f;

    qc::QualityAnalyser analyser (kRate, 1);
    analyser.process (signal.data(), signal.numSamples());

    check (analyser.getMeasurements().clipEvents.empty(),
           "a two-sample run should not be reported as clipping");
}

QC_TEST (clipRunStillOpenAtEndOfFileIsReported)
{
    const int numSamples = 1000;
    Signal signal (1, numSamples);

    for (int i = numSamples - 20; i < numSamples; ++i)
        signal[0][static_cast<std::size_t> (i)] = 1.0f;

    qc::QualityAnalyser analyser (kRate, 1);
    analyser.process (signal.data(), signal.numSamples());

    check (analyser.getMeasurements().clipEvents.size() == 1,
           "a clip run that reaches the end of the file must still be reported");
}

QC_TEST (correlationDetectsOutOfPhaseMaterial)
{
    const auto inPhase = makeSine (kRate, 1000.0, 0.5, 1.0, 2);

    qc::QualityAnalyser positive (kRate, 2);
    positive.process (inPhase.data(), inPhase.numSamples());
    checkClose (positive.getMeasurements().correlation, 1.0, 0.01, "identical channels");

    auto antiPhase = makeSine (kRate, 1000.0, 0.5, 1.0, 2);
    for (auto& sample : antiPhase[1])
        sample = -sample;

    qc::QualityAnalyser negative (kRate, 2);
    negative.process (antiPhase.data(), antiPhase.numSamples());

    const auto measurements = negative.getMeasurements();
    checkClose (measurements.correlation, -1.0, 0.01, "inverted channel");
    checkClose (measurements.negativeCorrelationFraction, 1.0, 0.01,
                "the whole programme is out of phase");
}

QC_TEST (silentBlocksDoNotCountAsOutOfPhase)
{
    Signal silence (2, static_cast<int> (kRate * 1.0));

    qc::QualityAnalyser analyser (kRate, 2);
    analyser.process (silence.data(), silence.numSamples());

    checkClose (analyser.getMeasurements().negativeCorrelationFraction, 0.0, 1.0e-9,
                "silence has no phase relationship to report");
}

// ---------------------------------------------------------------------------
// Orchestration
// ---------------------------------------------------------------------------

QC_TEST (analyserReportsMonoCompatibilityLoss)
{
    const auto correlated = makeSine (kRate, 1000.0, 0.5, 5.0, 2);

    qc::AudioAnalyser matched (kRate, 2);
    matched.process (correlated.data(), correlated.numSamples());

    checkClose (matched.getResult().monoCompatibilityLossDb, 0.0, 0.05,
                "identical channels sum without loss");

    auto antiPhase = makeSine (kRate, 1000.0, 0.5, 5.0, 2);
    for (auto& sample : antiPhase[1])
        sample = -sample;

    qc::AudioAnalyser cancelling (kRate, 2);
    cancelling.process (antiPhase.data(), antiPhase.numSamples());

    check (! std::isfinite (cancelling.getResult().monoCompatibilityLossDb),
           "channels that cancel completely should report infinite mono loss, not zero");
}

QC_TEST (analyserComputesPeakToLoudnessRatio)
{
    const auto signal = makeSine (kRate, 1000.0, 0.5, 5.0, 2);

    qc::AudioAnalyser analyser (kRate, 2);
    analyser.process (signal.data(), signal.numSamples());

    const auto result = analyser.getResult();
    checkClose (result.peakToLoudnessRatioDb,
                result.truePeakDb - result.loudness.integratedLufs,
                1.0e-9, "PLR is true peak minus integrated loudness");
}

QC_TEST (processingInChunksMatchesASinglePass)
{
    const auto signal = makeSine (kRate, 1000.0, 0.4, 5.0, 2);

    qc::LoudnessMeter singlePass (kRate, 2);
    singlePass.process (signal.data(), signal.numSamples());

    // Awkward chunk sizes must not shift block boundaries.
    qc::LoudnessMeter chunked (kRate, 2);
    const float* pointers[2];
    int offset = 0;
    int chunk = 1;

    while (offset < signal.numSamples())
    {
        const int size = std::min (chunk, signal.numSamples() - offset);
        pointers[0] = signal[0].data() + offset;
        pointers[1] = signal[1].data() + offset;
        chunked.process (pointers, size);
        offset += size;
        chunk = (chunk * 3) % 4099 + 1;
    }

    checkClose (chunked.getMeasurements().integratedLufs,
                singlePass.getMeasurements().integratedLufs,
                1.0e-9, "block size must not affect the result");
}

QC_TEST (dialogueGatingMeasuresOnlyTheStemsActiveIntervals)
{
    const int segmentSamples = static_cast<int> (kRate * 10.0);

    // Programme: loud dialogue for ten seconds, then loud music with no dialogue.
    Signal programme (2, segmentSamples * 2);
    Signal stem (2, segmentSamples * 2);

    for (int i = 0; i < segmentSamples; ++i)
    {
        const auto dialogue = static_cast<float> (0.1
            * std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / kRate));
        const auto music = static_cast<float> (0.5
            * std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / kRate));

        for (int channel = 0; channel < 2; ++channel)
        {
            programme[channel][static_cast<std::size_t> (i)] = dialogue;
            programme[channel][static_cast<std::size_t> (segmentSamples + i)] = music;
            stem[channel][static_cast<std::size_t> (i)] = dialogue;
        }
    }

    qc::LoudnessMeter programmeMeter (kRate, 2);
    programmeMeter.process (programme.data(), programme.numSamples());

    qc::LoudnessMeter stemMeter (kRate, 2);
    stemMeter.process (stem.data(), stem.numSamples());

    const double dialogueGated = qc::computeDialogueGatedLoudness (programmeMeter.getGatingBlockPowers(),
                                                                   stemMeter.getGatingBlockPowers());

    const auto dialogueOnly = makeSine (kRate, 1000.0, 0.1, 10.0, 2);

    checkClose (dialogueGated, measureIntegrated (dialogueOnly), 0.2,
                "dialogue gating should report the level of the dialogue passage");

    // The programme-gated figure is dominated by the music and must differ.
    check (programmeMeter.getMeasurements().integratedLufs > dialogueGated + 5.0,
           "programme gating and dialogue gating should not agree on this material");
}

QC_TEST (dialogueGatingRejectsMismatchedDurations)
{
    bool threw = false;

    try
    {
        qc::computeDialogueGatedLoudness ({ 0.1, 0.1, 0.1 }, { 0.1, 0.1 });
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    check (threw, "a stem of a different length must be rejected, not silently truncated");
}
