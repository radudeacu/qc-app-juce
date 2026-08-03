"""Styles, flowables and page furniture for the QC App documentation.

Split from the content so that a change to the look of the document and a change to
what it says stay independent of each other.
"""

from __future__ import annotations

import datetime
import html
import os
import subprocess

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import (
    BaseDocTemplate,
    CondPageBreak,
    Frame,
    Image,
    KeepTogether,
    NextPageTemplate,
    PageBreak,
    PageTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
)
from reportlab.platypus.tableofcontents import TableOfContents

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(REPO, "docs")
IMAGES = os.path.join(DOCS, "images")
OUTPUT = os.path.join(DOCS, "QC-App-Manual.pdf")

# Pulled from the application's own palette so the document and the product look
# related rather than merely adjacent.
INK = colors.HexColor("#12141a")
BODY = colors.HexColor("#2b2f38")
MUTED = colors.HexColor("#646c7a")
ACCENT = colors.HexColor("#3a4ba8")
ACCENT_LIGHT = colors.HexColor("#e8ebf7")
RULE = colors.HexColor("#d3d7e0")
CODE_BG = colors.HexColor("#f4f5f8")
PASS = colors.HexColor("#1f7a45")
WARN = colors.HexColor("#8a6212")
FAIL = colors.HexColor("#b3283a")

PAGE_WIDTH, PAGE_HEIGHT = A4
MARGIN = 20 * mm
CONTENT_WIDTH = PAGE_WIDTH - 2 * MARGIN


# ---------------------------------------------------------------------------
# Styles
# ---------------------------------------------------------------------------

def build_styles():
    sheet = getSampleStyleSheet()
    s = {}

    s["title"] = ParagraphStyle("title", parent=sheet["Title"], fontName="Helvetica-Bold",
                                fontSize=30, leading=35, textColor=INK, alignment=TA_CENTER,
                                spaceAfter=6)
    s["subtitle"] = ParagraphStyle("subtitle", parent=sheet["Normal"], fontName="Helvetica",
                                   fontSize=13, leading=18, textColor=MUTED, alignment=TA_CENTER)
    s["part"] = ParagraphStyle("part", parent=sheet["Normal"], fontName="Helvetica-Bold",
                               fontSize=26, leading=30, textColor=ACCENT, spaceBefore=0,
                               spaceAfter=4)
    s["part_sub"] = ParagraphStyle("part_sub", parent=sheet["Normal"], fontName="Helvetica",
                                   fontSize=11.5, leading=17, textColor=MUTED, spaceAfter=18)
    s["h1"] = ParagraphStyle("h1", parent=sheet["Normal"], fontName="Helvetica-Bold",
                             fontSize=17, leading=21, textColor=INK, spaceBefore=18,
                             spaceAfter=8)
    s["h2"] = ParagraphStyle("h2", parent=sheet["Normal"], fontName="Helvetica-Bold",
                             fontSize=12.5, leading=16, textColor=ACCENT, spaceBefore=14,
                             spaceAfter=5)
    s["h3"] = ParagraphStyle("h3", parent=sheet["Normal"], fontName="Helvetica-BoldOblique",
                             fontSize=10.5, leading=14, textColor=BODY, spaceBefore=10,
                             spaceAfter=3)
    s["body"] = ParagraphStyle("body", parent=sheet["Normal"], fontName="Helvetica",
                               fontSize=9.7, leading=14.6, textColor=BODY,
                               alignment=TA_JUSTIFY, spaceAfter=7)
    s["bullet"] = ParagraphStyle("bullet", parent=s["body"], leftIndent=12, bulletIndent=2,
                                 spaceAfter=3.5, alignment=0)
    s["code"] = ParagraphStyle("code", parent=sheet["Normal"], fontName="Courier",
                               fontSize=8.4, leading=11.6, textColor=INK)
    s["cell"] = ParagraphStyle("cell", parent=sheet["Normal"], fontName="Helvetica",
                               fontSize=8.4, leading=11.4, textColor=BODY)
    s["cell_bold"] = ParagraphStyle("cell_bold", parent=s["cell"], fontName="Helvetica-Bold",
                                    textColor=INK)
    s["cell_mono"] = ParagraphStyle("cell_mono", parent=s["cell"], fontName="Courier",
                                    fontSize=8.0)
    s["caption"] = ParagraphStyle("caption", parent=sheet["Normal"], fontName="Helvetica-Oblique",
                                  fontSize=8.5, leading=12, textColor=MUTED,
                                  alignment=TA_CENTER, spaceBefore=4, spaceAfter=10)
    s["note"] = ParagraphStyle("note", parent=s["body"], fontSize=9.3, leading=13.6,
                               textColor=INK, leftIndent=9, rightIndent=9, spaceBefore=3,
                               spaceAfter=3, alignment=0)
    s["toc1"] = ParagraphStyle("toc1", parent=sheet["Normal"], fontName="Helvetica-Bold",
                               fontSize=10.5, leading=17, textColor=INK, spaceBefore=7)
    s["toc2"] = ParagraphStyle("toc2", parent=sheet["Normal"], fontName="Helvetica",
                               fontSize=9.3, leading=14, textColor=BODY, leftIndent=14)
    return s


