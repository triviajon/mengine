#!/usr/bin/env bash
# Converts mengine-ae-<arch>.vmdk -> mengine-ae-<arch>.ova.
# Usage: package-ova.sh amd64|arm64
set -euo pipefail
ARCH=$1
VM=mengine-ae-$ARCH
VBOX=${VBOXMANAGE:-VBoxManage}

case $ARCH in
    amd64) OS="--ostype Ubuntu_64 --graphicscontroller vboxsvga"; CONTROLLER=sata ;;
    arm64) OS="--ostype Ubuntu_arm64 --firmware efi --graphicscontroller vmsvga"; CONTROLLER=virtio ;;
esac

# Refer to the VM by its UUID: an imported copy of an earlier OVA may have the same name.
ID=$("$VBOX" createvm --name "$VM" --basefolder vbox --register | sed -n 's/^UUID: //p' | tr -d '\r')
"$VBOX" modifyvm "$ID" $OS --cpus 4 --memory 8192 --vram 64 --clipboard-mode bidirectional \
    --mouse usbtablet --usb-ohci on --audio-enabled off \
    --nic1 nat --nat-pf1 "ssh,tcp,127.0.0.1,2222,,22"
"$VBOX" storagectl "$ID" --name Disk --add $CONTROLLER --portcount 1
"$VBOX" storageattach "$ID" --storagectl Disk --port 0 --type hdd --medium "$VM.vmdk"
"$VBOX" export "$ID" --output "$VM.ova"
# Also deletes the VMDK; build-vm.sh's disk.qcow2 remains.
"$VBOX" unregistervm "$ID" --delete
