# Kewetext

## Description
Kewetext is a text editor written in C. This was built up from
[snaptoken's](https://github.com/snaptoken) 
[Build Your Own Text Editor](https://viewsourcecode.org/snaptoken/kilo/)
step by step so I could learn about each individual part of the
kilo text editor. This means that this project is not a fork
of [antirez's](https://github.com/antirez) kilo repo, but does use the
code from this repository and [snaptoken's kilo](https://github.com/snaptoken/kilo-src)
repository.

Kilo was written by Salvatore Sanfilippo aka antirez and is released 
under the BSD 2 clause license.

This text editor extends [antirez's kilo](https://github.com/antirez/kilo)
with a bunch of new features. The editor has no extra dependencies,
and implements all basic features contained within a text editor,
plus custom syntax highlighting, search functionality, copy / paste fucntions,
and undo / redo support. Plenty of features can be configured with the
kewetextrc file.

## Installation

This text editor is supported on any Linux distribution and macOS; the Makefile
specifically should work on systems with the standard file hierarchy
and the user installing being a sudo-er 
(See [Linux File Hierarchy](https://refspecs.linuxfoundation.org/FHS_3.0/fhs/index.html)).

First, you need to have these build tools installed on all platforms:
```text
gcc
make
```

On macOS, it is recommended to have a terminal emulator like iTerm2 so
the option key can behave like the alt key. In the default terminal,
the option key does nothing in Kewetext by default.

You will need to get the source code, either through the
most recent release or by cloning the repository:

```shell
git clone https://github.com/Kewejuankenobie/kewetext.git
```
Next, navigate to this directory and run these commands to build and install to the 
`/usr/bin` directory:

```shell
make
make install
```

This should allow the editor to be run on the current users account.

## Usage

To open a file with Kewetext, run:
```shell
kewetext <file>
```
To run Kewetext with a a new file, run:
```shell
kewetext
```

Within the editor, you can move the cursor with the arrow keys,
page up, page down, home, and end keys.
You can select text with the alt-arrow keys.
You can activate different functions of the editor using these key
combinations:

* CTRL-G Help
* CTRL-Q Quit
* CTRL-S Save
* CTRL-N Save As
* CTRL-F Find
* CTRL-C Copy
* CTRL-V Paste
* CTRL-Z Undo
* CTRL-R Redo

## Configuration

Configuration of Kewetext's settings can be done in:
```text
~/.config/kewetextrc
```

In here, you can edit settings regarding the editors functionality, 
plus color and syntax highlighting customization settings. More details
about these settings are located in the provided kewetextrc file.

## Credits

I followed [the guide](https://viewsourcecode.org/snaptoken/kilo/) 
given by [snaptoken](https://github.com/snaptoken) on
how to build [their version of kilo](https://github.com/snaptoken/kilo-src)
so I could learn about how every part of the editor worked. Of course,
the original [kilo](https://github.com/antirez/kilo) was built by
[antirez](https://github.com/antirez). 

## License

This project is distrubuted under the BSD-2 Clause license.