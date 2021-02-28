# EggShell

EggShell is a command-line interface shell for Fuchsia.

Clone the EggShell repo into your Fuchsia source tree. For example,
from the fuchsia root directory, run:

```
git clone https://github.com/camrdale/eggshell.git vendor/camrdale/eggshell
```

To build EggShell, add the directory where you checked it out to the
`fx set` command, for example:

```
fx set workstation.x64 --with //vendor/camrdale/eggshell
```

Then build the tree:

```
fx build
```

To run the shell on your connected Fuchsia device (this assumes you
have a connected device or emulator, and `fx serve` is running):

```
fx shell esh
```
