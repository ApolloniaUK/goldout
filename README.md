## Synopsis

Goldout: a modern replacement for FixupResourceForks.

## Motivation

If you have been using SplitForks and FixupResourceForks to enable version
control with modern version control systems (Git &c), this technique stopped
working when FixupResourceForks disappeared in macOS Sierra.

Goldout is a near drop-in replacement for FixupResourceForks, with some
enhancements.

## Improvements over FixupResourceForks

To address some issues of FixupResourceForks, the following improvements have
been made:

- it can be built as 32- and 64-bit.[^1]
- it can be built for Intel and Apple Silicon.[^1], [^2]
- it does not use deprecated APIs.
- it does not assume there are both FinderInfo and resource fork entries in the AppleDouble file.
- long options are called with `--`.
- the source code is available.

## Usage

	$ goldout [options] [PATH ...]
	
	  Options:
	  --help, -h           Show this usage guide
	  --quiet, -q          Do not display the list of processed files
	  --nolegacy, -n       Require a path instead of defaulting to /
	  --nosetinfo          Do not set the FinderInfo
	  --nodelete           Do not delete the AppleDouble file
	  --norecurse          Do not descend into subdirectories
	  --version            Show the version of this tool

As with FixupResourceForks, if no path is provided, / will be used. Pass
`--nolegacy` (`-n`) to make a missing path an error instead. To make that the
default in your interactive shell, add this to `.bash_profile`:

	alias goldout='goldout -n'

FixupResourceForks accepted `-norecurse` but ignored it. Goldout implements
`--norecurse`: only the AppleDouble files directly inside each path are
processed, and subdirectories are skipped.

A resource fork in the AppleDouble file replaces any resource fork the data file
already has.

Goldout works on HFS, HFS+ and APFS volumes.

If an AppleDouble file cannot be processed, Goldout reports the error, leaves
the file in place and carries on, then exits with status 1 when it has
finished. Folders that cannot be read are reported and skipped.

[^1]: Depending on your build environment and host.
[^2]: Possibly even PPC if you have a modified Xcode.

## Compatibility

Minimum OS requirement is Mac OS X 10.8 at this time.

## Tests

Description forthcoming.

## License

Copyright (c) 2017, Stephane Sudre
Portions copyright (c) 2026, Alan Staniforth
All rights reserved.
 
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 
- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
- Neither the name of the WhiteBox nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 