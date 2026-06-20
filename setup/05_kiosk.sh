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

echo "=== Kiosk configured ==="
