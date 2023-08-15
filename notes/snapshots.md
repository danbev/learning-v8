### Snapshots
JavaScript specifies a lot of built-in functionality which every V8 context must
provide.  For example, you can run Math.PI and that will work in a JavaScript
console/repl.

The global object and all the built-in functionality must be setup and
initialized into the V8 heap. This can be time consuming and affect runtime
performance if this has to be done every time. 

Now this is where the file `snapshot_blob.bin` comes into play. 
But what is this bin file?  
This blob is a prepared snapshot that gets directly deserialized into the heap
to provide an initilized context.

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
The mksnapshot executable takes a number of arguments one of which is the target
OS which can be different from the host OS. Examples of targets can be found
in `BUILD.gn` and are `android`, `fuchsia`, `ios`, `linux`, `mac`, and `win`.

There is a template named `run_mksnapshot` in `BUILD.gn` that shows how
`mksnapshot` is called.

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
is not set `--startup_src` will be passed instead:
```
  } else {
      outputs += [ "$target_gen_dir/snapshot${suffix}.cc" ]
      args += [
        "--startup_src",
        rebase_path("$target_gen_dir/snapshot${suffix}.cc", root_build_dir),
      ]
    }
```
To see what is actually getting used when building one can look at the file
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
This option specifies a path to a C++ source file that will be generated by
mksnapshot, for example:
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
This is also a command line option that can be passed to `mksnapshot` which 
will generate a binary file in the path specified. Note that this can be
specified in addition to `startup-src` which I was not aware of:
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
This is also a command line option that can be passed to `mksnapshot`.
```console
$ ../v8_src/v8/out/x64.release_gcc/mksnapshot --startup_src=gen/example-snapshot.cc --startup-blob=gen/example-blob.bin --embedded-src=gen/example-embedded.S
```
`src/snapshot/embedded/embedded-file-writer.h` declares the class responsible
for generating this file.
How is this file used?  TODO: explain how this is used.

### embed-script
Not to be confused with `embedded-src` this is any extra javascript that can
be passed as the `extra` option argument on the command line to mksnapshot.
```console
$ ../v8_src/v8/out/x64.release_gcc/mksnapshot --startup_src=gen/example-snapshot.cc --startup-blob=gen/example-blob.bin --embedded-src=gen/example-embedded.S lib/embed-script.js
Loading script for embedding: lib/embed-script.js
```

### Snapshot usage
To understand what is actually happening 
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
Next, a new V8 environment is created but this time the Context is created using
the StartupData:
```c++
  Isolate::CreateParams create_params;
  // Specify the startup_data blob created earlier.
  create_params.snapshot_blob = &startup_data;
  isolate = Isolate::New(create_params);

  Local<Context> context = Context::FromSnapshot(isolate, index).ToLocalChecked();
```
After this we are going to call the function that we serialized previously and
verify that that works.

So that was a JavasCript function that was serialized into a snapshot blob and
then deserialized. 

Note, that the tests in snapshot_test will contain a bit of duplicated code
which is intentional as I think it makes it easer to see what is required to
perform these tasks plus this is only for learning.


### Context::FromSnapshot
This function can be used to deserialize a blob into a Context. This was used
in the CreateSnapshot test:
```c++
    Local<Context> context = Context::FromSnapshot(isolate, index).ToLocalChecked();
```
We only passed in the isolate and the index above, but `FromSnapshot` also takes
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

In our case Context::FromSnapshot will call:
```c++
return NewContext(external_isolate, extensions, MaybeLocal<ObjectTemplate>(), 
                    global_object, index_including_default_context,             
                    embedder_fields_deserializer, microtask_queue)
```

