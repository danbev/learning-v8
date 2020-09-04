### Heap
This document will try to describe the Heap in V8 and the related classes and
functions.

### Spaces
A space refers to parts of the heap that are handled in different ways with
regards to garbage collection:

```console
+----------------------- -----------------------------------------------------------+
|   Young Generation                  Old Generation          Large Object space    |
|  +-------------+--------------+  +-----------+-------------+ +------------------+ |
|  |        NEW_SPACE           |  | MAP_SPACE | OLD_SPACE   | | LO_SPACE         | |
|  +-------------+--------------+  +-----------+-------------+ +------------------+ |
|  |to_SemiSpace |from_SemiSpace|                                                   |
|  +-------------+--------------+  +-----------+-------------+ +------------------+ |
|  +-------------+                +-----------+               +------------------+  |
|  | NEW_LO_SPACE|                | CODE_SPACE|               | CODE_LO_SPACE    |  |
|  +-------------+                +-----------+               +------------------+  |
|                                                                                   |
|   Read-only                                                                       |
|  +--------------+                                                                 |
|  | RO_SPACE     |                                                                 |
|  +--------------+                                                                 |
+-----------------------------------------------------------------------------------+
```
The `Map space` contains only map objects and they are non-movable.

The base class for all spaces is BaseSpace.

#### BaseSpace
BaseSpace is a super class of all Space classes in V8 which are spaces that are
not read-only (sealed).

What are the spaces that are defined?  
* NewSpace
* PagedSpace
* SemiSpace
* LargeObjectSpace
* others ?

