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

# Let the kiosk user set panel brightness without root (for battery saving).
# udev makes the brightness file group-writable by 'video'; julian joins it.
sudo usermod -aG video julian
sudo tee /etc/udev/rules.d/90-backlight.rules > /dev/null << 'EOF'
SUBSYSTEM=="backlight", ACTION=="add", RUN+="/bin/chgrp video /sys/class/backlight/%k/brightness", RUN+="/bin/chmod g+w /sys/class/backlight/%k/brightness"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=backlight

echo "=== Kiosk configured ==="