This will trickle down into bootstrapper.cc which has the following call:
```c++
if (isolate->initialized_from_snapshot()) {                                       
    Handle<Context> context;                                                        
    if (Snapshot::NewContextFromSnapshot(isolate, global_proxy,                     
                                         context_snapshot_index,                    
                                         embedder_fields_deserializer)              
            .ToHandle(&context)) {                                                  
      native_context_ = Handle<NativeContext>::cast(context);                       
    }                                                                               
  }              
```
This call will land in snapshot.cc:
```c++
MaybeHandle<Context> Snapshot::NewContextFromSnapshot(
    Isolate* isolate, Handle<JSGlobalProxy> global_proxy, size_t context_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  ...
  const v8::StartupData* blob = isolate->snapshot_blob();
  bool can_rehash = ExtractRehashability(blob);
  Vector<const byte> context_data = SnapshotImpl::ExtractContextData(
      blob, static_cast<uint32_t>(context_index));
  SnapshotData snapshot_data(MaybeDecompress(context_data));

  MaybeHandle<Context> maybe_result = ContextDeserializer::DeserializeContext(
      isolate, &snapshot_data, can_rehash, global_proxy,
      embedder_fields_deserializer);

  Handle<Context> result;
  if (!maybe_result.ToHandle(&result)) return MaybeHandle<Context>();

  return result;
}
```
In this case the most interesting part is the
ContextDeserializer::DeserializeContext call (src/snapshot/context-deserializer.cc):
```c++
MaybeHandle<Context> ContextDeserializer::DeserializeContext(
    Isolate* isolate, const SnapshotData* data, bool can_rehash,
    Handle<JSGlobalProxy> global_proxy,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  ContextDeserializer d(isolate, data, can_rehash);

  MaybeHandle<Object> maybe_result =
      d.Deserialize(isolate, global_proxy, embedder_fields_deserializer);

  Handle<Object> result;
  return maybe_result.ToHandle(&result) ? Handle<Context>::cast(result)
                                        : MaybeHandle<Context>();
}
```
`d.Deserialize` will call `ContextDeserializer::Deserialize`.


### External references
When we have functions that are not defined in V8 itself these functions will
have addresses that V8 does not know about. The function will be a symbol that
needs to be resolved when V8 deserializes a blob.

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
One thing to keep in mind is that normally the creation of a snapshot would be
done in a separate process and stored on an external device like the file
system. In the tests everything is in the same process so the functions address
will remains the same and not change and one might ask why does this have to
be done at all, but the reason is that the function address would change.

So how does the SnapshotCreator use this list of addresses?  
Well this must be done during serialization, and to find out where we can comment
out the the adding of the external reference in our test. Doing that we'll find
that the following error is displayed:
```console
Unknown external reference 0x419822.
```
So we can set a break point where this is generated:
```console
(lldb) br s -f external-reference-encoder.cc -l 74
```
When an ExternalReferenceEncoder is created it is passed an isolate
(src/codegen/external-reference-encoder.cc).
```console
(lldb) br s -f external-reference-encoder.cc -l 34
```

```c++
ExternalReferenceEncoder::ExternalReferenceEncoder(Isolate* isolate) { 
  ...
  map_ = isolate->external_reference_map();                                     
  if (map_ != nullptr) return;                                                  
  map_ = new AddressToIndexHashMap();                                           
  isolate->set_external_reference_map(map_);                                    
  // Add V8's external references.                                              
  ExternalReferenceTable* table = isolate->external_reference_table();          
  for (uint32_t i = 0; i < ExternalReferenceTable::kSize; ++i) {                
    Address addr = table->address(i);                                           
    // Ignore duplicate references.                                             
    // This can happen due to ICF. See http://crbug.com/726896.                 
    if (map_->Get(addr).IsNothing()) map_->Set(addr, Value::Encode(i, false));  
    DCHECK(map_->Get(addr).IsJust());                                           
  }                  

  // Add external references provided by the embedder.                          
  const intptr_t* api_references = isolate->api_external_references();          
  if (api_references == nullptr) return;                                        
  for (uint32_t i = 0; api_references[i] != 0; ++i) {                           
    Address addr = static_cast<Address>(api_references[i]);                     
    // Ignore duplicate references.                                             
    // This can happen due to ICF. See http://crbug.com/726896.                 
    if (map_->Get(addr).IsNothing()) map_->Set(addr, Value::Encode(i, true));   
    DCHECK(map_->Get(addr).IsJust());                                           
  }            
}
```
Notice that the `map_` is retrieved from the Isolate which is of type
AddressToIndexHashMap:
```c++
class AddressToIndexHashMap : public PointerToIndexHashMap<Address> {}; 
```
Now, in our case `map_` is nullptr so a new map is created and also set
on the isolate.

