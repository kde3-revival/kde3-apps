#!/bin/bash
# inner.sh: kwhatsapp's test inside the container (see run.sh): its own X
# server (Xvfb), KDE (kdeinit, kwin with two desktops, kicker with the
# system tray), the test pages of server.py; kwhatsapp from /build/inst.
# Every step writes "ok STEP" or "FAIL STEP: why" to /out/summary.txt;
# screenshots and logs stay in /out.
#   PHASES  what to run (default "local"): local (the test page),
#           whatsapp (https://web.whatsapp.com: its login page, memory)
set -u
T=/src/apps/kwhatsapp/tests
O=/out
mkdir -p $O
SUMMARY=$O/summary.txt
: > $SUMMARY
export HOME=/tmp/home KDEHOME=/tmp/home/.kde
rm -rf $HOME; mkdir -p $KDEHOME/share/config $HOME/dl
export KDEDIRS=/build/inst/opt/kde3:/opt/kde3
export PATH=/build/inst/opt/kde3/bin:/opt/kde3/bin:/opt/kde3-qt/bin:/usr/bin:/bin
export DISPLAY=:55 LANG=C.UTF-8 LC_ALL=C.UTF-8
export XDG_RUNTIME_DIR=/tmp/xdg; mkdir -p -m 700 $XDG_RUNTIME_DIR
export KWT_OUT=$O
export KWHATSAPP_DEBUG=1   # the balloon's text on stderr (kwhatsapp-RUN.log)

ok() { echo "ok $1" | tee -a $SUMMARY; }
fail() { echo "FAIL $1: $2" | tee -a $SUMMARY; }
shot() { import -window root "$O/$1.png" 2> /dev/null; }
# waitfor FILE PATTERN SECONDS: the pattern (grep -E) shows up in the file
waitfor() {
  local i=0
  while [ $i -lt $(( $3 * 4 )) ]; do
    grep -qE "$2" "$1" 2> /dev/null && return 0
    sleep 0.25; i=$((i + 1))
  done
  return 1
}
check() {   # check STEP FILE PATTERN SECONDS
  if waitfor "$2" "$3" "$4"; then ok "$1"; else fail "$1" "no /$3/ in $(basename $2)"; fi
}
window() { xdotool search --onlyvisible --name "$1" 2> /dev/null | head -1; }
# click on the page at X Y (page coordinates: the menu bar is off)
click() { xdotool mousemove --window "$W" "$1" "$2" sleep 0.2 click 1; sleep 0.5; }
# answer a KDE file dialog: the path into its location field, Return
filedialog() {
  local d i=0
  while [ $i -lt 40 ]; do
    d=$(xdotool search --onlyvisible --name "$1" 2> /dev/null | head -1)
    [ -n "$d" ] && break
    sleep 0.25; i=$((i + 1))
  done
  if [ -z "$d" ]; then return 1; fi
  sleep 1
  xdotool windowactivate --sync "$d" 2> /dev/null
  xdotool key --clearmodifiers ctrl+a BackSpace
  xdotool type --delay 20 "$2"
  sleep 0.3
  xdotool key Return
  sleep 1
  return 0
}
# Kopete's balloon (kwhatsappballoon.cpp): its caption is "New message"
balloon() { xdotool search --onlyvisible --name '^New message$' 2> /dev/null | head -1; }
wait_balloon() {   # wait_balloon SECONDS: B and its geometry BX BY BW BH
  local i=0
  B=
  while [ $i -lt $(( $1 * 4 )) ]; do B=$(balloon); [ -n "$B" ] && break; sleep 0.25; i=$((i + 1)); done
  [ -n "$B" ] || return 1
  sleep 0.7
  eval "$(xdotool getwindowgeometry --shell $B 2> /dev/null)"
  BX=$X; BY=$Y; BW=$WIDTH; BH=$HEIGHT
}
wait_no_balloon() {
  local i=0
  while [ $i -lt $(( $1 * 4 )) ]; do [ -z "$(balloon)" ] && return 0; sleep 0.25; i=$((i + 1)); done
  return 1
}
balloon_shot() { import -window root -crop ${BW}x${BH}+${BX}+${BY} "$O/$1.png" 2> /dev/null; }
# the balloon's buttons: View left of its centre, Ignore right of it
balloon_view() { xdotool mousemove $(( BX + BW / 2 - 40 )) $(( BY + BH - 36 )) sleep 0.2 click 1; }
balloon_ignore() { xdotool mousemove $(( BX + BW / 2 + 40 )) $(( BY + BH - 36 )) sleep 0.2 click 1; }
pixels() {   # pixels FILE COLOUR FUZZ: how many pixels of about that colour
  convert "$1" -alpha off -fuzz "$3" -fill black +opaque "$2" -fill white -opaque "$2" \
          -format '%[fx:round(mean*w*h)]' info: 2> /dev/null || echo 0
}
away() { xdotool windowminimize "$W"; sleep 0.5; }   # the window not in front
front() {   # the window in front, where the clicks expect it
  xdotool windowactivate --sync "$W" 2> /dev/null; sleep 1
  xwininfo -id "$W" | grep -E 'Width|Height' | tr -d ' ' | tr '\n' ' ' >> $O/geometry.txt; echo >> $O/geometry.txt
  xdotool windowmove "$W" 0 0 windowsize "$W" 1240 940; sleep 0.5
}
pids_of_app() {   # kwhatsapp and its WebKit processes
  local p=$(pgrep -x kwhatsapp | head -1)
  [ -n "$p" ] || return
  echo $p $(pgrep -P $p) $(for c in $(pgrep -P $p); do pgrep -P $c; done)
}
memory() {   # memory LABEL: RSS and PSS of kwhatsapp and its WebKit processes
  local rss=0 pss=0 line= r s
  for p in $(pids_of_app); do
    [ -r /proc/$p/smaps_rollup ] || continue
    r=$(awk '/^Rss:/ {print $2}' /proc/$p/smaps_rollup); s=$(awk '/^Pss:/ {print $2}' /proc/$p/smaps_rollup)
    line="$line $(cat /proc/$p/comm)=${r}k/${s}k"
    rss=$((rss + r)); pss=$((pss + s))
  done
  echo "memory $1: processes:$line; total RSS $((rss / 1024)) MiB, PSS $((pss / 1024)) MiB" | tee -a $O/memory.txt
}
start_app() {   # start_app ARGS...: kwhatsapp, its window at 0,0 1240x940 in $W
  kwhatsapp --nofork "$@" > $O/kwhatsapp-$RUN.log 2>&1 &
  APP=$!
  W=
  local i=0
  while [ -z "$W" ] && [ $i -lt 60 ]; do sleep 0.5; W=$(window 'WhatsApp'); i=$((i + 1)); done
  if [ -z "$W" ]; then fail "start-$RUN" "no window"; return 1; fi
  xdotool windowmove "$W" 0 0 windowsize "$W" 1240 940
  sleep 1
}
quit_app() {
  dcop kwhatsapp MainApplication-Interface quit > /dev/null 2>&1
  local i=0
  while kill -0 $APP 2> /dev/null && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
  kill -0 $APP 2> /dev/null && { fail "quit-$RUN" "still running"; kill $APP; }
  sleep 1
}

