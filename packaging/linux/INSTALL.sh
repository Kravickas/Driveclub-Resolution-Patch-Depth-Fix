#!/bin/sh
# Puts it in your menu so it can be clicked. Nothing goes outside your home
# folder. Run it with:   sh INSTALL.sh
set -e
here=$(dirname "$0")
cd "$here"
BIN="${HOME}/.local/bin"
APPS="${HOME}/.local/share/applications"
mkdir -p "$BIN" "$APPS"
# an archive unpacked on Windows loses the mark that says a file may be run,
# so put it back rather than leaving a program that will not start
chmod +x DC-Res-Patch-Depth-Fix DC-Res-Patch-Depth-Fix-terminal 2>/dev/null || true
install -m 755 DC-Res-Patch-Depth-Fix "$BIN/"
install -m 755 DC-Res-Patch-Depth-Fix-terminal "$BIN/"
install -m 644 dc-res-patch-depth-fix.desktop "$APPS/"
update-desktop-database "$APPS" 2>/dev/null || true
echo ""
echo "Installed. Look for DC Res Patch Depth Fix in your menu."
echo "If it is not there yet, log out and back in."
