#!/usr/bin/env bash
# Blood Moon - build & run on a Linux gaming laptop (e.g. MSI GP66). Idempotent: safe to re-run.
#   UE_ROOT=~/UnrealEngine ./setup-linux.sh [check|build|materials|package|run|all]
# Unreal Engine for Linux needs an Epic account, so it is the one manual download:
#   https://www.unrealengine.com/linux  ->  Linux_Unreal_Engine_5.5.x.zip  ->  unzip to $UE_ROOT
set -euo pipefail

UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine}"
REPO_DIR="${REPO_DIR:-$HOME/lazy-mcp}"
BRANCH="${BRANCH:-claude/vampires-werewolves-game-p5rejb}"
PROJ_DIR="$REPO_DIR/games/blood-moon-ue"
PROJ="$PROJ_DIR/BloodMoon.uproject"
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
log() { printf '\033[1;31m[blood-moon]\033[0m %s\n' "$*"; }
die() { log "ERROR: $*"; exit 1; }

check() {
  log "Checking machine..."
  command -v nvidia-smi >/dev/null && nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || log "WARN: nvidia-smi not found - install the proprietary NVIDIA driver (535+) for Vulkan"
  command -v vulkaninfo >/dev/null && vulkaninfo --summary 2>/dev/null | grep -m1 -i "deviceName" || log "WARN: vulkaninfo missing (sudo apt install vulkan-tools) - UE5 on Linux renders with Vulkan"
  local free_gb; free_gb=$(df -BG --output=avail "$HOME" | tail -1 | tr -dc '0-9')
  [ "$free_gb" -ge 150 ] || log "WARN: only ${free_gb} GB free in \$HOME; UE5 + project needs ~150 GB"
  local ram_gb; ram_gb=$(awk '/MemTotal/ {print int($2/1048576)}' /proc/meminfo)
  [ "$ram_gb" -ge 16 ] || log "WARN: ${ram_gb} GB RAM; 32 GB recommended for compiling shaders"
  [ -x "$EDITOR" ] || die "Unreal Engine not found at $UE_ROOT (download the Linux build, see header)"
  log "OK: engine at $UE_ROOT"
}

deps() {
  log "Installing build/runtime packages (sudo)..."
  sudo apt-get update -y
  sudo apt-get install -y git git-lfs curl xdg-user-dirs vulkan-tools libvulkan1 nodejs npm
}

fetch() {
  if [ -d "$REPO_DIR/.git" ]; then git -C "$REPO_DIR" fetch origin "$BRANCH" && git -C "$REPO_DIR" checkout "$BRANCH" && git -C "$REPO_DIR" pull --ff-only origin "$BRANCH"
  else git clone --branch "$BRANCH" https://github.com/butterpurse-dot/lazy-mcp "$REPO_DIR"; fi
}

build() {
  log "Compiling BloodMoonEditor (first run takes a while)..."
  mkdir -p "$PROJ_DIR/Saved"
  "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" BloodMoonEditor Linux Development -project="$PROJ" -waitmutex 2>&1 | tee "$PROJ_DIR/Saved/build.log" \
    || die "Compile failed - see $PROJ_DIR/Saved/build.log"
}

materials() {
  log "Generating base materials (one-time)..."
  "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJ" -run=pythonscript -script="$PROJ_DIR/Content/Python/init_unreal.py" -unattended -nosplash 2>&1 | tail -20
}

package() {
  log "Packaging a Shipping build to $PROJ_DIR/Dist ..."
  mkdir -p "$PROJ_DIR/Saved"
  "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJ" -platform=Linux -clientconfig=Shipping \
    -build -cook -stage -pak -archive -archivedirectory="$PROJ_DIR/Dist" -utf8output > "$PROJ_DIR/Saved/package.log" 2>&1 \
    || die "Packaging failed - see $PROJ_DIR/Saved/package.log"
  grep -q "BUILD SUCCESSFUL" "$PROJ_DIR/Saved/package.log" || die "Packaging did not report success - see $PROJ_DIR/Saved/package.log"
}

run() {
  local bin; bin=$(find "$PROJ_DIR/Dist" -maxdepth 3 -name 'BloodMoon.sh' 2>/dev/null | head -1)
  if [ -n "$bin" ]; then "$bin" "/Engine/Maps/Entry?Team=${TEAM:-vampire}?Diff=${DIFF:-normal}" -log
  else "$EDITOR" "$PROJ" "/Engine/Maps/Entry?Team=${TEAM:-vampire}?Diff=${DIFF:-normal}" -game -log; fi
}

case "${1:-all}" in
  check) check ;; build) build ;; materials) materials ;; package) package ;; run) run ;;
  all) deps; fetch; check; build; materials; package; log "Done. Play: $0 run   (TEAM=werewolf DIFF=hard $0 run)" ;;
  *) die "usage: $0 [check|build|materials|package|run|all]" ;;
esac
