#!/bin/bash
# Display config: official 7-inch DSI panel on DSI port 0.
# Auto-detect is disabled and the panel is pinned explicitly with the overlay,
# which is what works on this hardware. To move the panel to the other DSI
# connector, change ",dsi0" to ",dsi1" on the overlay line below.
set -e

echo "=== [7/7] Display (7-inch DSI on dsi0) ==="

CONFIG=/boot/firmware/config.txt
[ -f "$CONFIG" ] || CONFIG=/boot/config.txt   # older image layout

# back up the original once, so the stock config isn't lost
if [ -f "$CONFIG" ] && [ ! -f "$CONFIG.sparkles-orig" ]; then
    cp "$CONFIG" "$CONFIG.sparkles-orig"
    echo "Backed up original to $CONFIG.sparkles-orig"
fi

cat > "$CONFIG" << 'EOF'
# For more options and information see
# http://rptl.io/configtxt

dtparam=audio=on
camera_auto_detect=0
display_auto_detect=0
auto_initramfs=1
dtoverlay=vc4-kms-v3d
dtoverlay=vc4-kms-dsi-7inch,dsi0
max_framebuffers=2
arm_64bit=1
disable_overscan=1
arm_boost=1

[cm4]
otg_mode=1

[cm5]
dtoverlay=dwc2,dr_mode=host

[all]
EOF

echo "Wrote $CONFIG (7-inch DSI on dsi0). Takes effect after reboot."
