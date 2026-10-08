#!/usr/bin/env bash
set -euo pipefail
qpitch_source_dir="$(cd "$(dirname "$0")/../.." && pwd)"
qpitch_build_dir="${1:-${qpitch_source_dir}/build-linux-v2}"
mkdir -p "$qpitch_build_dir"
qpitch_build_dir="$(cd "$qpitch_build_dir" && pwd)"
qpitch_container_engine="${QPITCH_CONTAINER_ENGINE:-}"
if [[ -z "$qpitch_container_engine" ]]; then
    if command -v podman >/dev/null; then qpitch_container_engine=podman; else qpitch_container_engine=docker; fi
fi
"$qpitch_container_engine" build -t qpitch-linux-builder:bookworm -f "$qpitch_source_dir/tools/linux/Dockerfile" "$qpitch_source_dir/tools/linux"
qpitch_container_args=(--rm --init --security-opt label=disable -v "$qpitch_source_dir:/src:ro" -v "$qpitch_build_dir:/build:rw")
if [[ "$qpitch_container_engine" == podman ]]; then
    qpitch_container_args+=(--userns=keep-id)
else
    qpitch_container_args+=(--user "$(id -u):$(id -g)")
fi
"$qpitch_container_engine" run "${qpitch_container_args[@]}" qpitch-linux-builder:bookworm bash -euc '
    cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build /build --target QPitch_VST3 QPitch_CLAP qpitch_shifter_test qpitch_detector_test qpitch_editor_test qpitch_clap_load_test -j2
    xvfb-run -a ctest --test-dir /build --output-on-failure
    xvfb-run -a /build/qpitch_editor_test /build/ui-check
'
python3 "$qpitch_source_dir/tools/linux/check_abi.py" "$qpitch_build_dir/QPitch_artefacts/Release"