Then, V8's external references are added to the newly created map and after
that we have our api_references that we set: 
```console
(lldb) expr addr
(v8::internal::Address) $0 = 4298802
```
Which matches the address of ExternalRefFunction we print in the test:
```console
address of ExternalRefFunction function: 4298802
```
```console
(lldb) expr map_->Get(addr)
(v8::Maybe<unsigned int>) $9 = (has_value_ = true, value_ = 2147483648)
````
So that is how at a later stage it is possible to reference:
```console
(lldb) expr isolate->external_reference_map()->Get(4298802)
(v8::Maybe<unsigned int>) $11 = (has_value_ = true, value_ = 2147483648)
```
TODO: How is the actually used?  

So after we have created the StartupData blob and then use it when creating
a new Isolate, the blob will be deserialised. In
Deserializer::ReadSingleBytecodeData we then have:
```c++
    case kApiReference: {                                                           
      uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());              
      Address address;                                                              
      if (isolate()->api_external_references()) {                                   
        address = static_cast<Address>(                                             
            isolate()->api_external_references()[reference_id]);                    
      } else {                                                                      
        address = reinterpret_cast<Address>(NoExternalReferencesCallback);          
      }                                                                             
      if (V8_HEAP_SANDBOX_BOOL && data == kSandboxedApiReference) {                 
        return WriteExternalPointer(slot_accessor.slot(), address,                  
                                    kForeignForeignAddressTag);                     
      } else {                                                                      
        DCHECK(!V8_HEAP_SANDBOX_BOOL);                                              
        return WriteAddress(slot_accessor.slot(), address);                         
      }                                                                             
    }                            
```

```console
(lldb) expr reference_id
(uint32_t) $15 = 0
(lldb) expr isolate()->api_external_references()[0]
(intptr_t) $18 = 4298802
```
So in the blob the current source_ position is of type `kApiReference` type and
the value is an index into the api_external_references array. This address
is then written to the using WriteAddress which does a memory copy:
```c++
template <typename TSlot>
int Deserializer::WriteAddress(TSlot dest, Address value) {
  base::Memcpy(dest.ToVoidPtr(), &value, kSystemPointerSize);
  return (kSystemPointerSize / TSlot::kSlotDataSize);
}
```
So 4298802 (the value of the passed in Address which is also the pointer to
our external function) will be written to the destination slot in memory.

Just to recap this a little and remember that I'm using a single test here but
in a real world situation the StartupData would have been written to some
external storage like the file system. Another program would then use the blob
and the address of the external function would most likely be a different
address, so this is where V8 need to link this with the correct address of the
function. In the ExternalReference test we now have two different functions
being used. One is used in the snapshot creation and the other is used when
creating a new Isolate using the blob. We can see that the second function
will be called in this case.

### SnapshotCreator
In the constructor of SnapshotCreator we have the following code:
```c++
SnapshotCreator::SnapshotCreator(Isolate* isolate,
                                 const intptr_t* external_references,
                                 StartupData* existing_snapshot) {
  SnapshotCreatorData* data = new SnapshotCreatorData(isolate)
}
```
`SnapshotCreatorData` is an internal struct that has the following members:
```c++
  explicit SnapshotCreatorData(Isolate* isolate)
      : isolate_(isolate),
        default_context_(),
        contexts_(isolate),
        created_(false) {}

  ArrayBufferAllocator allocator_;
  Isolate* isolate_;
  Persistent<Context> default_context_;
  SerializeInternalFieldsCallback default_embedder_fields_serializer_;
  PersistentValueVector<Context> contexts_;
  std::vector<SerializeInternalFieldsCallback> embedder_fields_serializers_;
  bool created_;
```
Next, in the constructor we have:
```c++
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  internal_isolate->set_array_buffer_allocator(&data->allocator_);
  internal_isolate->set_api_external_references(external_references);
  internal_isolate->enable_serializer();
  isolate->Enter();