S = build_styles()


# ---------------------------------------------------------------------------
# Flowable helpers
# ---------------------------------------------------------------------------

_heading_counter = {"seq": 0}


def esc(text: str) -> str:
    return html.escape(text, quote=False)


def para(text: str, style="body"):
    return Paragraph(text, S[style])


def bullets(items):
    return [Paragraph(f"•&nbsp;&nbsp;{t}", S["bullet"]) for t in items]


def heading(text: str, level: int):
    """A heading that also registers itself with the table of contents."""
    _heading_counter["seq"] += 1
    key = f"h{_heading_counter['seq']}"
    style = S["h1"] if level == 0 else S["h2"]
    p = Paragraph(f'<a name="{key}"/>{esc(text)}', style)
    p.toc_level = level
    p.toc_text = text
    p.toc_key = key
    return p


def code_block(lines):
    text = "<br/>".join(esc(line).replace(" ", "&nbsp;") for line in lines)
    table = Table([[Paragraph(text, S["code"])]], colWidths=[CONTENT_WIDTH])
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), CODE_BG),
        ("BOX", (0, 0), (-1, -1), 0.5, RULE),
        ("LEFTPADDING", (0, 0), (-1, -1), 9),
        ("RIGHTPADDING", (0, 0), (-1, -1), 9),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
    ]))
    return [Spacer(1, 3), table, Spacer(1, 9)]


def note(title: str, text: str, tint=ACCENT_LIGHT, bar=ACCENT):
    inner = [Paragraph(f"<b>{esc(title)}</b>", S["note"]), Paragraph(text, S["note"])]
    table = Table([[inner]], colWidths=[CONTENT_WIDTH])
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), tint),
        ("LINEBEFORE", (0, 0), (0, -1), 2.4, bar),
        ("LEFTPADDING", (0, 0), (-1, -1), 10),
        ("RIGHTPADDING", (0, 0), (-1, -1), 10),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
    ]))
    return [Spacer(1, 4), table, Spacer(1, 10)]


def data_table(header, rows, widths, aligns=None, mono_cols=()):
    head = [Paragraph(f"<b>{esc(h)}</b>", S["cell_bold"]) for h in header]
    body = []
    for row in rows:
        cells = []
        for i, value in enumerate(row):
            style = S["cell_mono"] if i in mono_cols else S["cell"]
            cells.append(Paragraph(value, style))
        body.append(cells)

    table = Table([head] + body, colWidths=widths, repeatRows=1, hAlign="LEFT")
    style = [
        ("BACKGROUND", (0, 0), (-1, 0), ACCENT_LIGHT),
        ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACCENT),
        ("LINEBELOW", (0, 1), (-1, -2), 0.3, RULE),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 6),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
    ]
    if aligns:
        for col, alignment in aligns.items():
            style.append(("ALIGN", (col, 0), (col, -1), alignment))
    table.setStyle(TableStyle(style))
    return [Spacer(1, 3), table, Spacer(1, 11)]


