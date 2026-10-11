#!/usr/bin/env bash
# Main script to build the VM disk mengine-ae-<arch>.vmdk using qemu
# Usage: build-vm.sh amd64|arm64 <ref> [<repository URL>]
set -euo pipefail
ARCH=$1
REF=$2
REPO=${3:-https://github.com/triviajon/mengine.git}

case $ARCH in
    amd64) QEMU="qemu-system-x86_64 -machine q35 -accel kvm" ;;
    arm64) QEMU="qemu-system-aarch64 -machine virt -accel hvf -bios $(brew --prefix)/share/qemu/edk2-aarch64-code.fd" ;;
esac

IMAGE=ubuntu-24.04-server-cloudimg-$ARCH.img
[ -f "$IMAGE" ] || curl -fLO "https://cloud-images.ubuntu.com/releases/noble/release/$IMAGE"
qemu-img create -f qcow2 -b "$IMAGE" -F qcow2 disk.qcow2 32G

# cloud-init reads its configuration from a web server on the host.
mkdir -p seed
echo "instance-id: mengine-ae" > seed/meta-data
cat > seed/user-data <<EOF
#cloud-config
hostname: mengine-ae
ssh_pwauth: true
users:
  - name: artifact
    shell: /bin/bash
    sudo: ALL=(ALL) NOPASSWD:ALL
    lock_passwd: false
    plain_text_passwd: artifact
runcmd:
  - git clone --branch $REF $REPO /home/artifact/mengine && chown -R artifact:artifact /home/artifact/mengine && /home/artifact/mengine/scripts/ae/vm-setup.sh; poweroff
EOF
python3 -m http.server -b 127.0.0.1 -d seed 8714 &
trap "kill $!" EXIT

$QEMU -cpu host -smp 4 -m 10G -nographic \
    -drive file=disk.qcow2,if=virtio,discard=unmap \
    -nic user,model=virtio-net-pci \
    -smbios "type=1,serial=ds=nocloud;s=http://10.0.2.2:8714/" | tee serial.log

grep -q "AE-VM-SETUP: OK" serial.log
qemu-img convert -O vmdk disk.qcow2 "mengine-ae-$ARCH.vmdk"
