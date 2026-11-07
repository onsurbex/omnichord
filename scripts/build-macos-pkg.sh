#!/bin/bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Este comando necesita macOS y las herramientas pkgbuild y codesign." >&2
    exit 1
fi

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(dirname "$script_dir")"
build_dir="${1:-build}"
output_dir="${2:-.}"
vst_bundle="$build_dir/Omnichord_artefacts/Release/VST3/Omnichord.vst3"
app_bundle="$build_dir/Omnichord_artefacts/Release/Standalone/Omnichord.app"
codesign --verify --deep --strict "$vst_bundle"
codesign --verify --deep --strict "$app_bundle"
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$app_bundle/Contents/Info.plist")"
icon_name="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIconFile' "$app_bundle/Contents/Info.plist")"
app_icon="$app_bundle/Contents/Resources/$icon_name"
test -n "$icon_name"
test -s "$app_icon"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
package_file="$output_dir/Omnichord-$version.pkg"

pkg_stage="$(mktemp -d "${TMPDIR:-/tmp}/omnichord-pkg.XXXXXX")"
trap 'rm -rf "$pkg_stage"' EXIT
# Decode the actual bundle icon using Apple's tool, including its Retina size.
iconutil --convert iconset --output "$pkg_stage/Omnichord.iconset" "$app_icon"
test -s "$pkg_stage/Omnichord.iconset/icon_512x512@2x.png"
payload="$pkg_stage/root"
plugin_dir="$payload/Library/Audio/Plug-Ins/VST3"
app_dir="$payload/Applications"
docs_dir="$payload/Library/Application Support/Omnichord"
mkdir -p "$plugin_dir" "$app_dir" "$docs_dir"
ditto "$vst_bundle" "$plugin_dir/Omnichord.vst3"
ditto "$app_bundle" "$app_dir/Omnichord.app"
cp "$repo_dir/THIRD_PARTY_NOTICES.md" "$docs_dir/"
cp "$repo_dir/README.md" "$docs_dir/"
cp -R "$repo_dir/licenses" "$docs_dir/"

pkgbuild --analyze --root "$payload" "$pkg_stage/components.plist"
python3 - "$pkg_stage/components.plist" <<'PY'
import plistlib
import sys

path = sys.argv[1]
with open(path, 'rb') as stream:
    components = plistlib.load(stream)
for component in components:
    # Install to the declared folders even if a copy exists in Downloads.
    component['BundleIsRelocatable'] = False
    # Allow reinstalling the same version, including builds without the editor.
    component['BundleIsVersionChecked'] = False
    component['BundleOverwriteAction'] = 'upgrade'
with open(path, 'wb') as stream:
    plistlib.dump(components, stream)
PY

pkgbuild --root "$payload" \
    --identifier audio.chordharp.instrument.pkg \
    --version "$version" \
    --install-location / \
    --ownership recommended \
    --component-plist "$pkg_stage/components.plist" \
    "$package_file"

# Check the actual installer payload, including signatures after packaging.
pkgutil --expand-full "$package_file" "$pkg_stage/expanded"
codesign --verify --deep --strict --verbose=2 \
    "$pkg_stage/expanded/Payload/Library/Audio/Plug-Ins/VST3/Omnichord.vst3"
codesign --verify --deep --strict --verbose=2 \
    "$pkg_stage/expanded/Payload/Applications/Omnichord.app"
cmp "$app_icon" "$pkg_stage/expanded/Payload/Applications/Omnichord.app/Contents/Resources/$icon_name"
echo "Instalador generado: $package_file"
