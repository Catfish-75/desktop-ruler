#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf 'Usage: %s install|uninstall [--prefix ABSOLUTE_PATH]\n' "$0" >&2
}

action=${1:-}
if [[ "$action" != install && "$action" != uninstall ]]; then
    usage
    exit 2
fi
shift

prefix=${HOME:?HOME is not set}/.local
while (($#)); do
    case "$1" in
        --prefix)
            if (($# < 2)); then usage; exit 2; fi
            prefix=$2
            shift 2
            ;;
        *) usage; exit 2 ;;
    esac
done

if [[ "$prefix" != /* || "$prefix" == / ]]; then
    printf 'Install prefix must be an absolute path other than /: %s\n' "$prefix" >&2
    exit 2
fi
case "$prefix" in
    *[!a-zA-Z0-9_./+-]*)
        printf 'Prefix may contain only letters, digits, /, ., _, + and -: %s\n' "$prefix" >&2
        exit 2
        ;;
esac

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
binary=$prefix/bin/desktop-ruler
desktop=$prefix/share/applications/desktop-ruler.desktop
icon=$prefix/share/icons/hicolor/scalable/apps/desktop-ruler.svg
manifest=$prefix/share/desktop-ruler/install-manifest

sha256_file() {
    sha256sum -- "$1" | awk '{print $1}'
}

if [[ "$action" == uninstall ]]; then
    if [[ ! -f "$manifest" ]]; then
        printf 'No Desktop Ruler install manifest at %s\n' "$manifest" >&2
        exit 1
    fi
    format=$(awk -F= '$1 == "format" {print substr($0, index($0, "=") + 1)}' "$manifest")
    expected_binary=$(awk -F= '$1 == "binary_sha256" {print $2}' "$manifest")
    expected_desktop=$(awk -F= '$1 == "desktop_sha256" {print $2}' "$manifest")
    expected_icon=$(awk -F= '$1 == "icon_sha256" {print $2}' "$manifest")
    if [[ "$format" != desktop-ruler-install-v1 || ! "$expected_binary" =~ ^[[:xdigit:]]{64}$ || ! "$expected_desktop" =~ ^[[:xdigit:]]{64}$ || ! "$expected_icon" =~ ^[[:xdigit:]]{64}$ ]]; then
        printf 'Install manifest is invalid; nothing was removed.\n' >&2
        exit 1
    fi
    for pair in "$binary:$expected_binary" "$desktop:$expected_desktop" "$icon:$expected_icon"; do
        file=${pair%:*}
        expected=${pair##*:}
        if [[ ! -f "$file" || $(sha256_file "$file") != "$expected" ]]; then
            printf 'Installed file is missing or changed: %s\nNothing was removed.\n' "$file" >&2
            exit 1
        fi
    done
    rm -- "$binary" "$desktop" "$icon" "$manifest"
    printf 'Removed Desktop Ruler from %s\n' "$prefix"
    exit 0
fi

source_binary=$script_dir/desktop-ruler
if [[ ! -x "$source_binary" ]]; then
    printf 'Build the application first; executable not found: %s\n' "$source_binary" >&2
    exit 1
fi
version=$("$source_binary" --version 2>&1) || {
    printf 'Could not read the application version from %s\n' "$source_binary" >&2
    exit 1
}
if [[ ! "$version" =~ ^desktop-ruler\ [0-9]+(\.[0-9]+)*$ ]]; then
    printf 'Application identity/version is invalid: expected "desktop-ruler <version>".\n' >&2
    exit 1
fi

stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
install -m 0755 -- "$source_binary" "$stage/desktop-ruler"

escape_exec_path() {
    local value=$1
    value=${value//\\/\\\\}
    value=${value//\"/\\\"}
    value=${value//\`/\\\`}
    value=${value//\$/\\\$}
    value=${value//%/%%}
    printf '"%s"' "$value"
}

exec_path=$(escape_exec_path "$binary")
sed -e "s|^Exec=.*|Exec=$exec_path|" -e "s|^TryExec=.*|TryExec=$binary|" \
    "$script_dir/desktop-ruler.desktop" > "$stage/desktop-ruler.desktop"
install -m 0644 -- "$script_dir/desktop-ruler.svg" "$stage/desktop-ruler.svg"

binary_sha256=$(sha256_file "$stage/desktop-ruler")
desktop_sha256=$(sha256_file "$stage/desktop-ruler.desktop")
icon_sha256=$(sha256_file "$stage/desktop-ruler.svg")
cat > "$stage/install-manifest" <<EOF
format=desktop-ruler-install-v1
version=$version
binary_sha256=$binary_sha256
desktop_sha256=$desktop_sha256
icon_sha256=$icon_sha256
EOF

if [[ -e "$manifest" ]]; then
    old_binary=$(awk -F= '$1 == "binary_sha256" {print $2}' "$manifest")
    old_desktop=$(awk -F= '$1 == "desktop_sha256" {print $2}' "$manifest")
    old_icon=$(awk -F= '$1 == "icon_sha256" {print $2}' "$manifest")
    if [[ "$old_binary" == "$binary_sha256" && "$old_desktop" == "$desktop_sha256" && "$old_icon" == "$icon_sha256" && -f "$binary" && -f "$desktop" && -f "$icon" && $(sha256_file "$binary") == "$old_binary" && $(sha256_file "$desktop") == "$old_desktop" && $(sha256_file "$icon") == "$old_icon" ]]; then
        printf 'Desktop Ruler is already installed in %s\n' "$prefix"
        exit 0
    fi
    printf 'An existing Desktop Ruler install differs. Uninstall it first; no files were changed.\n' >&2
    exit 1
fi

for target in "$binary" "$desktop" "$icon"; do
    if [[ -e "$target" || -L "$target" ]]; then
        printf 'Refusing to replace an existing file without an install manifest: %s\n' "$target" >&2
        exit 1
    fi
done

install -d -- "$(dirname -- "$binary")" "$(dirname -- "$desktop")" "$(dirname -- "$icon")" "$(dirname -- "$manifest")"
install -m 0755 -- "$stage/desktop-ruler" "$binary"
install -m 0644 -- "$stage/desktop-ruler.desktop" "$desktop"
install -m 0644 -- "$stage/desktop-ruler.svg" "$icon"
install -m 0644 -- "$stage/install-manifest" "$manifest"
printf 'Installed %s to %s\n' "$version" "$prefix"
