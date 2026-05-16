#!/usr/bin/env bash
#
# Run (or restart) the tpmkit Docker image with a host directory mounted for TPM state and logs.
# Default host path: <repo>/.tpm-state  ->  container: /home/app/.tpm-state (same as TPMKIT_STATE_DIR in the image).
# The repository is mounted read/write at /workspace/tpmkit for build and test commands.
#
# Environment overrides:
#   TPMKIT_IMAGE            Image reference (default: tpmkit:latest)
#   TPMKIT_CONTAINER_NAME   Container name (default: tpmkit-dev)
#   TPMKIT_HOST_DATA        Host directory to bind-mount (default: <repo>/.tpm-state)
#   TPMKIT_CONTAINER_REPO   In-container repository path (default: /workspace/tpmkit)
#   NO_COLOR                 Set (any value) to disable ANSI highlight for "already running"

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE="${TPMKIT_IMAGE:-tpmkit:latest}"
CONTAINER_NAME="${TPMKIT_CONTAINER_NAME:-tpmkit-dev}"
HOST_DATA="${TPMKIT_HOST_DATA:-$REPO_ROOT/.tpm-state}"
CONTAINER_STATE_DIR="/home/app/.tpm-state"
CONTAINER_REPO="${TPMKIT_CONTAINER_REPO:-/workspace/tpmkit}"

# Bold yellow on stdout when interactive; respect https://no-color.org/
emit_highlight() {
    local msg=$1
    if [[ -t 1 && -z ${NO_COLOR+x} ]]; then
        printf '\033[1;33m%s\033[0m\n' "$msg"
    else
        printf '%s\n' "$msg"
    fi
}

DETACH=true
while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--detach)
            DETACH=true
            shift
            ;;
        -f|--foreground)
            DETACH=false
            shift
            ;;
        -h|--help)
            echo "Usage: ${0##*/} [-d|--detach] [-f|--foreground] [-h|--help]"
            echo ""
            echo "Creates ${HOST_DATA} on the host (if needed) and mounts it at ${CONTAINER_STATE_DIR}"
            echo "so tpm-start.sh logs and NVChip are visible under the repo's .tpm-state/ tree."
            echo "Mounts ${REPO_ROOT} at ${CONTAINER_REPO} for build and test commands."
            echo "Uses docker run --init so PID 1 reaps zombie children (needed for tpm_server / tpm2-abrmd)."
            echo ""
            echo "See also: scripts/stop-tpmkit-docker.sh, scripts/tpmkit-docker-status.sh"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Try: ${0##*/} --help" >&2
            exit 1
            ;;
    esac
done

mkdir -p "$HOST_DATA"

if docker inspect "$CONTAINER_NAME" &>/dev/null; then
    running="$(docker inspect -f '{{.State.Running}}' "$CONTAINER_NAME")"
    if [[ "$running" == "true" ]]; then
        emit_highlight "Container '$CONTAINER_NAME' is already running."
    else
        echo "Starting existing container '$CONTAINER_NAME'..."
        docker start "$CONTAINER_NAME"
    fi
    init_on="$(docker inspect -f '{{.HostConfig.Init}}' "$CONTAINER_NAME" 2>/dev/null || echo false)"
    if [[ "$init_on" != "true" ]]; then
        echo "Note: this container was created without --init (no zombie reaper). For a clean TPM stack, recreate:" >&2
        echo "  docker rm -f $CONTAINER_NAME && $0" >&2
    fi
else
    if ! docker image inspect "$IMAGE" &>/dev/null; then
        echo "Error: Docker image '$IMAGE' not found." >&2
        echo "Build it from the repository root, for example:" >&2
        echo "  docker build -t \"$IMAGE\" \"$REPO_ROOT\"" >&2
        exit 1
    fi
    echo "Creating container '$CONTAINER_NAME' from image '$IMAGE'..."
    # PID 1 reaps orphaned children; without this, `tail` as CMD leaves tpm_server/tpm2-abrmd zombies.
    run_args=(
        --init
        --name "$CONTAINER_NAME"
        -v "$REPO_ROOT:$CONTAINER_REPO"
        -v "$HOST_DATA:$CONTAINER_STATE_DIR"
        -e "TPMKIT_STATE_DIR=$CONTAINER_STATE_DIR"
        -e "TPMKIT_REPO_DIR=$CONTAINER_REPO"
        -w "$CONTAINER_REPO"
    )
    if [[ "$DETACH" == "true" ]]; then
        docker run -d "${run_args[@]}" "$IMAGE"
    else
        docker run -it "${run_args[@]}" "$IMAGE"
    fi
fi

echo ""
echo "Host data (logs, ibmtpm, NVChip): $HOST_DATA"
echo "Shell:    docker exec -it $CONTAINER_NAME bash"
echo "TPM up:   docker exec -it $CONTAINER_NAME tpm-start.sh"
echo "TPM down: docker exec -it $CONTAINER_NAME tpm-stop.sh"
echo "Check:    ${SCRIPT_DIR}/tpmkit-docker-status.sh"
echo "Stop host: ${SCRIPT_DIR}/stop-tpmkit-docker.sh   # default: stop + rm; use --keep to retain"
echo "(Container only runs the image; tpm-start.sh starts the simulator and tpm2-abrmd.)"
