#!/usr/bin/env bash
#
# Run a shell command inside the tpmkit dev container (same default name as
# scripts/run-tpmkit-docker.sh). The command is executed with sh -c so pipes,
# redirections, and shell operators work as in the user's example.
#
# Environment:
#   TPMKIT_CONTAINER_NAME   Container name (default: tpmkit-dev)
#   TPMKIT_CONTAINER_REPO   In-container repository path (default: /workspace/tpmkit)
#   TPMKIT_DOCKER_USER      Optional docker exec user (for example root)
#
# Examples:
#   exec-tpmkit-docker.sh 'ps aux | grep -i tpm'
#   exec-tpmkit-docker.sh ps aux
#   exec-tpmkit-docker.sh bash -lc 'tpm2_getcap properties-fixed'

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CONTAINER_NAME="${TPMKIT_CONTAINER_NAME:-tpmkit-dev}"
CONTAINER_REPO="${TPMKIT_CONTAINER_REPO:-/workspace/tpmkit}"
DOCKER_USER="${TPMKIT_DOCKER_USER:-}"

usage() {
    echo "Usage: ${0##*/} [-h|--help] [--] <command...>"
    echo ""
    echo "Runs <command...> inside container '${CONTAINER_NAME}' via:"
    echo "  docker exec ... sh -c <quoted-command>"
    echo ""
    echo "Use one shell string for pipelines:"
    echo "  ${0##*/} 'ps aux | grep -i tpm'"
    echo ""
    echo "Context: ${REPO_ROOT}"
    echo "Env:     TPMKIT_CONTAINER_NAME (default: tpmkit-dev), TPMKIT_CONTAINER_REPO (default: /workspace/tpmkit), TPMKIT_DOCKER_USER"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h | --help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        -*)
            echo "Unknown option: $1" >&2
            echo "Try: ${0##*/} --help" >&2
            exit 1
            ;;
        *)
            break
            ;;
    esac
done

if [[ $# -eq 0 ]]; then
    echo "Error: no command given." >&2
    echo "Example: ${0##*/} 'ps aux | grep -i tpm'" >&2
    exit 1
fi

# -i/-t only when attached to a TTY (avoids broken pipes in scripts / CI).
docker_exec_opts=()
[[ -t 0 ]] && docker_exec_opts+=(-i)
[[ -t 1 ]] && docker_exec_opts+=(-t)

if ! docker inspect "$CONTAINER_NAME" &>/dev/null; then
    echo "Error: container '$CONTAINER_NAME' not found." >&2
    echo "Start it with: ${SCRIPT_DIR}/run-tpmkit-docker.sh" >&2
    exit 1
fi

running="$(docker inspect -f '{{.State.Running}}' "$CONTAINER_NAME")"
if [[ "$running" != "true" ]]; then
    echo "Error: container '$CONTAINER_NAME' is not running." >&2
    echo "Start it with: ${SCRIPT_DIR}/run-tpmkit-docker.sh" >&2
    exit 1
fi

if [[ $# -eq 1 ]]; then
    remote_cmd=$1
else
    remote_cmd=$(printf '%q ' "$@")
    remote_cmd=${remote_cmd%"${remote_cmd##*[![:space:]]}"} # trim trailing whitespace
fi

user_opts=()
if [[ -n "$DOCKER_USER" ]]; then
    user_opts=(-u "$DOCKER_USER")
fi

if ! docker exec "$CONTAINER_NAME" test -d "$CONTAINER_REPO" >/dev/null 2>&1; then
    echo "Error: repository path '$CONTAINER_REPO' is not available in container '$CONTAINER_NAME'." >&2
    echo "Recreate the container with: ${SCRIPT_DIR}/stop-tpmkit-docker.sh && ${SCRIPT_DIR}/run-tpmkit-docker.sh" >&2
    exit 1
fi

if [[ ${#docker_exec_opts[@]} -gt 0 ]]; then
    if [[ ${#user_opts[@]} -gt 0 ]]; then
        exec docker exec "${docker_exec_opts[@]}" "${user_opts[@]}" -w "$CONTAINER_REPO" "$CONTAINER_NAME" sh -c "$remote_cmd"
    fi

    exec docker exec "${docker_exec_opts[@]}" -w "$CONTAINER_REPO" "$CONTAINER_NAME" sh -c "$remote_cmd"
fi

if [[ ${#user_opts[@]} -gt 0 ]]; then
    exec docker exec "${user_opts[@]}" -w "$CONTAINER_REPO" "$CONTAINER_NAME" sh -c "$remote_cmd"
fi

exec docker exec -w "$CONTAINER_REPO" "$CONTAINER_NAME" sh -c "$remote_cmd"
