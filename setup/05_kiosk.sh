#!/bin/bash
# Configure Chromium kiosk autostart
set -e

echo "=== [5/5] Kiosk Setup ==="

# Openbox autostart directory
mkdir -p /home/julian/.config/openbox

# Autostart script — waits for server then launches Chromium
cat > /home/julian/.config/autostart.sh << 'EOF'
#!/bin/bash
xset s off
xset -dpms
xset s noblank
# unclutter-xfixes: hide the pointer from boot (--start-hidden), not just after
# the first mouse move like classic unclutter
unclutter --timeout 2 --hide-on-touch --start-hidden &

# dim the panel: saves battery and reduces light spill in the dark forest.
# 0-255, tune to taste. write permission comes from 90-backlight.rules below.
for b in /sys/class/backlight/*/brightness; do echo 40 > "$b" 2>/dev/null; done

# blank the panel after idle, wake on touch (backlight toggle; DPMS is
# unreliable on DSI, but the touch digitizer keeps working while dark)
/home/julian/.config/screen_idle.sh &

# Wait for sparkles server to be ready
until curl -s http://localhost/ > /dev/null 2>&1; do
    sleep 1
done

# loop so the kiosk self-heals on crash, and so `pkill chromium` (from update.sh)
# relaunches it with the freshly built UI
while true; do
    chromium --noerrdialogs --disable-infobars --kiosk --no-first-run http://localhost/karaoke
    sleep 2
done
EOF
chmod +x /home/julian/.config/autostart.sh

# Idle blanker: backlight off after IDLE_MS of no input, on at the next touch.
# Uses bl_power (not DPMS) because the DSI panel ignores DPMS; the touch
# digitizer still reports events while dark, so a tap wakes it.
cat > /home/julian/.config/screen_idle.sh << 'EOF'
#!/bin/bash
# Blank the panel after IDLE_S of no touch, wake on the next touch.
# This panel ignores bl_power/brightness for full-off, so we toggle the DSI
# output at the KMS level (xrandr). The touch digitizer keeps reporting while
# the output is off, so a blocking read of it gives instant wake.
export DISPLAY=:0
IDLE_S=120            # blank after 2 min of no touch

# auto-detect the connected display output (DSI-1/DSI-2 depends on which DSI
# port the panel is plugged into, and event/output names shuffle)
OUT=$(xrandr --query 2>/dev/null | awk '/ connected/{print $1}' | grep -iE '^DSI' | head -1)
[ -z "$OUT" ] && OUT=$(xrandr --query 2>/dev/null | awk '/ connected/{print $1; exit}')
[ -z "$OUT" ] && OUT=DSI-1

# find the touchscreen's event device by name (event numbers shuffle across
# reboots, so match the controller, not a fixed eventN)
TOUCH=$(grep -iE -A5 'touch|ft[0-9]|goodix|edt|elan|ilitek|raspberrypi-ts' /proc/bus/input/devices | grep -oiE 'event[0-9]+' | head -1)
TOUCH="/dev/input/${TOUCH:-event2}"

# one whole input_event (bs >= event size; count=1 returns on the first touch)
read_touch() { dd if="$TOUCH" bs=64 count=1 status=none >/dev/null 2>&1; }

state=on
while true; do
    if [ "$state" = on ]; then
        if timeout "$IDLE_S" dd if="$TOUCH" bs=64 count=1 status=none >/dev/null 2>&1; then
            :                                  # touched, stay awake
        else
            xrandr --output "$OUT" --off       # idle, blank
            state=off
        fi
    else
        read_touch                             # block until a touch
        xrandr --output "$OUT" --auto          # wake
        state=on
    fi
done
EOF
chmod +x /home/julian/.config/screen_idle.sh

# Openbox autostart
cat > /home/julian/.config/openbox/autostart << 'EOF'
/home/julian/.config/autostart.sh &
EOF

# startx needs an .xinitrc to launch openbox (which then runs the autostart above)
cat > /home/julian/.xinitrc << 'EOF'
exec openbox-session
EOF

# Auto-start X on console login
BASH_PROFILE=/home/julian/.bash_profile
if ! grep -q "startx" "$BASH_PROFILE" 2>/dev/null; then
    echo '[[ -z $DISPLAY && $XDG_VTNR -eq 1 ]] && startx -- -nocursor' >> "$BASH_PROFILE"
    echo "Added startx to $BASH_PROFILE"
fi

# Boot to console and auto-login julian on tty1, so .bash_profile runs startx
sudo raspi-config nonint do_boot_behaviour B2

chown julian:julian /home/julian/.config/autostart.sh
chown julian:julian /home/julian/.config/openbox/autostart
chown julian:julian /home/julian/.config/screen_idle.sh
chown julian:julian /home/julian/.xinitrc

# xprintidle lets the blanker measure how long there's been no touch
sudo apt-get install -y xprintidle

# Let the kiosk user set panel brightness / power without root (battery saving).
# udev makes brightness + bl_power group-writable by 'video'; julian joins it.
sudo usermod -aG video julian
sudo usermod -aG input julian   # so the blanker can read the touch device for wake
sudo tee /etc/udev/rules.d/90-backlight.rules > /dev/null << 'EOF'
SUBSYSTEM=="backlight", ACTION=="add", RUN+="/bin/chgrp video /sys/class/backlight/%k/brightness", RUN+="/bin/chmod g+w /sys/class/backlight/%k/brightness", RUN+="/bin/chgrp video /sys/class/backlight/%k/bl_power", RUN+="/bin/chmod g+w /sys/class/backlight/%k/bl_power"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=backlight

echo "=== Kiosk configured ==="
