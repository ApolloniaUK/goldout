# goldout: change history

Changes to Stéphane Sudre's goldout (1.0 → 1.1),
made by Alan Staniforth in September and October 2026.

- [goldout 1.1](#goldout-11)
  - [Changes](#goldout-changes)
  - [Bug fixes](#goldout-bug-fixes)
  - [Known unfixed bugs and limitations](#goldout-known-unfixed-bugs-and-limitations)

---

## goldout 1.1

### goldout: changes

- **`-n` / `--nolegacy`.** With no `PATH`, goldout still processes the whole
  boot volume, starting at `/`. Disassembling Apple's FixupResourceForks
  confirmed that it does the same, so the default is kept for compatibility.
  `-n` makes a missing `PATH` an error (status 2) instead. To make that the
  default in an interactive shell, add `alias goldout='goldout -n'` to
  `.bash_profile`.
- **`--norecurse`.** Only the AppleDouble files directly inside each `PATH` are
  processed. FixupResourceForks accepted `-norecurse` but ignored it.
- **Exit status 1 when any AppleDouble file fails.** goldout still carries on
  through the tree, then exits 1 when it has finished. FixupResourceForks also
  kept going and exited with an error. Unreadable folders are reported but
  don't affect the exit status, matching goldin.
- **Existing resource forks are replaced.** A resource fork in the AppleDouble
  file replaces any resource fork the data file already has. FixupResourceForks,
  like goldout 1.0, wrote over the start of an existing fork and left the rest
  (see the bug fix below).
- **Consistent error messages.** Every message now uses the form
  `Error processing "<path>" (<reason>)` and names the file or path concerned.
- **Usage text.** The usage line is now `goldout [options] [PATH ...]` and
  lists `-n` and `--norecurse`.
- **Documentation.** The goldout(1) man page now covers APFS support, file
  arguments, folder handling, replacing an existing resource fork, error
  handling and exit statuses. The README covers the new options, replacing an
  existing resource fork and error handling. `main.c` has a change record.

### goldout: bug fixes

- **Relative paths failed for every file.** `fts` changes directory as it
  walks the tree, so a relative path no longer pointed at the file. Fixed with
  `FTS_NOCHDIR`.
- **Folder AppleDouble files from goldin failed.** goldin writes an empty
  resource-fork entry for folders. goldout tried to write it to the folder,
  failed with "Operation not permitted" and left the `._` file behind. Empty
  resource-fork entries, and any resource fork for a folder, are now skipped.
- **Unneeded processing of FinderInfo-only AppleDouble files when
  `--nosetinfo` is passed.** Misplaced brackets meant such a file was treated
  as needing work. If its data file was missing, goldout reported "The data
  file was not found" and kept the file, instead of simply deleting it.
- **Short FinderInfo entries.** An entry shorter than 32 bytes was read as 32
  bytes anyway, so whatever followed it in the file was written as Finder
  information. Such files are now reported and left untouched.
- **A full disk could cut a resource fork short, and the AppleDouble file was
  then deleted.** When an HFS+ disk fills, `fsetxattr` reports success but
  writes nothing more. goldout couldn't tell, so it reported success, deleted
  the `._` file and left a truncated resource fork on the data file. goldout
  now checks the fork's size after writing it. If it is incomplete, goldout
  reports the error, removes the incomplete fork and keeps the AppleDouble file.
  The incomplete fork is also removed if reading or writing fails part-way.
- **An existing resource fork was only partly replaced.** Writing a resource
  fork from position 0 doesn't shorten one that is already longer, so the end
  of the old fork was left on the end of the new one. goldout now removes any
  existing resource fork before writing.
- **The AppleDouble header check only worked on little-endian Macs.** The
  magic number and version are now converted from big-endian before they are
  compared.
- **Memory leaks.** The table of entries read from each AppleDouble file was
  never freed, and the directory walk wasn't closed on an early exit.
- **A permission error printed two messages** because of a missing `break`.
- **Errors in unreadable folders named the top-level `PATH`** instead of the
  folder.
- **Errors that printed nothing.** Failures of `realpath`, `lstat`, `statfs`,
  `fts_open`, and of writing the resource fork or Finder information, now print
  a message.
- **`-h` exited with status 2.** It now exits 0.
- **Exit status 0 when files had failed.** See the exit status change above.
- **Message errors:** raw errno numbers instead of descriptions,
  "ApppleDouble", "can not", "filesystem", and an "Error (Memory is too low)"
  message that didn't name the file.

### goldout: known unfixed bugs and limitations

- **With no `PATH`, goldout processes the whole boot volume.**
  *Why:* this deliberately matches FixupResourceForks. Use `-n` or the alias
  above to avoid it.
- **Other extended attributes in AppleDouble files are not restored.** If a
  FinderInfo entry is longer than 32 bytes, goldout restores only the 32 bytes
  of Finder information and ignores the rest. AppleDouble files written by
  macOS itself store a file's other extended attributes there. Not tested.
  *Why:* this was already marked as unfinished in Sudre's code, and restoring
  those attributes needs a parser for the format macOS uses.

---

*Testing:* changes were tested on macOS with the Xcode 16.2 toolchain, using
temporary APFS and HFS+ disk images. The behaviour of Apple's
FixupResourceForks was taken from a disassembly of an i386 copy built against
the 10.10 SDK.