# --- the desktop -------------------------------------------------------
Xvfb :55 -screen 0 1600x1000x24 -nolisten tcp > $O/xvfb.log 2>&1 &
sleep 2
cat > $KDEHOME/share/config/kwinrc <<'EOC'
[Desktops]
Number=2
EOC
# the "browser" and the "mail program": logurl.sh
cat > $KDEHOME/share/config/kfmclientrc <<EOC
[Settings]
ExternalBrowser=$T/logurl.sh
EOC
cat > $KDEHOME/share/config/emaildefaults <<EOC
[Defaults]
Profile=Default

[PROFILE_Default]
EmailClient=$T/logurl.sh mailer %t
EOC
# knotify: the popup and a log file (no sound in a container)
cat > $KDEHOME/share/config/kwhatsapp.eventsrc <<EOC
[message]
presentation=20
logfile=$O/knotify.log
EOC
# no tips, no menu bar (the page starts at the window's origin)
cat > $KDEHOME/share/config/kwhatsapprc <<'EOC'
[MainWindow]
MenuBar=Disabled

[General]
PictureOrigins=http://127.0.0.1:8081
EOC
kdeinit +kded > $O/kdeinit.log 2>&1
sleep 3
kbuildsycoca > /dev/null 2>&1
kwin > $O/kwin.log 2>&1 &
sleep 2
kicker > $O/kicker.log 2>&1 &
sleep 5

python3 -I $T/server.py $O/page.log 8080 8081 8082 2> $O/server.log &
sleep 1

