# Cat Command Design

## Goal

Add a minimal shell command `cat <file>` that prints the contents of one file.

## Architecture

The shell should stay thin. It will validate the argument, call the filesystem API, and print bytes to VGA. The filesystem layer needs a small read-file API because the current shell-facing FS contract only supports directory listing and directory creation.

Boot media roots are switched into RAMFS, so file import must preserve file contents. Media drivers will expose file reads for ISO9660 and FAT12. The recursive importer will read each media file into RAMFS, and `cat` will read from the active root driver.

## Behavior

- `cat <path>` prints the file contents.
- `cat` prints `usage: cat <file>`.
- Directories and missing paths report through the existing FS error printer.
- The first version supports one file argument and no options.

## Testing

Use build checks as the regression gate:

- `make modules-32 modules-64 modules-img-32`

Manual OS verification after image build:

- boot a target
- run `cat /file.txt`
- confirm the file text prints
- run `cat /`
- confirm an FS error is shown
