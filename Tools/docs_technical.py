"""Part II of the documentation: how the application is built.

Everything here describes code that exists. The file reference table at the end is
generated from the source tree, so it cannot drift; the prose has to be maintained.
"""

from __future__ import annotations

from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.platypus import PageBreak, Paragraph, Spacer

from docs_style import (
    CONTENT_WIDTH,
    FILE_PURPOSE,
    S,
    WARN,
    bullets,
    code_block,
    count_tests,
    data_table,
    heading,
    note,
    para,
    survey,
)


def part_two():
    f = []
    f.append(Paragraph("Part II", S["part"]))
    f.append(Paragraph("Technical Documentation", S["part_sub"]))

    # -----------------------------------------------------------------------
    f.append(heading("16. Architecture", 0))
    f.append(para(
        "The codebase is four layers, and the dependency rule runs one way only: nothing in "
        "a lower layer knows the layer above it exists."))
    f.extend(data_table(
        ["Layer", "Directory", "Depends on", "What lives there"],
        [
            ["Engine", "Source/Engine", "nothing but the standard library",
             "K-weighting, loudness metering, true peak, sample-domain checks, the result struct."],
            ["Verdict", "Source/Verdict", "Engine",
             "Delivery specifications, pass/fail rules, fix hints."],
            ["Report", "Source/Report", "Engine, Verdict",
             "JSON serialisation, the PDF primitive writer, and the report layout."],
            ["App", "Source/App", "everything below, plus JUCE",
             "File reading, batching, playback, the interface."],
        ],
        widths=[22 * mm, 30 * mm, 40 * mm, CONTENT_WIDTH - 92 * mm]))
    f.extend(note(
        "The first three layers contain no JUCE",
        "Measurement, judgement and reporting are plain C++17 with no framework dependency at "
        "all. That is not purism: it means the entire analysis path builds and runs in a test "
        "binary with no window, no audio device, and no network fetch, which is what makes the "
        "suite fast enough to run on every change."))
    f.append(para(
        "The consequence worth internalising is that <b>AnalysisResult</b> is the seam. It is plain "
        "data. The graph, the verdict panel, the JSON writer and the PDF report all consume it "
        "and nothing else, so a result can be constructed by hand in a test without a file "
        "existing anywhere."))

    # -----------------------------------------------------------------------
    f.append(heading("17. Building", 0))
    f.append(para(
        "CMake with CPM, which fetches a pinned JUCE on first configure. A clean clone needs "
        "nothing installed beyond a compiler and CMake."))
    f.extend(code_block([
        "cmake -S . -B build -DQC_BUILD_APP=ON",
        "cmake --build build --config Release",
        "ctest --test-dir build -C Release --output-on-failure",
    ]))
    f.extend(data_table(
        ["Option", "Default", "Effect"],
        [
            ["QC_BUILD_TESTS", "ON", "Builds the test binaries."],
            ["QC_BUILD_APP", "OFF",
             "Builds the application. Off by default so a clean clone configures and tests with "
             "no network access; turning it on downloads JUCE."],
            ["QC_BUILD_UI_SNAPSHOT", "OFF", "Builds qc_uishot, the offscreen interface renderer."],
            ["QC_BUILD_ICON_TOOL", "OFF", "Builds qc_makeicon, the icon generator."],
        ],
        widths=[44 * mm, 18 * mm, CONTENT_WIDTH - 62 * mm], mono_cols=(0,)))
    f.append(para(
        "JUCE is pinned to 8.0.9. JUCE 9 exists but nothing here has been verified against its "
        "API changes; bump the tag in <font face=\"Courier\">Source/App/CMakeLists.txt</font> and run the "
        "application before trusting it. <font face=\"Courier\">JUCE_DISPLAY_SPLASH_SCREEN</font> is left at "
        "its default, because turning it off is a licence decision rather than a build decision."))

    # -----------------------------------------------------------------------
    f.append(heading("18. The measurement engine", 0))

    f.append(Paragraph("K-weighting", S["h3"]))
    f.append(para(
        "BS.1770-4 publishes filter coefficients for 48 kHz only. Reusing those values at "
        "another sample rate shifts the corner frequency and biases every measurement taken at "
        "that rate, so <b>KWeighting.cpp</b> re-fits the analogue prototype and bilinear-transforms "
        "it at whatever rate is passed in. At 48 kHz this reproduces the published table to "
        "within 1e-9, which is asserted by a test &#8212; if that assertion ever fails, every "
        "measurement at every sample rate is wrong."))
    f.append(para(
        "The filters run in double precision throughout. The RLB high-pass sits at 38 Hz, and "
        "in single precision its state decays into the noise floor over a long file."))

    f.append(Paragraph("Loudness metering", S["h3"]))
    f.append(para(
        "<b>LoudnessMeter</b> consumes audio in arbitrary-sized chunks and reduces it immediately to "
        "one mean-square value per channel per 100 ms sub-block. Every published window is then "
        "an average over a whole number of those sub-blocks:"))
    f.extend(bullets([
        "Momentary is 4 sub-blocks (400 ms), hopped 1.",
        "Short-term is 30 sub-blocks (3 s), hopped 1.",
        "The gating blocks for integrated loudness are the 400 ms windows at 75% overlap &#8212; "
        "which is exactly the momentary series, so it is computed once.",
        "LRA windows are 30 sub-blocks hopped 10, per EBU Tech 3342.",
    ]))
    f.append(para(
        "The consequence is that memory grows with duration rather than with sample count: an "
        "hour of stereo costs well under a megabyte, so a feature-length file is no harder than "
        "a thirty-second spot. A trailing partial sub-block is deliberately discarded, because "
        "BS.1770-4 gates over complete blocks and including a short final one would bias the "
        "result upward."))
    f.append(para(
        "Gating is exposed as free functions &#8212; <font face=\"Courier\">gatedBlockIndices</font> and "
        "<font face=\"Courier\">gatedLoudness</font> &#8212; rather than being buried in the class, because dialogue "
        "gating needs to run the same arithmetic over a restricted set of blocks."))

    f.append(Paragraph("True peak", S["h3"]))
    f.append(para(
        "<b>TruePeakDetector</b> oversamples with a Kaiser-windowed sinc polyphase interpolator: 4x "
        "below 96 kHz, 2x below 192 kHz, none above, where inter-sample peaks are already "
        "resolved. The filter is generated rather than taken from the table in BS.1770-4 Annex 2, "
        "because that table is specified for 48 kHz 4x only and generating it keeps the same code "
        "correct at other rates. Each polyphase branch is normalised to unit DC gain; without "
        "that the branches differ by a fraction of a decibel and a steady tone appears to wobble."))
    f.append(para(
        "Alongside the overall maximum the detector keeps a per-channel envelope of the highest "
        "interpolated magnitude in each 10 ms slice. Over events for any ceiling are derived from "
        "that envelope, which is why changing the selected target re-reports overs instantly "
        "without re-reading the file, and why memory is bounded by duration rather than by how "
        "loud the material is."))
    f.extend(note(
        "An earlier design stored every over candidate individually",
        "It would have hit its cap after about four seconds of any loud master, and its event "
        "merging failed across interleaved channels. The envelope replaced it before the first "
        "build. Bounded-by-duration is the property that matters here."))

    f.append(Paragraph("Sample-domain checks", S["h3"]))
    f.append(para(
        "<b>QualityAnalyser</b> covers what loudness cannot: sample peak per channel, runs of "
        "consecutive samples at or above the clip threshold, and stereo correlation. Correlation "
        "is evaluated in 100 ms blocks so the reported figure means <i>this much of the programme was "
        "out of phase</i> rather than <i>the whole file averaged out to roughly zero</i>, which is what a "
        "single global figure gives you on material that swings between wide and mono. Blocks "
        "below a silence floor are excluded, or every fade would be flagged."))

    f.append(Paragraph("Orchestration and mono compatibility", S["h3"]))
    f.append(para(
        "<b>AudioAnalyser</b> owns the meters and feeds them one pass of the audio. For stereo it also "
        "feeds a second loudness meter with the mono downmix, computed as (L + R) / sqrt(2) rather "
        "than (L + R) / 2. That normalisation is power-preserving: identical channels sum to the "
        "same loudness as the pair and report zero loss, while fully decorrelated content lands at "
        "3 dB. With a /2 downmix every mono-compatible file would report a 3 dB loss and the check "
        "would be useless."))
    f.append(para(
        "A stereo file whose mono sum falls below the gate has cancelled itself out entirely. That "
        "reports as infinite loss, and the verdict renders it in words rather than as a number. An "
        "earlier version reported it as <i>zero</i> loss, because the comparison was skipped when the "
        "mono figure was unmeasurable &#8212; the worst possible result presented as the best."))

    f.append(Paragraph("Dialogue gating", S["h3"]))
    f.append(para(
        "<font face=\"Courier\">computeDialogueGatedLoudness</font> takes the programme's gating-block powers and "
        "the stem's, gates the stem normally to find where dialogue is, and integrates the "
        "programme over those blocks."))
    f.append(para(
        "There is one subtlety that a test caught. Gating blocks are 400 ms hopped 100 ms, so the "
        "three blocks straddling the start or end of a dialogue passage are only partly dialogue. "
        "The stem passes them &#8212; a quarter of a phrase is far above the gate &#8212; but in the "
        "programme those same blocks contain whatever else is playing. On a mix where dialogue "
        "gives way to loud music that pulled the reading 1.3 dB high. The active set is therefore "
        "eroded by one window length at each edge, leaving only blocks that are dialogue the whole "
        "way through. The cost is that passages shorter than about 0.7 s drop out, which is the "
        "right way to be wrong: an under-sampled dialogue figure beats one contaminated by music."))

    # -----------------------------------------------------------------------
    f.append(heading("19. The verdict engine", 0))
    f.append(para(
        "<font face=\"Courier\">evaluate(result, target)</font> is pure: it reads an AnalysisResult and a Target and "
        "returns a TargetVerdict. No I/O, no state, no framework."))
    f.append(para("The status model has four values and one ordering rule:"))
    f.extend(code_block(["pass  <  warn  <  notMeasured  <  fail"]))
    f.append(para(
        "A verdict takes the worst status among its checks, and a batch row takes the worst across "
        "its verdicts. <b>notMeasured</b> outranks <b>warn</b> deliberately: an unjudgeable target must not "
        "be hidden behind a warning about something else."))
    f.append(para(
        "An empty verdict list returns notMeasured, not pass. This mattered in batch mode, where a "
        "row with no targets selected would otherwise have rendered green &#8212; nothing judged is "
        "not the same as nothing wrong."))
    f.append(para(
        "Each verdict carries the specification it was judged against, including its lastVerified "
        "date, so a report can state the numbers and their provenance rather than making a reader "
        "look them up."))

    # -----------------------------------------------------------------------
    f.append(heading("20. Reporting", 0))

    f.append(Paragraph("JSON", S["h3"]))
    f.append(para(
        "<b>JsonReport</b> is hand-written rather than built on juce::var, which keeps it in the "
        "dependency-free layer and lets the engine test binary cover it. The rules that matter:"))
    f.extend(bullets([
        "Anything non-finite is written as <font face=\"Courier\">null</font>. JSON cannot represent infinity, and a "
        "consumer reading -inf as a level would draw exactly the wrong conclusion.",
        "Status tokens are separate from the interface labels, so renaming a label cannot break a "
        "downstream parser.",
        "True-peak overs are listed per target with timestamps. The list is capped, but the count "
        "is always the real total &#8212; a capped list must never understate how bad a file is.",
        "Backslashes and quotes are escaped, which matters because every path on this platform is "
        "full of backslashes.",
    ]))

    f.append(Paragraph("PDF", S["h3"]))
    f.append(para(
        "JUCE provides no PDF or printing API on Windows. Rather than ship a library for one file "
        "format, <b>PdfWriter</b> emits PDF operators directly: filled and stroked rectangles, "
        "polylines, and text in the standard Helvetica faces. Using the standard fonts means "
        "nothing is embedded, so a report stays small and opens anywhere, and the whole path stays "
        "in the dependency-free layer."))
    f.append(para(
        "Text widths are approximated deliberately. Exact widths would mean shipping Helvetica "
        "metrics for three faces, and nothing in the report is centred or right-aligned against a "
        "measured width; the approximation is used only to decide where to truncate a long path."))
    f.append(para(
        "<b>PdfReport</b> lays out one A4 page per file. The loudness series is decimated to 900 points, "
        "keeping the loudest value in each bucket rather than the mean, so peaks survive. An hour "
        "of audio drawn point for point would be megabytes of detail no printer resolves; a test "
        "pins an hour-long file under 200 kB."))
    f.extend(note(
        "The PDF tests walk the cross-reference table",
        "Every byte offset is checked to resolve to the object it claims. A wrong offset there "
        "opens as a blank page, and no substring assertion would notice. Escaping is covered too: "
        "an unescaped parenthesis terminates a PDF string early and corrupts the rest of the page."))

    # -----------------------------------------------------------------------
    f.append(heading("21. The application layer", 0))

    f.append(Paragraph("File reading", S["h3"]))
    f.append(para(
        "<b>FileAnalysisJob</b> streams a file in 32,768-sample blocks into an AudioAnalyser, polling a "
        "cancellation callback and reporting progress between blocks. Failures name what went "
        "wrong: the extension that could not be decoded and what this build supports, the channel "
        "count that is out of scope, or the specific mismatch between a mix and its dialogue stem. "
        "A stem of a different rate or length is rejected rather than truncated, because gating "
        "against it would produce a confident number about the wrong intervals."))

    f.append(Paragraph("AAC and M4A", S["h3"]))
    f.append(para(
        "JUCE's WindowsMediaAudioFormat registers only .wmv/.asf/.wm/.wma, which leaves out the "
        "container podcast and streaming deliverables actually arrive in. "
        "<b>MediaFoundationAudioFormat</b> fills that gap with an IMFSourceReader-backed "
        "juce::AudioFormat, using decoders already present on every supported Windows install."))
    f.extend(bullets([
        "Float output is requested, with 16-bit PCM as a fallback converted on the way in, so the "
        "rest of the application never sees which path was taken.",
        "Position is tracked from the decoder's own timestamps, so a seek lands where it claims.",
        "Any shortfall is zero-filled: a compressed file's duration is metadata, and encoder "
        "padding routinely overstates what actually decodes.",
        "Media Foundation decodes from a path, so the format only accepts a juce::FileInputStream "
        "&#8212; and fails cleanly rather than silently for anything else.",
    ]))
    f.append(para(
        "The tests do not commit an encoded fixture. They encode AAC with the system encoder via "
        "IMFSinkWriter and decode it back, because a committed binary proves nothing about the "
        "machine running the suite."))

    f.append(Paragraph("Batching", S["h3"]))
    f.append(para(
        "<b>BatchAnalyser</b> runs files through a thread pool capped at four workers rather than one "
        "per core. Analysis is memory-bandwidth bound rather than compute bound, so saturating "
        "every core buys little and makes the machine unusable while a folder is checked."))
    f.append(para(
        "Entries are mutated only on the message thread: a worker produces an outcome and hands it "
        "over with <font face=\"Courier\">MessageManager::callAsync</font>. That is why there is no lock in the class. "
        "Callbacks from a superseded run are dropped, since a run can be replaced while its "
        "callbacks are still in flight."))
    f.append(para(
        "<font face=\"Courier\">reevaluate()</font> re-judges completed entries against a new target selection without "
        "touching the disk &#8212; the measurements do not change, only what they are compared with."))

    f.append(Paragraph("Playback", S["h3"]))
    f.append(para(
        "<b>PlaybackEngine</b> wraps an AudioDeviceManager, an AudioSourcePlayer and an "
        "AudioTransportSource. The device is opened lazily on first use, output-only, so a session "
        "spent checking files never takes a device from anything else and never prompts for "
        "microphone permission."))
    f.extend(note(
        "The read-ahead thread initialises COM for itself",
        "The Media Foundation reader creates its objects on the thread that opened the file and "
        "then has samples pulled from the read-ahead thread. Calling into those objects from a "
        "thread with no apartment fails at run time &#8212; and only for compressed formats, so WAVs "
        "would play fine and podcasts would fail in the field. This is the kind of defect that "
        "ships if the threading is not thought about explicitly.",
        tint=colors.HexColor("#fdf4e3"), bar=WARN))

    f.append(Paragraph("Target loading", S["h3"]))
    f.append(para(
        "<b>TargetLibrary</b> parses targets.json, preferring a user copy in the application data "
        "directory over the compiled-in default and writing that copy on first run. Malformed "
        "entries are skipped and reported rather than dropped, and targets with no lastVerified "
        "date are collected so the interface can say how many are unverified."))

    # -----------------------------------------------------------------------
    f.append(heading("22. The interface layer", 0))
    f.append(para(
        "<b>GlassStyle</b> holds the entire visual language: palette, panel metrics, and three depths "
        "(recessed, raised, floating) that everything is drawn at. Panels darken what is behind "
        "them rather than lightening it. Tinting glass white looks convincing over a photograph, "
        "but this interface is almost entirely light text, and a lightened panel costs most of its "
        "contrast. <b>GlassLookAndFeel</b> restyles the stock JUCE controls; leaving them is the single "
        "biggest giveaway in a themed JUCE application."))
    f.append(para(
        "The backdrop is drawn at a tenth scale and resampled up rather than blurred at full size "
        "&#8212; cheaper, and smoother falloff than a large-radius blur. A faint grain is laid over "
        "it, without which gradients that wide band visibly on 8-bit displays."))
    f.append(para(
        "<b>MainComponent</b> owns the layout and the wiring. The verdict list takes the height its "
        "content needs and the graph takes the rest, rather than the graph taking a fixed slice and "
        "leaving a dead band across the middle whenever one or two targets are selected."))
    f.append(para(
        "<b>LoudnessGraph</b> plots the series, the target band, the overs and the playhead, and is also "
        "the scrub surface: it is where you can see the passage you want to hear. The playhead "
        "repaints only the two columns it moved between, because at thirty updates a second across "
        "a full-width plot, repainting the whole graph is visible as a stutter."))

    # -----------------------------------------------------------------------
    f.append(heading("23. Threading model", 0))
    f.extend(data_table(
        ["Thread", "What runs on it", "Rules"],
        [
            ["Message thread", "All interface work; every mutation of batch entries; all callbacks "
                               "delivered from workers.",
             "Never block it. Analysis and playback both hand results back to it."],
            ["Analysis pool", "Single-file analysis (one job).",
             "Produces an outcome, posts it with callAsync. Polls a cancellation flag between blocks."],
            ["Batch pool", "Up to four concurrent file analyses.",
             "Same contract. Never touches shared state directly."],
            ["Audio device", "Pulls from AudioTransportSource.",
             "No allocation, no locks, no file reads. Buffering happens on the read-ahead thread."],
            ["Read-ahead", "Fills the transport's buffer from the file.",
             "Initialises COM for itself so Media Foundation readers work from here."],
        ],
        widths=[26 * mm, 56 * mm, CONTENT_WIDTH - 82 * mm]))

    # -----------------------------------------------------------------------
    f.append(heading("24. Testing", 0))
    total = count_tests()
    f.append(para(
        f"There are {total} tests across two binaries, run by ctest. The split is deliberate."))
    f.extend(data_table(
        ["Binary", "Covers", "Dependencies"],
        [
            ["qc_engine_tests", "K-weighting, loudness and gating, LRA, true peak, sample-domain "
                                "checks, dialogue gating, the verdict engine, JSON, PDF.",
             "None at all. Builds and runs with no JUCE."],
            ["qc_file_tests", "File reading, AAC round trips, batching, playback.",
             "JUCE audio modules; only built when the application is."],
        ],
        widths=[34 * mm, 66 * mm, CONTENT_WIDTH - 100 * mm], mono_cols=(0,)))
    f.append(para(
        "The harness is a hand-rolled 100-line header rather than a package, so the engine suite "
        "builds from a clean clone with no network access and no third-party dependency to justify. "
        "If it outgrows that, Catch2 is the obvious replacement."))
    f.append(para("What is worth understanding is <i>how</i> the measurements are validated:"))
    f.extend(bullets([
        "Loudness tests do not compare against recorded fixtures. They predict what the meter "
        "should read from the filter's transfer function and the BS.1770-4 summation, then assert "
        "the meter agrees within 0.05 dB. That validates the implementation against theory rather "
        "than against a previous run of itself.",
        "The 48 kHz coefficient table is asserted directly, because if the derivation drifts every "
        "measurement at every rate is wrong.",
        "Gating, linearity, channel summation and chunk-size independence each have a test.",
        "The true-peak test uses a sine at fs/4 offset by a quarter cycle: every sample lands at "
        "-3 dBFS while the waveform reaches full scale between samples. Sample peak says -3, true "
        "peak says 0. That is the case the detector exists for.",
        "AAC fixtures are encoded by the test at run time and decoded back.",
        "Playback asserts the position actually advances on a real device, and skips with a printed "
        "note where no output device exists &#8212; a silently skipped test is indistinguishable from "
        "a passing one.",
    ]))

    # -----------------------------------------------------------------------
    f.append(heading("25. Developer tools", 0))
    f.append(para(
        "<b>qc_uishot</b> renders the real MainComponent to a PNG without opening a window. A screenshot "
        "of a running application cannot be taken repeatably or compared across a change; this can, "
        "and every layout defect fixed during the interface work was found with it rather than "
        "guessed at."))
    f.extend(code_block([
        "cmake -S . -B build -DQC_BUILD_APP=ON -DQC_BUILD_UI_SNAPSHOT=ON",
        "cmake --build build --config Release --target qc_uishot",
        "qc_uishot out.png 1280 880 \"D:\\Deliveries\\episode_01.wav\"",
    ]))
    f.append(para(
        "<b>qc_makeicon</b> draws the application icon and emits a preview strip of every size from 16 to "
        "256 on light and dark backgrounds. The icon is generated rather than committed as an opaque "
        "bitmap so it can be adjusted without a graphics editor. The build consumes the committed PNG, "
        "not the tool."))

    # -----------------------------------------------------------------------
    f.append(heading("26. Extending the application", 0))

    f.append(Paragraph("Adding a delivery specification", S["h3"]))
    f.append(para(
        "Edit targets.json. No code change, no rebuild. Set lastVerified once you have checked the "
        "numbers against the published specification."))

    f.append(Paragraph("Adding an input format", S["h3"]))
    f.append(para(
        "Register another juce::AudioFormat in <font face=\"Courier\">getFormatManager()</font> in FileAnalysisJob.cpp. "
        "Analysis, playback and the file chooser all read from that one manager, so a format added "
        "there appears everywhere at once. Add its extensions to the test that asserts the promised "
        "formats are actually openable."))

    f.append(Paragraph("Adding a measurement", S["h3"]))
    f.append(para(
        "Add the field to AnalysisResult, populate it in AudioAnalyser, and it becomes available to "
        "the verdict engine, the JSON writer, the PDF report and the interface at once. If it should "
        "affect a verdict, add a CheckResult in VerdictEngine and decide deliberately whether it "
        "warns or fails &#8212; the rule is that loudness and true peak fail, everything else warns."))

    f.append(Paragraph("Supporting surround", S["h3"]))
    f.append(para(
        "The channel-count guards in LoudnessMeter and FileAnalysisJob are the gate. BS.1770-4 "
        "channel weighting (Ls/Rs at +1.5 dB, LFE excluded) goes in "
        "<font face=\"Courier\">LoudnessMeter::windowedPower</font>, which currently assumes all weights are 1.0 and "
        "says so in a comment. The mono-compatibility check and the downmix would need rethinking."))

    # -----------------------------------------------------------------------
    f.append(heading("27. File reference", 0))
    rows = []
    for rel, lines in survey("Source") + survey("Tools"):
        rows.append([rel, str(lines), FILE_PURPOSE.get(rel, "")])
    f.extend(data_table(
        ["File", "Lines", "Purpose"],
        rows,
        widths=[64 * mm, 14 * mm, CONTENT_WIDTH - 78 * mm],
        aligns={1: "RIGHT"}, mono_cols=(0,)))

    test_rows = [[rel, str(lines), ""] for rel, lines in survey("Tests")]
    f.append(Paragraph("Tests", S["h3"]))
    f.extend(data_table(
        ["File", "Lines", ""],
        test_rows,
        widths=[64 * mm, 14 * mm, CONTENT_WIDTH - 78 * mm],
        aligns={1: "RIGHT"}, mono_cols=(0,)))

    # -----------------------------------------------------------------------
    f.append(heading("28. References", 0))
    f.extend(bullets([
        "ITU-R BS.1770-4 &#8212; Algorithms to measure audio programme loudness and true-peak audio level.",
        "EBU Tech 3341 &#8212; Loudness metering: EBU Mode metering to supplement loudness normalisation.",
        "EBU Tech 3342 &#8212; Loudness range: a measure to supplement loudness normalisation.",
        "EBU R 128 &#8212; Loudness normalisation and permitted maximum level of audio signals.",
        "ATSC A/85 &#8212; Techniques for establishing and maintaining audio loudness for digital television.",
    ]))
    return f