# Kopete's balloon at the tray icon (TX TY: the icon's centre, TRAYTOP)
balloon_tests() {
  # a data: picture; the tail at the icon; a click on the balloon: the chat
  front; click 130 675
  H0=$(xwininfo -id "$W" | awk '/Height:/ {print $2}')
  away
  if ! wait_balloon 10; then fail balloon-shown "no balloon"; shot 12-no-balloon; return; fi
  ok balloon-shown
  shot 12-balloon; balloon_shot 12-balloon-crop
  BW1=$BW
  n=$(pixels $O/12-balloon-crop.png '#ff0000' 8%)
  [ "${n:-0}" -gt 300 ] && ok balloon-picture-data-url || fail balloon-picture-data-url "$n red pixels"
  tipx=$(( BX + BW - 1 )); tipy=$(( BY + BH - 1 ))
  if [ $(( tipx - TX )) -ge -2 ] && [ $(( tipx - TX )) -le 2 ] && [ $(( tipy - TRAYTOP )) -ge -2 ] && [ $(( tipy - TRAYTOP )) -le 2 ]; then
    ok balloon-at-tray-icon
  else fail balloon-at-tray-icon "tip $tipx,$tipy icon centre $TX top $TRAYTOP"; fi
  xdotool mousemove $(( BX + 46 )) $(( BY + 46 )) sleep 0.2 click 1
  check balloon-click-opens-chat $O/page.log '^notification click t2' 10
  sleep 1; shot 13-after-view
  # shown again as it was (its size: not un-maximized by showNormal())
  h=$(xwininfo -id "$W" | awk '/Height:/ {print $2}')
  [ "$h" = "$H0" ] && ok balloon-view-keeps-size || fail balloon-view-keeps-size "height $h, was $H0"
  cur=$(xdotool get_desktop); wd=$(xdotool get_desktop_for_window "$W" 2> /dev/null)
  xwininfo -id "$W" | grep -q IsViewable && [ "$cur" = "$wd" ] && ok balloon-click-shows-window \
    || fail balloon-click-shows-window "map=$(xwininfo -id $W | grep 'Map State') desktop=$cur window=$wd"
  wait_no_balloon 3 && ok balloon-gone-after-view || fail balloon-gone-after-view "still shown"

  # a picture of the page's own origin; Ignore: closed, no click
  front; click 370 675; away
  if wait_balloon 10; then
    balloon_shot 13-balloon-own-origin-crop
    n=$(pixels $O/13-balloon-own-origin-crop.png '#ff0000' 8%)
    [ "${n:-0}" -gt 300 ] && ok balloon-picture-own-origin || fail balloon-picture-own-origin "$n red pixels"
    balloon_ignore
    wait_no_balloon 3 && ok balloon-ignore-closes || fail balloon-ignore-closes "still shown"
    sleep 2
    grep -q '^notification click t3' $O/page.log && fail balloon-ignore-no-click "clicked" || ok balloon-ignore-no-click
    xwininfo -id "$W" | grep -q IsViewable && fail balloon-ignore-window-away "shown" || ok balloon-ignore-window-away
  else fail balloon-picture-own-origin "no balloon"; fi

  # a picture of another site: not shown, kwhatsapp's icon instead
  front; click 610 675; away
  if wait_balloon 10; then
    balloon_shot 14-balloon-foreign-icon
    n=$(pixels $O/14-balloon-foreign-icon.png '#ff0000' 8%)
    g=$(pixels $O/14-balloon-foreign-icon.png '#3cc24e' 15%)
    [ "${n:-0}" -lt 20 ] && [ "${g:-0}" -gt 200 ] && ok balloon-foreign-picture-not-shown \
      || fail balloon-foreign-picture-not-shown "$n red, $g green pixels"
    # (WebKit's web process loads a notification's icon itself, as any
    # browser does; nothing else may: kwhatsapp, KIO)
    if grep 'avatar fetched by' $O/server.log | grep -qv 'AppleWebKit'; then
      fail picture-fetched-by-webkit-only "$(grep 'avatar fetched' $O/server.log)"
    else ok picture-fetched-by-webkit-only; fi
    balloon_ignore; wait_no_balloon 3
  else fail balloon-foreign-picture-not-shown "no balloon"; fi

  # two at once: one balloon after the other; View: the window, that chat
  front; click 850 675; away
  if wait_balloon 10; then
    balloon_shot 15-balloon-queue-1
    balloon_ignore
    sleep 1
    if wait_balloon 5; then
      ok balloon-queue-next
      balloon_shot 16-balloon-queue-2
      balloon_view
      check balloon-view-opens-chat $O/page.log '^notification click q2' 10
      grep -q '^notification click q1' $O/page.log && fail balloon-queue-ignored-first "q1 clicked" || ok balloon-queue-ignored-first
      sleep 1
      xwininfo -id "$W" | grep -q IsViewable && ok balloon-view-shows-window || fail balloon-view-shows-window "not shown"
    else fail balloon-queue-next "no second balloon"; fi
  else fail balloon-queue-next "no balloon"; fi

  # the page closes its notification: the balloon goes
  front; click 1110 675; away
  if wait_balloon 10; then
    check page-closed-notification $O/page.log '^page closed t5' 10
    wait_no_balloon 3 && ok balloon-closed-by-page || fail balloon-closed-by-page "still shown"
  else fail balloon-closed-by-page "no balloon"; fi
}

# kwhatsapp's log from line $MARK on (mark: the end of it now)
mark() { MARK=$(( $(wc -l < $O/kwhatsapp-$RUN.log) + 1 )); }
since() { tail -n +$MARK $O/kwhatsapp-$RUN.log; }
# balloon_line N: the Nth balloon's text since the mark (KWHATSAPP_DEBUG)
balloon_line() { since | grep '^kwhatsapp: balloon [0-9]*:' | sed -n "${1}p"; }

# The notifications' pictures (kde3-wpepart's bridge), the balloon's queue
bridge_tests() {
  # a picture of known pixels (noise: an 8,7k characters data: URL):
  # exactly these in the balloon
  python3 -c 'import sys
s, out = 2463534242, bytearray()
for i in range(40 * 40):
    s ^= (s << 13) & 0xffffffff; s ^= s >> 17; s ^= (s << 5) & 0xffffffff
    out += bytes((s & 255, (s >> 8) & 255, (s >> 16) & 255))
sys.stdout.buffer.write(b"P6 40 40 255\n" + bytes(out))' > $O/known.ppm
  convert $O/known.ppm $O/known.png
  known_pixels() {   # known_pixels STEP SHOT
    balloon_shot $2
    convert $O/$2.png +repage $O/$2-r.png   # (import keeps the crop's offset)
    local r=$(compare -metric AE -subimage-search $O/$2-r.png $O/known.png null: 2>&1)
    case "$r" in "0 "*|0) ok "$1" ;; *) fail "$1" "compare: $r" ;; esac
  }
  front; click 130 775; away
  n=$(sed -n 's/^pixels url //p' $O/page.log | tail -1)
  [ "${n:-0}" -gt 4096 ] && ok picture-url-long || fail picture-url-long "${n:-none} characters"
  if wait_balloon 10; then
    known_pixels balloon-picture-known-pixels 18-balloon-known-pixels
    balloon_ignore; wait_no_balloon 3
  else fail balloon-picture-known-pixels "no balloon"; fi

  # announcements flooding a notification that waits for its picture (5
  # seconds away), from 2 seconds after the click for 8 seconds; the
  # notification 2.5 seconds after it: it still waits 2 seconds at most,
  # its balloon comes before the flood is over
  front; click 610 775; away
  if wait_balloon 7; then
    if grep -q '^flood over' $O/page.log; then fail flood-does-not-hold-back "after the flood"
    else ok flood-does-not-hold-back; fi
    check flood-over $O/page.log '^flood over' 15
    wait_balloon 5 && { balloon_ignore; wait_no_balloon 3; }
  else fail flood-does-not-hold-back "no balloon 7 s after the click"; check flood-over $O/page.log '^flood over' 15; fi

  # 30 at once: 20 in the queue, the oldest waiting dropped
  front; mark; click 850 775; away
  if wait_balloon 10; then
    sleep 2
    n=$(since | grep -c 'balloon queue full')
    [ "$n" = 10 ] && ok balloon-queue-bounded || fail balloon-queue-bounded "$n dropped (10 expected)"
    balloon_line 1 | grep -q 'Over 1[^0-9]' && ok balloon-queue-first || fail balloon-queue-first "$(balloon_line 1)"
    balloon_ignore; sleep 1
    if wait_balloon 5; then
      balloon_line 2 | grep -q 'Over 12[^0-9]' && ok balloon-queue-oldest-dropped || fail balloon-queue-oldest-dropped "$(balloon_line 2)"
    else fail balloon-queue-oldest-dropped "no second balloon"; fi
  else fail balloon-queue-bounded "no balloon"; fi
  front; sleep 1

  # the constructor's arguments read once, the same given to WebKit; the
  # wrapper looks like the constructor; no handler in the page's world
  click 130 855
  check constructor-probe $O/page.log '^probe reads title=1 body=1 tag=1 icon=1 constructed=Getter T1/body1/probe1 ctor=true instanceof=true name=Notification length=1 permission=granted handler=absent' 10
  check constructor-needs-new $O/page.log '^probe call without new: TypeError' 5
  check constructor-unsupported-not-read $O/page.log '^probe actions ignored' 5
  check constructor-invalid-dir-stops $O/page.log '^probe dir TypeError later reads=0$' 5

  # the sender's override cannot reach the balloon's text; the cut is
  # not inside the emoji (a surrogate pair)
  front; mark; click 370 855; away
  if wait_balloon 10; then
    balloon_shot 19-balloon-unicode
    balloon_line 1 > $O/balloon-unicode.txt
    if python3 -I -c 'import sys
