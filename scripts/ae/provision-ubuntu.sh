#!/usr/bin/env bash
# This script installs the dependencies required to run all parts of the artifact, including
# mengine and several abalation binaries, Rocq 9.1.1 with the coqutil@68c4f0, and Lean v4.32.0-rc1. 
# (assuming the system is running Ubuntu/Debian-like OS)
set -euo pipefail

# mengine
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential clang git curl ca-certificates unzip m4 \
    pkg-config libgmp-dev opam python3 python3-matplotlib

# Rocq 9.1.1
opam init -y --bare --no-setup --disable-sandboxing
opam switch create rocq-9.1 ocaml-base-compiler.5.2.1 -y
opam install --switch=rocq-9.1 -y rocq-core.9.1.1 rocq-stdlib.9.1.0 coq-core.9.1.1
echo 'eval "$(opam env --switch=rocq-9.1 --set-switch)"' >> ~/.bashrc

# coqutil
git clone https://github.com/mit-plv/coqutil.git ~/coqutil
git -C ~/coqutil checkout 68c4f06a731973fe137c03d627feaf43df9e92d1
opam exec --switch=rocq-9.1 -- make -C ~/coqutil -j"$(nproc)"

# Lean v4.32.0-rc1
curl -sSf https://raw.githubusercontent.com/leanprover/elan/master/elan-init.sh \
    | sh -s -- -y --no-modify-path --default-toolchain leanprover/lean4:v4.32.0-rc1
~/.elan/bin/elan toolchain install leanprover/lean4:v4.32.0-rc1
echo 'export PATH="$HOME/.elan/bin:$PATH"' >> ~/.bashrc

# mengine abalation binaries
make ablation-binaries OPT_FLAGS=-O2