def figure(filename: str, caption: str, width=CONTENT_WIDTH):
    path = os.path.join(IMAGES, filename)
    if not os.path.exists(path):
        return [para(f"<i>[missing figure: {esc(filename)}]</i>")]

    from reportlab.lib.utils import ImageReader
    iw, ih = ImageReader(path).getSize()
    height = width * ih / iw
    image = Image(path, width=width, height=height)
    image.hAlign = "CENTER"
    return [Spacer(1, 4), image, Paragraph(esc(caption), S["caption"])]


# ---------------------------------------------------------------------------
# Page furniture
# ---------------------------------------------------------------------------

def page_decoration(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 7.6)
    canvas.setFillColor(MUTED)

    canvas.drawString(MARGIN, PAGE_HEIGHT - MARGIN + 7 * mm, "QC App — Manual and Technical Documentation")
    canvas.setStrokeColor(RULE)
    canvas.setLineWidth(0.4)
    canvas.line(MARGIN, PAGE_HEIGHT - MARGIN + 5 * mm, PAGE_WIDTH - MARGIN, PAGE_HEIGHT - MARGIN + 5 * mm)

    canvas.line(MARGIN, MARGIN - 5 * mm, PAGE_WIDTH - MARGIN, MARGIN - 5 * mm)
    canvas.drawString(MARGIN, MARGIN - 9 * mm, "Measured to ITU-R BS.1770-4 and EBU Tech 3341-3342")
    canvas.drawRightString(PAGE_WIDTH - MARGIN, MARGIN - 9 * mm, str(canvas.getPageNumber()))
    canvas.restoreState()


def cover_decoration(canvas, doc):
    canvas.saveState()
    # Deep enough to contain the wordmark and its subtitle. Sized by eye against a
    # rendered page: light grey type spilling past the band onto white is invisible.
    band = 122 * mm
    canvas.setFillColor(colors.HexColor("#0d1122"))
    canvas.rect(0, PAGE_HEIGHT - band, PAGE_WIDTH, band, stroke=0, fill=1)
    canvas.restoreState()


class DocTemplate(BaseDocTemplate):
    """Adds table-of-contents notification and a distinct cover page template."""

    def __init__(self, filename, **kw):
        super().__init__(filename, **kw)

        frame = Frame(MARGIN, MARGIN, CONTENT_WIDTH, PAGE_HEIGHT - 2 * MARGIN, id="body")
        cover_frame = Frame(MARGIN, MARGIN, CONTENT_WIDTH, PAGE_HEIGHT - 2 * MARGIN, id="cover")

        self.addPageTemplates([
            PageTemplate(id="cover", frames=[cover_frame], onPage=cover_decoration),
            PageTemplate(id="body", frames=[frame], onPage=page_decoration),
        ])

    def afterFlowable(self, flowable):
        if hasattr(flowable, "toc_level"):
            self.notify("TOCEntry", (flowable.toc_level, flowable.toc_text,
                                     self.page, flowable.toc_key))


# ---------------------------------------------------------------------------
# Source tree survey
# ---------------------------------------------------------------------------

