
CRANEA
Encrypted Text Adventure

Overview
========

Cranea consists of two executable programs, a "compiler" and a
"player". The compiler transforms an XML document describing the
text adventure into an encrypted binary format. The player reads
the encrypted binary file and acts like a normal console text
adventure. The source code (in C++) can be compiled and run on
many platforms including Windows, Mac, Linux, Solaris, etc.

If you have downloaded this as part of a release package, pre-
built executables can be found in the Release directory.

Building
========

1. Download and build dependencies:
    * Expat 2.0.1 - http://sourceforge.net/projects/expat/
    * Crypto++ 5.5.2 - http://www.cryptopp.com/#download
    
2a. To build with Visual Studio:

    * Open src\cranea.sln in Visual Studio
    * Edit the project properties for both compiler and player 
      projects to point to the Expat/Crypto++ lib/include files 
      (AdditionalIncludeDirectories/AdditionalDependencies)
    * Build
    
2b. To build with g++:

    * cd src
    * Edit Makefile and change CRYPTOPP_DIR and EXPAT_DIR as necessary
    * make

Creating a text adventure
=========================

First, write the text adventure in XML using your favorite text
editor. (See the examples/ directory for sample text adventures,
and ADVENTURE.txt for documentation on the XML schema.)

Then, run cranea.exe, and enter the path to the .xml file above.

If there are no errors, this will create a game/ directory with a
copy of player.exe (renamed to the name of your adventure) and an
encrypted data file (with the .cra extension). Provide both of
these files to players (e.g. in a zip archive).

If players will be running the adventure on multiple platforms,
you will need to compile player.exe once for each platform. The
.cra files can be generated once and are portable between
platforms.

Background
==========

Competitive puzzle-solving events such as The Game (
http://en.wikipedia.org/wiki/The_Game_%28treasure_hunt%29) or
Puzzle Hunts ( http://en.wikipedia.org/wiki/Puzzlehunt) frequently
provide participants with custom software as a means of delivering
puzzles, hints, solutions, or backstory during the event. However,
the competitive nature of these events gives a strong incentive
for technically-skilled participants to "cheat" by extracting data
from the software using methods that the event organizers did not
intend. Often, such software makes no attempt to protect its data,
and relevant strings can be extracted simply by viewing the
executable in a hex editor. In cases where this type of software
does make some attempt to protect its data (such as by encrypting
a data file with a secret key that is stored somewhere in the
executable), it is often insufficient to deter someone skilled at
using a debugger to examine the memory of the process while it is
running. These techniques fundamentally rely on "security through
obscurity", and their usefulness at hiding data would be severely
compromised if the software's source code were released to the
players.

Existing text adventure / interactive fiction games are generally
intended to be played in a non-competitive environment, and thus
make little effort to encrypt their data files. Most commonly,
data files for interactive fiction games are encoded in a format
called "Z-code" (http://en.wikipedia.org/wiki/Z-machine), which at
least obscures strings by encoding them in "ZSCII" instead of
ASCII but can easily be decoded by anyone who reads the Z-code
specification.

Cranea allows people to create text adventures which are
practically impossible for even the most technically-skilled
players to "cheat" at, even if the source code is provided. To do
so, it uses strong encryption on a very fine-grained scale. Each
location, action, item, and external file within the text
adventure is individually encrypted using a separate AES key.
Nearly all of these keys are selected at random, except that the
keys for actions are derived from the command that the player
would have to type to execute that action. The program starts with
only a single encryption key -- the key for the starting location
-- and obtains encryption keys for other objects as the player
progresses through the text adventure. In this way, a technically-
skilled player wishing to "cheat" by examining the memory of the
running program would only be able to decrypt those objects he had
already seen in the natural course of the text adventure.

As an example of how progressive key revelation works in Cranea,
the command "take the sword" might cause the program to search for
an action with a key derived from SHA1("take sword"). The
decrypted data for this action would then contain an AES key
corresponding to the "sword" item, allowing the user to execute
actions which need the sword in the inventory. Then, the command
"slay dragon" could use a key derived from SHA1("slay dragon") in
combination with the sword's key, to decrypt the action that slays
the dragon.

License
=======

Cranea is provided under the BSD license; see LICENSE.txt.