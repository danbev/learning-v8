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
This executable is used to create snapshot and does so starting a V8 Isolate
and then serializing runtime data into binary format. 

There is an executable named `mksnapshot` which is defined in 
`src/snapshot/mksnapshot.cc`. This can be executed on the command line:
```console
$ mksnapshot --help
Usage: ./out/x64.release_gcc/mksnapshot [--startup-src=file] [--startup-blob=file] [--embedded-src=file] [--embedded-variant=label] [--target-arch=arch] [--target-os=os] [extras]
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
https://chromium-review.googlesource.com/c/v8/v8/+/2290852 to address this.

With this change `--help`will print the usage message:
```console
$ ./out/x64.release_gcc/mksnapshot --help | head -n 1
Usage: ./out/x64.release_gcc/mksnapshot --startup_src=... --startup_blob=... [extras]
```

What mksnapshot does is much the same start up of a normal V8 application, it
has to initialize V8 itself (`src/snapshot/snapshot.cc`):
```c++
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
```
Notice that this is just was we have in [hello-world](../hello-world.cc) or
in [v8_test_fixture.h](../test/v8_test_fixture.h).

Next, we have:
```c++
  SnapshotFileWriter snapshot_writer;
  snapshot_writer.SetSnapshotFile(i::FLAG_startup_src);
  snapshot_writer.SetStartupBlobFile(i::FLAG_startup_blob);
```
`i::FLAG_startup_src` is defined in `src/flags/flag-definitions.h`:
```c++
DEFINE_STRING(startup_src, nullptr,
              "Write V8 startup as C++ src. (mksnapshot only)")

#define DEFINE_STRING(nam, def, cmt) FLAG(STRING, const char*, nam, def, cmt)

#undef FLAG
#ifdef ENABLE_GDB_JIT_INTERFACE
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  V8_EXPORT_PRIVATE extern ctype FLAG_##nam;

#define FLAG_READONLY(ftype, ctype, nam, def, cmt) \
  static constexpr ctype FLAG_##nam = def;
```
So the above macro would be expanded by the preprocessor into:
```c
  V8_EXPORT_PRIVATE extern const char* FLAG_startup_src;
```

### How V8 creates a snapshot
The mksnapshot executable takes a number of arguments on of which is the target
os which can be differnt from the host os. Examples of targets can be found
in `BUILD.gn` and are `android`, `fuchsia`, `ios`, `linux`, `mac`, and `win`.


There is a template named `run_mksnapshot` in `BUILD.gn` that shows how
mksnapshot is called.

If `v8_use_external_startup_data` is set in args.gn, the option `--startup_blob`
will be passed to mksnapshot:
```console
  if (v8_use_external_startup_data) {
      outputs += [ "$root_out_dir/snapshot_blob${suffix}.bin" ]
      data += [ "$root_out_dir/snapshot_blob${suffix}.bin" ]
      args += [
        "--startup_blob",
        rebase_path("$root_out_dir/snapshot_blob${suffix}.bin", root_build_dir),
      ]
```
And this binary file can then be deserialized. If `v8_use_external_startup_data`
if not set the `--startup_src` will be passed instread:
```
  } else {
      outputs += [ "$target_gen_dir/snapshot${suffix}.cc" ]
      args += [
        "--startup_src",
        rebase_path("$target_gen_dir/snapshot${suffix}.cc", root_build_dir),
      ]
    }
```
To see what is actually getting used on when building one can look at the file
`out/x64.release_gcc/toolchain.ninja`:
```console
rule ___run_mksnapshot_default___build_toolchain_linux_x64__rule
  command = python ../../tools/run.py ./mksnapshot --turbo_instruction_scheduling 
    --target_os=linux 
    --target_arch=x64 
    --embedded_src gen/embedded.S 
    --embedded_variant Default 
    --random-seed 314159265 
    --startup_src gen/snapshot.cc 
    --native-code-counters 
    --verify-heap
  description = ACTION //:run_mksnapshot_default(//build/toolchain/linux:x64)
  restat = 1
  pool = build_toolchain_action_pool
```

### startup-src
This option will specifies a path to a c++ source file that will be generated
by mksnapshot, for example:
```console
$ ../v8_src/v8/out/x64.release_gcc/mksnapshot --startup_src=gen/example_snapshot.cc
```
After this we can inspect `gen/example_snapshot.cc` which will contain the
serialized snapshot in a byte array named `blob_data`.
```c++
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

alignas(kPointerAlignment) static const byte blob_data[] = {
  ...
};
static const int blob_size = 133115;
static const v8::StartupData blob = { (const char*) blob_data, blob_size };
const v8::StartupData* Snapshot::DefaultSnapshotBlob() {
  return &blob;
}
```
This file is generated by `MaybeWriteSnapshotFile`.

### startup-blob
This is also a command line options that can be passed to `mksnapshot` which 
will generate a binary file in the path specified. Note that this can be specified
in addition to `startup-src` which I was not aware of:
```
$ ../v8_src/v8/out/x64.release_gcc/mksnapshot --startup_src=gen/example-snapshot.cc --startup-blob=gen/example-blob.bin
$ ls -l -h gen/example-blob.bin
-rw-rw-r--. 1 danielbevenius danielbevenius 130K Jul 14 09:52 gen/example-blob.bin
```
This binary can then be specified using:
```c++
  V8::InitializeExternalStartupDataFromFile("../gen/example-blob.bin");
