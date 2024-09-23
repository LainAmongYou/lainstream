# Lain's OBS plugin

Just Lain's little stream plugin. Contains the underlay and probably other
things later.

## How to build

Okay, this is dumb, but OBS' plugin template isn't really built for easy
building, it's meant for bigger plugins that require a proper distribution
setup, so I decided to just do things the old-fashioned (lazy) way and build
in-tree. You'd probably need to build OBS yourself either way so might as well
build the plugin the easy way: in-tree when you build OBS.

All you need to do is clone and build OBS yourself (easy, right?), make a
branch for yourself for your stream build of OBS, then in the `plugins/`
directory, clone this repo as a submodule, then in `plugins/CMakeLists.txt`,
just add:
```cmake
add_obs_plugin(lainstream)
```
at the bottom. Then build OBS and you'll have it with your OBS. Then the plugin
will be available when you build!
