#!/bin/bash
# Install and enable systemd services
set -e

echo "=== [4/5] Installing Services ==="

REPO=/home/julian/sparkles
SERVICES_DIR="$REPO/setup/services"

for service in serial_mux sparkles aubio keyboard_midi; do
    echo ">> Installing $service.service"
    sudo cp "$SERVICES_DIR/$service.service" /etc/systemd/system/
done

sudo systemctl daemon-reload

for service in serial_mux sparkles aubio keyboard_midi; do
    echo ">> Enabling $service"
    sudo systemctl enable "$service"
done

# Start in dependency order
for service in serial_mux sparkles aubio keyboard_midi; do
    echo ">> Starting $service"
    sudo systemctl restart "$service"
done

echo ""
echo "Service status:"
for service in serial_mux sparkles aubio keyboard_midi; do
    status=$(systemctl is-active "$service" 2>/dev/null || echo "unknown")
    echo "  $service: $status"
done

# Configure nginx as reverse proxy on port 80
echo ">> Configuring nginx"
cp "$REPO/setup/nginx_sparkles.conf" /etc/nginx/sites-available/sparkles
ln -sf /etc/nginx/sites-available/sparkles /etc/nginx/sites-enabled/sparkles
rm -f /etc/nginx/sites-enabled/default
nginx -t
systemctl enable nginx
systemctl restart nginx

# Allow sparkles service user to stop/start these services without a password
# (needed so compile.py can release /dev/sparkles before flashing master)
SUDOERS_LINE="julian ALL=(ALL) NOPASSWD: /bin/systemctl stop serial_mux, /bin/systemctl start serial_mux, /bin/systemctl start sparkles, /bin/systemctl stop sparkles"
SUDOERS_FILE=/etc/sudoers.d/sparkles-flash
echo "$SUDOERS_LINE" | sudo tee "$SUDOERS_FILE" > /dev/null
sudo chmod 440 "$SUDOERS_FILE"
echo ">> Sudoers entry written to $SUDOERS_FILE"

echo "=== Services installed ==="
