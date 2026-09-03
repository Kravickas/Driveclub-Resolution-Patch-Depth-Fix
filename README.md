# DC Res Patch Depth Fix

Fixes DriveClub rendering above its intended 1080p.

The half-res depth buffer is filled up to a hardcoded 960x540. Above 1080p the
buffer is bigger than that, so most of it is never filled. This makes it read
the real size instead.

Linux gets an AppImage, macOS an app with its libraries inside. Neither needs
anything installed.

Format research: Nenkai (DriveClubFS, MIT), RokkuDayo.
