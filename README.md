# DC Res Patch Depth Fix

Fixes DriveClub rendering above its intended 1080p.

The half-res depth buffer is filled up to a hardcoded 960x540. Above 1080p the
buffer is bigger than that, so most of it is never filled. This makes it read
the real size instead.

Press Select game patch folder, then Apply the fix. Revert puts the game's
own values back, byte for byte.

Works on a game whose files are loose and on one still in its archives. A
packed game has the archive written again with the changed pack in its place,
which takes a moment; the index is updated to match and both are only swapped
in once they are whole.

Linux gets an AppImage, macOS an app with its libraries inside. Neither needs
anything installed.

If a shell script here ever loses its executable bit in git, put it back with

    git update-index --chmod=+x .github/workflows/scripts/linux-appimage.sh

Format research: Nenkai (DriveClubFS, MIT), RokkuDayo.
