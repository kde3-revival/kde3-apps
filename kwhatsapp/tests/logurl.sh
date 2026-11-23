#!/bin/sh
# The "browser" and the "mail program" of the test: what kwhatsapp handed
# to KDE ends up in $KWT_OUT/opened.txt.
echo "$*" >> "${KWT_OUT:-/out}/opened.txt"
