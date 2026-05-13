#!/usr/bin/env bash
#
# Build the tpmkit Docker image (Ubuntu + IBM TPM2 simulator, TSS, abrmd, tools).
# Intended for use with scripts/run-tpmkit-docker.sh.
#
# Environment overrides:
#   TPMKIT_IMAGE      Image tag to assign (default: tpmkit:latest)
#   TPMKIT_DOCKERFILE Path to Dockerfile (default: <repo>/Dockerfile)
#
# Any additional arguments are passed through to `docker build` before the context
# path, for example:  ./build-tpmkit-docker.sh --no-cache

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE="${TPMKIT_IMAGE:-tpmkit:latest}"
DOCKERFILE="${TPMKIT_DOCKERFILE:-$REPO_ROOT/Dockerfile}"

usage() {
    echo "Usage: ${0##*/} [--help] [docker build options...]"
    echo ""
    echo "Builds image '$IMAGE' from '$DOCKERFILE' with build context '$REPO_ROOT'."
    echo "Override the tag with TPMKIT_IMAGE, the Dockerfile path with TPMKIT_DOCKERFILE."
    echo ""
    echo "Examples:"
    echo "  ${0##*/}"
    echo "  ${0##*/} --no-cache"
    echo "  TPMKIT_IMAGE=tpmkit:dev ${0##*/}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h | --help)
            usage
            exit 0
            ;;
        *)
            break
            ;;
    esac
done

if [[ ! -f "$DOCKERFILE" ]]; then
    echo "Error: Dockerfile not found: $DOCKERFILE" >&2
    exit 1
fi

echo "Building image '$IMAGE' (context: $REPO_ROOT, dockerfile: $DOCKERFILE)..."
docker build -f "$DOCKERFILE" -t "$IMAGE" "$@" "$REPO_ROOT"
