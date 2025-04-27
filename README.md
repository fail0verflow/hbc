# ARCHIVED

This repository is archived and will not accept any further contributions.

Like most Wii homebrew software, this software depends on [libogc](https://github.com/devkitPro/libogc).
After development of The Homebrew Channel had already started, we discovered that large portions of libogc
were stolen directly from the Nintendo SDK or games using the Nintendo SDK (decompiled and cleaned up).
We thought that at least significant parts of libogc, such as its threading implementation, were original,
and reluctantly continued to use the project while distancing ourselves from it.

It has recently been revealed that the threading/OS implementation in libogc is, in fact,
[stolen from RTEMS](https://github.com/derek57/libogc). The authors of libogc didn't just steal proprietary
Nintendo code, but also saw it fit to steal an *open source* RTOS and remove all attribution and copyright
information. This goes far beyond ignorance about the copyright implications of reverse engineering Nintendo
binaries, and goes straight into outright deliberate, malicious code theft and copyright infringement.

The current developers of libogc are [not interested](https://github.com/devkitPro/libogc/issues/201) in
tracking this issue, finding a solution, nor informing the community of the problematic copyright status of
the project. When we filed an issue about it, they immediately closed it, replied with verbal abuse, and then
completely deleted it from public view.

For this reason, we consider it impossible to legally and legitimately compile this software at this point,
and cannot encourage any further development.

The Wii homebrew community was all built on top of a pile of lies and copyright infringement, and it's all
thanks to shagkur (who did the stealing) and the rest of the team (who enabled it and did nothing when it was discovered). Together, the developers deceived everyone into believing their work was original.

Please demand that the leaders and major contributors to console or other proprietary device SDKs and
toolkits that you use and work with do things legally, and do not tolerate this kind of behavior.

If you wish to check for yourself, for example, you can compare
[this](https://github.com/devkitPro/libogc/blob/52c525a13fd1762c10395c78875e3260f94368b5/libogc/lwp_threads.c#L580)
function in libogc to
[this](https://github.com/atgreen/RTEMS/blob/2f200c7e642c214accb7cc6bd7f0f1784deec833/c/src/exec/score/src/thread.c#L385)
function in a really old version of RTEMS. While the code has been simplified and many identifiers renamed, it
is clear that the libogc version is a direct descendant of the RTEMS version. It is not possible for two code
implementations to end up this similar purely by chance.

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
