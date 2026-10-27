# AutoSense plugin SDK v1

This folder is everything a plugin author needs. It's the same `sdk/` folder as
in the AutoSense firmware repository (AutoSense 2.x).

| | |
|---|---|
| `include/AutoSensePlugin.h` | the SDK: `Plugin`, `Descriptor`, `Context`, `Readings`, `Status`, `AUTOSENSE_REGISTER` |
| `templates/PXXX.cpp` | starting point used by `tools/new_plugin.py` |
| `docs/PLUGIN_GUIDE.md` | step-by-step guide and reference |
| `tools/new_plugin.py` | `python sdk/tools/new_plugin.py auto "Title"` creates a plugin |
| `tools/check_plugin.py` | rules, schema preview and a simulated run on the PC |
| `tools/sim/` | the simulator used by `check_plugin.py` (Arduino stand-ins, no hardware) |

`AUTOSENSE_SDK_VERSION` in the header is bumped only when a change would break
existing plugins.
