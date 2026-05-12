#!/bin/bash

# Stops the TPM simulator and related daemons started by tpm-start.sh.
# Usage: tpm-stop.sh [--clean]
#   --clean: Also delete the NVChip file to completely reset TPM state
#
# NVChip and paths use TPMKIT_STATE_DIR (default: $HOME/.tpm-state), matching tpm-start.sh.

set -e

TPMKIT_STATE_DIR="${TPMKIT_STATE_DIR:-$HOME/.tpm-state}"

# Parse command line arguments
CLEAN_NVCHIP=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)
            CLEAN_NVCHIP=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [--clean]"
            echo "  --clean: Delete NVChip file to completely reset TPM state"
            echo "  State directory: TPMKIT_STATE_DIR (default: \$HOME/.tpm-state)"
            echo "  Requires a Linux container unless TPMKIT_ALLOW_NONCONTAINER=1."
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--clean]"
            exit 1
            ;;
    esac
done

# Linux OCI container — same idea as tpm-start.sh; after --help so usage works on the host.
is_linux_oci_container() {
    [[ -f /.dockerenv ]] && return 0
    [[ -f /run/.containerenv ]] && return 0
    [[ -r /proc/1/cgroup ]] || return 1
    grep -qE '(docker|moby|containerd|libpod|lxc|kubepods)' /proc/1/cgroup 2>/dev/null
}

if [[ ${TPMKIT_ALLOW_NONCONTAINER:-0} != 1 ]] && ! is_linux_oci_container; then
    echo "tpm-stop.sh is intended to run inside a Linux container (e.g. tpmkit image)." >&2
    echo "Run: docker exec -it tpmkit-dev tpm-stop.sh" >&2
    echo "Override (not recommended): TPMKIT_ALLOW_NONCONTAINER=1 $0 $*" >&2
    exit 1
fi

# Zombies (ps state Z, "[name] <defunct>") are already dead; /proc/*/cmdline is empty so pgrep -f
# will not see them. PID 1 must reap them — use `docker run --init` (see run-tpmkit-docker.sh).
if ps -eo stat=,args= 2>/dev/null | grep '^Z' | grep -E 'tpm_server|tpm2-abrmd' >/dev/null; then
    echo "Warning: zombie tpm_server / tpm2-abrmd entries found (defunct). They are not running daemons." >&2
    echo "Recreate the container with ./scripts/run-tpmkit-docker.sh (uses docker run --init), then tpm-start.sh again." >&2
fi

# Stop daemons by full command line (pgrep -f), not /proc/comm: comm is 15 bytes and can disagree
# with argv0, so pgrep -x / pkill -x often mis-match. Patterns are anchored so pgrep does not match
# itself. Uses sudo kill: tpm2-abrmd drops to user "tss" and app cannot signal it.
stop_matching_cmdline() {
    local ere_pattern="$1"
    local display_name="$2"

    mapfile -t pids < <(pgrep -f "$ere_pattern" || true)
    if [[ ${#pids[@]} -eq 0 ]]; then
        echo "$display_name is not running."
        return 0
    fi

    echo "Stopping $display_name..."
    sudo kill -TERM "${pids[@]}" 2>/dev/null || true

    local i
    for i in $(seq 1 15); do
        mapfile -t pids < <(pgrep -f "$ere_pattern" || true)
        if [[ ${#pids[@]} -eq 0 ]]; then
            echo "$display_name stopped successfully."
            return 0
        fi
        sleep 1
    done

    mapfile -t pids < <(pgrep -f "$ere_pattern" || true)
    echo "$display_name still running after SIGTERM; sending SIGKILL..."
    if [[ ${#pids[@]} -gt 0 ]]; then
        sudo kill -KILL "${pids[@]}" 2>/dev/null || true
    fi
    sleep 1
    if pgrep -f "$ere_pattern" > /dev/null; then
        echo "Failed to stop $display_name. PIDs still matching pattern; try: sudo ps aux | grep -E 'tpm2-abrmd|tpm_server'"
        exit 1
    fi
    echo "$display_name stopped successfully."
}

# Match tpm-start.sh: tpm2-abrmd … --tcti=mssim (argv0 bare or under /usr/bin, etc.).
stop_matching_cmdline '^(tpm2-abrmd|/usr/bin/tpm2-abrmd|/usr/sbin/tpm2-abrmd|/usr/local/bin/tpm2-abrmd)[[:space:]]+.*--tcti=mssim' 'tpm2-abrmd'

# Match tpm-start.sh: tpm_server -v
stop_matching_cmdline '^(tpm_server|/usr/bin/tpm_server|/usr/sbin/tpm_server|/usr/local/bin/tpm_server)[[:space:]]+-v' 'the TPM simulator'

# Delete NVChip if requested
if [ "$CLEAN_NVCHIP" = true ]; then
    NVCHIP_PATH="${TPMKIT_STATE_DIR}/ibmtpm/NVChip"
    if [ -f "$NVCHIP_PATH" ]; then
        echo "Deleting NVChip file to reset TPM state..."
        rm -f "$NVCHIP_PATH"
        echo "NVChip deleted successfully."
    else
        echo "NVChip file not found at $NVCHIP_PATH"
    fi
fi