FILE_PURPOSE = {
    "Source/Engine/KWeighting.h": "K-weighting filter design and biquad",
    "Source/Engine/KWeighting.cpp": "Per-sample-rate coefficient derivation",
    "Source/Engine/LoudnessMeter.h": "Loudness measurement interface and gating helpers",
    "Source/Engine/LoudnessMeter.cpp": "Sub-block accumulation, gating, LRA",
    "Source/Engine/TruePeakDetector.h": "True-peak interface and envelope type",
    "Source/Engine/TruePeakDetector.cpp": "Polyphase oversampling and over events",
    "Source/Engine/QualityChecks.h": "Sample-domain check interface",
    "Source/Engine/QualityChecks.cpp": "Peak, clipping, correlation",
    "Source/Engine/AnalysisResult.h": "The result struct every layer consumes",
    "Source/Engine/AudioAnalyser.h": "Orchestrator interface, dialogue gating",
    "Source/Engine/AudioAnalyser.cpp": "Meter composition, mono downmix, erosion",
    "Source/Verdict/Target.h": "Delivery specification struct",
    "Source/Verdict/VerdictEngine.h": "Status model and verdict types",
    "Source/Verdict/VerdictEngine.cpp": "Pass/fail rules and fix hints",
    "Source/Report/JsonReport.h": "JSON writer interface",
    "Source/Report/JsonReport.cpp": "Serialisation with null discipline",
    "Source/Report/PdfWriter.h": "Minimal PDF primitive interface",
    "Source/Report/PdfWriter.cpp": "PDF object model, xref, content streams",
    "Source/Report/PdfReport.h": "Report item and options",
    "Source/Report/PdfReport.cpp": "Page layout, graph, decimation",
    "Source/App/Main.cpp": "Application entry, window, look and feel",
    "Source/App/MainComponent.h": "Main view interface",
    "Source/App/MainComponent.cpp": "Layout, wiring, exports, transport",
    "Source/App/FileAnalysisJob.h": "File analysis and collection interface",
    "Source/App/FileAnalysisJob.cpp": "Block streaming, stem validation",
    "Source/App/MediaFoundationAudioFormat.h": "AAC/M4A format interface",
    "Source/App/MediaFoundationAudioFormat.cpp": "IMFSourceReader-backed reader",
    "Source/App/BatchAnalyser.h": "Batch run interface",
    "Source/App/BatchAnalyser.cpp": "Worker pool, message-thread state",
    "Source/App/BatchTable.h": "Batch table interface",
    "Source/App/BatchTable.cpp": "Sortable rows, dynamic target columns",
    "Source/App/PlaybackEngine.h": "Transport interface",
    "Source/App/PlaybackEngine.cpp": "Device, transport, COM read-ahead thread",
    "Source/App/TargetLibrary.h": "Target loading interface",
    "Source/App/TargetLibrary.cpp": "targets.json parsing and user copy",
    "Source/App/LoudnessGraph.h": "Graph interface",
    "Source/App/LoudnessGraph.cpp": "Plot, playhead, seeking",
    "Source/App/VerdictPanel.h": "Verdict list interface",
    "Source/App/VerdictPanel.cpp": "Verdict rows and fix hints",
    "Source/App/GlassStyle.h": "Design tokens and panel primitives",
    "Source/App/GlassStyle.cpp": "Backdrop, panels, pills",
    "Source/App/GlassLookAndFeel.h": "Control styling interface",
    "Source/App/GlassLookAndFeel.cpp": "Buttons, toggles, scrollbars, headers",
    "Tools/UiSnapshot.cpp": "Offscreen interface renderer",
    "Tools/MakeIcon.cpp": "Application icon generator",
}


def survey(directory, extensions=(".cpp", ".h")):
    found = []
    for root, _, files in os.walk(os.path.join(REPO, directory)):
        for name in sorted(files):
            if name.endswith(extensions):
                path = os.path.join(root, name)
                rel = os.path.relpath(path, REPO).replace("\\", "/")
                with open(path, "r", encoding="utf-8", errors="replace") as handle:
                    lines = sum(1 for _ in handle)
                found.append((rel, lines))
    return sorted(found)


def count_tests():
    total = 0
    for rel, _ in survey("Tests"):
        with open(os.path.join(REPO, rel), "r", encoding="utf-8", errors="replace") as handle:
            total += sum(1 for line in handle if line.startswith("QC_TEST"))
    return total


def git_revision():
    try:
        out = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=REPO,
                             capture_output=True, text=True, timeout=15)
        return out.stdout.strip() or "unknown"
    except Exception:
        return "unknown"
