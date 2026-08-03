"""Builds docs/QC-App-Manual.pdf.

Run from the repository root:

    python Tools/make_docs.py

Requires reportlab. The screenshots in docs/images are produced by qc_uishot; see
section 25 of the document itself.
"""

from __future__ import annotations

import datetime
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from reportlab.lib.units import mm
from reportlab.platypus import Image, NextPageTemplate, PageBreak, Paragraph, Spacer
from reportlab.platypus.tableofcontents import TableOfContents

from docs_content import part_one
from docs_style import (
    ACCENT,
    CONTENT_WIDTH,
    DOCS,
    IMAGES,
    INK,
    MUTED,
    OUTPUT,
    S,
    DocTemplate,
    count_tests,
    git_revision,
    para,
    survey,
)
from docs_technical import part_two


def cover():
    f = [Spacer(1, 16 * mm)]

    icon = os.path.join(IMAGES, "icon.png")
    if os.path.exists(icon):
        image = Image(icon, width=34 * mm, height=34 * mm)
        image.hAlign = "CENTER"
        f.append(image)

    f.append(Spacer(1, 10 * mm))
    f.append(Paragraph('<font color="#ffffff">QC App</font>', S["title"]))
    f.append(Paragraph(
        '<font color="#aab3c8">Loudness compliance for broadcast, streaming and podcast delivery</font>',
        S["subtitle"]))
    f.append(Spacer(1, 34 * mm))

    f.append(Paragraph("User Manual and Technical Documentation", S["title"]))
    f.append(Spacer(1, 6 * mm))

    source_lines = sum(n for _, n in survey("Source"))
    test_lines = sum(n for _, n in survey("Tests"))

    summary = (
        f"{datetime.date.today().isoformat()} &#183; revision {git_revision()}<br/>"
        f"{source_lines:,} lines of source &#183; {test_lines:,} lines of tests &#183; "
        f"{count_tests()} automated tests"
    )
    f.append(Paragraph(summary, S["subtitle"]))

    f.append(Spacer(1, 58 * mm))
    f.append(Paragraph(
        "Measurement conforms to ITU-R BS.1770-4 and EBU Tech 3341/3342. "
        "The delivery specifications shipped with the application are seed values and must be "
        "verified against each platform's current published specification before a verdict is "
        "relied upon.",
        S["caption"]))

    f.append(NextPageTemplate("body"))
    f.append(PageBreak())
    return f


def contents():
    toc = TableOfContents()
    toc.levelStyles = [S["toc1"], S["toc2"]]

    return [
        Paragraph("Contents", S["h1"]),
        Spacer(1, 4),
        toc,
        PageBreak(),
    ]


def build():
    os.makedirs(DOCS, exist_ok=True)

    doc = DocTemplate(
        OUTPUT,
        title="QC App - User Manual and Technical Documentation",
        author="QC App",
        subject="Loudness compliance analyser: user manual and implementation reference",
    )

    story = cover() + contents() + part_one() + part_two()

    # Two passes: the first discovers the page numbers, the second lays out a table of
    # contents that reflects them.
    doc.multiBuild(story)

    size = os.path.getsize(OUTPUT)
    print(f"wrote {OUTPUT} ({size / 1024:.0f} kB)")


if __name__ == "__main__":
    build()
