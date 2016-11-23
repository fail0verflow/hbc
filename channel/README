The Homebrew Channel v1.1.2
Copyright (C) 2007-2012 Team Twiizers, all rights reserved.
All rights reserved; do not redistribute


       Update instructions:


If you have previously installed The Homebrew Channel, you can update it.  If
this is the first time you are installing it, see README.txt for installation
instructions.  The easiest way to update is using the built-in online update
functionality.  Simply configure the WiFi network settings for your Wii for
proper Internet connectivity, and boot up the channel.  If the connection is
established, you'll see an opaque white (not semitransparent) world icon
in the lower right corner, and an update prompt will automatically appear.
Accept it to begin downloading the update.  If you cannot or do not want to
connect your Wii to the Internet, simply run the boot.elf file using any
homebrew booting method.  For example, you can upload it using wiiload or
you can make a directory inside /apps (for example, /apps/Update) and copy
boot.elf there.  Then, simply run it from the previous version of The Homebrew
Channel.


      Adding and customizing apps:


All user applications should be stored in their own subdirectory inside of
apps/; some examples have been provided.  Each subdirectory should have at
least three files; ScummVM will be used as an example.

* apps/ScummVM/boot.elf         main executable to be loaded
* apps/ScummVM/icon.png         icon to be displayed in The Homebrew Channel
                                Menu; should be 128 x 48 pixels
* apps/ScummVM/meta.xml         XML description of the channel.  This format
                                might change for future releases of The
                                Homebrew channel, but we will try to remain
                                backwards-compatible.  See
                                http://wiibrew.org/wiki/Homebrew_Channel
                                for information on what data should be included
                                in this file.


      Staying current with new releases:


Relax, you will not need to do anything to keep up with new releases of the
Homebrew Channel.  When a new version is available, a message will appear
giving you the option to download and install the new version, if your Wii
is configured to connect to the Internet.


      Uninstallation:


You may uninstall the channel as you would any other channel, by using the Data
Management screen of the Wii Menu.  Erasing every last trace of The Homebrew
Channel is not practical on a complex system such as the Wii.  If a need
arises, we will develop a more thorough uninstaller application.


      Reminder about elf files:


Old ELF files were incorrect and will not work.  Please use DOL files or
recompile with the latest version of devkitPPC.  You can use the following
command to convert a broken ELF file to a DOL file:

   powerpc-gekko-objcopy -O binary boot.elf boot.dol

