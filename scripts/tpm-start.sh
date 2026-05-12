#!/bin/bash
#
# Starts IBM swtpm (tpm_server), system D-Bus, and tpm2-abrmd.
# Intended for use inside the tpmkit Docker image (or a similar environment).
#
# State, simulator files, and logs live under TPMKIT_STATE_DIR (default: $HOME/.tpm-state).
# Mount a host directory to that path (e.g. /home/app/.tpm-state) to inspect logs on the host.

set -e

# Linux OCI container (Docker/Podman/etc.): /.dockerenv, podman marker, or cgroup hint.
# Not a perfect "tpmkit image" detector, but blocks accidental use on macOS/Windows host shells.
is_linux_oci_container() {
    [[ -f /.dockerenv ]] && return 0
    [[ -f /run/.containerenv ]] && return 0
    [[ -r /proc/1/cgroup ]] || return 1
    grep -qE '(docker|moby|containerd|libpod|lxc|kubepods)' /proc/1/cgroup 2>/dev/null
}

if [[ ${TPMKIT_ALLOW_NONCONTAINER:-0} != 1 ]] && ! is_linux_oci_container; then
    echo "tpm-start.sh is intended to run inside a Linux container (e.g. tpmkit image)." >&2
    echo "Run: docker exec -it tpmkit-dev tpm-start.sh" >&2
    echo "Override (not recommended): TPMKIT_ALLOW_NONCONTAINER=1 $0 $*" >&2
    exit 1
fi

TPMKIT_STATE_DIR="${TPMKIT_STATE_DIR:-$HOME/.tpm-state}"
HOME_DIR="$TPMKIT_STATE_DIR"
TPM_SERVER_DATA_DIR=${HOME_DIR}/ibmtpm
DBUS_DIR=/run/dbus

# Match real daemons by cmdline (same idea as tpm-stop.sh). pgrep -x matches /proc/comm and can
# see stale zombie rows, so we would wrongly skip starting tpm_server / tpm2-abrmd.
PTN_TPM_SERVER='^(tpm_server|/usr/bin/tpm_server|/usr/sbin/tpm_server|/usr/local/bin/tpm_server)[[:space:]]+-v'
PTN_TPM2_ABRMD='^(tpm2-abrmd|/usr/bin/tpm2-abrmd|/usr/sbin/tpm2-abrmd|/usr/local/bin/tpm2-abrmd)[[:space:]]+.*--tcti=mssim'

LOG_DIR=${HOME_DIR}/log
TPM_SERVER_LOG_DIR=${LOG_DIR}/ibmtpm
TPM_ABRMD_LOG_DIR=${LOG_DIR}/tpm2-abrmd
DBUS_LOG_DIR=${LOG_DIR}/dbus

echo "Using TPMKIT_STATE_DIR=${HOME_DIR}"

# Create necessary directories
echo "Creating directories..."
mkdir -p ${TPM_SERVER_DATA_DIR}
mkdir -p ${TPM_SERVER_LOG_DIR}
mkdir -p ${TPM_ABRMD_LOG_DIR}
mkdir -p ${DBUS_LOG_DIR}

# Start the TPM simulator (setsid: avoid SIGHUP when `docker exec -it … tpm-start.sh` bash exits)
if ! pgrep -f "$PTN_TPM_SERVER" > /dev/null; then
    echo "Starting the TPM simulator..."
    cd $TPM_SERVER_DATA_DIR
    /usr/bin/setsid tpm_server -v >${TPM_SERVER_LOG_DIR}/tpm_server.log 2>&1 &
    TPM_SERVER_PID=$!
    sleep 2  # Allow time for initialization

    # Check if the process is still running
    if ! ps -p $TPM_SERVER_PID > /dev/null; then
        echo "Failed to start tpm_server. Check logs at ${TPM_SERVER_LOG_DIR}/tpm_server.log"
        tail -n 20 ${TPM_SERVER_LOG_DIR}/tpm_server.log
        exit 1
    fi
else
    echo "tpm_server is running."
fi

# Start the D-Bus daemon
sudo mkdir -p ${DBUS_DIR}

# Clean up stale dbus socket if it exists
if ! pgrep -x "dbus-daemon" > /dev/null; then
    if [ -e ${DBUS_DIR}/system_bus_socket ]; then
        echo "Cleaning up existing dbus socket..."
        sudo rm -f ${DBUS_DIR}/system_bus_socket
    fi

    if [ -e ${DBUS_DIR}/pid ]; then
        echo "Cleaning up existing dbus pid..."
        sudo rm -f ${DBUS_DIR}/pid
    fi

    # Start dbus-daemon if not already running (setsid: detach from docker exec tty session)
    echo "Starting dbus-daemon..."
    sudo /usr/bin/setsid dbus-daemon --system --fork >${DBUS_LOG_DIR}/dbus-daemon.log 2>&1

    # Check if the dbus-daemon command failed
    if [ $? -ne 0 ]; then
        echo "Failed to start dbus-daemon. Check logs at ${DBUS_LOG_DIR}/dbus-daemon.log"
        tail -n 20 ${DBUS_LOG_DIR}/dbus-daemon.log
        exit 1
    fi
else
    echo "dbus-daemon is already running."
fi

# Start the TPM2 Access Broker and Resource Management Daemon
if ! pgrep -f "$PTN_TPM2_ABRMD" > /dev/null; then
    echo "Starting the TPM2 Access Broker and Resource Management Daemon..."
    # sudo -u tss G_MESSAGES_DEBUG=all tpm2-abrmd --tcti=mssim > ${TPM_ABRMD_LOG_DIR}/tpm2-abrmd.log 2>&1 &
    /usr/bin/setsid env G_MESSAGES_DEBUG=all tpm2-abrmd --allow-root --tcti=mssim >${TPM_ABRMD_LOG_DIR}/tpm2-abrmd.log 2>&1 &
    TPM2_ABRMD_PID=$!
    sleep 2  # Allow time for initialization

    # Check if the process is still running
    if ! ps -p $TPM2_ABRMD_PID > /dev/null; then
        echo "Failed to start tpm2-abrmd. Check logs at ${TPM_ABRMD_LOG_DIR}/tpm2-abrmd.log"
        tail -n 20 ${TPM_ABRMD_LOG_DIR}/tpm2-abrmd.log
        exit 1
    fi
else
    echo "tpm2-abrmd is already running."
fi

sudo chown app:messagebus /run/dbus/system_bus_socket
sudo chmod 660 /run/dbus/system_bus_socket

# echo "Setting up environment variables..."
# export TSS2_FAPICONF=$(pwd)/dist/fapi/fapi-config.json

if ! pgrep -f "$PTN_TPM_SERVER" > /dev/null; then
    echo "ERROR: tpm_server is not running after start. See ${TPM_SERVER_LOG_DIR}/tpm_server.log" >&2
    exit 1
fi
if ! pgrep -f "$PTN_TPM2_ABRMD" > /dev/null; then
    echo "ERROR: tpm2-abrmd is not running after start. See ${TPM_ABRMD_LOG_DIR}/tpm2-abrmd.log" >&2
    exit 1
fi

echo "All services started successfully."