t = open(sys.argv[1], "rb").read().decode("utf-8")
assert "‮" not in t, "override kept"
assert "‪Mallory evil name‬‎" in t, "sender not isolated"
assert "‪" + "a" * 29 + " ...‬‎" in t, "body cut wrong"
assert "�" not in t, "a broken character"' $O/balloon-unicode.txt 2> $O/balloon-unicode.err; then
      ok balloon-unicode-cut-isolated
    else fail balloon-unicode-cut-isolated "$(tail -1 $O/balloon-unicode.err)"; fi
    balloon_ignore; sleep 1
    # a ZWJ family at the sender's cut, a flag at the text's: whole or not at all
    if wait_balloon 5; then
      balloon_line 2 > $O/balloon-grapheme.txt
      if python3 -I -c 'import sys
t = open(sys.argv[1], "rb").read().decode("utf-8")
assert "‪Family " + "b" * 31 + " ...‬" in t, "sender cut wrong"
assert "‪" + "a" * 28 + " ...‬" in t, "text cut wrong"
assert "🇷" not in t and "👨" not in t, "part of a cluster"' $O/balloon-grapheme.txt 2> $O/balloon-grapheme.err; then
        ok balloon-grapheme-cut
      else fail balloon-grapheme-cut "$(tail -1 $O/balloon-grapheme.err)"; fi
      balloon_ignore; sleep 1
      if wait_balloon 5; then
        balloon_line 3 > $O/balloon-conjunct.txt
        if python3 -I -c 'import sys
