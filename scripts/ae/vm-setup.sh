#!/usr/bin/env bash
# This script runs as root at first boot of the VM to setup the artifact and VM.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

# Disable updates
apt-get purge -y snapd unattended-upgrades

# this was causing the VM to hang
apt-get purge -y mdadm btrfs-progs

# Setup artifact
cd /home/artifact/mengine
sudo -u artifact -H scripts/ae/provision-ubuntu.sh

# Setup desktop env
apt-get install -y --no-install-recommends xserver-xorg xfce4 xfce4-terminal lightdm lightdm-gtk-greeter \
    dbus-x11 adwaita-icon-theme fonts-dejavu-core ristretto
if [ "$(dpkg --print-architecture)" = amd64 ]; then
    apt-get install -y --no-install-recommends virtualbox-guest-x11
fi
printf '[Seat:*]\nautologin-user=artifact\nautologin-session=xfce\n' > /etc/lightdm/lightdm.conf.d/50-artifact.conf
systemctl set-default graphical.target

# Disable cloud-init 
touch /etc/cloud/cloud-init.disabled
rm /etc/netplan/50-cloud-init.yaml
echo 'network: {version: 2, ethernets: {any: {match: {name: "en*"}, dhcp4: true}}}' > /etc/netplan/01-any.yaml

apt-get clean
fstrim -a
echo "AE-VM-SETUP: OK"
