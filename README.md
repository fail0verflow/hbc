# ARCHIVED

**Update**: [Statement from RTEMS](https://www.rtems.org/news/2025-05-06-rtems-devkit-libogc-response/)

This repository is archived and will not accept any further contributions.

Like most Wii homebrew software, this software depends on [libogc](https://github.com/devkitPro/libogc).
After development of The Homebrew Channel had already started, we discovered that large portions of libogc
were stolen directly from the Nintendo SDK (decompiled and cleaned up), including portions directly copied
and pasted from SDK header files (One example:
[libogc](https://github.com/devkitPro/libogc/blob/52c525a13fd1762c10395c78875e3260f94368b5/libogc/video.c#L91),
[Nintendo SDK](https://github.com/RenaKunisaki/SFA-Amethyst/blob/7844a5fe15485bf79fe672ef7d47f44d76b98b0f/include/gc/revolution/gx/GXFrameBuffer.h#L220),
but there are many others and casually browsing the headers will show that the vast majority of APIs are all
but identical). There are also bits of documentation copied from Nintendo SDK documentation, as well as
[executable binaries](https://github.com/devkitPro/libogc/blob/52c525a13fd1762c10395c78875e3260f94368b5/libogc/card.c#L129)
(disguised by being called "data" in the identifier) that are copied verbatim from the SDK. We thought that
at least significant parts of libogc, such as its threading implementation, were original, and reluctantly
continued to use the project while distancing ourselves from it.

Had we known the extent to which libogc was tainted when we first bootstrapped the Wii homebrew community
with the release of the Twilight Hack and other tools, we would never have condoned the use of devkitPro and
libogc, and we would have committed to developing an alternate library without copyright infringement issues.
However, this fact was hidden from us at the time, as the devkitPro and libogc developers always presented
their offerings as completely free of any copyright problems, and these issues were not publicly documented
despite libogc existing long before the release of the Wii, as a GameCube development library. By the time
the truth became known to us, the Wii homebrew ecosystem was already established and locked into libogc.
This was particularly insidious, as it is precisely those *without* access to or no intent to reference the
leaked Nintendo SDK that were deceived; those *with* a copy of the leaked SDK and eager to browse its
documentation and headers could quickly see the extent of the copying, but were unlikely to raise the alarm.

It has recently been revealed that the threading/OS implementation in libogc is, in fact,
[stolen from RTEMS](https://github.com/derek57/libogc). The authors of libogc didn't just steal proprietary
Nintendo code, but also saw it fit to steal an *open source* RTOS and remove all attribution and copyright
information. This goes far beyond ignorance about the copyright implications of reverse engineering Nintendo
binaries, and goes straight into outright deliberate, malicious code theft and copyright infringement.

The current developers of libogc are [not interested](https://github.com/devkitPro/libogc/issues/201) in
tracking this issue, finding a solution, nor informing the community of the problematic copyright status of
the project. When we filed an issue about it, they immediately closed it, replied with verbal abuse, and then
completely deleted it from public view.
[Another issue pointing out explicit Nintendo SDK code theft evidence](https://github.com/devkitPro/libogc/issues/204)
filed by another person has also been immediately closed and locked.

For this reason, we consider it impossible to legally and legitimately compile this software at this point,
and cannot encourage any further development.

The Wii homebrew community was all built on top of a pile of lies and copyright infringement, and it's all
thanks to shagkur (who did the stealing) and the rest of the team (who enabled it and did nothing when it was
discovered). Together, the developers deceived everyone into believing their work was original.

Please demand that the leaders and major contributors to console or other proprietary device SDKs and
toolkits that you use and work with do things legally, and do not tolerate this kind of behavior.

If you wish to check for yourself, for example, you can compare
[this](https://github.com/devkitPro/libogc/blob/52c525a13fd1762c10395c78875e3260f94368b5/libogc/lwp_threads.c#L580)
function in libogc to
[this](https://github.com/atgreen/RTEMS/blob/2f200c7e642c214accb7cc6bd7f0f1784deec833/c/src/exec/score/src/thread.c#L385)
function in a really old version of RTEMS. While the code has been simplified and many identifiers renamed, it
is clear that the libogc version is a direct descendant of the RTEMS version. It is not possible for two code
implementations to end up this similar purely by chance.

**Update**: The libogc developers have restored the issue and are now claiming that the code
[was not stolen](https://mardy.it/blog/2025/04/no-libogc-did-not-steal-rtems-code.html). What they are in fact
arguing is that the code was not copied verbatim and then changed to obfuscate its origin, but rather that it was
developed by "referencing" RTEMS. Indeed, the original commits of the code to libogc are a less complete copy
of RTEMS than the current version. What that means is that, instead of literally duplicating RTEMS and then
reducing it, they instead *piecewise* incorporated RTEMS code by re-typing or copyediting it line by line,
over time. This is equivalent to opening up a copy of The Lord of the Rings, pulling up a blank document, and
meticulously re-typing the whole story in different words, with different names for the characters, while
preserving the entirety of the plot. Unfortunately for shagkur and the other libogc authors, this is still
plagiarism and copyright infringement. It doesn't matter that they didn't literally Ctrl-C and Ctrl-V the
entirety of RTEMS. The end result is, very clearly, still plagiarized. (The author of the above blog post
is wrong about the language translation example; a 1:1 translation of a project into another programming
language would absolutely be considered a derivative work, just like a translation of a novel into another
human language would be. Otherwise what libogc did with Nintendo SDK code would be legal and non-infringing,
since it is a manual translation from assembly to C. It is widely understood and accepted in the development
community that this kind of action does not erase copyright and creates a derivative work.)

Feel free to check out another example: 
[this RTEMS function](https://github.com/atgreen/RTEMS/blob/f926b34f663debae055330a9e54ee71fc1f65d12/c/src/exec/score/src/thread.c#L1218)
is 1:1 identical to
[this libogc function](https://github.com/devkitPro/libogc/blob/52c525a13fd1762c10395c78875e3260f94368b5/libogc/lwp_threads.c#L388),
other than slightly renamed identifiers and different code formatting. It would be clear in any court of
law that this constitutes copyright infringement, regardless of whether it was achieved in one shot or
incrementally over time. It is simply not possible for this kind of non-trivial code to wind up completely
identical like this, purely on accident. This kind of conduct is, in fact, the same conduct that led libogc
to contain large parts of decompiled Nintendo SDK code verbatim. We just thought that that was a result of a
lack of understanding (or caring) of how copyright works when it related to reverse engineering proprietary
binary code, but it seems shagkur believes that he is entitled to manually copy and re-type *any* code, even
open source code, and the mere action of doing so erases its original copyright.

WinterMute is also is not innocent, and not just by virtue of being complicit with shagkur and enabling his
plagiarism behavior. He, himself, was previously
[caught](https://github.com/devkitPro/libnds/commit/426a369220dcb43320e203f9087de74e43452d84#diff-f376c160388ca187fe35e962eb047fe606338869b515128a6257d3a7a6694ff0R17-R25)
referencing the official Nintendo DS ("nitro") SDK while writing code for libnds\*. WinterMute has a huge
siege mentality issue when it comes to DevkitPro, which is why nobody has been able to get through to him,
as any criticism of his work or the way he manages his project has always been met with extreme hostility.
This is why nothing has ever been done about these issues with the project, even after all these years.

\* Edited to note: This should not be taken to imply that any or all of libnds itself is tainted with SDK
code. The commit in question itself only adds fairly standard cache maintenance functions, and is not itself
evidence that there was substantial copying. It just reflects poorly on WinterMute that he was referencing
the leaked SDK, especially for something as trivial as this. For a more thorough take on the general
mismanagement of the devkitPro project as a whole, please read
[this](https://heyquark.com/brew/2020/07/13/leaving-devkitpro/) blog post. The stories told there track
very closely with our experience with that team way back in the Wii days, and it seems nothing has changed
nor improved after all these years.

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