```
The casting to the internal isolate is something we have seen before. After that
the arraybuffer allocator will be set followed by the external references. 
`set_api_external_references` is generated by the preprocessor macro
`ISOLATE_INIT_LIST` which can be found in src/execution/isolate.h:
```c++
#define GLOBAL_ACCESSOR(type, name, initialvalue)                \
  inline type name() const {                                     \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_); \
    return name##_;                                              \
  }                                                              \
  inline void set_##name(type value) {                           \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_); \
    name##_ = value;                                             \
  }
  ISOLATE_INIT_LIST(GLOBAL_ACCESSOR)
#undef GLOBAL_ACCESSOR

#define ISOLATE_INIT_LIST(V)                                                   \
...
  V(const intptr_t*, api_external_references, nullptr)                         \
```
And this is simple generating a getter named `api_external_references` and the
setter will be named `set_api_external_references`:
```console
(lldb) expr internal_isolate->api_external_references()
(const intptr_t *) $1 = 0x0000000000495cf0
```
Next, the serializer is enabled and the Isolate Entered followed by the creation
of a StartupData pointer:
```c++
  const StartupData* blob = existing_snapshot
                                ? existing_snapshot
                                : i::Snapshot::DefaultSnapshotBlob();
```
In our case there is no existing_snapshot so the default snapshot blob will be
used. Now, the DefaultSnapshotBlob was created by V8 using `mksnapshot` which
like node_mkshapshot also produces a C++ file, this one is names snapshot.cc,
and we are setting the StartupData pointer to point to it.
After that the StartupData pointer is set on the internal_isolate and
Snapshot::Initialize is called:
```c++
    internal_isolate->set_snapshot_blob(blob);
    i::Snapshot::Initialize(internal_isolate);
```
Snapshot::Initialize can be found in src/snapshot/snapshot.cc:
```c++
  const v8::StartupData* blob = isolate->snapshot_blob();
  SnapshotImpl::CheckVersion(blob);
  CHECK(VerifyChecksum(blob));
  Vector<const byte> startup_data = SnapshotImpl::ExtractStartupData(blob);
  Vector<const byte> read_only_data = SnapshotImpl::ExtractReadOnlyData(blob);

  SnapshotData startup_snapshot_data(MaybeDecompress(startup_data));
  SnapshotData read_only_snapshot_data(MaybeDecompress(read_only_data));

  bool success = isolate->InitWithSnapshot(&startup_snapshot_data,
                                           &read_only_snapshot_data,
                                           ExtractRehashability(blob));
