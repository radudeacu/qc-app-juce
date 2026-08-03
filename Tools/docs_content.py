"""The text of the QC App manual and technical documentation.

Kept apart from the styling so that editing what the document says does not mean
reading past a wall of layout code.
"""

from __future__ import annotations

from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.platypus import PageBreak, Paragraph, Spacer

from docs_style import (
    CONTENT_WIDTH,
    S,
    WARN,
    bullets,
    code_block,
    data_table,
    figure,
    heading,
    note,
    para,
)


# ---------------------------------------------------------------------------
# Part I - user manual
# ---------------------------------------------------------------------------

def part_one():
    f = []
    f.append(Paragraph("Part I", S["part"]))
    f.append(Paragraph("User Manual", S["part_sub"]))

    f.append(heading("1. What this application is", 0))
    f.append(para(
        "QC App measures audio files against broadcast, streaming and podcast delivery "
        "specifications and tells you, per platform, whether the file passes. It is a "
        "measurement and reporting tool: it reads files, it never writes audio, and it never "
        "alters the material you point it at."))
    f.append(para(
        "Every measurement follows ITU-R BS.1770-4 and EBU Tech 3341/3342: integrated, "
        "short-term and momentary loudness, loudness range, and true peak by oversampling. "
        "Alongside those it runs sample-domain checks that a loudness figure alone will never "
        "catch &#8212; clipping, polarity problems, and mono compatibility."))
    f.append(para("<b>What it deliberately does not do:</b>"))
    f.extend(bullets([
        "Correct or render audio. It measures and advises; fixing the file is your job, in your own tools.",
        "Measure a live input. An output device is opened only so you can audition a file.",
        "Handle surround or immersive audio. Mono and stereo only.",
        "Open video containers. Audio files only.",
    ]))

    f.append(heading("2. Installing and running", 0))
    f.append(para(
        "The application is a single executable with no installer and no runtime dependencies "
        "beyond Windows 10 or later. Copy it wherever you like and run it. On first launch it "
        "writes an editable copy of its delivery specifications to your application data folder "
        "(see section 12)."))
    f.append(para(
        "You can also hand it files or a folder on the command line, which is what a shell "
        "association does:"))
    f.extend(code_block([
        '"QC App.exe" "D:\\Deliveries\\episode_01.wav"',
        '"QC App.exe" "D:\\Deliveries\\Series 3"',
    ]))
    f.append(para(
        "A single file opens in detail view. Several files, or a folder, start a batch. This is "
        "exactly the routing a drag-and-drop takes."))

    f.append(heading("3. The interface", 0))
    f.extend(figure("ui-single.jpg", "Figure 1 - detail view after analysing a single file."))
    f.append(para("The window has six regions:"))
    f.extend(data_table(
        ["Region", "What it is for"],
        [
            ["<b>Header bar</b>",
             "Opening files, loading a dialogue stem, exporting, and the subfolder toggle. The "
             "right-hand text shows the source format and any warning about the target definitions."],
            ["<b>Targets column</b>",
             "Every delivery specification loaded from targets.json. Tick the ones you are "
             "delivering to. The selection is remembered between launches."],
            ["<b>Measurement strip</b>", "The headline numbers for the file on screen."],
            ["<b>Verdict list</b>",
             "One block per selected target: a status, the checks behind it, and a fix hint when "
             "something failed."],
            ["<b>Transport</b>", "Play, stop, and the playback position."],
            ["<b>Loudness graph</b>",
             "Loudness over time, the target band, true-peak overs, and the playhead."],
        ],
        widths=[42 * mm, CONTENT_WIDTH - 42 * mm]))

    f.append(heading("4. Analysing a file", 0))
    f.append(para(
        "Drop an audio file anywhere in the window, or use <b>Open file...</b>. Analysis runs on a "
        "background thread and faster than real time; the window stays responsive throughout. "
        "Nothing is written anywhere unless you explicitly export."))
    f.append(para(
        "If a file cannot be decoded, the status line says so and names the extension and the "
        "formats this build supports. Analysis never fails silently."))

    f.append(heading("5. Reading the measurements", 0))
    f.extend(data_table(
        ["Figure", "Meaning"],
        [
            ["<b>Integrated</b>",
             "Loudness of the whole programme, gated per BS.1770-4. This is the number every "
             "delivery specification is written against."],
            ["<b>Short-term max</b>", "Loudest 3-second window."],
            ["<b>Momentary max</b>", "Loudest 400-millisecond window."],
            ["<b>LRA</b>",
             "Loudness range: the spread between the quiet and loud parts of the programme, in LU. "
             "A large value means wide dynamics, which some platforms discourage."],
            ["<b>True peak</b>",
             "The highest level the waveform reaches between samples, found by oversampling. It can "
             "exceed sample peak by a decibel or more, which is why a file that looks safe in a DAW "
             "can still clip a listener's decoder."],
            ["<b>Sample peak</b>", "The highest individual sample value, in dBFS."],
            ["<b>PLR</b>",
             "Peak-to-loudness ratio: true peak minus integrated loudness. A rough measure of how "
             "much dynamic range is left."],
            ["<b>Duration</b>", "Length of the file."],
        ],
        widths=[32 * mm, CONTENT_WIDTH - 32 * mm]))
    f.append(para(
        "A value shown as <b>--</b> was not measured, rather than measured as zero. A file that is "
        "silent, or shorter than one 400 ms gating block, has no integrated loudness at all, and "
        "the application will not invent one."))

    f.append(heading("6. Reading the graph", 0))
    f.append(para("The graph plots the programme against the first target you selected."))
    f.extend(bullets([
        "The <b>white trace</b> is short-term loudness (3-second window).",
        "The <b>teal trace</b> behind it is momentary loudness (400 ms), which moves faster and shows transients.",
        "The <b>green band</b> is the selected target's tolerance, and the line through it is the target itself.",
        "The <b>dashed white line</b> is the file's integrated loudness &#8212; where the whole programme sits.",
        "<b>Red columns</b> mark true-peak overs, drawn the full height of the plot so they can be found against the trace.",
        "The <b>blue playhead</b> follows playback. Click or drag anywhere in the plot to seek there.",
    ]))
    f.append(para(
        "The vertical scale follows the material rather than a fixed window, so a quiet podcast and "
        "a loud master are both legible."))

    f.append(heading("7. Targets and verdicts", 0))
    f.append(para("Each selected target produces one of four results:"))
    f.extend(data_table(
        ["Status", "Meaning"],
        [
            ['<font color="#1f7a45"><b>PASS</b></font>',
             "Loudness is within tolerance and true peak is under the ceiling."],
            ['<font color="#8a6212"><b>WARN</b></font>',
             "Deliverable, but something is worth looking at: clipping, out-of-phase content, poor "
             "mono compatibility, or a loudness range wider than the target's guidance."],
            ['<font color="#b3283a"><b>FAIL</b></font>',
             "Loudness is outside tolerance, or true peak exceeds the ceiling."],
            ["<b>NOT MEASURED</b>",
             "The measurement this target needs was not available &#8212; a silent file, or a "
             "dialogue-gated target with no stem loaded. It is never rendered as a pass."],
        ],
        widths=[34 * mm, CONTENT_WIDTH - 34 * mm]))
    f.extend(note(
        "Loudness and true peak are pass-or-fail only",
        "Warnings are reserved for the sample-domain checks and loudness-range guidance. A green "
        "result therefore always means <i>this is deliverable</i>, never <i>this is deliverable apart "
        "from the bit that is not</i>. In particular, a file whose loudness is within tolerance but "
        "whose true peak is over the ceiling is a <b>FAIL</b>."))
    f.append(para(
        "Some platforms normalise on playback rather than rejecting a delivery. For those, loudness "
        "is reported with the offset from target but is not a failure; only the true-peak ceiling "
        "can fail."))

    f.append(heading("8. Fix hints", 0))
    f.append(para(
        "When a target fails, the application states the change that would fix it. The gain figure "
        "and the resulting true peak are exact, because gain is linear. Only the amount of limiting "
        "is an estimate, and it is worded as one."))
    f.extend(data_table(
        ["Situation", "What you are told"],
        [
            ["Loudness off, headroom available",
             "<i>Apply -2.3 dB gain -&gt; -14.0 LUFS, true peak lands at -1.4 dBTP. Passes.</i>"],
            ["Loudness off, gain would breach the ceiling",
             "<i>Apply +1.8 dB gain -&gt; -14.0 LUFS, but true peak would hit -0.2 dBTP (ceiling -1.0). "
             "Needs limiting, around 0.8 dB of gain reduction.</i>"],
            ["Loudness fine, true peak over",
             "<i>Loudness passes. Reduce true peak by 1.3 dB - limiter ceiling at -1.0 dBTP.</i>"],
            ["Loudness range wide",
             "<i>LRA 21.4 LU is wide for this target - compression or level-riding needed.</i>"],
        ],
        widths=[50 * mm, CONTENT_WIDTH - 50 * mm]))

    f.append(heading("9. Dialogue-gated targets", 0))
    f.append(para(
        "Some specifications &#8212; Netflix among them &#8212; are written against dialogue-gated "
        "loudness, measured only over the intervals where dialogue is present. That is a different "
        "measurement, not a different threshold, so it cannot be answered from the programme figure."))
    f.append(para(
        "Load a dialogue stem with <b>Dialogue stem...</b>. The stem must be the same sample rate and "
        "the same length as the mix; anything else is rejected rather than silently stretched to "
        "fit. The application then measures the mix over the intervals where the stem is active."))
    f.extend(note(
        "This is not Dolby Dialog Intelligence",
        "The result will not agree with a proprietary dialogue detector sample for sample. It is an "
        "honest measurement of the mix over the dialogue intervals your stem defines. Without a "
        "stem, dialogue-gated targets report NOT MEASURED rather than guessing.",
        tint=colors.HexColor("#fdf4e3"), bar=WARN))

    f.append(heading("10. Batch mode", 0))
    f.extend(figure("ui-batch.jpg", "Figure 2 - batch view. The failed file carries its reason on the row."))
    f.append(para(
        "Drop a folder, or several files, to start a batch. Files are analysed in parallel and the "
        "table fills in as they finish. Click any row to see that file in the measurement strip, "
        "the verdict list and the graph."))
    f.extend(bullets([
        "Every column sorts, including the per-target status columns.",
        "A file that cannot be read becomes an <b>ERROR</b> row carrying its reason, and the run "
        "continues. One bad file in a delivery folder never costs you the other ninety.",
        "Non-audio files are skipped silently rather than filling the table with errors.",
        "<b>Include subfolders</b> is off by default, so dropping a project folder does not pull in "
        "every bounce and stem underneath it.",
        "Changing which targets are selected re-judges every row instantly &#8212; no file is read again.",
    ]))

    f.append(heading("11. Playback", 0))
    f.append(para(
        "The file on screen can be auditioned while its report is visible. <b>Play</b> and <b>Stop</b> "
        "control the transport; the readout shows position and length. Click or drag anywhere in the "
        "graph to seek, which is usually faster than scrubbing, because the graph is where you can "
        "see the passage you want to hear."))
    f.append(para(
        "The output device is opened the first time you play something, not at launch. A machine "
        "with no output device analyses normally; the transport is disabled and says why. Playback "
        "never influences a measurement."))

    f.append(heading("12. Delivery specifications", 0))
    f.append(para("Targets are data, not code. On first run the application writes a copy of its defaults to:"))
    f.extend(code_block(["%APPDATA%\\QC App\\targets.json"]))
    f.append(para(
        "That copy is preferred from then on, so an application update never overwrites a correction "
        "you made. Edit it and restart. Each entry looks like this:"))
    f.extend(code_block([
        "{",
        '  "id": "ebu-r128",',
        '  "name": "EBU R128",',
        '  "integratedLufs": -23.0,',
        '  "toleranceLu": 0.5,',
        '  "maxTruePeakDb": -1.0,',
        '  "gating": "program",',
        '  "lastVerified": "2026-08-03"',
        "}",
    ]))
    f.extend(data_table(
        ["Field", "Required", "Meaning"],
        [
            ["id", "yes", "Stable identifier used in exports. Do not change it once reports exist."],
            ["name", "yes", "What appears in the interface."],
            ["integratedLufs", "yes", "Target integrated loudness."],
            ["toleranceLu", "no",
             "Permitted deviation. Omit for platforms that normalise on playback rather than reject a delivery."],
            ["maxTruePeakDb", "yes", "True-peak ceiling in dBTP."],
            ["gating", "no", 'Either "program" (the default) or "dialogue".'],
            ["maxLoudnessRangeLu", "no", "Guidance only. Exceeding it warns, never fails."],
            ["lastVerified", "no", "ISO date. Empty means the numbers have never been checked."],
        ],
        widths=[36 * mm, 18 * mm, CONTENT_WIDTH - 54 * mm], mono_cols=(0,)))
    f.extend(note(
        "Verify the numbers before you rely on a verdict",
        "The shipped values are seeds, and platform specifications drift. Every target whose "
        "lastVerified is empty is flagged in the interface and printed on every PDF page as "
        "<i>spec not verified</i>. A verdict against a specification nobody has checked is worth less "
        "than no verdict at all, so the application says so rather than looking authoritative.",
        tint=colors.HexColor("#fdf4e3"), bar=WARN))
    f.append(para(
        "An entry that cannot be understood is skipped and reported, never silently dropped &#8212; a "
        "target missing from the list would otherwise look like a pass."))

    f.append(heading("13. Exporting", 0))
    f.append(para(
        "<b>JSON</b> writes a complete machine-readable result for the file on screen: source metadata, "
        "every measurement, the sample-domain checks, each verdict with the specification it was "
        "judged against, true-peak overs with timestamps, and the full loudness time series."))
    f.append(para(
        "<b>PDF</b> writes a print-ready QC report, one page per file. In batch mode it covers every "
        "file analysed so far, including those that failed to read &#8212; a report that silently "
        "omitted them would overstate what was checked."))
    f.extend(note(
        "Values that were never measured are written as null",
        "JSON has no representation for infinity. A silent file's loudness is written as null, never "
        "as a number, because a consumer reading -inf as a level would draw exactly the wrong "
        "conclusion. Status tokens in JSON (pass, warn, fail, notMeasured) are deliberately separate "
        "from the labels shown on screen, so renaming something in the interface cannot break a tool "
        "parsing your output."))

    f.append(heading("14. Supported formats", 0))
    f.extend(data_table(
        ["Family", "Extensions", "How it is decoded"],
        [
            ["Uncompressed", ".wav .bwf .aiff .aif", "Native. All bit depths and sample rates."],
            ["Lossless / open", ".flac .ogg", "Native."],
            ["MP3", ".mp3", "Native decoder; no system codec needed."],
            ["AAC family", ".m4a .aac .adts .m4b", "Windows Media Foundation, using codecs already on the system."],
            ["Windows Media", ".wma .wmv .asf .wm", "Windows Media Foundation."],
        ],
        widths=[30 * mm, 40 * mm, CONTENT_WIDTH - 70 * mm], mono_cols=(1,)))
    f.append(para(
        "Video containers are out of scope. Files with more than two channels are rejected, with "
        "their channel count stated."))

    f.append(heading("15. Troubleshooting", 0))
    f.extend(data_table(
        ["What you see", "What it means and what to do"],
        [
            ["<i>Cannot decode .xyz on this machine</i>",
             "The format is not supported by this build; the message lists what is. Convert the file, "
             "or check that the extension matches the actual contents."],
            ["<i>This build measures mono and stereo only</i>",
             "The file has more than two channels. Surround is out of scope; deliver a stereo fold-down "
             "or measure the stems separately."],
            ["<i>File is silent or falls below the -70 LUFS gate</i>",
             "No block in the file survived gating. Either the file really is silent, or it is shorter "
             "than one 400 ms block."],
            ["<i>Dialogue stem required</i>",
             "A dialogue-gated target is selected but no stem is loaded. Load one, or deselect the target."],
            ["<i>Dialogue stem is N s but the mix is M s</i>",
             "They must be the same length and sample rate. Export the stem again from the same session."],
            ["<i>No audio output available</i>",
             "Playback could not open a device. Analysis is unaffected &#8212; the report on screen is valid."],
            ["<i>N target(s) not yet verified</i>",
             "A reminder that targets.json still carries unchecked numbers. See section 12."],
            ["<i>ERROR</i> row in a batch",
             "That file could not be read; the reason is on the row. The rest of the run is unaffected."],
        ],
        widths=[56 * mm, CONTENT_WIDTH - 56 * mm]))

    f.append(PageBreak())
    return f
