### Snapshots
JavaScript specifies a lot of built-in functionality which every V8 context must
provide.  For example, you can run Math.PI and that will work in a JavaScript
console/repl.
The global object and all the built-in functionality must be setup and
initialized into the V8 heap. This can be time consuming and affect runtime
performance if this has to be done every time. 

Now this is where the file `snapshot_blob.bin` comes into play. 
But what is this bin file?  
This blob is a prepared snapshot that get directly deserialized into the heap to
provide an initilized context.

When V8 is built with `v8_use_external_startup_data` the build process will
create a snapshot_blob.bin file using a template in BUILD.gn named `run_mksnapshot`
, but if false it will generate a file named snapshot.cc. 

#### mksnapshot
There is an executable named `mksnapshot` which is defined in 
`src/snapshot/mksnapshot.cc`. This can be executed on the command line:
```console
$ mksnapshot --help
```
But the strange things is that the usage is never printed, instead the normal
V8/D8 options. If we look in `src/snapshot/snapshot.cc` we can see:
```c++
  // Print the usage if an error occurs when parsing the command line
  // flags or if the help flag is set.
  int result = i::FlagList::SetFlagsFromCommandLine(&argc, argv, true, false);
  if (result > 0 || (argc > 3) || i::FLAG_help) {
    ::printf("Usage: %s --startup_src=... --startup_blob=... [extras]\n",
             argv[0]);
    i::FlagList::PrintHelp();
    return !i::FLAG_help;
  }
```
`SetFlagsFromCommandLine` is defined in `src/flags/flags.cc`:
```c++
int FlagList::SetFlagsFromCommandLine(int* argc, char** argv, bool remove_flags) {
  ...
  if (FLAG_help) {
    PrintHelp();
    exit(0);
  }
}
```
So even if we specify `--help`` the usage string for snapshot will not be
printed. As a suggetion I've created
https://chromium-review.googlesource.com/c/v8/v8/+/2290852 to fix this.
With this change `--help`will print the usage message:
```console
$ ./out/x64.release_gcc/mksnapshot --help | head -n 1
Usage: ./out/x64.release_gcc/mksnapshot --startup_src=... --startup_blob=... [extras]
```
