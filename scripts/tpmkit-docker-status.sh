#!/usr/bin/env bash
#
# Report whether the tpmkit dev container exists and is running.
# Intended for humans, CI, and agents (use exit codes; optional --quiet).
#
# Environment:
#   TPMKIT_CONTAINER_NAME   Container name (default: tpmkit-dev)
#
# Exit codes:
#   0   Container exists and State.Running is true
#   1   Container exists but is stopped
#   2   No container with that name
#   3   Bad CLI usage
#   4   Docker daemon not reachable (docker info fails)

set -euo pipefail

CONTAINER_NAME="${TPMKIT_CONTAINER_NAME:-tpmkit-dev}"
QUIET=false

usage() {
    echo "Usage: ${0##*/} [-q|--quiet] [-h|--help]"
    echo ""
    echo "Exit: 0=running, 1=stopped, 2=missing, 3=usage, 4=docker unreachable"
    echo "Env:  TPMKIT_CONTAINER_NAME (default: tpmkit-dev)"
    echo "Stop: scripts/stop-tpmkit-docker.sh (default removes container; --keep to retain)"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -q|--quiet)
            QUIET=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 3
            ;;
    esac
done

if ! docker info >/dev/null 2>&1; then
    [[ "$QUIET" != true ]] && echo "Docker is not running or not reachable (docker info failed)." >&2
    exit 4
fi

if ! docker inspect "$CONTAINER_NAME" &>/dev/null; then
    [[ "$QUIET" != true ]] && echo "Container '$CONTAINER_NAME' does not exist." >&2
    exit 2
fi

running="$(docker inspect -f '{{.State.Running}}' "$CONTAINER_NAME")"
if [[ "$running" != "true" ]]; then
    [[ "$QUIET" != true ]] && echo "Container '$CONTAINER_NAME' exists but is not running." >&2
    exit 1
fi

[[ "$QUIET" != true ]] && echo "Container '$CONTAINER_NAME' is running."
exit 0
