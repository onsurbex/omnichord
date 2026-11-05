#!/bin/bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
source_bundle="${1:-$script_dir/Omnichord.vst3}"
plugin_dir="${OMNICHORD_VST3_DIR:-$HOME/Library/Audio/Plug-Ins/VST3}"
destination="$plugin_dir/Omnichord.vst3"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Este instalador necesita macOS." >&2
    exit 1
fi
if [[ ! -d "$source_bundle" ]]; then
    echo "No se encuentra Omnichord.vst3. Coloca el instalador junto al plugin." >&2
    exit 1
fi
codesign --verify --deep --strict "$source_bundle"
mkdir -p "$plugin_dir"
if [[ -e "$destination" && ! -d "$destination" ]]; then
    echo "El destino existe y no es un bundle VST3: $destination" >&2
    exit 1
fi

# Stage the complete bundle so an older installation cannot leave stale files.
install_stage="$(mktemp -d "$plugin_dir/.omnichord-install.XXXXXX")"
finish_install() {
    result=$?
    trap - EXIT
    if [[ "$result" != 0 && -d "$install_stage/previous.vst3" ]]; then
        if [[ -e "$destination" ]]; then
            if ! mv "$destination" "$install_stage/failed.vst3"; then
                echo "La copia anterior se conserva en $install_stage/previous.vst3" >&2
                exit "$result"
            fi
        fi
        if ! mv "$install_stage/previous.vst3" "$destination"; then
            echo "La copia anterior se conserva en $install_stage/previous.vst3" >&2
            exit "$result"
        fi
    fi
    rm -rf "$install_stage"
    exit "$result"
}
trap finish_install EXIT
ditto "$source_bundle" "$install_stage/Omnichord.vst3"
codesign --verify --deep --strict "$install_stage/Omnichord.vst3"
if [[ -d "$destination" ]]; then
    mv "$destination" "$install_stage/previous.vst3"
fi
mv "$install_stage/Omnichord.vst3" "$destination"
codesign --verify --deep --strict --verbose=2 "$destination"
echo "Omnichord instalado con la firma verificada en:"
echo "$destination"
echo "Vuelve a explorar los plugins en Ableton Live."
