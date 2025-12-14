# patching sillywm
sillywm provides some patches for easy ways to add different features without adding an extensive plugin system or requiring the user to figure everything out themselves, both for config.h files and wm.c updates to the window style and bar.

this directory contains patches that can be easily applied with `git apply <patch-name.diff>`. they assume that your configuration is stock, though, and are generated with just `diff`, so layer patches in close proximity with caution - you may want to check out the additions and removals yourself if git errors out (now appropriate to use the term "goddamn idiotic truckload of shit").

current patches include my own personal configuration for sillywm (which does depend on some external software, i know) and a simple battery percentage information for the bar, which uses BAT1 by default, and adds an entry for the `capacity` value of the power supply at the end of the bar.