```

`InitWithSnapshot` will call `Init`
```c++
#define ASSIGN_ELEMENT(CamelName, hacker_name)                  \
  isolate_addresses_[IsolateAddressId::k##CamelName##Address] = \
      reinterpret_cast<Address>(hacker_name##_address());
  FOR_EACH_ISOLATE_ADDRESS_NAME(ASSIGN_ELEMENT)
#undef ASSIGN_ELEMENT
```
This can be expanded using the preprocessor:
```console
$ g++ -Iout/learning_v8/gen -Iinclude -I. -E src/execution/isolate.cc
```
```c++
isolate_addresses_[IsolateAddressId::kHandlerAddress] = reinterpret_cast<Address>(handler_address());
isolate_addresses_[IsolateAddressId::kCEntryFPAddress] = reinterpret_cast<Address>(c_entry_fp_address());
isolate_addresses_[IsolateAddressId::kCFunctionAddress] = reinterpret_cast<Address>(c_function_address());
isolate_addresses_[IsolateAddressId::kContextAddress] = reinterpret_cast<Address>(context_address());
isolate_addresses_[IsolateAddressId::kPendingExceptionAddress] =
    reinterpret_cast<Address>(pending_exception_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerContextAddress] =
    reinterpret_cast<Address>(pending_handler_context_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerEntrypointAddress] =
    reinterpret_cast<Address>(pending_handler_entrypoint_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerConstantPoolAddress] =
    reinterpret_cast<Address>(pending_handler_constant_pool_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerFPAddress] =
    reinterpret_cast<Address>(pending_handler_fp_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerSPAddress] =
    reinterpret_cast<Address>(pending_handler_sp_address());
isolate_addresses_[IsolateAddressId::kExternalCaughtExceptionAddress] =
    reinterpret_cast<Address>(external_caught_exception_address());
isolate_addresses_[IsolateAddressId::kJSEntrySPAddress] =
    reinterpret_cast<Address>(js_entry_sp_address());
```
Next CodeRanges will be initialized.
```c++
  InitializeCodeRanges();
```
I'm going to skip along here a little as I'm most interested in the external
references and their usage in this function:
```c++
  isolate_data_.external_reference_table()->Init(this);
```
So isolate_data has a pointer to an ExternalReferenceTable and `init` will land
in src/codegen/external-reference-table.cc:
```c++
void ExternalReferenceTable::Init(Isolate* isolate) {
  int index = 0;

  // kNullAddress is preserved through serialization/deserialization.
  Add(kNullAddress, &index);
  AddReferences(isolate, &index);
  AddBuiltins(&index);
  AddRuntimeFunctions(&index);
  AddIsolateAddresses(isolate, &index);
  AddAccessors(&index);
  AddStubCache(isolate, &index);
  AddNativeCodeStatsCounters(isolate, &index);
  is_initialized_ = static_cast<uint32_t>(true);

  CHECK_EQ(kSize, index);
}
```
Lets take a look at these Add functions and see what they are doing.
First, `ref_addr_` is an array of v8::internal::Address's:
```c++
void ExternalReferenceTable::Add(Address address, int* index) {                 
  ref_addr_[(*index)++] = address;                                              
}
```

```console
(lldb) expr ref_addr_
(v8::internal::Address [986]) $14 = {
  [0] = 16045690975706529519
  [1] = 16045690975706529519
```
And the first entry in this array will be set to the kNullAddress, after the
first call to Add:
```console
(v8::internal::Address [986]) $18 = {
  [0] = 0
  [1] = 16045690975706529519
```
Next, we have:
```c++
  AddReferences(isolate, &index);
```
AddReferences has a few macros which can be expanded using:
```console
$ g++ -Iout/learning_v8/gen -Iinclude -I. -E src/codegen/external-reference-table.cc
```

Lets take a concrete example:
```c++
Add(ExternalReference::abort_with_reason().address(), index);
```
```console
(lldb) expr ExternalReference::abort_with_reason
(v8::internal::ExternalReference (*)()) $19 = 0x00007ffff610069e (libv8.so`v8::internal::ExternalReference::abort_with_reason() at external-reference.cc:420:1)
```
And if we look in external-reference.cc we find:
```c++
FUNCTION_REFERENCE(abort_with_reason, i::abort_with_reason)

ExternalReference ExternalReference::abort_with_reason() {
  static_assert(IsValidExternalReferenceType<decltype(&i::abort_with_reason)>::value, "IsValidExternalReferenceType<decltype(&i::abort_with_reason)>::value");

  return ExternalReference(Redirect((reinterpret_cast<v8::internal::Address>(i::abort_with_reason))));
}
```
Now, `abort_with_reason` can be found in src/codegen/external-reference.cc:
```c++
void abort_with_reason(int reason) {                                                  
  if (IsValidAbortReason(reason)) {                                                   
    const char* message = GetAbortReason(static_cast<AbortReason>(reason));     
    base::OS::PrintError("abort: %s\n", message);                                     
  } else {                                                                            
    base::OS::PrintError("abort: <unknown reason: %d>\n", reason);                    
  }                                                                                   
  base::OS::Abort();                                                                  
  V8_Fatal("unreachable code");                                                       
}
```
So what is happening here is that this function is being added to the array
of Address's at location 1. Remember that V8 also supports snapshots and has
the command mksnapshot which creates a snapshot. It too has functions addresses
that need to be resolved when the snapshot is loaded.

But we have not seen any of the external references that we passed in, these
are still V8's references as far as I can tell. And also notice that these
are added to the Isolate when it is created. When we later create a context
is when we pass in the StartupData:
```c++
  Local<Context> context = Context::FromSnapshot(isolate, index).ToLocalChecked();
```

After we have returned from this function we will be back in Isolate::Init
and after stepping through a little we come to:
```c++
   if (create_heap_objects) {                                                  
      heap_.read_only_space()->ClearStringPaddingIfNeeded();                    
      read_only_heap_->OnCreateHeapObjectsComplete(this);                       
    } else {                                                                    
      StartupDeserializer startup_deserializer(this, startup_snapshot_data, can_rehash);                     
      startup_deserializer.DeserializeIntoIsolate();                            
    }                                                  
```
So we are creating a new StartupDeserializer and passing in the current Isolate,
the startup_snapshot_data.

Now in Deserializer::Deserializer we find the following:
```c++
#ifdef DEBUG
 num_api_references_ = 0;                                                      
  // The read-only deserializer is run by read-only heap set-up before the          
  // heap is fully set up. External reference table relies on a few parts of    
  // this set-up (like old-space), so it may be uninitialized at this point.    
  if (isolate->isolate_data()->external_reference_table()->is_initialized()) {  
    // Count the number of external references registered through the API.          
    if (isolate->api_external_references() != nullptr) {                        
      while (isolate->api_external_references()[num_api_references_] != 0) {    
        num_api_references_++;                                                  
      }                                                                         
    }                                                                           
  }                
```
This is only for debug but these are the references that were set earlier in
IsolateData:
```c++ 
  internal_isolate->set_api_external_references(external_references);
```
After this we the StartupSerializer constructor is finished and the next line (
now aback in isolate.cc and Isolate::Init) we have:
```c++
  startup_deserializer.DeserializeIntoIsolate();
``` 
In src/snapshot/startup-deserializer.cc we find the following:
```c++
void StartupDeserializer::DeserializeIntoIsolate() {
{                                                                             
  ...
  {

    isolate()->heap()->IterateSmiRoots(this);                                   
    isolate()->heap()->IterateRoots(                                            
        this,                                                                   
        base::EnumSet<SkipRoot>{SkipRoot::kUnserializable, SkipRoot::kWeak});   
    Iterate(isolate(), this);                                                   
    DeserializeStringTable();                                                   
                                                                                
    isolate()->heap()->IterateWeakRoots(                                        
        this, base::EnumSet<SkipRoot>{SkipRoot::kUnserializable});              
    DeserializeDeferredObjects();                                               
    for (Handle<AccessorInfo> info : accessor_infos()) {                        
      RestoreExternalReferenceRedirector(isolate(), info);                      
    }                                                                           
    for (Handle<CallHandlerInfo> info : call_handler_infos()) {                 
      RestoreExternalReferenceRedirector(isolate(), info);                      
    }              

   }

}
```
StartUpDeserializer extends Deserializer which extends SerializerDeserializer
which in turn extends RootVisitor. And in Deserializer::VisitRootPointers
we can find:
```
void Deserializer::ReadData(FullMaybeObjectSlot start,                          
                            FullMaybeObjectSlot end) {                          
  FullMaybeObjectSlot current = start;                                          
  while (current < end) {                                                       
    byte data = source_.Get();                                                  
    current += ReadSingleBytecodeData(data, SlotAccessorForRootSlots(current)); 
  }                                                                             
  CHECK_EQ(current, end);                                                       
}
```
If we look at ReadSingleBytecodeData we can find rather large switch statement
that takes the data passed in:
```c++
  switch (data) {
    ....

    case kApiReference: {                                                       
      uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());          
      Address address;                                                          
      if (isolate()->api_external_references()) {                               
        DCHECK_WITH_MSG(reference_id < num_api_references_,                     
                        "too few external references provided through the API");
        address = static_cast<Address>(                                         
            isolate()->api_external_references()[reference_id]);                
      } else {                                                                  
        address = reinterpret_cast<Address>(NoExternalReferencesCallback);      
      }                                                                         
      if (V8_HEAP_SANDBOX_BOOL && data == kSandboxedApiReference) {             
        return WriteExternalPointer(slot_accessor.slot(), address,              
                                    kForeignForeignAddressTag);                 
      } else {                                                                  
        DCHECK(!V8_HEAP_SANDBOX_BOOL);                                          
        return WriteAddress(slot_accessor.slot(), address);                     
      }                                                                         
    }                                           
```
This is where the references that we passed come into play. If the instance_type
of the Map of the HeapObject that is being read from the snapshot is of type
`kApiReference` then use the value reference_id as the index into the
api_external_references array.

```console
(lldb) br s -f deserializer.cc -l 1016
```


### StartupDeserializer



### Internal Fields
We looked at how external functions can be handled when creating a snapshot,
but we also have to be able to deal with things like classes and be able to
serialize/deserialize them (at least that is my understading so far).

When adding a Context to SnapshotCreator there is a parameter named
`callback` which is of type `SerializeInternalFieldsCallback` :
```c++
size_t AddContext(Local<Context> context,
    SerializeInternalFieldsCallback callback = SerializeInternalFieldsCallback());

struct SerializeInternalFieldsCallback {

  typedef StartupData (*CallbackFunction)(Local<Object> holder, int index,
                                          void* data);

  SerializeInternalFieldsCallback(CallbackFunction function = nullptr,
                                  void* data_arg = nullptr)
      : callback(function), data(data_arg) {}

  CallbackFunction callback;
  void* data;
};
```
An implementation can be provided by creating a new instance of this struct
and passing in the data that should be passed to it's callback function.

When is the callback called?
There is a test case that demonstrates this and it has a callback in which
a break point can be set:
```console
./test/snapshot_test --gtest_filter=SnapshotTest.InternalFields
````

`src/snapshot/context-serializer.cc` contains the following function:
```c++
bool ContextSerializer::SerializeJSObjectWithEmbedderFields(                    
    Handle<HeapObject> obj) {     
```
Lets stick a break point in that function and work backwards from there:
```console
(lldb) br s -f context-serializer.cc -l 208
```
Starting from Snapshot::Create, which will iterate over all the contexts passed
in:
```c++
v8::StartupData Snapshot::Create(
    Isolate* isolate, std::vector<Context>* contexts,
    const std::vector<SerializeInternalFieldsCallback>& embedder_fields_serializers,
    const DisallowGarbageCollection& no_gc, SerializerFlags flags) {

    const int num_contexts = static_cast<int>(contexts->size());
    for (int i = 0; i < num_contexts; i++) {
      ContextSerializer context_serializer(isolate, flags, &startup_serializer,
                                         embedder_fields_serializers[i]);
      context_serializer.Serialize(&contexts->at(i), no_gc);
      can_be_rehashed = can_be_rehashed && context_serializer.can_be_rehashed();
      context_snapshots.push_back(new SnapshotData(&context_serializer));
    }
  }
```
Notice that each context will have a ContextSerializer created for it, and it
gets passed the SerializeInternalFieldsCallback and Serialized will be called
on that instance passing in the first context. So that leads us to
ContextSeralizer::Serialize:
```c++
void ContextSerializer::Serialize(Context* o, const DisallowGarbageCollection& no_gc) {
```c++
  VisitRootPointer(Root::kStartupObjectCache, nullptr, FullObjectSlot(o));
```
The second argument is for the description (cont char*). VisitRootPointers can
be found in visitors.h and will delegate to Serializer::VisitRootPointers:
(src/snapshot/serializer.cc)
```c++
void Serializer::VisitRootPointers(Root root, const char* description,          
                                   FullObjectSlot start, FullObjectSlot end) {  
  for (FullObjectSlot current = start; current < end; ++current) {              
    SerializeRootObject(current);                                               
  }                                                                             
}
```
And SerializeRootObject will call SerializeObject in our case:
```c++
void Serializer::SerializeRootObject(FullObjectSlot slot) {
  Object o = *slot;
  if (o.IsSmi()) {
    PutSmiRoot(slot);
  } else {
    SerializeObject(Handle<HeapObject>(slot.location()));
  }
}

void Serializer::SerializeObject(Handle<HeapObject> obj) {
  // ThinStrings are just an indirection to an internalized string, so elide the
  // indirection and serialize the actual string directly.
  if (obj->IsThinString(isolate())) {
    obj = handle(ThinString::cast(*obj).actual(isolate()), isolate());
  }
  SerializeObjectImpl(obj);
}
```
This will bring us into ContextSerializer::SerializeObjectImpl
(src/snapshot/context-serializer.cc):
```c++
void ContextSerializer::SerializeObjectImpl(Handle<HeapObject> obj) {
  ...
  if (SerializeJSObjectWithEmbedderFields(obj)) {
    return;
  }
  ...
}
```
`SerializeJSObjectWithEmbedderFields` (src/snapshot/context-serializer.cc):
```c++
bool ContextSerializer::SerializeJSObjectWithEmbedderFields(Handle<HeapObject> obj) {
  ...
  std::vector<StartupData> serialized_data;

  for (int i = 0; i < embedder_fields_count; i++) {
    EmbedderDataSlot embedder_data_slot(*js_obj, i);
    original_embedder_values.emplace_back(embedder_data_slot.load_raw(isolate(), no_gc));
    Object object = embedder_data_slot.load_tagged();
    if (object.IsHeapObject()) {
      serialized_data.push_back({nullptr, 0});
    } else {
      if (serialize_embedder_fields_.callback == nullptr && object == Smi::zero()) {
        serialized_data.push_back({nullptr, 0});
      } else {
        StartupData data = serialize_embedder_fields_.callback(
            api_obj, i, serialize_embedder_fields_.data);
        serialized_data.push_back(data);
      }
    }
  }
``` 
Notice that this call to the callback, `serialize_embedder_fields_.callback`
returns an instance of StartupData, which is then added to the serialized_data
vector:
```c++
class V8_EXPORT StartupData {                                                   
   public:                                                                        
    bool CanBeRehashed() const;                                                        
    bool IsValid() const;                                                              
    const char* data;                                                             
    int raw_size;                                                                 
  };          
```
A little further down we then have:
```c++
  for (int i = 0; i < embedder_fields_count; i++) {
    StartupData data = serialized_data[i];
    if (DataIsEmpty(data)) continue;
    // Restore original values from cleared fields.
    EmbedderDataSlot(*js_obj, i).store_raw(isolate(), original_embedder_values[i], no_gc);
    embedder_fields_sink_.Put(kNewObject, "embedder field holder");
    embedder_fields_sink_.PutInt(reference->back_ref_index(), "BackRefIndex");
    embedder_fields_sink_.PutInt(i, "embedder field index");
    embedder_fields_sink_.PutInt(data.raw_size, "embedder fields data size");
    embedder_fields_sink_.PutRaw(reinterpret_cast<const byte*>(data.data),
                                 data.raw_size, "embedder fields data");
    delete[] data.data;
  }
```
TODO: take a closer look at how this works.
So what the StartupData that is returned from the callback will be placed into
the output sink for the. 

```console
$ lldb -- ./test/snapshot_test --gtest_filter=SnapshotTest.InternalFields
(lldb) br s -f snapshot_test.cc -l 244
Breakpoint 4: where = snapshot_test`SnapshotTest_InternalFields_Test::TestBody() + 25 at snapshot_test.cc:247:25, address = 0x00000000004192e3
(lldb) r
-> 283 	      index = snapshot_creator.AddContext(context, si_cb);
(lldb) s
```
In SnapshotCreator (api.cc) we can find the following:
```c++
size_t SnapshotCreator::AddContext(Local<Context> context,
                                   SerializeInternalFieldsCallback callback) {
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  ...
  size_t index = data->contexts_.Size();
  data->contexts_.Append(context);
  data->embedder_fields_serializers_.push_back(callback)
  return index;
```
So the SnapshotCreateData instance has a vector that contains these callback:
```c++
std::vector<SerializeInternalFieldsCallback> embedder_fields_serializers_;
```
So that is all `AddContext` does. Lets take a look at the call to actually
create the blob:
```c++
  startup_data = snapshot_creator.CreateBlob(SnapshotCreator::FunctionCodeHandling::kKeep);
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
