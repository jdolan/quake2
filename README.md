<!-- Name: Quake2 -->
<!-- Version: 14 -->
<!-- Last-Modified: 2012/12/27 15:03:02 -->
<!-- Author: jdolan -->
# Quake II for Mac and Linux

![Quake II for Mac and Linux](blob/master/github-screenshot.jpg)

## Overview

Here you'll find pre-compiled packages of classic _[Quake II](http://en.wikipedia.org/wiki/Quake_II)_ for Mac OSX and 64 bit GNU/Linux. These builds are based on the abandoned *AprQ2* project by _maniac_. I've adopted this code and have made numerous fixes, updates and enhancements to it so that Mac and Linux users can have a stable[-ish], feature-rich _Quake II_ client. Some of my changes include:

 * Build scripts and packaging for Mac OS X and GNU/Linux
 * Updated R1Q2 Protocol 35 support
 * Anisotropic filtering for SDL video
 * Multisample (FSAA) for SDL video
 * Stencil shadows for SDL video
 * Numerous stability fixes around SDL audio
 * Numerous 64 bit compatibility fixes

A full list of my changes is available in the [CHANGELOG](blob/master/CHANGELOG).

## Downloads

These packages come with the 3.14 demo and 3.20 point release data (one Single Player map, all of the official Deathmatch maps, and Capture the Flag). You can play the game immediately after installing.

 * [Quake II (Quake2World.net)](http://quake2world.net/files/Quake%20II%20(Quake2World.net).dmg) for Mac OSX 10.6 or later
 * [Quake II (Quake2World.net)](http://quake2world.net/files/quake2-quake2world.net-x86_64.tar.gz) for GNU/Linux 64 bit

## Installation

If you want to play the full single-player game, you must provide the retail game data that came on your _Quake II_ CD-ROM. Locate the `pak0.pak` file on your _Quake II_ CD-ROM and copy it to your user-specific `.quake2` folder:


    mkdir -p ~/.quake2/baseq2
    cp /Volumes/Quake\ II/Install/Data/baseq2/pak0.pak ~/.quake2/baseq2

## Compiling

Compiling this version of _Quake II_ is actually quite easy if you follow the [Quake2World developers' guide](http://quake2world.net/books/documentation/developing-and-modding) to install all of the game's dependencies. Once you've installed the dependencies, simply typing `make` should build the game for you. The [INSTALL](blob/master/INSTALL) file located in the source directory covers the details and walks you through installing the _Quake II_ game data, too.

## Support
 * The best way to reach me for support is in *#quetoo* on *irc.freenode.net*
 * Alternatively, you can visit the [Quake2World Forums](http://quake2world.net/forum) and post a question there

## Related Pages
 * [wiki:Quake2World]
 * [wiki:Quetoo]
