#!/usr/bin/env python3
#
#    This file is part of kde3-apps, the applications of the KDE3 revival
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
# make-test-pdfs.py DIR: writes small synthetic PDF files for kpdfcheck
# (see run-tests.sh): text whose ToUnicode gives a ligature, a character
# outside the BMP and Hebrew letters, pages with /Rotate and outline
# destinations, an image near the top of a page, a very wide page.

import sys, os, zlib

def pdf(objects):
    """objects: list of bytes (object bodies, numbered from 1); 1 = catalog"""
    out = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = []
    for i, body in enumerate(objects, 1):
        offsets.append(len(out))
        out += b"%d 0 obj\n" % i + body + b"\nendobj\n"
    xref = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objects) + 1)
    for o in offsets:
        out += b"%010d 00000 n \n" % o
    out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objects) + 1, xref)
    return bytes(out)

def stream(data, extra=b""):
    return b"<< /Length %d %s>>\nstream\n" % (len(data), extra) + data + b"\nendstream"

# a Helvetica with codes 1..4 mapped by ToUnicode to: U+FB00 (ff),
# U+1F600 (outside the BMP), U+05D0 and U+05D1 (Hebrew alef, bet)
TOUNICODE = b"""/CIDInit /ProcSet findresource begin
12 dict begin
begincmap
/CMapName /KPDFTest def
/CMapType 2 def
1 begincodespacerange
<00> <FF>
endcodespacerange
4 beginbfchar
<01> <FB00>
<02> <D83DDE00>
<03> <05D0>
<04> <05D1>
endbfchar
endcmap
CMapName currentdict /CMap defineresource pop
end
end"""

def text_page_pdf(content, width, height):
    widths = b" ".join([b"500"] * 256)
    return pdf([
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] /Contents 4 0 R "
        b"/Resources << /Font << /F1 5 0 R >> >> >>" % (width, height),
        stream(content),
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /FirstChar 0 /LastChar 255 "
        b"/Widths [" + widths + b"] /ToUnicode 6 0 R >>",
        stream(TOUNICODE),
    ])

def main(d):
    os.makedirs(d, exist_ok=True)
    w = lambda name, data: open(os.path.join(d, name), "wb").write(data)

    # 'o<ff>ice' and 'o<ff>ice' again: a ligature, searched as 'f' and 'ff'
    w("ligature.pdf", text_page_pdf(
        b"BT /F1 10 Tf 20 50 Td (o\\001ice) Tj 0 -15 Td (o\\001ice) Tj ET", 200, 100))

    # 'a', U+1F600, 'b' at x 100, 105, 110 (font size 10, widths 500)
    w("nonbmp.pdf", text_page_pdf(
        b"BT /F1 10 Tf 100 50 Td (a\\002b) Tj ET", 200, 100))

    # Hebrew alef bet, drawn right to left: bet at x 100..105, alef at 105..110
    w("hebrew.pdf", text_page_pdf(
        b"BT /F1 10 Tf 100 50 Td (\\004) Tj 5 0 Td (\\003) Tj ET", 200, 100))

    # four 100x200 pages with /Rotate 0, 90, 180, 270; the outline goes
    # to the top left corner of each page as it was made (0, 200)
    objs = [b"<< /Type /Catalog /Pages 2 0 R /Outlines 7 0 R /PageMode /UseOutlines >>",
            b"<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R 6 0 R] /Count 4 >>"]
    for rot in (0, 90, 180, 270):
        objs.append(b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 200] /Rotate %d >>" % rot)
    objs.append(b"<< /Type /Outlines /First 8 0 R /Last 11 0 R /Count 4 >>")
    for i, rot in enumerate((0, 90, 180, 270)):
        prev = b" /Prev %d 0 R" % (7 + i) if i > 0 else b""
        nxt = b" /Next %d 0 R" % (9 + i) if i < 3 else b""
        objs.append(b"<< /Title (Rotate %d) /Parent 7 0 R /Dest [%d 0 R /XYZ 0 200 0]%s%s >>"
                    % (rot, 3 + i, prev, nxt))
    w("rotate.pdf", pdf(objs))

    # a gray image of 20x30 at (10, 160): top left (10, 10) as shown
    img = zlib.compress(bytes([128] * 4))
    w("image.pdf", pdf([
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 200] /Contents 4 0 R "
        b"/Resources << /XObject << /Im1 5 0 R >> >> >>",
        stream(b"q 20 0 0 30 10 160 cm /Im1 Do Q"),
        stream(img, b"/Type /XObject /Subtype /Image /Width 2 /Height 2 /ColorSpace /DeviceGray "
                    b"/BitsPerComponent 8 /Filter /FlateDecode "),
    ]))

    # a 400x10 page, black: rendered 40000 pixels wide
    w("wide.pdf", pdf([
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 10] /Contents 4 0 R >>",
        stream(b"0 0 0 rg 0 0 400 10 re f"),
    ]))

    # an outline nested 5000 levels deep
    n = 5000
    objs = [b"<< /Type /Catalog /Pages 2 0 R /Outlines 4 0 R >>",
            b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
            b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] >>",
            b"<< /Type /Outlines /First 5 0 R /Last 5 0 R /Count %d >>" % n]
    for i in range(n):
        me = 5 + i
        parent = 4 if i == 0 else me - 1
        kids = b" /First %d 0 R /Last %d 0 R /Count 1" % (me + 1, me + 1) if i < n - 1 else b""
        objs.append(b"<< /Title (Level %d) /Parent %d 0 R /Dest [3 0 R /Fit]%s >>" % (i, parent, kids))
    w("deep-outline.pdf", pdf(objs))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else ".")
