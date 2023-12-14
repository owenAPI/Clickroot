# ascii-patrol

Random ASCII game project. Mainly inspired by Moon Patrol and Dune Buggy.

<img src="./ascii-patrol.gif" alt="ascii-patrol" width="100%" />

Currently game can be built using C++ compilers such as Clang, GCC, or Intel oneAPI including operating systems:

- Windows (Visual Studio, Cygwin, MinGW, MSYS2, Windows Subsystem for Linux)
- Linux, MacOS
- DOS (Watcom)
- HTML5 browsers (Emscripten)

## Quick install on Linux with _snap_

[![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-white.svg)](https://snapcraft.io/ascii-patrol)<br/>
_snapped by @popey_

- `sudo snap install ascii-patrol`

## Packages

Arch Linux:

- https://aur.archlinux.org/packages/ascii-patrol-git/
  by @SlavMetal
- https://aur.archlinux.org/packages/ascii-patrol/
  by Towtow10 (only x64)

## Build instructions

These instructions assume the system has Git and a C++ compiler.

```sh
# clone sources repo
git clone https://github.com/msokalski/ascii-patrol.git

# enter sources directory
cd ascii-patrol
```

### Linux

The needed libraries are installed like:

```sh
# install required packages, (no apt-get -> use pacman)
sudo apt-get install libx11-dev libpulse-dev
# OR
sudo dnf install libX11-devel pulseaudio-libs-devel

# to fix problems with no keyboard input in few gnome terminals we require XI2
sudo apt-get install libxi-dev
# OR
sudo dnf install libXi-devel
```

Build with CMake:

```sh
cmake -Bbuild
cmake --build build
```

If the system doesn't have CMake:

```sh
# enable execution of build.sh and run.sh scripts
chmod +x *.sh
# invoke compilation
./build.sh
```

### MacOS

Homebrew install libraries (works with Clang or GCC):

```sh
brew install libx11 libxi pulseaudio
```

Build:

```sh
cmake -Bbuild
cmake --build build
```

### Windows

No extra libraries are needed on Windows.
Works with Visual Studio, MinGW / MSYS2, Cygwin, or WSL.

```sh
cmake -Bbuild
cmake --build build
```

### HTML5

On a system with emscripten installed, build for HTML5 web browsers, producing "build/asciipat.[js,wasm]"

```sh
emcmake cmake -B build -Dweb=on
cmake --build build
```

## Running

Ascii Patrol uses curl to communicate with hi-score server (ascii-patrol.com)
So optionally let's install it if needed.
`sudo apt-get install curl`

Finally let's piu-piu !
`./asciipat`
or
`./run.sh`

More on asciipat arguments can be found here: http://ascii-patrol.com/forum/index.php?topic=63

## Filepaths

Game creates two files: _asciipat.cfg_ and _asciipat.rec_.

_asciipat.cfg_ is created when you change your player name, avatar, or key bindings.
_asciipat.rec_ is created when you start playing the game, and get a score.

On Linux, by default, the game saves these files in $HOME. If $HOME is not set, then the game saves the files in the current directory.

However, if $ASCII_PATROL_CONFIG_DIR or $ASCII_PATROL_RECORD_DIR are set, then the game will save the files in those directories respectively. The directories should already exist; Ascii Patrol will not create them.

## Problems?

- no sound? -> install / start pulseaudio ( ie: `pulseaudio -v -C` )
- weird colors? -> try another terminal emulator or run from raw console (ctrl-alt-F1 or so)
- poor font look? -> try changing font in your terminal or in case of raw console use setfont
- keyboard doesn't work? -> try another terminal emulator or run from raw console
- no hi-scores table? -> install curl
