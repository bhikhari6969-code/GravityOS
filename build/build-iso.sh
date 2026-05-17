#!/bin/bash
set -e

echo "GravityOS V1 ISO Builder"
echo "========================"

# Check for live-build
if ! command -v lb &> /dev/null; then
    echo "Error: live-build is not installed. Please run on Debian/Ubuntu and run: sudo apt install live-build xorriso"
    exit 1
fi

# Create build directory
mkdir -p build_tmp
cd build_tmp

# Clean previous builds
sudo lb clean

# Configure build
lb config \
    --architecture amd64 \
    --distribution bookworm \
    --archive-areas "main contrib non-free non-free-firmware" \
    --iso-application "GravityOS Singularity" \
    --iso-publisher "GravityOS Foundation" \
    --iso-volume "GRAVITYOS" \
    --apt-recommends false \
    --bootappend-live "boot=live components quiet splash logo.nologo=1 vt.global_cursor_default=0"

# Create config directories
mkdir -p config/package-lists
mkdir -p config/includes.chroot/etc/lightdm
mkdir -p config/includes.chroot/etc/xdg/openbox
mkdir -p config/includes.chroot/opt/gravityos
mkdir -p config/hooks/live
mkdir -p config/bootloaders/grub-pc

# Define minimal packages
cat > config/package-lists/gravityos.list.chroot <<EOF
xorg
openbox
chromium
lightdm
unclutter
xdotool
pulseaudio
EOF

# LightDM Autologin Config
cat > config/includes.chroot/etc/lightdm/lightdm.conf <<EOF
[SeatDefaults]
autologin-user=gravity
autologin-user-timeout=0
user-session=openbox
EOF

# Openbox Autostart (Kiosk mode)
cat > config/includes.chroot/etc/xdg/openbox/autostart <<EOF
# Disable screen blanking
xset s off
xset -dpms
xset s noblank

# Hide cursor
unclutter -idle 0.5 -root &

# Launch GravityOS Shell
chromium --kiosk --no-first-run --disable-infobars --disable-features=Translate --no-errdialogs "file:///opt/gravityos/shell/demo.html" &
EOF

# Hook to create the gravity user and set permissions
cat > config/hooks/live/0100-setup-user.hook.chroot <<EOF
#!/bin/sh
useradd -m -s /bin/bash gravity
passwd -d gravity
chown -R gravity:gravity /opt/gravityos
EOF
chmod +x config/hooks/live/0100-setup-user.hook.chroot

# Copy the shell into the ISO
cp -r ../../shell config/includes.chroot/opt/gravityos/

# Build the ISO
echo "Building ISO... This will take a while."
sudo lb build

# Move the ISO back
if [ -f live-image-amd64.hybrid.iso ]; then
    mv live-image-amd64.hybrid.iso ../GravityOS-v0.1.0-Singularity.iso
    echo "Done! ISO generated at GravityOS-v0.1.0-Singularity.iso"
else
    echo "Error: ISO build failed."
fi
