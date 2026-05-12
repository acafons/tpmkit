#!/usr/bin/env bash
#
# Stop the tpmkit dev container and remove it by default (docker stop + docker rm).
# Complements scripts/run-tpmkit-docker.sh. Use --keep to stop only and leave the container.
#
# Environment:
#   TPMKIT_CONTAINER_NAME   Container name (default: tpmkit-dev)
#
# Exit codes:
#   0   Desired state reached (stopped+removed by default, or absent; see --keep)
#   3   Bad CLI usage
#   4   Docker daemon not reachable
#   5   docker stop / docker rm failed unexpectedly

set -euo pipefail

CONTAINER_NAME="${TPMKIT_CONTAINER_NAME:-tpmkit-dev}"
REMOVE=true
QUIET=false

usage() {
    echo "Usage: ${0##*/} [--keep|-k] [-q|--quiet] [-h|--help]"
    echo ""
    echo "  By default: docker stop then docker rm (fresh run next time: run-tpmkit-docker.sh)."
    echo "  --keep, -k   Stop only; do not remove the container."
    echo "  -q       No informational messages on stdout (errors still on stderr)."
    echo ""
    echo "Exit: 0=success, 3=usage, 4=docker unreachable, 5=docker command failed"
    echo "Env:  TPMKIT_CONTAINER_NAME (default: tpmkit-dev)"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --keep|-k)
            REMOVE=false
            shift
            ;;
        --rm)
            # Back-compat: removal is now default; --rm is a no-op.
            shift
            ;;
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
    [[ "$QUIET" != true ]] && echo "Container '$CONTAINER_NAME' does not exist (nothing to stop)."
    exit 0
fi

running="$(docker inspect -f '{{.State.Running}}' "$CONTAINER_NAME")"
if [[ "$running" == "true" ]]; then
    [[ "$QUIET" != true ]] && echo "Stopping container '$CONTAINER_NAME'..."
    if ! docker stop "$CONTAINER_NAME" >/dev/null; then
        echo "docker stop failed for '$CONTAINER_NAME'." >&2
        exit 5
    fi
    [[ "$QUIET" != true ]] && echo "Container '$CONTAINER_NAME' stopped."
else
    [[ "$QUIET" != true ]] && echo "Container '$CONTAINER_NAME' was already stopped."
fi

if [[ "$REMOVE" == true ]]; then
    [[ "$QUIET" != true ]] && echo "Removing container '$CONTAINER_NAME'..."
    if ! docker rm "$CONTAINER_NAME" >/dev/null; then
        echo "docker rm failed for '$CONTAINER_NAME'." >&2
        exit 5
    fi
    [[ "$QUIET" != true ]] && echo "Container '$CONTAINER_NAME' removed."
fi

exit 0
