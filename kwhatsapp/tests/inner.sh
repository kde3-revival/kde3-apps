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
EOC
kdeinit +kded > $O/kdeinit.log 2>&1
sleep 3
kbuildsycoca > /dev/null 2>&1
kwin > $O/kwin.log 2>&1 &
sleep 2
kicker > $O/kicker.log 2>&1 &
sleep 5

python3 -I $T/server.py $O/page.log 8080 8081 2> $O/server.log &
sleep 1

phase_local() {
  RUN=1
  start_app --url http://localhost:8080/app.html || return
  check load-1 $O/page.log '^load visit=1 ' 20
  grep '^load visit=1' $O/page.log | grep -q 'permission=default' && ok permission-default-at-start \
    || fail permission-default-at-start "$(grep '^load visit=1' $O/page.log)"
  shot 01-start
  memory "test page, after loading"

  # notifications: the page asks, kwhatsapp allows, the answer is kept
  click 130 55
  check permission-granted $O/page.log '^permission granted' 10
  grep -q 'Allowed=http://localhost:8080' $KDEHOME/share/apps/kwhatsapp/webkit/kwpepart-permissions 2> /dev/null \
    && ok permission-stored || fail permission-stored "$(cat $KDEHOME/share/apps/kwhatsapp/webkit/kwpepart-permissions 2>&1)"
  click 370 55
  check notification-shown $O/page.log '^notification shown' 10
  check knotify-content $O/knotify.log 'Alice &lt;Test&gt;.*Hello &lt;b&gt;bold&lt;/b&gt; &amp; co<br>second line' 10
  sleep 0.5; shot 02-notification

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
  start_app --url http://localhost:8080/app.html || return
  # (WebKit reports an origin denied at start as "default")
  check permissions-not-granted $O/page.log '^load visit=.*permission=(denied|default)' 20
  check permissions-query-not-granted $O/page.log '^query (denied|prompt)' 10
  click 130 55
  check permissions-denial-wins $O/page.log '^permission denied' 10
  quit_app
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
