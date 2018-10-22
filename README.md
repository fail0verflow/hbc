# The Homebrew Channel

This repository contains the public release of the source code for
The Homebrew Channel.

Included portions:

* The Homebrew Channel
* Reload stub
* Banner
* PyWii (includes Alameda for banner creation)
* WiiPAX (LZMA executable packer)

Not included:

* Installer

Note that the code in this repository differs from the source code used to build
the official version of The Homebrew Channel, which includes additional
protection features (i.e. we had to add reverse-DRM to stop scammers from
selling it).

This code is released with no warranty, and hasn't even been tested on a real
Wii, only under Dolphin (yes, this release runs under Dolphin).

## Build instructions

You need devkitPPC and libogc installed, and the DEVKITPRO/DEVKITPPC environment
variables correctly set. Use the latest available versions. Make sure you have
libogc/libfat, and also install the following 3rd party libraries:

* zlib
* libpng
* mxml
* freetype

You can obtain binaries of those with
[devkitPro pacman](https://devkitpro.org/wiki/devkitPro_pacman). Simply use

    sudo (dkp-)pacman -S ppc-zlib ppc-libpng ppc-mxml ppc-freetype

Additionally, you'll need the following packages on your host machine:

* pycryptodomex (for PyWii)
* libpng headers (libpng-dev)
* gettext
* sox

The build process has only been tested on Linux. You're on your own if you
want to try building this on OSX or Windows.

You'll need the Wii common key installed as ~/.wii/common-key.

First run 'make' in wiipax, then 'make' in channel. You'll find a .wad file
that you can install or directly run with Dolphin under
channel/title/channel_retail.wad. You'll also find executable binaries under
channel/channelapp, but be advised that the NAND save file / theme storage
features won't work properly if HBC isn't launched as a channel with its
correct title identity/permissions.

## License

Unless otherwise noted in an individual file header, all source code in this
repository is released under the terms of the GNU General Public License,
version 2 or later. The full text of the license can be found in the COPYING
file.
