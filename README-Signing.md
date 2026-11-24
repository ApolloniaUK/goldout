# Code signing for goldout

The Xcode project keeps no signing identity or team of its own, so that a fresh
clone builds on any Mac without signing errors. Signing settings come from two
files in `Config/`:

| File | Tracked by git | Purpose |
|---|---|---|
| `Config/Signing.xcconfig` | Yes | Defaults that work for everyone |
| `Config/Local.xcconfig` | No (git-ignored) | Your own identity and team, if you have them |

`Config/Local.xcconfig.example` is a tracked template for `Local.xcconfig`.

## How it works

`Signing.xcconfig` is the base configuration of the goldout target, in both Debug
and Release. It sets ad hoc signing ("Sign to Run Locally"), which needs no
certificate:

```
CODE_SIGN_STYLE = Manual
CODE_SIGN_IDENTITY = -
DEVELOPMENT_TEAM =
ENABLE_HARDENED_RUNTIME = NO
OTHER_CODE_SIGN_FLAGS =
```

Its last line is an optional include:

```
#include? "Local.xcconfig"
```

The `?` makes the include optional: if `Local.xcconfig` doesn't exist, Xcode
ignores the line and builds with ad hoc signing. If it does exist, its settings
come later in the file and so override the defaults.

## Signing with your own identity

Copy the template and edit it:

```bash
cp Config/Local.xcconfig.example Config/Local.xcconfig
```

For a Developer ID build, the result looks like this, with your own team ID:

```
CODE_SIGN_IDENTITY = Developer ID Application
DEVELOPMENT_TEAM = YOURTEAMID
ENABLE_HARDENED_RUNTIME = YES
OTHER_CODE_SIGN_FLAGS = --timestamp
```

Your team ID is the 10-character code in brackets after your name in the output
of `security find-identity -v -p codesigning`. Hardened runtime and a secure
timestamp are needed if you go on to notarize the tools.

To check which identity a build used:

```bash
codesign -dv --verbose=2 path/to/goldout
```

An ad hoc build shows `Signature=adhoc`; a Developer ID build lists your
certificate as `Authority=Developer ID Application: …`.

## Notes

- Don't put an identity or team ID in `Signing.xcconfig` or in the project's
  build settings: settings made in the project override the xcconfig files.
- Xcode 3.1 (Mac OS X 10.5) doesn't understand `#include?`. It prints "Unable
  to find included file" and carries on with the ad hoc defaults.
