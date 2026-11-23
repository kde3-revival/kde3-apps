#!/bin/bash
#
#    This file is part of kde3-apps, the applications of the KDE3 revival
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
# run-tests.sh KPDFCHECK DIR: regression cases of KPDF's document core on
# the synthetic files of make-test-pdfs.py (written into DIR).  Needs an
# X display (KApplication); prints one line per case, exits 1 if any fails.

K=${1:?kpdfcheck}; D=${2:?directory}
python3 -I "$(dirname "$0")/make-test-pdfs.py" "$D" || exit 2
fails=0
check() { # NAME EXPECTED ACTUAL
  if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; fails=$((fails+1)); fi
}
q() { timeout 60 "$K" "$@" 2>/dev/null; }
strip_marks() { python3 -I -c 'import sys; print("".join(c for c in sys.stdin.read() if c not in "‪‫‬‭‮‎‏").strip())'; }

# search continues after the text of the last match: one hit per ligature
check "ligature: 'f' (two U+FB00)" "hits 2 pages 1" "$(q search $D/ligature.pdf f | tail -1 | cut -d' ' -f1-4)"
check "ligature: 'ff'" "hits 2 pages 1" "$(q search $D/ligature.pdf ff | tail -1 | cut -d' ' -f1-4)"
check "ligature: 'office'" "hits 2 pages 1" "$(q search $D/ligature.pdf office | tail -1 | cut -d' ' -f1-4)"

# U+1F600 is two QChars: the cursor goes before or after both
check "non-BMP: right half of U+1F600" "cursor 3 of 4" "$(q cursor $D/nonbmp.pdf 1 0.545 0.47)"
check "non-BMP: left half of U+1F600" "cursor 1 of 4" "$(q cursor $D/nonbmp.pdf 1 0.528 0.47)"
check "non-BMP: selection a..U+1F600" "a😀" "$(q select $D/nonbmp.pdf 1 0.501 0.47 0.545 0.47 | sed -n 2p)"

# Hebrew drawn right to left: from the right edge of alef to the left edge
# of bet (the end of the run) selects both letters
check "Hebrew: whole run" "אב" "$(q select $D/hebrew.pdf 1 0.549 0.47 0.501 0.47 | sed -n 2p | strip_marks)"
check "Hebrew: left edge of bet ends the run" "ב" "$(q select $D/hebrew.pdf 1 0.524 0.47 0.501 0.47 | sed -n 2p | strip_marks)"

# destinations on rotated pages: the corner where the page was made
out=$(q outline $D/rotate.pdf)
check "/Rotate 0 destination" "Rotate 0 -> page 1 at 0.000,0.000" "$(echo "$out" | grep 'Rotate 0 ')"
check "/Rotate 90 destination" "Rotate 90 -> page 2 at 1.000,0.000" "$(echo "$out" | grep 'Rotate 90 ')"
check "/Rotate 180 destination" "Rotate 180 -> page 3 at 1.000,1.000" "$(echo "$out" | grep 'Rotate 180 ')"
check "/Rotate 270 destination" "Rotate 270 -> page 4 at 0.000,1.000" "$(echo "$out" | grep 'Rotate 270 ')"

# image areas: origin at the top
check "image map" "image 0.100 0.050 0.300 0.200" "$(q images $D/image.pdf 1)"

# cairo and X pixmaps stop at 32767 pixels: a 40000 pixel request is
# refused (no blank "success"); the page view asks for 32000 at most
check "40000 px request refused" "no pixmap" "$(q render $D/wide.pdf 1 40000 - | tail -1)"
r=$(q render $D/wide.pdf 1 32000 -)
check "32000 px render size" "size 32000x800" "$(echo "$r" | grep -o 'size [0-9x]*')"
ink=$(echo "$r" | sed -n 's/non_white_pixels //p')
check "32000 px render not blank" "yes" "$( [ "${ink:-0}" -gt 25000000 ] && echo yes || echo "no ($ink)")"

# a deep outline: bounded, no stack overflow
n=$(q outline $D/deep-outline.pdf | grep -c 'Level')
check "deep outline bounded" "65" "$n"

# the test driver itself: page numbers out of range
check "render page 0" "bad page or width" "$(q render $D/wide.pdf 0 100 -)"

# printing reports a failure to write the output
# (kdeprint shows an error dialog first: Return closes it)
( sleep 8; xdotool key Return ) &
check "print to an unwritable place" "print failed" "$(q print $D/wide.pdf /nonexistent/dir/out.ps 1 1 | grep -o 'print [a-z]*')"
wait

echo "failures: $fails"
[ $fails -eq 0 ]
