#!/bin/bash -ex

GITDIR="$(git rev-parse --show-toplevel)"
APPIMAGEDIR="$GITDIR/AppDir"
EXE="$GITDIR/DC-Res-Patch-Depth-Fix"

rm -rf "$APPIMAGEDIR"
mkdir -p "$APPIMAGEDIR/usr/bin"
cp "$EXE" "$APPIMAGEDIR/usr/bin/"

wget -q -O linuxdeploy https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -q -O linuxdeploy-plugin-gtk.sh https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh
chmod +x linuxdeploy linuxdeploy-plugin-gtk.sh

export DEPLOY_GTK_VERSION=3
export PATH="$PWD:$PATH"

./linuxdeploy --appdir "$APPIMAGEDIR" \
  -e "$EXE" \
  -d "$GITDIR/packaging/linux/dc-res-patch-depth-fix.desktop" \
  -i "$GITDIR/dc-res-patch-depth-fix.png" \
  --plugin gtk --output appimage

mv DC*.AppImage DC-Res-Patch-Depth-Fix.AppImage
