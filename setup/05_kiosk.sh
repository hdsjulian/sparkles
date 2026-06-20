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
unclutter -idle 0 &

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
BL=/sys/class/backlight/10-0045
IDLE_MS=120000        # blank after 2 min of no touch
state=on
while true; do
    idle=$(xprintidle 2>/dev/null || echo 0)
    if [ "$state" = on ] && [ "$idle" -gt "$IDLE_MS" ]; then
        echo 1 > "$BL/bl_power" 2>/dev/null && state=off
    elif [ "$state" = off ] && [ "$idle" -lt "$IDLE_MS" ]; then
        echo 0 > "$BL/bl_power" 2>/dev/null && state=on
    fi
    sleep 1
done
EOF
chmod +x /home/julian/.config/screen_idle.sh

# Openbox autostart
cat > /home/julian/.config/openbox/autostart << 'EOF'
/home/julian/.config/autostart.sh &
EOF

# Auto-start X on console login
BASH_PROFILE=/home/julian/.bash_profile
if ! grep -q "startx" "$BASH_PROFILE" 2>/dev/null; then
    echo '[[ -z $DISPLAY && $XDG_VTNR -eq 1 ]] && startx -- -nocursor' >> "$BASH_PROFILE"
    echo "Added startx to $BASH_PROFILE"
fi

chown julian:julian /home/julian/.config/autostart.sh
chown julian:julian /home/julian/.config/openbox/autostart
chown julian:julian /home/julian/.config/screen_idle.sh

# xprintidle lets the blanker measure how long there's been no touch
sudo apt-get install -y xprintidle

# Let the kiosk user set panel brightness / power without root (battery saving).
# udev makes brightness + bl_power group-writable by 'video'; julian joins it.
sudo usermod -aG video julian
sudo tee /etc/udev/rules.d/90-backlight.rules > /dev/null << 'EOF'
SUBSYSTEM=="backlight", ACTION=="add", RUN+="/bin/chgrp video /sys/class/backlight/%k/brightness", RUN+="/bin/chmod g+w /sys/class/backlight/%k/brightness", RUN+="/bin/chgrp video /sys/class/backlight/%k/bl_power", RUN+="/bin/chmod g+w /sys/class/backlight/%k/bl_power"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=backlight

echo "=== Kiosk configured ==="