BaseSpace extends Malloced:
```c++
class V8_EXPORT_PRIVATE BaseSpace : public Malloced {
```
`Malloced` which can be found in in `src/utils/allocation.h`:
```c++
// Superclass for classes managed with new & delete.
class V8_EXPORT_PRIVATE Malloced {
 public:
  static void* operator new(size_t size);
  static void operator delete(void* p);
};
```
And we can find the implemention in `src/utils/allocation.cc`:
```c++
void* Malloced::operator new(size_t size) {
  void* result = AllocWithRetry(size);
  if (result == nullptr) {
    V8::FatalProcessOutOfMemory(nullptr, "Malloced operator new");
  }
  return result;
}

void Malloced::operator delete(void* p) { free(p); }
```
So for all classes of type BaseSpace when those are created using the `new`
operator, the `Malloced::operator new` version will be called. This is like
other types of operator overloading in c++. Here is a standalone example
[new-override.cc](https://github.com/danbev/learning-cpp/blob/master/src/fundamentals/new-override.cc). 

We can try this out using the debugger:
```console
$ lldb -- test/heap_test --gtest_filter=HeapTest.AllocateRaw
(lldb) br s -f heap_test.cc -l 30
(lldb) r
(lldb) s
```
This will land us in v8::internal::Malloced::operator new. The function
`AllocWithRetry' can be found in `allocation.cc` and will call `malloc`.

So a space aquires chunks of memory from the operating system. 

An instance of BaseSpace has a Heap associated with it, an AllocationSpace, and
committed and uncommited counters.

`AllocationSpace` is a enum found in `src/common/globals.h`:
```c++
enum AllocationSpace {
  RO_SPACE,      // Immortal, immovable and immutable objects,
  NEW_SPACE,     // Young generation semispaces for regular objects collected with
                 // Scavenger.
  OLD_SPACE,     // Old generation regular object space.
  CODE_SPACE,    // Old generation code object space, marked executable.
  MAP_SPACE,     // Old generation map object space, non-movable.
  LO_SPACE,      // Old generation large object space.
  CODE_LO_SPACE, // Old generation large code object space.
  NEW_LO_SPACE,  // Young generation large object space.

  FIRST_SPACE = RO_SPACE,
  LAST_SPACE = NEW_LO_SPACE,
  FIRST_MUTABLE_SPACE = NEW_SPACE,
  LAST_MUTABLE_SPACE = NEW_LO_SPACE,
  FIRST_GROWABLE_PAGED_SPACE = OLD_SPACE,
  LAST_GROWABLE_PAGED_SPACE = MAP_SPACE
};
```
There is an abstract superclass that extends BaseSpace named `Space`.

### Space
Is a concrete implementation of `BaseSpace` and the superclass of all other
spaces.
What are the spaces that are defined?  
* NewSpace
* PagedSpace
* SemiSpace
* LargeObjectSpace

A space has a list of MemoryChunks that belong to the space.

### BasicMemoryChunk
A BasicMemoryChunk has a size, a start address, and an end address. It also has
flags that describe the memory chunk, like if it is executable, pinned (cannot be
moved etc). 

A BasicMemoryChunk is created by specifying the size and the start and end
address:
```c++
BasicMemoryChunk::BasicMemoryChunk(size_t size, Address area_start,
                                   Address area_end) {
  size_ = size;
  area_start_ = area_start;
  area_end_ = area_end;
}
```
So there is nothing else to creating an instance of a BasicMemoryChunk.

But note that BasicMemoryChunk also has a Heap member which was not specified
in the above constructor.
There is a static function named `Initialize` that does take a pointer to a
Heap though:
```c++
 static BasicMemoryChunk* Initialize(Heap* heap, Address base, size_t size,
                                      Address area_start, Address area_end,
                                      BaseSpace* owner,
                                      VirtualMemory reservation);
```


### MemoryChunk
Represents a memory area that is owned by a Space.

Is a struct defined in `src/heap/memory-chunk.h` (there is another MemoryChunk
struct in the namespace `v8::heap_internal` just in case you find it instead).
```c++
class MemoryChunk : public BasicMemoryChunk {

};
```

### Page
Is a class that extends MemoryChunk and has a size of 256K.

The `Page` class can be found in `src/heap/spaces.h`
```c++
class Page : public MemoryChunk {
}
```

### Heap



Lets take a look when the heap is constructed by using `test/heap_test` and
setting a break point in Heap's constructor:
```console
$ make test/heap_test
(lldb) br s -f v8_test_fixture.h -l 30
(lldb) r
```
So when is a Heap created?
`Heap heap_;` is a private member of Isolate so it's constructor will be called 
when a new Isolate is created:
```console
(lldb) bt
* thread #1: tid = 0xc5c373, 0x0000000100c70a65 libv8.dylib`v8::internal::Heap::Heap(this=0x0000000104001c20) + 21 at heap.cc:247, queue = 'com.apple.main-thread', stop reason = step over
  * frame #0: 0x0000000100c70a65 libv8.dylib`v8::internal::Heap::Heap(this=0x0000000104001c20) + 21 at heap.cc:247
    frame #1: 0x0000000100ecb542 libv8.dylib`v8::internal::Isolate::Isolate(this=0x0000000104001c00) + 82 at isolate.cc:2482
    frame #2: 0x0000000100ecc875 libv8.dylib`v8::internal::Isolate::Isolate(this=0x0000000104001c00) + 21 at isolate.cc:2544
    frame #3: 0x000000010011f110 libv8.dylib`v8::Isolate::Allocate() + 32 at api.cc:8204
    frame #4: 0x000000010011f6e1 libv8.dylib`v8::Isolate::New(params=0x0000000100076768) + 17 at api.cc:8275
    frame #5: 0x0000000100066233 heap_test`V8TestFixture::SetUp(this=0x0000000104901650) + 35 at v8_test_fixture.h:30
```
The constructor for Heap can be found in `src/heap/heap.cc`. Lets take a look at the fields of
a Heap instance (can be found in src/heap/heap.h);
```c++
Object* roots_[kRootListLength];
```
kRootListLength is an entry in the RootListIndex enum:
```console
(lldb) expr ::RootListIndex::kRootListLength
(int) $8 = 529
```
Notice that the types in this array is `Object*`. The `RootListIndex` enum is populated using a macro.
You can call the `root` function on a heap instace to get the object at that index:
```c++
Object* root(RootListIndex index) { 
  return roots_[index]; 
}
```

These are only the indexes into the array, the array itself has not been populated yet.
The array is created in Heap's constructor:
```c++
memset(roots_, 0, sizeof(roots_[0]) * kRootListLength);
```
And as we can see it is initially empty:
```console
(lldb) expr roots_
(v8::internal::Object *[529]) $10 = {
  [0] = 0x0000000000000000
  [1] = 0x0000000000000000
  [2] = 0x0000000000000000
  ...
  [529] = 0x0000000000000000
```
After returning from Heap's constructor we are back in Isolate's constructor.
An Isolate has a private member `ThreadLocalTop thread_local_top_;` which calls
`InitializeInternal` when constructed:
```c++
void ThreadLocalTop::InitializeInternal() {
  c_entry_fp_ = 0;
  c_function_ = 0;
  handler_ = 0;
#ifdef USE_SIMULATOR
  simulator_ = nullptr;
#endif
  js_entry_sp_ = kNullAddress;
  external_callback_scope_ = nullptr;
  current_vm_state_ = EXTERNAL;
  try_catch_handler_ = nullptr;
  context_ = nullptr;
  thread_id_ = ThreadId::Invalid();
  external_caught_exception_ = false;
  failed_access_check_callback_ = nullptr;
  save_context_ = nullptr;
  promise_on_stack_ = nullptr;

  // These members are re-initialized later after deserialization
  // is complete.
  pending_exception_ = nullptr;
  wasm_caught_exception_ = nullptr;
  rethrowing_message_ = false;
  pending_message_obj_ = nullptr;
  scheduled_exception_ = nullptr;
}
```
TODO: link these fields with entry code when v8 is about to call a builtin or javascript function.
Most of the rest of the initialisers set the members to null or equivalent. But lets take a look
at what the constructor does:
```c++
id_ = base::Relaxed_AtomicIncrement(&isolate_counter_, 1);
```
```console
(lldb) expr id_
(v8::base::Atomic32) $13 = 1
```
```c++
memset(isolate_addresses_, 0, sizeof(isolate_addresses_[0]) * (kIsolateAddressCount + 1));
```
What is `isolate_addresses_`?
```c++
Address isolate_addresses_[kIsolateAddressCount + 1];
```
`Address` can be found in `src/globals.h`:

```c++
typedef uintptr_t Address;
```

Also in `src/globals.h` we find:

```c++
#define FOR_EACH_ISOLATE_ADDRESS_NAME(C)                \
  C(Handler, handler)                                   \
  C(CEntryFP, c_entry_fp)                               \
  C(CFunction, c_function)                              \
  C(Context, context)                                   \
  C(PendingException, pending_exception)                \
  C(PendingHandlerContext, pending_handler_context)     \
  C(PendingHandlerCode, pending_handler_code)           \
  C(PendingHandlerOffset, pending_handler_offset)       \
  C(PendingHandlerFP, pending_handler_fp)               \
  C(PendingHandlerSP, pending_handler_sp)               \
  C(ExternalCaughtException, external_caught_exception) \
  C(JSEntrySP, js_entry_sp)

enum IsolateAddressId {
#define DECLARE_ENUM(CamelName, hacker_name) k##CamelName##Address,
  FOR_EACH_ISOLATE_ADDRESS_NAME(DECLARE_ENUM)
#undef DECLARE_ENUM
      kIsolateAddressCount
};
```
Will expand to:
```c++
enum IsolateAddressId {
  kHandlerAddress,
  kCEntryAddress,
  kCFunctionAddress,
  kContextAddress,
  kPendingExceptionAddress,
  kPendingHandlerAddress,
  kPendingHandlerCodeAddress,
  kPendingHandlerOffsetAddress,
  kPendingHandlerFPAddress,
  kPendingHandlerSPAddress,
  kExternalCaughtExceptionAddress,
  kJSEntrySPAddress,
  kIsolateAddressCount
};
```
Alright, so we know where the `kIsolateAddressCount` comes from and that memory is allocated
and set to zero for this in the Isolate constructor. Where then are these entries filled?
In isolate.cc when an Isolate is initialized by `bool Isolate::Init(StartupDeserializer* des)`:

```c++
#define ASSIGN_ELEMENT(CamelName, hacker_name)                  \
  isolate_addresses_[IsolateAddressId::k##CamelName##Address] = \
      reinterpret_cast<Address>(hacker_name##_address());
  FOR_EACH_ISOLATE_ADDRESS_NAME(ASSIGN_ELEMENT)
#undef ASSIGN_ELEMENT
```
```c++
  isolate_addressess_[IsolateAddressId::kHandlerAddress] = reinterpret_cast<Address>(handler_address());
  isolate_addressess_[IsolateAddressId::kCEntryAddress] = reinterpret_cast<Address>(c_entry_fp_address());
  isolate_addressess_[IsolateAddressId::kFunctionAddress] = reinterpret_cast<Address>(c_function_address());
  isolate_addressess_[IsolateAddressId::kContextAddress] = reinterpret_cast<Address>(context_address());
  isolate_addressess_[IsolateAddressId::kPendingExceptionAddress] = reinterpret_cast<Address>(pending_exception_address());
  isolate_addressess_[IsolateAddressId::kPendingHandlerContextAddress] = reinterpret_cast<Address>(pending_handler_context_address());
  isolate_addressess_[IsolateAddressId::kPendingHandlerCodeAddress] = reinterpret_cast<Address>(pending_handler_code_address());
  isolate_addressess_[IsolateAddressId::kPendingHandlerOffsetAddress] = reinterpret_cast<Address>(pending_handler_offset_address());
  isolate_addressess_[IsolateAddressId::kPendingHandlerFPAddress] = reinterpret_cast<Address>(pending_handler_fp_address());
  isolate_addressess_[IsolateAddressId::kPendingHandlerSPAddress] = reinterpret_cast<Address>(pending_handler_sp_address());
  isolate_addressess_[IsolateAddressId::kExternalCaughtExceptionAddress] = reinterpret_cast<Address>(external_caught_exception_address());
  isolate_addressess_[IsolateAddressId::kJSEntrySPAddress] = reinterpret_cast<Address>(js_entry_sp);
```

So where does `handler_address()` and the rest of those functions come from?
This is defined in `isolate.h`:
```c++
inline Address* handler_address() { return &thread_local_top_.handler_; }
inline Address* c_entry_fp_address() { return &thread_local_top_.c_entry_fp_; }
inline Address* c_function_address() { return &thread_local_top_.c_function_; }
Context** context_address() { return &thread_local_top_.context_; }
inline Address* js_entry_sp_address() { return &thread_local_top_.js_entry_sp_; }
Address pending_message_obj_address() { return reinterpret_cast<Address>(&thread_local_top_.pending_message_obj_); }

THREAD_LOCAL_TOP_ADDRESS(Context*, pending_handler_context)
THREAD_LOCAL_TOP_ADDRESS(Code*, pending_handler_code)
THREAD_LOCAL_TOP_ADDRESS(intptr_t, pending_handler_offset)
THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_fp)
THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_sp)


#define THREAD_LOCAL_TOP_ADDRESS(type, name) \
  type* name##_address() { return &thread_local_top_.name##_; }

Context* pending_handler_context_address() { return &thread_local_top_.pending_handler_context_;
```

When is `Isolate::Init` called?
```console
// v8_test_fixture.h
virtual void SetUp() {
  isolate_ = v8::Isolate::New(create_params_);
}

Isolate* Isolate::New(const Isolate::CreateParams& params) {
  Isolate* isolate = Allocate();
  Initialize(isolate, params);
  return isolate;
}
```
Lets take a closer look at `Initialize`. First the array buffer allocator is set on the 
internal isolate which is just a setter and not that interesting.

Next the snapshot blob is set (just showing the path we are taking):
```c++
i_isolate->set_snapshot_blob(i::Snapshot::DefaultSnapshotBlob());
```
Now, `i_isolate->set_snapshot_blob` is generated by a macro. The macro can be found in
`src/isolate.h`:

```c++
  // Accessors.
#define GLOBAL_ACCESSOR(type, name, initialvalue)                       \
  inline type name() const {                                            \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_);        \
    return name##_;                                                     \
  }                                                                     \
  inline void set_##name(type value) {                                  \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_);        \
    name##_ = value;                                                    \
  }
  ISOLATE_INIT_LIST(GLOBAL_ACCESSOR)
#undef GLOBAL_ACCESSOR
```
In this case the entry in ISOLATE_INIT_LIST is:
```c++
V(const v8::StartupData*, snapshot_blob, nullptr)
```
So this will generate two functions:
```c++
  inline type snapshot_blob() const {
    DCHECK(OFFSET_OF(Isolate, snapshot_blob_) == snapshot_blob_debug_offset_);
    return snapshot_blob_;
  }

  inline void set_snapshot_blob(const v8::StartupData* value) {
    DCHECK(OFFSET_OF(Isolate, snapshot_blob_) == snapshot_blob_debug_offset_);
    snapshot_blob_ = value;
  }
```
Next we have:
```c++
if (params.entry_hook || !i::Snapshot::Initialize(i_isolate)) {
...
```
```c++
bool Snapshot::Initialize(Isolate* isolate) {
  if (!isolate->snapshot_available()) return false;
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  CheckVersion(blob);
  Vector<const byte> startup_data = ExtractStartupData(blob);
  SnapshotData startup_snapshot_data(startup_data);
  Vector<const byte> builtin_data = ExtractBuiltinData(blob);
  BuiltinSnapshotData builtin_snapshot_data(builtin_data);
  StartupDeserializer deserializer(&startup_snapshot_data,
                                   &builtin_snapshot_data);
  deserializer.SetRehashability(ExtractRehashability(blob));
  bool success = isolate->Init(&deserializer);
  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int bytes = startup_data.length();
    PrintF("[Deserializing isolate (%d bytes) took %0.3f ms]\n", bytes, ms);
  }
  return success;
}
```
Lets take a closer look at `bool success = isolate->Init(&deserializer);`:
```c++
#define ASSIGN_ELEMENT(CamelName, hacker_name)                  \
  isolate_addresses_[IsolateAddressId::k##CamelName##Address] = \
      reinterpret_cast<>(hacker_name##_address());
  FOR_EACH_ISOLATE_ADDRESS_NAME(ASSIGN_ELEMENT)
#undef ASSIGN_ELEMENT
```
Like mentioned before we have the isolate_address_ array that gets populated with 
pointers.
```c++
global_handles_ = new GlobalHandles(this);
```
```c++
GlobalHandles::GlobalHandles(Isolate* isolate)
    : isolate_(isolate),
      number_of_global_handles_(0),
      first_block_(nullptr),
      first_used_block_(nullptr),
      first_free_(nullptr),
      post_gc_processing_count_(0),
      number_of_phantom_handle_resets_(0) {}
```
TODO: take a closer look at the GlobalHandles class.
```c++
  global_handles_ = new GlobalHandles(this);
  eternal_handles_ = new EternalHandles();
  bootstrapper_ = new Bootstrapper(this);
  handle_scope_implementer_ = new HandleScopeImplementer(this);
  load_stub_cache_ = new StubCache(this);
  store_stub_cache_ = new StubCache(this);
...
  call_descriptor_data_ =
      new CallInterfaceDescriptorData[CallDescriptors::NUMBER_OF_DESCRIPTORS];
...
  if (!heap_.SetUp()) {
    V8::FatalProcessOutOfMemory(this, "heap setup");
    return false;
  }
```
Now, lets look at `head_.SetUp()`:
```c++
  if (!configured_) {
    if (!ConfigureHeapDefault()) return false;
  }
```
```c++
bool Heap::ConfigureHeapDefault() { return ConfigureHeap(0, 0, 0); }
```
Looking at the code this sets up the generation in the heap.

```c++
// Initialize the interface descriptors ahead of time.
#define INTERFACE_DESCRIPTOR(Name, ...) \
  { Name##Descriptor(this); }
  INTERFACE_DESCRIPTOR_LIST(INTERFACE_DESCRIPTOR)
#undef INTERFACE_DESCRIPTOR
```
`INTERFACE_DESCRIPTOR_LIST` can be found in `src/interface-descriptors.h`
```c++
#define INTERFACE_DESCRIPTOR_LIST(V)  \
  V(Void)                             \
  V(ContextOnly)                      \
  V(Load)                             \
  ...
```
So the first entry `Void` will expand to:
```c++
  { VoidDescriptor(this); }
```
```c++
class V8_EXPORT_PRIVATE VoidDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(VoidDescriptor, CallInterfaceDescriptor)
};
```

```c++
InitializeThreadLocal();
bootstrapper_->Initialize(create_heap_objects);
setup_delegate_->SetupBuiltins(this);


isolate->heap()->IterateSmiRoots(this);

#define SMI_ROOT_LIST(V)                                                       \
  V(Smi, stack_limit, StackLimit)                                              \
  V(Smi, real_stack_limit, RealStackLimit)                                     \
  V(Smi, last_script_id, LastScriptId)                                         \
  V(Smi, last_debugging_id, LastDebuggingId)                                   \
  V(Smi, hash_seed, HashSeed)                                                  \
  V(Smi, next_template_serial_number, NextTemplateSerialNumber)                \
  V(Smi, arguments_adaptor_deopt_pc_offset, ArgumentsAdaptorDeoptPCOffset)     \
  V(Smi, construct_stub_create_deopt_pc_offset,                                \
    ConstructStubCreateDeoptPCOffset)                                          \
  V(Smi, construct_stub_invoke_deopt_pc_offset,                                \
    ConstructStubInvokeDeoptPCOffset)                                          \
  V(Smi, interpreter_entry_return_pc_offset, InterpreterEntryReturnPCOffset)
```

The Isolate class extends HiddenFactory (src/execution/isolate.h):
```c++
// HiddenFactory exists so Isolate can privately inherit from it without making
// Factory's members available to Isolate directly.
class V8_EXPORT_PRIVATE HiddenFactory : private Factory {};

class Isolate : private HiddenFactory {
  ...
}
```
So `isolate->factory()` looks like this:
```c++
v8::internal::Factory* factory() {
  // Upcast to the privately inherited base-class using c-style casts to avoid
  // undefined behavior (as static_cast cannot cast across private bases).
  return (v8::internal::Factory*)this;  // NOLINT(readability/casting)
}
```
The factory class can be found in `src/heap/factory`.

Next, lets take a closer look at NewMap:
```c++
i::Handle<i::Map> map = factory->NewMap(i::JS_OBJECT_TYPE, 24);
```
What we are doing is creating a new Map, or hidden class. Every HeapObject has a Map
which can be set/get. The header for map can be found in `src/objects/map.h`.
The map contains information about its size and how to iterate over it which is required
for GC.

```c++
  HeapObject* result = isolate()->heap()->AllocateRawWithRetryOrFail(Map::kSize, MAP_SPACE);

  allocation = map_space_->AllocateRawUnaligned(size_in_bytes);

```
In `src/heap/spaces-inl.h` we find `AllocateLinearly`:
```c++
  HeapObject* object = AllocateLinearly(size_in_bytes);
```
`AllocateLinearly` in `spaces-inl.h`:
```c++
  Address current_top = allocation_info_.top();

```
```console
(lldb) expr allocation_info_
(v8::internal::LinearAllocationArea) $13 = (top_ = 16313499525632, limit_ = 16313500041216)
```
`LinearAllocationArea` can also be found in src/heap/spaces.h and is pretty simple with only
two member fields:
```c++
Address top_;
Address limit_;
```
Back to `AllocateLinearly`:
```c++
HeapObject* PagedSpace::AllocateLinearly(int size_in_bytes) {
  Address current_top = allocation_info_.top();
  Address new_top = current_top + size_in_bytes;
  DCHECK_LE(new_top, allocation_info_.limit());
  allocation_info_.set_top(new_top);
  return HeapObject::FromAddress(current_top);
}
```
We can see that this function takes the current allocation top and adds the
size to that to get a new allaction top which is then set. A pointer to a heap
object is returned which points to the old top which makes sense, as it has to point
to the beginning of the instance in memory. 

```c++
  // Converts an address to a HeapObject pointer.
  static inline HeapObject* FromAddress(Address address) {
    DCHECK_TAG_ALIGNED(address);
    return reinterpret_cast<HeapObject*>(address + kHeapObjectTag);
  }
```
`kHeapObjectTag` can be found in `include/v8.h`:
```c++
const int kHeapObjectTag = 1;
const int kWeakHeapObjectTag = 3;
const int kHeapObjectTagSize = 2;
const intptr_t kHeapObjectTagMask = (1 << kHeapObjectTagSize) - 1;
```
```console
(lldb) expr ::kHeapObjectTag
(const int) $17 = 1
```
So in:
```c++
return reinterpret_cast<HeapObject*>(address + kHeapObjectTag);
```
`address` is the current_top and we are adding 1 to it before casting this memory location to be used as the pointer to a HeapObject.
To recap, at this stage we are creating a Map instance which is a type of HeapObject:
```c++
class Map : public HeapObject {
  ...
}
```
But we have not created an instance yet, instead only reserved memory by moving the current top allowing room for the new Map.
```console
(lldb) expr address
(v8::internal::Address) $18 = 16313499525632
(lldb) expr address + ::kHeapObjectTag
(unsigned long) $19 = 16313499525633
```

So back in `Factory::NewMap` we have:
```c++
  HeapObject* result = isolate()->heap()->AllocateRawWithRetryOrFail(Map::kSize, MAP_SPACE);
  result->set_map_after_allocation(*meta_map(), SKIP_WRITE_BARRIER);
```

TODO: split `InitializeMap` so it is a little more readable here.
```c++
  return handle(InitializeMap(Map::cast(result), type, instance_size,
                              elements_kind, inobject_properties),
```

### Spaces
From `src/heap/spaces.h`



A heap instance has the following members:
```c++
  NewSpace* new_space_ = nullptr;
  OldSpace* old_space_ = nullptr;
  CodeSpace* code_space_ = nullptr;
  MapSpace* map_space_ = nullptr;
  OldLargeObjectSpace* lo_space_ = nullptr;
  CodeLargeObjectSpace* code_lo_space_ = nullptr;
  NewLargeObjectSpace* new_lo_space_ = nullptr;
  ReadOnlySpace* read_only_space_ = nullptr;
```


### v8::internal::HeapObject
Is just a pointer type with functions but no members.
I've yet to find any types that have members. How are things stored?
For example, the Map* in HeapObject must be stored somewhere right?
```c++
inline Map* map() const;
inline void set_map(Map* value);
```
Lets take a look at the definition of `map` in objects-inl.h:
```c++
Map* HeapObject::map() const {
  return map_word().ToMap();
}

MapWord HeapObject::map_word() const {
  return MapWord(
      reinterpret_cast<uintptr_t>(RELAXED_READ_FIELD(this, kMapOffset)));
}
```

`RELAXED_READ_FIELD` will expand to:
```c++
  reinterpret_cast<Object*>(base::Relaxed_Load(
      reinterpret_cast<const base::AtomicWord*>(
          (reinterpret_cast<const byte*>(this) + offset - kHeapObjectTag)))

```

### AllocateRaw
This is a method declared in `src/heap/heap.h` and will allocate an uninitialized
object:
```c++
 V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime,
      AllocationAlignment alignment = kWordAligned);
```
So we have the size of the object in bytes. And the other types are described
in separate sections below.

The implemementation for this method can be found in `src/heap/heap-inl.h`.
```c++
  ...
  HeapObject object;
  AllocationResult allocation;
```
This is then followed by checks or the AllocationType 
```c++
  if (AllocationType::kYoung == type) {
    ...
    } else if (AllocationType::kCode == type) {
      if (size_in_bytes <= code_space()->AreaSize() && !large_object) {
        allocation = code_space_->AllocateRawUnaligned(size_in_bytes);
      } else {
        allocation = code_lo_space_->AllocateRaw(size_in_bytes);
      }

  }
```
Lets take a closer look at `code_lo_space` which is a field in the Heap class.
```c++
 private:
  CodeLargeObjectSpace* code_lo_space_ = nullptr;
```
And it's `AllocateRaw` function looks like this:
```c++
AllocationResult CodeLargeObjectSpace::AllocateRaw(int object_size) {
  return OldLargeObjectSpace::AllocateRaw(object_size, EXECUTABLE);
}
```
OldLargeObjectSpace's AllocateRaw can be found in `src/heap/large-spaces.cc`.

There is an example of AllocateRaw in [heap_test.cc](./test/heap_test.cc).


### AllocationType
```c++
enum class AllocationType : uint8_t {
  kYoung,    // Regular object allocated in NEW_SPACE or NEW_LO_SPACE
  kOld,      // Regular object allocated in OLD_SPACE or LO_SPACE
  kCode,     // Code object allocated in CODE_SPACE or CODE_LO_SPACE
  kMap,      // Map object allocated in MAP_SPACE
  kReadOnly  // Object allocated in RO_SPACE
};

```

### AllocationOrigin
```c++
enum class AllocationOrigin {
  kGeneratedCode = 0,
  kRuntime = 1,
  kGC = 2,
  kFirstAllocationOrigin = kGeneratedCode,
  kLastAllocationOrigin = kGC,
  kNumberOfAllocationOrigins = kLastAllocationOrigin + 1
};
```

### AllocationAlignement
```c++
enum AllocationAlignment {
  kWordAligned,
  kDoubleAligned,
  kDoubleUnaligned,
  kCodeAligned
};
```
