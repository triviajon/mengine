#!/usr/bin/env bash
# Converts mengine-ae-<arch>.vmdk -> mengine-ae-<arch>.ova.
# Usage: package-ova.sh amd64|arm64
set -euo pipefail
ARCH=$1
VM=mengine-ae-$ARCH
VBOX=${VBOXMANAGE:-VBoxManage}

case $ARCH in
    amd64) OS="--ostype Ubuntu_64 --graphicscontroller vboxsvga"; CONTROLLER=sata ;;
    arm64) OS="--ostype Ubuntu_arm64 --firmware efi"; CONTROLLER=virtio ;;
esac

"$VBOX" createvm --name "$VM" --basefolder vbox --register
"$VBOX" modifyvm "$VM" $OS --cpus 4 --memory 8192 --clipboard-mode bidirectional \
    --nic1 nat --nat-pf1 "ssh,tcp,127.0.0.1,2222,,22"
"$VBOX" storagectl "$VM" --name Disk --add $CONTROLLER
"$VBOX" storageattach "$VM" --storagectl Disk --port 0 --type hdd --medium "$VM.vmdk"
"$VBOX" export "$VM" --output "$VM.ova"
# Also deletes the VMDK; build-vm.sh's disk.qcow2 remains.
"$VBOX" unregistervm "$VM" --delete