t = open(sys.argv[1], "rb").read().decode("utf-8")
assert "Conjunct" in t, "not the third"
assert "\u202a" + "a" * 28 + " ...\u202c" in t, "conjunct cut"' $O/balloon-conjunct.txt 2> $O/balloon-conjunct.err; then
          ok balloon-conjunct-cut
        else fail balloon-conjunct-cut "$(tail -1 $O/balloon-conjunct.err)"; fi
        balloon_ignore; wait_no_balloon 3
      else fail balloon-conjunct-cut "no third balloon"; fi
    else fail balloon-grapheme-cut "no second balloon"; fi
  else fail balloon-unicode-cut-isolated "no balloon"; fi

  # a construction that failed (DataCloneError) left no announcement:
  # the next notification of the same title, body and tag has no picture
  front; click 610 855; away
  check construction-failed $O/page.log '^clone DataCloneError' 5
  if wait_balloon 10; then
    balloon_shot 21-balloon-after-failed
    n=$(pixels $O/21-balloon-after-failed.png '#ff0000' 8%)
    [ "${n:-0}" -lt 20 ] && ok failed-construction-no-picture || fail failed-construction-no-picture "$n red pixels"
    balloon_ignore; wait_no_balloon 3
  else fail failed-construction-no-picture "no balloon"; fi

  # two notifications alike, the first with an icon that takes a second:
  # the one without an icon never shows the other's picture
  front; mark; click 1110 855; away
  check twin-notified $O/page.log '^twin notified' 10
  if wait_balloon 10; then
    balloon_ignore; sleep 1; wait_balloon 5; balloon_ignore; wait_no_balloon 3
    first=$(grep -m1 '^twin shown' $O/page.log | sed 's/^twin shown //')
    ids=$(since | sed -n 's/^kwhatsapp: balloon \([0-9]*\): .*Twin.*/\1/p' | tr '\n' ' ')
    set -- $ids
    if [ $# != 2 ]; then fail twin-no-wrong-picture "balloons: $ids"
    else
      plain=$2; [ "$first" = plain ] && plain=$1
      if since | grep -q "^kwhatsapp: balloon picture $plain\$"; then fail twin-no-wrong-picture "the one without an icon has a picture (shown first: $first)"
      else ok twin-no-wrong-picture; fi
    fi
  else fail twin-no-wrong-picture "no balloon"; fi

  # pictures of an origin the application lists: with CORS shown; the
  # foreign one before (the same origin, no CORS) failed; one of an
  # origin not listed refused.  The debug lines: scheme and host only.
  front; click 850 360; away
  if wait_balloon 10; then
    balloon_shot 22-balloon-cors
    n=$(pixels $O/22-balloon-cors.png '#ff0000' 8%)
    [ "${n:-0}" -gt 300 ] && ok balloon-picture-cors-origin || fail balloon-picture-cors-origin "$n red pixels"
    balloon_ignore; wait_no_balloon 3
  else fail balloon-picture-cors-origin "no balloon"; fi
  check picture-without-cors-reported $O/kwhatsapp-$RUN.log "^kwpe: app: a notification's picture failed: http://127.0.0.1:8081\$" 2
  front; click 850 410; away
  check picture-unlisted-reported $O/kwhatsapp-$RUN.log "^kwpe: app: a notification's picture refused: http://127.0.0.1:8082\$" 10
  wait_balloon 10 && { balloon_ignore; wait_no_balloon 3; }
  # a picture of the listed origin that redirects to one not listed (both
  # allow CORS): no redirect is followed, no picture, the listed origin
  # reported
  front; mark; click 850 460; away
  if wait_balloon 10; then
    balloon_shot 23-balloon-redirected
    n=$(pixels $O/23-balloon-redirected.png '#ff0000' 8%)
    g=$(pixels $O/23-balloon-redirected.png '#3cc24e' 15%)
    [ "${n:-0}" -lt 20 ] && [ "${g:-0}" -gt 200 ] && ok redirected-picture-not-shown \
      || fail redirected-picture-not-shown "$n red, $g green pixels"
    balloon_ignore; wait_no_balloon 3
  else fail redirected-picture-not-shown "no balloon"; fi
  if since | grep -q "^kwpe: app: a notification's picture failed: http://127.0.0.1:8081\$"; then
    ok redirected-picture-reported
  else fail redirected-picture-reported "$(since | grep "a notification's picture")"; fi
  if grep "a notification's picture" $O/kwhatsapp-$RUN.log | grep -qE 'avatar|secret|\?|Picture'; then
    fail picture-report-host-only "$(grep "a notification's picture" $O/kwhatsapp-$RUN.log | head -3)"
  else ok picture-report-host-only; fi

  # a burst drops an announcement: the notification alike it does not
  # take another one's picture (both pictures come within the bridge's
  # timeout: B's would be there to take)
  front; mark; click 850 580
  check burst-notified $O/page.log '^burst notified' 10
  sleep 6
  n=$(since | grep -c '^kwhatsapp: notification [0-9]*\( with a picture\)*: Burst$')
  if [ "$n" != 2 ]; then fail burst-no-wrong-picture "$n of 2 shown"
  elif since | grep -q '^kwhatsapp: notification [0-9]* with a picture: Burst$'; then
    fail burst-no-wrong-picture "$(since | grep 'with a picture: Burst')"
  else ok burst-no-wrong-picture; fi

  # last (the page stays tampered): everything replaced, still the right
  # picture
  front; click 370 775; away
  if wait_balloon 10; then
    known_pixels balloon-picture-tampered-page 20-balloon-tampered
    balloon_ignore; wait_no_balloon 3
  else fail balloon-picture-tampered-page "no balloon"; fi

  # after it: a burst of 86 announcements drops more than the part
  # remembers (no picture for the rest of the document); neither of the
  # two alike gets a picture
  front; mark; click 850 510
  check big-burst-notified $O/page.log '^big burst notified' 10
  sleep 6
  n=$(since | grep -c '^kwhatsapp: notification [0-9]*\( with a picture\)*: Big Burst$')
  if [ "$n" != 2 ]; then fail big-burst-no-wrong-picture "$n of 2 shown"
  elif since | grep -q '^kwhatsapp: notification [0-9]* with a picture: Big Burst$'; then
    fail big-burst-no-wrong-picture "$(since | grep 'with a picture: Big Burst')"
  else ok big-burst-no-wrong-picture; fi

  # 106: the part keeps 100; the first one (in the balloon) dropped by it:
  # the balloon goes, the next one's click works (after the pictures: its
  # 86 dropped announcements end them for the document too)
  front; sleep 1; mark; click 1110 775; away
  if wait_balloon 10; then
    balloon_line 1 | grep -q 'Evict 0[^0-9]' && ok evict-first-shown || fail evict-first-shown "$(balloon_line 1)"
    check evict-notified $O/page.log '^notified 106' 15
    check evict-closed-in-webkit $O/page.log '^notification closed ev0$' 10
    sleep 3
    next=$(balloon_line 2 | sed -n 's/.*Evict \([0-9]*\).*/\1/p')
    # (the part keeps Evict 6 to 105; the queue the newest)
    if [ -n "$next" ] && [ "$next" -ge 6 ]; then ok evict-retires-balloon
    else fail evict-retires-balloon "next: $(balloon_line 2)"; fi
    if [ -n "$next" ] && wait_balloon 5; then
      balloon_view
      check evict-next-click $O/page.log "^notification click ev$next\$" 10
    else fail evict-next-click "no balloon"; fi
  else fail evict-first-shown "no balloon"; fi
  front; sleep 1
}

