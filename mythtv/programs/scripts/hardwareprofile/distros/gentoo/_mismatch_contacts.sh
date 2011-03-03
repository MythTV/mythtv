#!/usr/bin/env bash
get_overlay_contact() {
    overlay=$1
    layman -i "${overlay}" | fgrep 'Contact :' | grep -o '[^ ]\+@[^ ]\+'
}

echo 'missing'
for overlay in $(python _mismatch.py |& fgrep 'lacks' |& sed -e 's|^ERROR: Overlay "||' -e 's|" lacks repo_name entry$||'); do
    contact=$(get_overlay_contact "${overlay}")
    echo "  ${overlay} overlay contact <${contact}>"
done
echo

echo 'mismatch'
while read overlay ; do
    contact=$(get_overlay_contact "${overlay}")
    echo "  ${overlay} overlay contact <${contact}>"
done < <(python _mismatch.py 2>/dev/null | grep -o '# [^ ]\+$' | sed 's|^# ||')