```

`src/init/startup-data-util.cc` contains the implementation for this and notice
that it is guarded by a macro:
```c++
void InitializeExternalStartupDataFromFile(const char* snapshot_blob) {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  LoadFromFile(snapshot_blob);
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
}
```
So we need to make sure that we have set `v8_use_external_startup_data` to true
in `args.gn` and rebuild v8.

There is an example, [snapshot_test](./test/snapshot_test.cc) that explores
using the binary blob.

### embedded-src
This is also a command line options that can be passed to `mksnapshot`.
```console
$ ../v8_src/v8/out/x64.release_gcc/mksnapshot --startup_src=gen/example-snapshot.cc --startup-blob=gen/example-blob.bin --embedded-src=gen/example-embedded.S
```
`src/snapshot/embedded/embedded-file-writer.h` declares the class responsible
for generating this file.
How is this file used?  
TODO: explain how this is used.

### embed-script
Not to be confused with `embedded-src` this is any extra javascript that can
be passed as the `extra` option argument on the command line to mksnapshot.
```console
$ ../v8_src/v8/out/x64.release_gcc/mksnapshot --startup_src=gen/example-snapshot.cc --startup-blob=gen/example-blob.bin --embedded-src=gen/example-embedded.S lib/embed-script.js
Loading script for embedding: lib/embed-script.js
```

### Snapshot usage
Do understand what is actually happening 
[snapshot_test](./test/snapshot_test.cc) has a test named `CreateSnapshot`
which creates a creates a V8 environment (initializes the Platform, creates
an Isolate, and a Context), adds a function named `test_snapshot` to the 
Context and then creates a snapshot using SnapshotCreator.

SnapshotCreator creates the binary in a class named `StartupData `which can be
found in `v8.h`:
```c++
class V8_EXPORT StartupData {
  bool CanBeRehashed() const;
  bool IsValid() const;
  const char* data;
  int raw_size;
};
```
After creating the StartupData (which is called a blob (Binary Large Object)
the V8 environment is disposed of. 
Next, a new V8 environmentis created but this time the Context is created using
```c++
  Isolate::CreateParams create_params;
  // Specify the startup_data blob created earlier.
  create_params.snapshot_blob = &startup_data;
  isolate = Isolate::New(create_params);

  Local<Context> context = Context::FromSnapshot(isolate, index).ToLocalChecked();
```
After this we are going to call the function that we serialized previously and
verify that that works.

So that was a javascript function that was serialized into a snapshot blob and
then deserialized. 

Note, that the tests in snapshot_test will contain a bit of duplicated code
which is intentional as I think it makes it easer to see what is required to
perform these tasks and this is only for learning.


### Context::FromSnapshot
This function can be used to deserialize a blob into a Context. This was used
in the simple CreateSnapshot test:
```c++
    Local<Context> context = Context::FromSnapshot(isolate, index).ToLocalChecked();
```
We only passed in the isolate and the index above but `FromSnapshot` also takes
other arguments that have default values when not specified:
```c++
static MaybeLocal<Context> FromSnapshot(
      Isolate* isolate, size_t context_snapshot_index,
      DeserializeInternalFieldsCallback embedder_fields_deserializer =
          DeserializeInternalFieldsCallback(),
      ExtensionConfiguration* extensions = nullptr,
      MaybeLocal<Value> global_object = MaybeLocal<Value>(),
      MicrotaskQueue* microtask_queue = nullptr);
```


### External references
When we have functions that are not defined in V8 itself these functions will
have addresses that V8 does not know about. The function will be a symbol that
needs to be resolved when V8 deserialzes or when it tries to run the function.
TODO: Verify that this is actually what happend!

These symbols are serialized and when V8 deserializes a snapshot it will try
to match symbols to addresses. This is done using `external_references` which
is a null terminated vector which is populated with the addresses to functions
and then passed into SnapshotCreator:
```c++
  std::vector<intptr_t> external_refs;
  external_refs.push_back(reinterpret_cast<intptr_t>(ExternalRefFunction));
  ...
  isolate = Isolate::Allocate();
  SnapshotCreator snapshot_creator(isolate, external_refs.data());
```
After creating the blob and perhaps storing it in a file, it can later be
used to create a new Context:
```c++
  Isolate::CreateParams create_params;
  create_params.snapshot_blob = &startup_data;
  create_params.external_references = external_refs.data();

  Isolate* isolate = Isolate::New(create_params);
  ...
  Local<Context> context = Context::FromSnapshot(isolate, index).ToLocalChecked();
```
There is a test case that explores this feature and it can be run using:
```console
$ ./test/snapshot_test --gtest_filter=SnapshotTest.ExternalReference
```

### Snapshot binary format
The layout of the binary snapshot can be found in `src/snapshot/snapshot.cc`:
```c++
  // Snapshot blob layout:
  // [0] number of contexts N
  // [1] rehashability
  // [2] checksum
  // [3] (128 bytes) version string
  // [4] offset to readonly
  // [5] offset to context 0
  // [6] offset to context 1
  // ...
  // ... offset to context N - 1
  // ... startup snapshot data
  // ... read-only snapshot data
  // ... context 0 snapshot data
  // ... context 1 snapshot data
```
