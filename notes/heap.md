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
|  +-------------+--------------+                                                   |
|  +-------------+                 +-----------+               +------------------+ |
|  | NEW_LO_SPACE|                 | CODE_SPACE|               | CODE_LO_SPACE    | |
|  +-------------+                 +-----------+               +------------------+ |
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

void* AllocWithRetry(size_t size) {
  void* result = nullptr;
  for (int i = 0; i < kAllocationTries; ++i) {
    result = malloc(size);
    if (result != nullptr) break;
    if (!OnCriticalMemoryPressure(size)) break;
  }
  return result;
}

void Malloced::operator delete(void* p) { free(p); }
```
So for all classes of type BaseSpace when those are created using the `new`
operator, the `Malloced::operator new` version will be called. This is like
other types of operator overloading in c++. Here is a standalone example
[new-override.cc](https://github.com/danbev/learning-cpp/blob/master/src/fundamentals/new-override.cc). 


The spaces are create by `Heap::SetupSpaces`, which is called from
`Isolate::Init`:
```c++
void Heap::SetUpSpaces() {
  space_[NEW_SPACE] = new_space_ =
      new NewSpace(this, memory_allocator_->data_page_allocator(),
                   initial_semispace_size_, max_semi_space_size_);
  space_[OLD_SPACE] = old_space_ = new OldSpace(this);
  space_[CODE_SPACE] = code_space_ = new CodeSpace(this);
  space_[MAP_SPACE] = map_space_ = new MapSpace(this);
  space_[LO_SPACE] = lo_space_ = new OldLargeObjectSpace(this);
  space_[NEW_LO_SPACE] = new_lo_space_ =
      new NewLargeObjectSpace(this, new_space_->Capacity());
  space_[CODE_LO_SPACE] = code_lo_space_ = new CodeLargeObjectSpace(this);
  ...
```
Notice that this use the `new` operator and that they all extend Malloced so
these instance will be allocated by Malloced.

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

### NewSpace
A NewSpace contains two SemiSpaces, one name `to_space_` and the other
`from_space`.


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
### Heap::SetUp
```c++
  memory_allocator_.reset(                                                          
      new MemoryAllocator(isolate_, MaxReserved(), code_range_size_));
```

### MemoryAllocator
A memory allocator is what requires chunks of memory from the OS.
Is defined in `src/heap/memory-allocator.h` and is created in Heap::SetUpg

I want to know/understand where memory is allocated.

When a MemoryAllocator is created it is passes an internal isolate, a capacity,
and a code_range_size.
Each instance has a PageAllocator for non-executable pages which is called
data_page_allocator, and another that is for executable pages called
code_page_allocator.

The constructor can be found in `src/heap/memory-allocator.cc`:
```c++
MemoryAllocator::MemoryAllocator(Isolate* isolate, size_t capacity,
                                 size_t code_range_size)
    : isolate_(isolate),
      data_page_allocator_(isolate->page_allocator()),
      code_page_allocator_(nullptr),
      capacity_(RoundUp(capacity, Page::kPageSize)),
      size_(0),
      size_executable_(0),
      lowest_ever_allocated_(static_cast<Address>(-1ll)),
      highest_ever_allocated_(kNullAddress),
      unmapper_(isolate->heap(), this) {
  InitializeCodePageAllocator(data_page_allocator_, code_range_size);
}
```
We can see that the data_page_allocator_ is set to the isolates page_allocator.

Notice that the above constructor calls `InitializeCodePageAllocator` which
will set the `code_page_allocator`:
```c++
void MemoryAllocator::InitializeCodePageAllocator(
    v8::PageAllocator* page_allocator, size_t requested) {

  code_page_allocator_ = page_allocator;
  ...
  code_page_allocator_instance_ = std::make_unique<base::BoundedPageAllocator>(
      page_allocator, aligned_base, size,
      static_cast<size_t>(MemoryChunk::kAlignment));
  code_page_allocator_ = code_page_allocator_instance_.get();
```

### PageAlloctor
Is responsible for allocating pages from the underlying OS. This class is a
abstract class (has virtual members that subclasses must implement).
Can be found in `src/base/page-alloctor.h` and notice that it is in the
`v8::base` namespace and it extends `v8::PageAllocator` which can be found
in `include/v8-platform.h`.


BoundedPageAllocator is 


When we create a new Isolate (using Isolate::New) the default allocation mode
will be `IsolateAllocationMode::kInV8Heap`:
```c++
  static Isolate* New(
      IsolateAllocationMode mode = IsolateAllocationMode::kDefault);
```
```c++
  // Allocate Isolate in C++ heap using default new/delete operators.
  kInCppHeap,

  // Allocate Isolate in a committed region inside V8 heap reservation.
  kInV8Heap,

#ifdef V8_COMPRESS_POINTERS
  kDefault = kInV8Heap,
#else
  kDefault = kInCppHeap,
#endif
};
```
The first thing that happens in `New` is that an instance of IsolateAllocator
is created.
```c++
// IsolateAllocator allocates the memory for the Isolate object according to
// the given allocation mode.
  std::unique_ptr<IsolateAllocator> isolate_allocator = std::make_unique<IsolateAllocator>(mode);
```
The constructor takes an AllocationMode as mentioned earlier.
```c++
IsolateAllocator::IsolateAllocator(IsolateAllocationMode mode) {
#if V8_TARGET_ARCH_64_BIT
  if (mode == IsolateAllocationMode::kInV8Heap) {
    Address heap_reservation_address = InitReservation();
    CommitPagesForIsolate(heap_reservation_address);
    return;
  }
#endif  // V8_TARGET_ARCH_64_BIT
```
Lets take a closer look at InitReservation (`src/init/isolate-alloctor.cc`).
```c++
Address IsolateAllocator::InitReservation() {
  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();
  ...
   VirtualMemory padded_reservation(platform_page_allocator,
                                    reservation_size * 2,
                                    reinterpret_cast<void*>(hint));
```
Notice that a VirtualMemory instance is created and the arguments passed. This
is actually part of a retry loop but I've excluded that above.
VirtualMemory's constructor can be found in `src/utils/allocation.cc`:
```c++
VirtualMemory::VirtualMemory(v8::PageAllocator* page_allocator, size_t size,
                             void* hint, size_t alignment)
    : page_allocator_(page_allocator) {
  size_t page_size = page_allocator_->AllocatePageSize();
  alignment = RoundUp(alignment, page_size);
  Address address = reinterpret_cast<Address>(
      AllocatePages(page_allocator_, hint, RoundUp(size, page_size), alignment,
                    PageAllocator::kNoAccess));
  ...
}
```
And `AllocatePages` can be found in `src/base/page-allocator.cc`:
```c++
void* PageAllocator::AllocatePages(void* hint, size_t size, size_t alignment,
                                   PageAllocator::Permission access) {
  return base::OS::Allocate(hint, size, alignment,
                            static_cast<base::OS::MemoryPermission>(access));
}
```
Which will lead us to `base::OS::Allocate` which can be found in platform-posic.cc
which can be found in `src/base/platform/platform-posix.cc`:
```c++
void* Allocate(void* hint, size_t size, OS::MemoryPermission access, PageType page_type) {
  int prot = GetProtectionFromMemoryPermission(access);
  int flags = GetFlagsForMemoryPermission(access, page_type);
  void* result = mmap(hint, size, prot, flags, kMmapFd, kMmapFdOffset);
  if (result == MAP_FAILED) return nullptr;
  return result;
}
```
Notice the call to `mmap` which is a system call. So this is as low as we get.
I've written a small example of using
[mmap](https://github.com/danbev/learning-linux-kernel/blob/master/mmap.c) and
[notes](https://github.com/danbev/learning-linux-kernel#mmap-sysmmanh) which
migth help to look at mmap in isolation and better understand what `Allocate`
is doing.

`prot` is the memory protection wanted for this mapping and is retrived by
calling `GetProtectionFromMemoryPermission`:
```c++
int GetProtectionFromMemoryPermission(OS::MemoryPermission access) {
  switch (access) {
    case OS::MemoryPermission::kNoAccess:
      return PROT_NONE;
    case OS::MemoryPermission::kRead:
      return PROT_READ;
    case OS::MemoryPermission::kReadWrite:
      return PROT_READ | PROT_WRITE;
    case OS::MemoryPermission::kReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    case OS::MemoryPermission::kReadExecute:
      return PROT_READ | PROT_EXEC;
  }
  UNREACHABLE();
}
```
In our case this will return `PROT_NONE`.
Next we get the flags to be used in the mmap call:
```c++
enum class PageType { kShared, kPrivate };

int GetFlagsForMemoryPermission(OS::MemoryPermission access,
                                PageType page_type) {
  int flags = MAP_ANONYMOUS;
  flags |= (page_type == PageType::kShared) ? MAP_SHARED : MAP_PRIVATE;
  ...
  return flags;
```
PageType in our case is `kPrivate` so this will add the `MAP_PRIVATE` flag to
the flags, in addition to `MAP_ANONYMOUS`.

So the following values are used in the call to mmap:
```c++
  void* result = mmap(hint, size, prot, flags, kMmapFd, kMmapFdOffset);
```
```console
(lldb) expr hint
(void *) $20 = 0x00001b9100000000
(lldb) expr request_size/1073741824
(unsigned long) $24 = 8
(lldb) expr kMmapFd
(const int) $17 = -1
```
`kMmapFdOffset` seems to be optimized out but we can see that it is `0x0` and
passed in register `r9d` (as it is passed as the sixth argument):
```console
(lldb) disassemble
->  0x7ffff39ba4ca <+53>:  mov    ecx, dword ptr [rbp - 0xc]
    0x7ffff39ba4cd <+56>:  mov    edx, dword ptr [rbp - 0x10]
    0x7ffff39ba4d0 <+59>:  mov    rsi, qword ptr [rbp - 0x20]
    0x7ffff39ba4d4 <+63>:  mov    rax, qword ptr [rbp - 0x18]
    0x7ffff39ba4d8 <+67>:  mov    r9d, 0x0
    0x7ffff39ba4de <+73>:  mov    r8d, 0xffffffff
    0x7ffff39ba4e4 <+79>:  mov    rdi, rax
    0x7ffff39ba4e7 <+82>:  call   0x7ffff399f000            ; symbol stub for: mmap64
```
So if I've understood this correctly this is requesting a mapping of 8GB, with
a memory protection of `PROT_NONE` which can be used to protect that mapping space
and also allow it to be subdivied by calling mmap later with specific addresses
in this mapping. These could then be called with other memory protections depending
on the type of contents that should be stored there, for example for code objects
it would be executable.

So that will return the address to the new mapping, this will return us to
```c++
void* OS::Allocate(void* hint, size_t size, size_t alignment,
                   MemoryPermission access) {
  ...
  void* result = base::Allocate(hint, request_size, access, PageType::kPrivate);
  if (result == nullptr) return nullptr;

  uint8_t* base = static_cast<uint8_t*>(result);
  uint8_t* aligned_base = reinterpret_cast<uint8_t*>(
      RoundUp(reinterpret_cast<uintptr_t>(base), alignment));
  if (aligned_base != base) {
    DCHECK_LT(base, aligned_base);
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    CHECK(Free(base, prefix_size));
    request_size -= prefix_size;
  }
  // Unmap memory allocated after the potentially unaligned end.
  if (size != request_size) {
    DCHECK_LT(size, request_size);
    size_t suffix_size = request_size - size;
    CHECK(Free(aligned_base + size, suffix_size));
    request_size -= suffix_size;
  }

  DCHECK_EQ(size, request_size);
  return static_cast<void*>(aligned_base);
}
```
TODO: figure out what why this unmapping is done.
This return will return to `AllocatePages` which will just return to
VirtualMemory's constructor where the address will be casted to an Address.
```c++
 Address address = reinterpret_cast<Address>(
      AllocatePages(page_allocator_, hint, RoundUp(size, page_size), alignment,
                    PageAllocator::kNoAccess));
  if (address != kNullAddress) {
    DCHECK(IsAligned(address, alignment));
    region_ = base::AddressRegion(address, size);
  }
}
```
`AddressRegion` can be found in `src/base/address-region.h` and is a helper
class to manage region with a start `address` and a size.
After this call the VirtualMemory instance will look like this:
```console
(lldb) expr *this
(v8::internal::VirtualMemory) $35 = {
  page_allocator_ = 0x00000000004936b0
  region_ = (address_ = 30309584207872, size_ = 8589934592)
}
```

After `InitReservation` returns we will be in `IsolateAllocator::IsolateAllocator`:
```c++
  if (mode == IsolateAllocationMode::kInV8Heap) {
    Address heap_reservation_address = InitReservation();
    CommitPagesForIsolate(heap_reservation_address);
    return;
  }
```

In CommitPagesForIsolate we have :
```c++
void IsolateAllocator::CommitPagesForIsolate(Address heap_reservation_address) {
  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();

  Address isolate_root = heap_reservation_address + kIsolateRootBiasPageSize;


  size_t page_size = RoundUp(size_t{1} << kPageSizeBits,
                             platform_page_allocator->AllocatePageSize());

  page_allocator_instance_ = std::make_unique<base::BoundedPageAllocator>(
      platform_page_allocator, isolate_root, kPtrComprHeapReservationSize,
      page_size);
  page_allocator_ = page_allocator_instance_.get();
```
```
(lldb) expr isolate_root
(v8::internal::Address) $54 = 30309584207872
```
And notice that the VirtualMemory that we allocated above has the same memory
address. 
So we are creating a new instance of BoundedPageAllocator and setting this to
page_allocator_instance_ (smart pointer) and also setting the pointer
page_allocator_.

```c++
  Address isolate_address = isolate_root - Isolate::isolate_root_bias();
  Address isolate_end = isolate_address + sizeof(Isolate);
  {
    Address reserved_region_address = isolate_root;
    size_t reserved_region_size =
        RoundUp(isolate_end, page_size) - reserved_region_address;

    CHECK(page_allocator_instance_->AllocatePagesAt(
        reserved_region_address, reserved_region_size,
        PageAllocator::Permission::kNoAccess));
  }
```
`AllocatePagesAt` will call `BoundedPageAllocator::AllocatePagesAt`:
```c++
bool BoundedPageAllocator::AllocatePagesAt(Address address, size_t size,
                                           PageAllocator::Permission access) {
  if (!region_allocator_.AllocateRegionAt(address, size)) {
    return false;
  }
  CHECK(page_allocator_->SetPermissions(reinterpret_cast<void*>(address), size,
                                        access));
  return true;
}
```
`SetPermissions` can be found in `platform-posix.cc` and looks like this:
```c++
bool OS::SetPermissions(void* address, size_t size, MemoryPermission access) {
  ...
  int prot = GetProtectionFromMemoryPermission(access);
  int ret = mprotect(address, size, prot);
```
`mprotect` is a system call that can change a memory mappings protection.


Now, lets see what goes on when we create a new Object, say of type String.
```console
(lldb) bt
* thread #1, name = 'string_test', stop reason = step in
  * frame #0: 0x00007ffff633d55a libv8.so`v8::internal::FactoryBase<v8::internal::Factory>::AllocateRawWithImmortalMap(this=0x00001d1000000000, size=20, allocation=kYoung, map=Map @ 0x00007fffffffcab8, alignment=kWordAligned) at factory-base.cc:817:3
    frame #1: 0x00007ffff633c2f1 libv8.so`v8::internal::FactoryBase<v8::internal::Factory>::NewRawOneByteString(this=0x00001d1000000000, length=6, allocation=kYoung) at factory-base.cc:533:14
    frame #2: 0x00007ffff634771e libv8.so`v8::internal::Factory::NewStringFromOneByte(this=0x00001d1000000000, string=0x00007fffffffcbd0, allocation=kYoung) at factory.cc:607:3
    frame #3: 0x00007ffff5fc1c54 libv8.so`v8::(anonymous namespace)::NewString(factory=0x00001d1000000000, type=kNormal, string=(start_ = "bajja", length_ = 6)) at api.cc:6381:46
    frame #4: 0x00007ffff5fc2284 libv8.so`v8::String::NewFromOneByte(isolate=0x00001d1000000000, data="bajja", type=kNormal, length=6) at api.cc:6439:3
    frame #5: 0x0000000000413fa4 string_test`StringTest_create_Test::TestBody(this=0x000000000048dff0) at string_test.cc:18:8
```
If we look at the call `AllocateRawWithImmortalMap` we find:
```c++
  HeapObject result = AllocateRaw(size, allocation, alignment);
```
AllocateRaw will end up in FactoryBase<Impl>::AllocateRaw:
```c++
HeapObject FactoryBase<Impl>::AllocateRaw(int size, AllocationType allocation,
                                          AllocationAlignment alignment) {
  return impl()->AllocateRaw(size, allocation, alignment);
}
```
Which will end up in `Factory::AllocateRaw`:
```c++
HeapObject Factory::AllocateRaw(int size, AllocationType allocation,
                                AllocationAlignment alignment) {
  return isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(
      size, allocation, AllocationOrigin::kRuntime, alignment);
  
}
```

```c++
   case kRetryOrFail:
-> 297 	      return AllocateRawWithRetryOrFailSlowPath(size, allocation, origin,
   298 	                                                alignment);
```
```c++
 5165	  AllocationResult alloc;
   5166	  HeapObject result =
-> 5167	      AllocateRawWithLightRetrySlowPath(size, allocation, origin, alignment);
```

```c++
-> 5143	  HeapObject result;
   5144	  AllocationResult alloc = AllocateRaw(size, allocation, origin, alignment);
```

```c++
-> 210 	        allocation = new_space_->AllocateRaw(size_in_bytes, alignment, origin);
```
```c++
-> 95  	    result = AllocateFastUnaligned(size_in_bytes, origin);
```

```c++
AllocationResult NewSpace::AllocateFastUnaligned(int size_in_bytes,
                                                 AllocationOrigin origin) {
  Address top = allocation_info_.top();

  HeapObject obj = HeapObject::FromAddress(top);
  allocation_info_.set_top(top + size_in_bytes);
```
Where `allocation_info_` is of type `LinearAllocationArea`. Notice that a
HeapObject is created using the value of top. This is a pointer to the top
of the address of mapped memory that this space manages. And after this top
is increased by the size of the object. 

`AllocateRawWithImmortalMap`:
```c++
   818 	  HeapObject result = AllocateRaw(size, allocation, alignment);
-> 819 	  result.set_map_after_allocation(map, SKIP_WRITE_BARRIER);
   820 	  return result;
```



Now, every Space will have a region of mapped memory that it can manage. For
example the new space would have it and it would manage it with a pointer
to the top of allocated memory. To create an object it could return the top address
and increase the top with the size of the object in question. Functions that
set values on the object would have to write to the field specified by the
layout of the object. TODO: verify if that is actually what is happening.