phase_local() {
  RUN=1
  start_app --url http://localhost:8080/app.html || return
  check load-1 $O/page.log '^load visit=1 ' 20
  grep '^load visit=1' $O/page.log | grep -q 'permission=default' && ok permission-default-at-start \
    || fail permission-default-at-start "$(grep '^load visit=1' $O/page.log)"
  shot 01-start
  memory "test page, after loading"
  # the bridge's message handler: not in the page's world, of the top or
  # of a frame of another site; that frame cannot reach the top document
  check handler-not-in-page $O/page.log '^top kwpe handler absent' 5
  check handler-not-in-frame $O/page.log '^frame kwpe handler absent' 5
  check frame-cannot-announce $O/page.log '^frame cannot reach the top document' 5

  # notifications: the page asks, kwhatsapp allows, the answer is kept
  click 130 55
  check permission-granted $O/page.log '^permission granted' 10
  grep -q 'Allowed=http://localhost:8080' $KDEHOME/share/apps/kwhatsapp/webkit/kwpepart-permissions 2> /dev/null \
    && ok permission-stored || fail permission-stored "$(cat $KDEHOME/share/apps/kwhatsapp/webkit/kwpepart-permissions 2>&1)"
  click 370 55
  check notification-shown $O/page.log '^notification shown' 10
  check knotify-content $O/knotify.log 'Alice &lt;Test&gt;.*Hello &lt;b&gt;bold&lt;/b&gt; &amp; co<br>second line' 10
  sleep 0.5; shot 02-notification
  # the window is in front: KNotify's log, no balloon
  [ -z "$(balloon)" ] && ok balloon-not-when-in-front || fail balloon-not-when-in-front "a balloon"

  # unread chats: the title, the tray icon
  click 610 55
  check title-unread $O/page.log '^title \(3\) WhatsApp' 5
  sleep 1
  xprop -id "$W" WM_NAME | grep -q '(3) WhatsApp' && ok caption-unread || fail caption-unread "$(xprop -id $W WM_NAME)"
  shot 03-unread

  # links: another site and target=_blank to the browser, the same site in the app
  : > $O/opened.txt
  click 130 155
  check link-blank-to-browser $O/opened.txt 'https://example.org/blank-target' 15
  click 370 155
  check link-other-site-to-browser $O/opened.txt 'http://127.0.0.1:8081/other.html' 15
  sleep 2
  grep -q 'other site loaded IN THE APP' $O/page.log && fail other-site-not-in-app "loaded in the app" || ok other-site-not-in-app
  click 850 155
  check mailto-to-mailer $O/opened.txt 'mailer.*someone@example.org' 15
  click 610 155
  check same-site-in-app $O/page.log '^page2 loaded http://localhost:8080/page2.html' 10
  click 610 155   # page2's link back
  check back-in-app $O/page.log '^load visit=2 ' 10

  # downloads: KDE's save dialog
  click 130 255
  if filedialog '^Save As' $HOME/dl/report.bin; then
    sleep 2
    [ "$(stat -c %s $HOME/dl/report.bin 2> /dev/null)" = 100000 ] && ok download-attachment \
      || fail download-attachment "$(ls -l $HOME/dl)"
  else
    fail download-attachment "no save dialog"; shot 04-no-save-dialog
  fi
  click 370 255
  if filedialog '^Save As' $HOME/dl/blob.txt; then
    sleep 2
    grep -q 'blob content from the page' $HOME/dl/blob.txt 2> /dev/null && ok download-blob \
      || fail download-blob "$(ls -l $HOME/dl)"
  else
    fail download-blob "no save dialog"; shot 05-no-blob-dialog
  fi

  # upload: KDE's file dialog
  printf 'upload test file\n' > $HOME/upload.txt
  click 610 255
  if filedialog '^Choose a File to Upload' $HOME/upload.txt; then
    check upload $O/page.log '^upload upload.txt 17 "upload test file' 10
  else
    fail upload "no file dialog"; shot 06-no-open-dialog
  fi

  # microphone: denied, and said so
  click 850 255
  check microphone-not-granted $O/page.log "^mic (NotAllowedError|OverconstrainedError|NotFoundError|no-api)" 10
  sleep 0.5; shot 07-microphone

  # drag and drop: files from another application onto the page
  head -c 300000 /dev/urandom > $HOME/photo.jpg
  printf 'dropped text\n' > $HOME/note.txt
  /build/dragsource $HOME/note.txt $HOME/photo.jpg > $O/dragsource.log 2>&1 &
  DRAG=$!
  sleep 2
  S=$(window '^dragsource$')
  xdotool windowmove "$S" 1300 80; sleep 0.5
  xdotool mousemove 1400 180 sleep 0.3 mousedown 1
  for x in 1350 1250 1100 900 700 500 480; do xdotool mousemove $x 450; sleep 0.15; done
  sleep 0.5; xdotool mouseup 1
  check drop-event $O/page.log '^drop types=Files files=2' 10
  sum() { python3 -c 'import sys
s=0
for b in open(sys.argv[1],"rb").read(): s=(s*31+b)&0xffffffff
print(s)' "$1"; }
  check drop-file-text $O/page.log "^dropped note.txt 13 text/plain sum=$(sum $HOME/note.txt)" 10
  check drop-file-binary $O/page.log "^dropped photo.jpg 300000 image/jpeg sum=$(sum $HOME/photo.jpg)" 10
  kill $DRAG 2> /dev/null
  shot 08-dropped

  # the tray: a click hides the window in front, a click shows it on the
  # desktop the user is on (not on the one it was on)
  click 610 55   # unread chats again (the page was loaded again since)
  sleep 1
  TRAY=
  for w in $(xdotool search --classname kwhatsapp 2> /dev/null); do
    eval "$(xdotool getwindowgeometry --shell $w 2> /dev/null)"
    if [ "${WIDTH:-0}" -le 48 ] && [ "${HEIGHT:-0}" -le 48 ] && [ "${WIDTH:-0}" -ge 8 ] && [ "${Y:-0}" -gt 900 ]; then
      TRAY="$X $Y $WIDTH $HEIGHT"
    fi
  done
  if [ -z "$TRAY" ]; then
    fail tray-icon "not found in the panel"
  else
    ok tray-icon
    set -- $TRAY
    TX=$(( $1 + $3 / 2 )); TY=$(( $2 + $4 / 2 ))
    import -window root -crop $(( $3 + 16 ))x$(( $4 + 16 ))+$(( $1 - 8 ))+$(( $2 - 8 )) $O/09-tray-unread.png
    xdotool windowactivate --sync "$W"
    xdotool mousemove $TX $TY sleep 0.2 click 1; sleep 1
    xwininfo -id "$W" | grep -q IsUnMapped && ok tray-click-hides || fail tray-click-hides "$(xwininfo -id $W | grep 'Map State')"
    xdotool set_desktop 1; sleep 1
    xdotool mousemove $TX $TY sleep 0.2 click 1; sleep 1.5
    cur=$(xdotool get_desktop); wd=$(xdotool get_desktop_for_window "$W" 2> /dev/null)
    if xwininfo -id "$W" | grep -q IsViewable && [ "$cur" = 1 ] && [ "$wd" = 1 ]; then
      ok tray-click-shows-on-current-desktop
    else
      fail tray-click-shows-on-current-desktop "map=$(xwininfo -id $W | grep 'Map State') desktop=$cur window=$wd"
    fi
    shot 10-tray-shown-desktop2
    # the page's title back to no unread chats: the plain icon
    click 850 55
    sleep 1
    import -window root -crop $(( $3 + 16 ))x$(( $4 + 16 ))+$(( $1 - 8 ))+$(( $2 - 8 )) $O/11-tray-read.png
    TRAYTOP=$2
    balloon_tests
    bridge_tests
    front
  fi

  # closing the window keeps the application (in the tray)
  xdotool windowactivate --sync "$W"; xdotool key --clearmodifiers alt+F4; sleep 1.5
  if kill -0 $APP 2> /dev/null && xwininfo -id "$W" | grep -q IsUnMapped; then ok close-hides-to-tray
  else fail close-hides-to-tray "running=$(kill -0 $APP 2>/dev/null && echo yes || echo no)"; fi
  # starting it again shows the window of the running one
  kwhatsapp --url http://localhost:8080/app.html > $O/second-start.log 2>&1
  sleep 2
  xwininfo -id "$W" | grep -q IsViewable && [ "$(pgrep -cx kwhatsapp)" = 1 ] && ok second-start-shows-window \
    || fail second-start-shows-window "$(pgrep -ax kwhatsapp)"
  memory "test page, after the test"
  quit_app

  # a restart: still logged in (cookies, local storage, IndexedDB), the
  # notification permission kept; only "New message" if so set
  cat >> $KDEHOME/share/config/kwhatsapprc <<'EOC'

[General]
NotificationContent=false
EOC
  RUN=2
  start_app --url http://localhost:8080/app.html || return
  check restart-persistent $O/page.log '^load visit=3 cookie=\[kwtest=2\] idb=3 permission=granted' 20
  check restart-permission-query $O/page.log '^query granted' 10
  : > $O/knotify.log
  click 370 55
  check notification-no-content $O/knotify.log ': New message$' 10
  grep -q Alice $O/knotify.log && fail notification-no-content-hidden "sender shown" || ok notification-no-content-hidden
  # the balloon: "New message" only, no picture (kwhatsapp's icon)
  click 130 675; away
  if wait_balloon 10; then
    shot 17-balloon-no-content; balloon_shot 17-balloon-no-content-crop
    n=$(pixels $O/17-balloon-no-content-crop.png '#ff0000' 8%)
    [ "${n:-0}" -lt 20 ] && ok balloon-no-content-no-picture || fail balloon-no-content-no-picture "$n red pixels"
    [ -n "${BW1:-}" ] && [ "$BW" -lt $(( BW1 - 40 )) ] && ok balloon-no-content-no-text \
      || fail balloon-no-content-no-text "width $BW (with the content ${BW1:-?})"
    balloon_ignore; wait_no_balloon 3
  else fail balloon-no-content-no-picture "no balloon"; fi
  front

  # a frame of another site sends the top to about:blank: not trusted,
  # the application's page comes back
  : > $O/page.log
  click 1110 545
  check frame-top-blank-sent $O/page.log '^frame sent top to about:blank' 10
  check frame-top-blank-reported $O/kwhatsapp-$RUN.log 'committed about:blank, not one of the application' 10
  check frame-top-blank-back-to-app $O/page.log '^load visit=' 15
  sleep 1
  # a frame of another site asks for the location (delegated): refused,
  # not in the name of the application's origin
  click 1110 595
  check frame-geolocation-not-granted $O/page.log '^frame geolocation error' 15
  if grep -q 'denied geolocation for http' $O/kwhatsapp-$RUN.log; then
    fail frame-geolocation-not-attributed "$(grep 'denied geolocation' $O/kwhatsapp-$RUN.log)"
  else ok frame-geolocation-not-attributed; fi

  # another site without a link the user followed: never in the window
  confined() {   # confined STEP: no other site in the app, the page is the app's
    sleep 3
    if grep -q 'other site loaded IN THE APP' $O/page.log; then fail "$1" "another site was shown"
    elif ! xprop -id "$W" WM_NAME | grep -q 'WhatsApp'; then fail "$1" "$(xprop -id $W WM_NAME)"
    else ok "$1"; fi
  }
  click 1110 55; confined confined-location-replace
  click 1110 155; confined confined-form-post
  click 1110 255; confined confined-redirect
  click 1110 355; confined confined-data-url
  click 1110 455; check refresh-page-loaded $O/page.log '^page3 loaded' 10; confined confined-meta-refresh
  check confinement-reported $O/kwhatsapp-$RUN.log 'is not one of the application' 2
  quit_app

  # a hand-edited permissions file: canonicalized, a denial wins
  printf '[Notifications]\nAllowed=http://localhost:8080/,http://localhost:8080,garbage\nDenied=HTTP://LOCALHOST:8080\n' \
    > $KDEHOME/share/apps/kwhatsapp/webkit/kwpepart-permissions
  RUN=3
  : > $O/page.log
  sed -i 's/^MenuBar=Disabled/MenuBar=Enabled/' $KDEHOME/share/config/kwhatsapprc
  start_app --url http://localhost:8080/app.html || return
  # the menus (File, Edit, View, Settings, Help; Quit once), the tray's
  xdotool windowactivate --sync "$W"; sleep 1
  shot 30-menubar
  for m in f e v s; do
    xdotool key --clearmodifiers alt+$m; sleep 1; shot 3$(( $(echo fevs | sed "s/$m.*//" | wc -c) ))-menu-$m
    xdotool key Escape; sleep 0.5
  done
  if [ -n "${TX:-}" ]; then
    xdotool mousemove $TX $TY sleep 0.2 click 3; sleep 1; shot 35-tray-menu
    xdotool key Escape; sleep 0.5
  fi
  xdotool windowactivate --sync "$W"; sleep 0.5
  # (WebKit reports an origin denied at start as "default")
  check permissions-not-granted $O/page.log '^load visit=.*permission=(denied|default)' 20
  check permissions-query-not-granted $O/page.log '^query (denied|prompt)' 10
  click 130 55
  check permissions-denial-wins $O/page.log '^permission denied' 10
  quit_app

  # a large font: the balloon no wider than the screen (its lines may
  # break); the window closed (not to the tray) with the balloon open:
  # the balloon goes with it
  printf '[Notifications]\nAllowed=http://localhost:8080\n' > $KDEHOME/share/apps/kwhatsapp/webkit/kwpepart-permissions
  printf '[MainWindow]\nMenuBar=Disabled\n\n[General]\nCloseToTray=false\n' > $KDEHOME/share/config/kwhatsapprc
  cp $KDEHOME/share/config/kdeglobals $O/kdeglobals.saved 2> /dev/null
  printf '\n[General]\nfont=Sans Serif,72,-1,5,50,0,0,0,0,0\n' >> $KDEHOME/share/config/kdeglobals
  RUN=4
  : > $O/page.log
  start_app --url http://localhost:8080/app.html || return
  check load-4 $O/page.log '^load visit=' 20
  # notifications of a document, another document of the site announces
  # the second one's title, body and tag with a picture: not given to it.
  # (WebKit shows a notification with an icon only once it loaded the
  # icon: "Nav Wait" never comes, its load stopped by the navigation;
  # and it closes the old document's notifications soon after.)
  : > $O/page.log; mark; click 850 855; away
  check nav-forged $O/page.log '^nav forged' 15
  sleep 3
  id=$(since | sed -n 's/^kwhatsapp: balloon \([0-9]*\): .*Nav Same.*/\1/p' | head -1)
  if [ -z "$id" ]; then fail nav-other-document-picture "no balloon"
  elif since | grep -q "^kwhatsapp: balloon picture $id\$"; then fail nav-other-document-picture "it has the picture"
  else ok nav-other-document-picture; fi
  check nav-back $O/page.log '^load visit=' 20
  front
  click 130 675; away
  if wait_balloon 10; then
    shot 40-balloon-large-font
    check balloon-narrowed $O/kwhatsapp-$RUN.log '^kwhatsapp: balloon narrowed' 2
    [ "$BX" -ge 0 ] && [ $(( BX + BW )) -le 1600 ] && ok balloon-fits-screen || fail balloon-fits-screen "$BX+$BW"
    /build/netclose "$W"
    i=0; while kill -0 $APP 2> /dev/null && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
    if kill -0 $APP 2> /dev/null; then fail window-closed-app-ends "still running"; quit_app
    else
      wait $APP; rc=$?
      [ $rc = 0 ] && ok window-closed-app-ends || fail window-closed-app-ends "exit status $rc"
      b=$(grep -n '^kwhatsapp: balloon destroyed' $O/kwhatsapp-$RUN.log | tail -1 | cut -d: -f1)
      m=$(grep -n '^kwhatsapp: main window destroyed' $O/kwhatsapp-$RUN.log | cut -d: -f1)
      [ -n "$b" ] && [ -n "$m" ] && [ "$b" -lt "$m" ] && ok balloon-destroyed-with-window \
        || fail balloon-destroyed-with-window "balloon line ${b:-none}, window line ${m:-none}"
      [ -z "$(balloon)" ] && ok balloon-gone-with-window || fail balloon-gone-with-window "still shown"
    fi
  else fail balloon-narrowed "no balloon"; quit_app; fi
  if [ -e $O/kdeglobals.saved ]; then cp $O/kdeglobals.saved $KDEHOME/share/config/kdeglobals
  else rm -f $KDEHOME/share/config/kdeglobals; fi
}

phase_whatsapp() {
  RUN=whatsapp
  start_app || return
  sleep 25
  shot 20-whatsapp
  # its login page (the QR code) or "browser not supported": the screenshots
  xprop -id "$W" WM_NAME | grep -q '"WhatsApp' && ok whatsapp-title || fail whatsapp-title "$(xprop -id $W WM_NAME)"
  memory "web.whatsapp.com login page, 25 s"
  sleep 35
  shot 21-whatsapp-60s
  memory "web.whatsapp.com login page, 60 s"
  quit_app
}

for p in ${PHASES:-local}; do phase_$p; done
shot 99-end
grep -c '^ok' $SUMMARY | sed 's/^/passed: /'
grep '^FAIL' $SUMMARY && exit 1
exit 0
