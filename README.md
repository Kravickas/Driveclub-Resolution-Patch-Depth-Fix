# DC Res Patch Depth Fix

Fixes DriveClub rendering above its intended 1080p.

The half-res depth buffer is filled up to a hardcoded 960x540. Above 1080p the
buffer is bigger than that, so most of it is never filled. This makes it read
the real size instead.

On Linux and macOS the downloaded files are not executable, because a zip
cannot carry that. Run `chmod +x` on them, or on Linux run `sh INSTALL.sh`
which does it and puts the app in your menu.

Format research: Nenkai (DriveClubFS, MIT), RokkuDayo.
