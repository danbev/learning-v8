### [GN](https://chromium.googlesource.com/chromium/src/+/master/tools/gn/docs/quick_start.md)
GN stands for Generate Ninja.
```console
$ tools/dev/v8gen.py --help
```

Show all executables
```console
$ gn ls out/x64.release_gcc --type=executable
```
If you want details of a specific target you can use `desc`:
```console
$ gn desc out/x64.release_gcc/ //:mksnapshot
``` 
`//:mksnapshot` is a label where `//` indicates the root directory and then the
name of the label. If the label was in a subdirectory that subdirectory would
come before the `:`. 
This command is useful to see which files are included in an executable.

When gn starts it will search for a .gn file in the current directory. The one
in specifies the following:
```
import("//build/dotfile_settings.gni")                                          
```
This includes a bunch of gni files from the build directory
(the dotfile_settings.gni) that is.

```console
buildconfig = "//build/config/BUILDCONFIG.gn"                                   
```
This is the master GN build configuration file.


Show all shared_libraries:
```console
$ gn ls out/x64.release_gcc --type=shared_libraries
```

Edit build arguments:
```console
$ vi out/x64.release_gcc/args.gn
$ gn args out/x64.release_gcc
```

This will open an editor where you can set configuration options. I've been using the following:
```console
v8_monolithic = true
use_custom_libcxx = false
v8_use_external_startup_data = false
is_debug = true
target_cpu = "x64"
v8_enable_backtrace = true
v8_enable_slow_dchecks = true
v8_optimized_debug = false
```

List avaiable build arguments:
```console
$ gn args --list out/x64.release
```

List all available targets:
```console
$ ninja -C out/x64.release/ -t targets all
```

Building:
```console
$ env CPATH=/usr/include ninja -C out/x64.release_gcc/
```

Running quickchecks:
```
$ ./tools/run-tests.py --outdir=out/x64.release --quickcheck
```
You can use `./tools-run-tests.py -h` to list all the options that can be passed
to run-tests.

