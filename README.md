### Learning Google V8
The sole purpose of this project is to aid me in leaning Google's V8 JavaScript engine.


## Contents
1. [Introduction](#introduction)
2. [Building V8](#building-v8)
3. [Contributing a change](#contributing-a-change)
4. [Debugging](#debugging)
5. [Inline caches](#inline-caches)
6. [Small Integers](#small-integers)
7. [Building chromium](#building-chromium)
8. [Compiler pipeline](#compiler-pipeline)

## Introduction
V8 is bascially consists of the memory management of the heap and the execution stack (very simplified but helps
make my point). Things like the callback queue, the event loop and other things like the WebAPIs (DOM, ajax, 
setTimeout etc) are found inside Chrome or in the case of Node the APIs are Node.js APIs:

    +------------------------------------------------------------------------------------------+
    | Google Chrome                                                                            |
    |                                                                                          |
    | +----------------------------------------+          +------------------------------+     |
    | | Google V8                              |          |            WebAPIs           |     |
    | | +-------------+ +---------------+      |          |                              |     |
    | | |    Heap     | |     Stack     |      |          |                              |     |
    | | |             | |               |      |          |                              |     |
    | | |             | |               |      |          |                              |     |
    | | |             | |               |      |          |                              |     |
    | | |             | |               |      |          |                              |     |
    | | |             | |               |      |          |                              |     |
    | | +-------------+ +---------------+      |          |                              |     |
    | |                                        |          |                              |     |
    | +----------------------------------------+          +------------------------------+     |
    |                                                                                          |
    |                                                                                          |
    | +---------------------+     +---------------------------------------+                    |
    | |     Event loop      |     |          Callback queue               |                    |
    | |                     |     |                                       |                    |
    | +---------------------+     +---------------------------------------+                    |
    |                                                                                          |
    |                                                                                          |
    +------------------------------------------------------------------------------------------+

The execution stack is a stack of frame pointers. For each function called that function will be pushed onto 
the stack. When that function returns it will be removed. If that function calls other functions
they will be pushed onto the stack. When they have all returned execution can proceed from the returned to 
point. If one of the functions performs an operation that takes time progress will not be made until it 
completes as the only way to complete is that the function returns and is popped off the stack. This is 
what happens when you have a single threaded programming language.

So that describes synchronous functions, what about asynchronous functions?  
Lets take for example that you call setTimeout, the setTimeout function will be
pushed onto the call stack and executed. This is where the callback queue comes into play and the event loop. The setTimeout function can add functions to the callback queue. This queue will be processed by the event loop when the call stack is empty.

TODO: Add mirco task queue

### Isolate
An Isolate is an independant copy of the V8 runtime which includes its own heap.
Two different Isolates can run in parallel and can be seen as entierly different
sandboxed instances of a V8 runtime.

### Context
To allow separate JavaScript applications to run in the same isolate a context must be specified for each one.
This is to avoid them interfering with each other, for example by changing the builtin objects provided.

#### Threads
V8 is single threaded (the execution of the functions of the stack) but there are supporting threads used
for garbage collection, profiling (IC, and perhaps other things) (I think).
Lets see what threads there are:

    $ lldb -- hello-world
    (lldb) br s -n main
    (lldb) r
    (lldb) thread list
    thread #1: tid = 0x2efca6, 0x0000000100001e16 hello-world`main(argc=1, argv=0x00007fff5fbfee98) + 38 at hello-world.cc:40, queue = 'com.apple.main-thread', stop reason = breakpoint 1.1

So at startup there is only one thread which is what we expected. Lets skip ahead to where we create the platform:

    Platform* platform = platform::CreateDefaultPlatform();
    ...
    DefaultPlatform* platform = new DefaultPlatform(idle_task_support, tracing_controller);
    platform->SetThreadPoolSize(thread_pool_size);

    (lldb) fr v thread_pool_size
    (int) thread_pool_size = 0

Next there is a check for 0 and the number of processors -1 is used as the size of the thread pool:

    (lldb) fr v thread_pool_size
    (int) thread_pool_size = 7

This is all that `SetThreadPoolSize` does. After this we have:

    platform->EnsureInitialized();

    for (int i = 0; i < thread_pool_size_; ++i)
      thread_pool_.push_back(new WorkerThread(&queue_));

`new WorkerThread` will create a new pthread (on my system which is MacOSX):

    result = pthread_create(&data_->thread_, &attr, ThreadEntry, this);

ThreadEntry can be found in src/base/platform/platform-posix.


### International Component for Unicode (ICU)
International Components for Unicode (ICU) deals with internationalization (i18n).
ICU provides support locale-sensitve string comparisons, date/time/number/currency formatting
etc. 

There is an optional API called ECMAScript 402 which V8 suppports and which is enabled by
default. [i18n-support](https://github.com/v8/v8/wiki/i18n-support) says that even if your application does 
not use ICU you still need to call InitializeICU :

    V8::InitializeICU();

### Snapshots
JavaScript specifies a lot of built-in functionality which every V8 context must provide.
For example, you can run Math.PI and that will work in a JavaScript console/repl. The global object
and all the built-in functionality must be setup and initialized into the V8 heap. This can be time
consuming and affect runtime performance if this has to be done every time. 


Now this is where the files `natives_blob.bin` and `snapshot_blob.bin` come into play. But what are these bin files?  
The blobs above are prepared snapshots that get directly deserialized into the heap to provide an initilized context.
If you take a look in `src/js` you'll find a number of javascript files. These files referenced in `src/v8.gyp` and are used
by the target `js2c`. This target calls `tools/js2c.py` which is a tool for converting
JavaScript source code into C-Style char arrays. This target will process all the library_files specified in the variables section.
For a GN build you'll find the configuration in BUILD.GN.
Just note that there is ongoing work to move the .js files to V8 builtins and builtins will be discuessed later in this document.

The output of this out/Debug/obj/gen/libraries.cc. So how is this file actually used?
The `js2c` target produces the libraries.cc file which is used by other targets, for example by `v8_snapshot` which produces a 
snapshot_blob.bin file.

    $ lldb hello_world
    (lldb) br s -n main
    (lldb) r

Step through to the following line:

    V8::InitializeExternalStartupData(argv[0]);

This call will land us in `src/api.cc`:

    void v8::V8::InitializeExternalStartupData(const char* directory_path) {
      i::InitializeExternalStartupData(directory_path);
    }

The implementation of `InitializeExternalStartupData` can be found in `src/startup-data-util.cc`:

    void InitializeExternalStartupData(const char* directory_path) {
    #ifdef V8_USE_EXTERNAL_STARTUP_DATA
      char* natives;
      char* snapshot;
      LoadFromFiles(
        base::RelativePath(&natives, directory_path, "natives_blob.bin"),
        base::RelativePath(&snapshot, directory_path, "snapshot_blob.bin"));
      free(natives);
      free(snapshot);
    #endif  // V8_USE_EXTERNAL_STARTUP_DATA
}

Lets take a closer look at `LoadFromFiles`, the implementation if also in `src/startup-data-util.cc`:

    void LoadFromFiles(const char* natives_blob, const char* snapshot_blob) {
      Load(natives_blob, &g_natives, v8::V8::SetNativesDataBlob);
      Load(snapshot_blob, &g_snapshot, v8::V8::SetSnapshotDataBlob);

      atexit(&FreeStartupData);
    }


    (lldb) p blob_file
    (const char *) $1 = 0x0000000104200000 "/Users/danielbevenius/work/nodejs/learning-v8/natives_blob.bin"

This file is then read and set by calling:

    void V8::SetNativesDataBlob(StartupData* natives_blob) {
      i::V8::SetNativesBlob(natives_blob);
    }


### Local

```c++
Local<String> script_name = ...;
```
So what is script_name. Well it is an object reference that is managed by the v8 GC.
The GC needs to be able to move things (pointers around) and also track if things should be GC'd. Local handles
as opposed to persistent handles are light weight and mostly used local operations. These handles are managed by
HandleScopes so you must have a handlescope on the stack and the local is only valid as long as the handlescope is
valid. This uses Resource Acquisition Is Initialization (RAII) so when the HandleScope instance goes out of scope
it will remove all the Local instances.

The `Local` class (in `include/v8.h`) only has one member which is of type pointer to the type `T`. So for the above example it would be:
```c++
  String* val_;
```
You can find the available operations for a Local in `include/v8.h`.

```shell
(lldb) p script_name.IsEmpty()
(bool) $12 = false
````

A Local<T> has overloaded a number of operators, for example ->:
```shell
(lldb) p script_name->Length()
(int) $14 = 7
````
Where Length is a method on the v8 String class.

The handle stack is not part of the C++ call stack, but the handle scopes are embedded in the C++ stack. Handle scopes can only
be stack-allocated, not allocated with new.

### Persistent
https://github.com/v8/v8/wiki/Embedder's-Guide:
Persistent handles provide a reference to a heap-allocated JavaScript Object, just like a local handle. There are two flavors, which differ in the lifetime management of the reference they handle. Use a persistent handle when you need to keep a reference to an object for more than one function call, or when handle lifetimes do not correspond to C++ scopes. Google Chrome, for example, uses persistent handles to refer to Document Object Model (DOM) nodes.

A persistent handle can be made weak, using PersistentBase::SetWeak, to trigger a callback from the garbage collector when the only references to an object are from weak persistent handles.


A UniquePersistent<SomeType> handle relies on C++ constructors and destructors to manage the lifetime of the underlying object.
A Persistent<SomeType> can be constructed with its constructor, but must be explicitly cleared with Persistent::Reset.

So how is a persistent object created?  
Let's write a test and find out (`test/persistent-object_text.cc`):
```console
$ make test/persistent-object_test
$ ./test/persistent-object_test --gtest_filter=PersistentTest.value
```
Now, to create an instance of Persistent we need a Local<T> instance or the Persistent instance will
just be empty.
```c++
Local<Object> o = Local<Object>::New(isolate_, Object::New(isolate_));
```
`Local<Object>::New` can be found in `src/api.cc`:
```c++
Local<v8::Object> v8::Object::New(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Object, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSObject> obj =
      i_isolate->factory()->NewJSObject(i_isolate->object_function());
  return Utils::ToLocal(obj);
}
```
The first thing that happens is that the public Isolate pointer is cast to an pointer to the internal
Isolate type.
`LOG_API` is a macro in the same sourcd file:
```c++
#define LOG_API(isolate, class_name, function_name)                           \
  i::RuntimeCallTimerScope _runtime_timer(                                    \
      isolate, i::RuntimeCallCounterId::kAPI_##class_name##_##function_name); \
  LOG(isolate, ApiEntryCall("v8::" #class_name "::" #function_name))
```
If our case the preprocessor would expand that to:
```c++
  i::RuntimeCallTimerScope _runtime_timer(
      isolate, i::RuntimeCallCounterId::kAPI_Object_New);
  LOG(isolate, ApiEntryCall("v8::Object::New))
```
`LOG` is a macro that can be found in `src/log.h`:
```c++
#define LOG(isolate, Call)                              \
  do {                                                  \
    v8::internal::Logger* logger = (isolate)->logger(); \
    if (logger->is_logging()) logger->Call;             \
  } while (false)
```
And this would expand to:
```c++
  v8::internal::Logger* logger = isolate->logger();
  if (logger->is_logging()) logger->ApiEntryCall("v8::Object::New");
```
So with the LOG_API macro expanded we have:
```c++
Local<v8::Object> v8::Object::New(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::RuntimeCallTimerScope _runtime_timer( isolate, i::RuntimeCallCounterId::kAPI_Object_New);
  v8::internal::Logger* logger = isolate->logger();
  if (logger->is_logging()) logger->ApiEntryCall("v8::Object::New");

  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSObject> obj =
      i_isolate->factory()->NewJSObject(i_isolate->object_function());
  return Utils::ToLocal(obj);
}
```
Next we have `ENTER_V8_NO_SCRIPT_NO_EXCEPTION`:
```c++
#define ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate)                    \
  i::VMState<v8::OTHER> __state__((isolate));                       \
  i::DisallowJavascriptExecutionDebugOnly __no_script__((isolate)); \
  i::DisallowExceptions __no_exceptions__((isolate))
```
So with the macros expanded we have:
```c++
Local<v8::Object> v8::Object::New(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::RuntimeCallTimerScope _runtime_timer( isolate, i::RuntimeCallCounterId::kAPI_Object_New);
  v8::internal::Logger* logger = isolate->logger();
  if (logger->is_logging()) logger->ApiEntryCall("v8::Object::New");

  i::VMState<v8::OTHER> __state__(i_isolate));
  i::DisallowJavascriptExecutionDebugOnly __no_script__(i_isolate);
  i::DisallowExceptions __no_exceptions__(i_isolate));

  i::Handle<i::JSObject> obj =
      i_isolate->factory()->NewJSObject(i_isolate->object_function());

  return Utils::ToLocal(obj);
}
```
TODO: Look closer at `VMState`.  

First, `i_isolate->object_function()` is called and the result passed to `NewJSObject`. `object_function` is generated by a macro named `NATIVE_CONTEXT_FIELDS`:
```c++
#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name)     \
  Handle<type> Isolate::name() {                             \
    return Handle<type>(raw_native_context()->name(), this); \
  }                                                          \
  bool Isolate::is_##name(type* value) {                     \
    return raw_native_context()->is_##name(value);           \
  }
NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSOR)
```
`NATIVE_CONTEXT_FIELDS` is a macro in `src/contexts` and it c
```c++
#define NATIVE_CONTEXT_FIELDS(V)                                               \
...                                                                            \
  V(OBJECT_FUNCTION_INDEX, JSFunction, object_function)                        \
```

```c++
  Handle<type> Isolate::object_function() {
    return Handle<JSFunction>(raw_native_context()->object_function(), this);
  }

  bool Isolate::is_object_function(JSFunction* value) {
    return raw_native_context()->is_object_function(value);
  }
```
I'm not clear on the different types of context, there is a native context, a "normal/public" context.
In `src/contexts-inl.h` we have the native_context function:
```c++
Context* Context::native_context() const {
  Object* result = get(NATIVE_CONTEXT_INDEX);
  DCHECK(IsBootstrappingOrNativeContext(this->GetIsolate(), result));
  return reinterpret_cast<Context*>(result);
}
```
`Context` extends `FixedArray` so the get function is the get function of FixedArray and `NATIVE_CONTEXT_INDEX` 
is the index into the array where the native context is stored.

Now, lets take a closer look at `NewJSObject`. If you search for NewJSObject in `src/heap/factory.cc`:
```c++
Handle<JSObject> Factory::NewJSObject(Handle<JSFunction> constructor, PretenureFlag pretenure) {
  JSFunction::EnsureHasInitialMap(constructor);
  Handle<Map> map(constructor->initial_map(), isolate());
  return NewJSObjectFromMap(map, pretenure);
}
```
`NewJSObjectFromMap` 
```c++
...
  HeapObject* obj = AllocateRawWithAllocationSite(map, pretenure, allocation_site);
```
So we have created a new map

### Map
So an HeapObject contains a pointer to a Map, or rather has a function that returns a pointer to Map. I can't see
any member map in the HeapObject class.

Lets take a look at when a map is created.
```console
(lldb) br s -f map_test.cc -l 63
```

```c++
Handle<Map> Factory::NewMap(InstanceType type,
                            int instance_size,
                            ElementsKind elements_kind,
                            int inobject_properties) {
  HeapObject* result = isolate()->heap()->AllocateRawWithRetryOrFail(Map::kSize, MAP_SPACE);
  result->set_map_after_allocation(*meta_map(), SKIP_WRITE_BARRIER);
  return handle(InitializeMap(Map::cast(result), type, instance_size,
                              elements_kind, inobject_properties),
                isolate());
}
```
We can see that the above is calling `AllocateRawWithRetryOrFail` on the heap instance passing a size of `88` and
specifying the `MAP_SPACE`:
```c++
HeapObject* Heap::AllocateRawWithRetryOrFail(int size, AllocationSpace space,
                                             AllocationAlignment alignment) {
  AllocationResult alloc;
  HeapObject* result = AllocateRawWithLigthRetry(size, space, alignment);
  if (result) return result;

  isolate()->counters()->gc_last_resort_from_handles()->Increment();
  CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
  {
    AlwaysAllocateScope scope(isolate());
    alloc = AllocateRaw(size, space, alignment);
  }
  if (alloc.To(&result)) {
    DCHECK(result != exception());
    return result;
  }
  // TODO(1181417): Fix this.
  FatalProcessOutOfMemory("CALL_AND_RETRY_LAST");
  return nullptr;
}
```
The default value for `alignment` is `kWordAligned`. Reading the docs in the header it says that this function
will try to perform an allocation of size `88` in the `MAP_SPACE` and if it fails a full GC will be performed
and the allocation retried.
Lets take a look at `AllocateRawWithLigthRetry`:
```c++
  AllocationResult alloc = AllocateRaw(size, space, alignment);
```
`AllocateRaw` can be found in `src/heap/heap-inl.h`. There are different paths that will be taken depending on the
`space` parameteter. Since it is `MAP_SPACE` in our case we will focus on that path:
```c++
AllocationResult Heap::AllocateRaw(int size_in_bytes, AllocationSpace space, AllocationAlignment alignment) {
  ...
  HeapObject* object = nullptr;
  AllocationResult allocation;
  if (OLD_SPACE == space) {
  ...
  } else if (MAP_SPACE == space) {
    allocation = map_space_->AllocateRawUnaligned(size_in_bytes);
  }
  ...
}
```
`map_space_` is a private member of Heap (src/heap/heap.h):
```c++
MapSpace* map_space_;
```
`AllocateRawUnaligned` can be found in `src/heap/spaces-inl.h`:
```c++
AllocationResult PagedSpace::AllocateRawUnaligned( int size_in_bytes, UpdateSkipList update_skip_list) {
  if (!EnsureLinearAllocationArea(size_in_bytes)) {
    return AllocationResult::Retry(identity());
  }

  HeapObject* object = AllocateLinearly(size_in_bytes);
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object->address(), size_in_bytes);
  return object;
}
```
The default value for `update_skip_list` is `UPDATE_SKIP_LIST`.
So lets take a look at `AllocateLinearly`:
```c++
HeapObject* PagedSpace::AllocateLinearly(int size_in_bytes) {
  Address current_top = allocation_info_.top();
  Address new_top = current_top + size_in_bytes;
  allocation_info_.set_top(new_top);
  return HeapObject::FromAddress(current_top);
}
```
Recall that `size_in_bytes` in our case is `88`.
```console
(lldb) expr current_top
(v8::internal::Address) $5 = 24847457492680
(lldb) expr new_top
(v8::internal::Address) $6 = 24847457492768
(lldb) expr new_top - current_top
(unsigned long) $7 = 88
```
Notice that first the top is set to the new_top and then the current_top is returned and that will be a pointer
to the start of the object in memory (which in this case is of v8::internal::Map which is also of type HeapObject).
I've been wondering why Map (and other HeapObject) don't have any member fields and only/mostly 
getters/setters for the various fields that make up an object. Well the answer is that pointers to instances of
for example Map point to the first memory location of the instance. And the getters/setter functions use indexed
to read/write to memory locations. The indexes are mostly in the form of enum fields that define the memory layout
of the type.

Next, in `AllocateRawUnaligned` we have the `MSAN_ALLOCATED_UNINITIALIZED_MEMORY` macro:
```c++
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object->address(), size_in_bytes);
```
`MSAN_ALLOCATED_UNINITIALIZED_MEMORY` can be found in `src/msan.h` and `ms` stands for `Memory Sanitizer` and 
would only be used if `V8_US_MEMORY_SANITIZER` is defined.
The returned `object` will be used to construct an `AllocationResult` when returned.
Back in `AllocateRaw` we have:
```c++
if (allocation.To(&object)) {
    ...
    OnAllocationEvent(object, size_in_bytes);
  }

  return allocation;
```
This will return us in `AllocateRawWithLightRetry`:
```c++
AllocationResult alloc = AllocateRaw(size, space, alignment);
if (alloc.To(&result)) {
  DCHECK(result != exception());
  return result;
}
```
This will return us back in `AllocateRawWithRetryOrFail`:
```c++
  HeapObject* result = AllocateRawWithLigthRetry(size, space, alignment);
  if (result) return result;
```
And that return will return to `NewMap` in `src/heap/factory.cc`:
```c++
  result->set_map_after_allocation(*meta_map(), SKIP_WRITE_BARRIER);
  return handle(InitializeMap(Map::cast(result), type, instance_size,
                              elements_kind, inobject_properties),
                isolate());
```
`InitializeMap`:
```c++
  map->set_instance_type(type);
  map->set_prototype(*null_value(), SKIP_WRITE_BARRIER);
  map->set_constructor_or_backpointer(*null_value(), SKIP_WRITE_BARRIER);
  map->set_instance_size(instance_size);
  if (map->IsJSObjectMap()) {
    DCHECK(!isolate()->heap()->InReadOnlySpace(map));
    map->SetInObjectPropertiesStartInWords(instance_size / kPointerSize - inobject_properties);
    DCHECK_EQ(map->GetInObjectProperties(), inobject_properties);
    map->set_prototype_validity_cell(*invalid_prototype_validity_cell());
  } else {
    DCHECK_EQ(inobject_properties, 0);
    map->set_inobject_properties_start_or_constructor_function_index(0);
    map->set_prototype_validity_cell(Smi::FromInt(Map::kPrototypeChainValid));
  }
  map->set_dependent_code(DependentCode::cast(*empty_fixed_array()), SKIP_WRITE_BARRIER);
  map->set_weak_cell_cache(Smi::kZero);
  map->set_raw_transitions(MaybeObject::FromSmi(Smi::kZero));
  map->SetInObjectUnusedPropertyFields(inobject_properties);
  map->set_instance_descriptors(*empty_descriptor_array());

  map->set_visitor_id(Map::GetVisitorId(map));
  map->set_bit_field(0);
  map->set_bit_field2(Map::IsExtensibleBit::kMask);
  int bit_field3 = Map::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
                   Map::OwnsDescriptorsBit::encode(true) |
                   Map::ConstructionCounterBits::encode(Map::kNoSlackTracking);
  map->set_bit_field3(bit_field3);
  map->set_elements_kind(elements_kind); //HOLEY_ELEMENTS
  map->set_new_target_is_base(true);
  isolate()->counters()->maps_created()->Increment();
  if (FLAG_trace_maps) LOG(isolate(), MapCreate(map));
  return map;
```

Creating a new map ([map_test.cc](./test/map_test.cc):
```c++
  i::Handle<i::Map> map = i::Map::Create(asInternal(isolate_), 10);
  std::cout << map->instance_type() << '\n';
```
`Map::Create` can be found in objects.cc:
```c++
Handle<Map> Map::Create(Isolate* isolate, int inobject_properties) {
  Handle<Map> copy = Copy(handle(isolate->object_function()->initial_map()), "MapCreate");
```
So, the first thing that will happen is `isolate->object_function()` will be called. This is function
that is generated by the preprocessor.

```c++
// from src/context.h
#define NATIVE_CONTEXT_FIELDS(V)                                               \
  ...                                                                          \
  V(OBJECT_FUNCTION_INDEX, JSFunction, object_function)                        \

// from src/isolate.h
#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name)     \
  Handle<type> Isolate::name() {                             \
    return Handle<type>(raw_native_context()->name(), this); \
  }                                                          \
  bool Isolate::is_##name(type* value) {                     \
    return raw_native_context()->is_##name(value);           \
  }
NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSOR)
```
`object_function()` will become:
```c++
  Handle<JSFunction> Isolate::object_function() {
    return Handle<JSFunction>(raw_native_context()->object_function(), this);
  }
```

Lets look closer at `JSFunction::initial_map()` in in object-inl.h:
```c++
Map* JSFunction::initial_map() {
  return Map::cast(prototype_or_initial_map());
}
```

`prototype_or_initial_map` is generated by a macro:
```c++
ACCESSORS_CHECKED(JSFunction, prototype_or_initial_map, Object,
                  kPrototypeOrInitialMapOffset, map()->has_prototype_slot())
```
`ACCESSORS_CHECKED` can be found in `src/objects/object-macros.h`:
```c++
#define ACCESSORS_CHECKED(holder, name, type, offset, condition) \
  ACCESSORS_CHECKED2(holder, name, type, offset, condition, condition)

#define ACCESSORS_CHECKED2(holder, name, type, offset, get_condition, \
                           set_condition)                             \
  type* holder::name() const {                                        \
    type* value = type::cast(READ_FIELD(this, offset));               \
    DCHECK(get_condition);                                            \
    return value;                                                     \
  }                                                                   \
  void holder::set_##name(type* value, WriteBarrierMode mode) {       \
    DCHECK(set_condition);                                            \
    WRITE_FIELD(this, offset, value);                                 \
    CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);  \
  }

#define FIELD_ADDR(p, offset) \
  (reinterpret_cast<Address>(p) + offset - kHeapObjectTag)

#define READ_FIELD(p, offset) \
  (*reinterpret_cast<Object* const*>(FIELD_ADDR(p, offset)))
```
The preprocessor will expand `prototype_or_initial_map` to:
```c++
  JSFunction* JSFunction::prototype_or_initial_map() const {
    JSFunction* value = JSFunction::cast(
        (*reinterpret_cast<Object* const*>(
            (reinterpret_cast<Address>(this) + kPrototypeOrInitialMapOffset - kHeapObjectTag))))
    DCHECK(map()->has_prototype_slot());
    return value;
  }
```
Notice that `map()->has_prototype_slot())` will be called first which looks like this:
```c++
Map* HeapObject::map() const {
  return map_word().ToMap();
}
```
__TODO: Add notes about MapWord__  
```c++
MapWord HeapObject::map_word() const {
  return MapWord(
      reinterpret_cast<uintptr_t>(RELAXED_READ_FIELD(this, kMapOffset)));
}
```
First thing that will happen is `RELAXED_READ_FIELD(this, kMapOffset)`
```c++
#define RELAXED_READ_FIELD(p, offset)           \
  reinterpret_cast<Object*>(base::Relaxed_Load( \
      reinterpret_cast<const base::AtomicWord*>(FIELD_ADDR(p, offset))))

#define FIELD_ADDR(p, offset) \
  (reinterpret_cast<Address>(p) + offset - kHeapObjectTag)
```
This will get expanded by the preprocessor to:
```c++
  reinterpret_cast<Object*>(base::Relaxed_Load(
      reinterpret_cast<const base::AtomicWord*>(
          (reinterpret_cast<Address>(this) + kMapOffset - kHeapObjectTag)))
```
`src/base/atomicops_internals_portable.h`:
```c++
inline Atomic8 Relaxed_Load(volatile const Atomic8* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}
```
So this will do an atomoic load of the ptr with the memory order of __ATOMIC_RELELAXED.


`ACCESSORS_CHECKED` also generates a `set_prototyp_or_initial_map`:
```c++
  void JSFunction::set_prototype_or_initial_map(JSFunction* value, WriteBarrierMode mode) {
    DCHECK(map()->has_prototype_slot());
    WRITE_FIELD(this, kPrototypeOrInitialMapOffset, value);
    CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kPrototypeOrInitialMapOffset, value, mode);
  }
```
What does `WRITE_FIELD` do?  
```c++
#define WRITE_FIELD(p, offset, value)                             \
  base::Relaxed_Store(                                            \
      reinterpret_cast<base::AtomicWord*>(FIELD_ADDR(p, offset)), \
      reinterpret_cast<base::AtomicWord>(value));
```
Which would expand into:
```c++
  base::Relaxed_Store(                                            \
      reinterpret_cast<base::AtomicWord*>(
          (reinterpret_cast<Address>(this) + kPrototypeOrInitialMapOffset - kHeapObjectTag)
      reinterpret_cast<base::AtomicWord>(value));
```

Lets take a look at what `instance_type` does:
```c++
InstanceType Map::instance_type() const {
  return static_cast<InstanceType>(READ_UINT16_FIELD(this, kInstanceTypeOffset));
}
```

To see what the above is doing we can do the same thing in the debugger:
Note that I got `11` below from `map->kInstanceTypeOffset - i::kHeapObjectTag`
```console
(lldb) memory read -f u -c 1 -s 8 `*map + 11`
0x6d4e6609ed4: 585472345729139745
(lldb) expr static_cast<InstanceType>(585472345729139745)
(v8::internal::InstanceType) $34 = JS_OBJECT_TYPE
```

Take `map->has_non_instance_prototype()`:
```console
(lldb) br s -n has_non_instance_prototype
(lldb) expr -i 0 -- map->has_non_instance_prototype()
```
The above command will break in `src/objects/map-inl.h`:
```c++
BIT_FIELD_ACCESSORS(Map, bit_field, has_non_instance_prototype, Map::HasNonInstancePrototypeBit)

// src/objects/object-macros.h
#define BIT_FIELD_ACCESSORS(holder, field, name, BitField)      \
  typename BitField::FieldType holder::name() const {           \
    return BitField::decode(field());                           \
  }                                                             \
  void holder::set_##name(typename BitField::FieldType value) { \
    set_##field(BitField::update(field(), value));              \
  }
```
The preprocessor will expand that to:
```c++
  typename Map::HasNonInstancePrototypeBit::FieldType Map::has_non_instance_prototype() const {
    return Map::HasNonInstancePrototypeBit::decode(bit_field());
  }                                                             \
  void holder::set_has_non_instance_prototype(typename BitField::FieldType value) { \
    set_bit_field(Map::HasNonInstancePrototypeBit::update(bit_field(), value));              \
  }
```
So where can we find `Map::HasNonInstancePrototypeBit`?  
It is generated by a macro in `src/objects/map.h`:
```c++
// Bit positions for |bit_field|.
#define MAP_BIT_FIELD_FIELDS(V, _)          \
  V(HasNonInstancePrototypeBit, bool, 1, _) \
  ...
  DEFINE_BIT_FIELDS(MAP_BIT_FIELD_FIELDS)
#undef MAP_BIT_FIELD_FIELDS

#define DEFINE_BIT_FIELDS(LIST_MACRO) \
  DEFINE_BIT_RANGES(LIST_MACRO)       \
  LIST_MACRO(DEFINE_BIT_FIELD_TYPE, LIST_MACRO##_Ranges)

#define DEFINE_BIT_RANGES(LIST_MACRO)                               \
  struct LIST_MACRO##_Ranges {                                      \
    enum { LIST_MACRO(DEFINE_BIT_FIELD_RANGE_TYPE, _) kBitsCount }; \
  };

#define DEFINE_BIT_FIELD_RANGE_TYPE(Name, Type, Size, _) \
  k##Name##Start, k##Name##End = k##Name##Start + Size - 1,
```
Alright, lets see what preprocessor expands that to:
```c++
  struct MAP_BIT_FIELD_FIELDS_Ranges {
    enum { 
      kHasNonInstancePrototypeBitStart, 
      kHasNonInstancePrototypeBitEnd = kHasNonInstancePrototypeBitStart + 1 - 1,
      ... // not showing the rest of the entries.
      kBitsCount 
    };
  };

```
So this would create a struct with an enum and it could be accessed using:
`i::Map::MAP_BIT_FIELD_FIELDS_Ranges::kHasNonInstancePrototypeBitStart`
The next part of the macro is
```c++
  LIST_MACRO(DEFINE_BIT_FIELD_TYPE, LIST_MACRO##_Ranges)

#define DEFINE_BIT_FIELD_TYPE(Name, Type, Size, RangesName) \
  typedef BitField<Type, RangesName::k##Name##Start, Size> Name;
```
Which will get expanded to:
```c++
  typedef BitField<HasNonInstancePrototypeBit, MAP_BIT_FIELD_FIELDS_Ranges::kHasNonInstancePrototypeBitStart, 1> HasNonInstancePrototypeBit;
```
So this is how `HasNonInstancePrototypeBit` is declared and notice that it is of type `BitField` which can be
found in `src/utils.h`:
```c++
template<class T, int shift, int size>
class BitField : public BitFieldBase<T, shift, size, uint32_t> { };

template<class T, int shift, int size, class U>
class BitFieldBase {
 public:
  typedef T FieldType;
```

Map::HasNonInstancePrototypeBit::decode(bit_field());
first bit_field is called:
```c++
byte Map::bit_field() const { return READ_BYTE_FIELD(this, kBitFieldOffset); }

```
And the result of that is passed to `Map::HasNonInstancePrototypeBit::decode`:

```console
(lldb) br s -n bit_field
(lldb) expr -i 0 --  map->bit_field()
```

```c++
byte Map::bit_field() const { return READ_BYTE_FIELD(this, kBitFieldOffset); }
```
So, `this` is the current Map instance, and we are going to read from.
```c++
#define READ_BYTE_FIELD(p, offset) \
  (*reinterpret_cast<const byte*>(FIELD_ADDR(p, offset)))

#define FIELD_ADDR(p, offset) \
  (reinterpret_cast<Address>(p) + offset - kHeapObjectTag)
```
Which will get expanded to:
```c++
byte Map::bit_field() const { 
  return *reinterpret_cast<const byte*>(
      reinterpret_cast<Address>(this) + kBitFieldOffset - kHeapObjectTag)
}
```

The instance_size is the instance_size_in_words << kPointerSizeLog2 (3 on my machine):
```console
(lldb) memory read -f x -s 1 -c 1 *map+8
0x24d1cd509ed1: 0x03
(lldb) expr 0x03 << 3
(int) $2 = 24
(lldb) expr map->instance_size()
(int) $3 = 24
```
`i::HeapObject::kHeaderSize` is 8 on my system  which is used in the `DEFINE_FIELD_OFFSET_CONSTANTS:
```c++
#define MAP_FIELDS(V)
V(kInstanceSizeInWordsOffset, kUInt8Size)
V(kInObjectPropertiesStartOrConstructorFunctionIndexOffset, kUInt8Size)
...
DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, MAP_FIELDS)
```

So we can use this information to read the `inobject_properties_start_or_constructor_function_index` directly from memory using:
```console
(lldb) expr map->inobject_properties_start_or_constructor_function_index()
(lldb) memory read -f x -s 1 -c 1 map+9
error: invalid start address expression.
error: address expression "map+9" evaluation failed
(lldb) memory read -f x -s 1 -c 1 *map+9
0x17b027209ed2: 0x03
```

Inspect the visitor_id (which is the last of the first byte):
```console
lldb) memory read -f x -s 1 -c 1 *map+10
0x17b027209ed3: 0x15
(lldb) expr (int) 0x15
(int) $8 = 21
(lldb) expr map->visitor_id()
(v8::internal::VisitorId) $11 = kVisitJSObjectFast
(lldb) expr (int) $11
(int) $12 = 21
```

Inspect the instance_type (which is part of the second byte):
```console
(lldb) expr map->instance_type()
(v8::internal::InstanceType) $41 = JS_OBJECT_TYPE
(lldb) expr v8::internal::InstanceType::JS_OBJECT_TYPE
(uint16_t) $35 = 1057
(lldb) memory read -f x -s 2 -c 1 *map+11
0x17b027209ed4: 0x0421
(lldb) expr (int)0x0421
(int) $40 = 1057
```
Notice that `instance_type` is a short so that will take up 2 bytes
```console
(lldb) expr map->has_non_instance_prototype()
(bool) $60 = false
(lldb) expr map->is_callable()
(bool) $46 = false
(lldb) expr map->has_named_interceptor()
(bool) $51 = false
(lldb) expr map->has_indexed_interceptor()
(bool) $55 = false
(lldb) expr map->is_undetectable()
(bool) $56 = false
(lldb) expr map->is_access_check_needed()
(bool) $57 = false
(lldb) expr map->is_constructor()
(bool) $58 = false
(lldb) expr map->has_prototype_slot()
(bool) $59 = false
```

Verify that the above is correct:
```console
(lldb) expr map->has_non_instance_prototype()
(bool) $44 = false
(lldb) memory read -f x -s 1 -c 1 *map+13
0x17b027209ed6: 0x00

(lldb) expr map->set_has_non_instance_prototype(true)
(lldb) memory read -f x -s 1 -c 1 *map+13
0x17b027209ed6: 0x01

(lldb) expr map->set_has_prototype_slot(true)
(lldb) memory read -f x -s 1 -c 1 *map+13
0x17b027209ed6: 0x81
```


Inspect second int field (bit_field2):
```console
(lldb) memory read -f x -s 1 -c 1 *map+14
0x17b027209ed7: 0x19
(lldb) expr map->is_extensible()
(bool) $78 = true
(lldb) expr -- 0x19 & (1 << 0)
(bool) $90 = 1

(lldb) expr map->is_prototype_map()
(bool) $79 = false

(lldb) expr map->is_in_retained_map_list()
(bool) $80 = false

(lldb) expr map->elements_kind()
(v8::internal::ElementsKind) $81 = HOLEY_ELEMENTS
(lldb) expr v8::internal::ElementsKind::HOLEY_ELEMENTS
(int) $133 = 3
(lldb) expr  0x19 >> 3
(int) $134 = 3
```

Inspect third int field (bit_field3):
```console
(lldb) memory read -f b -s 4 -c 1 *map+15
0x17b027209ed8: 0b00001000001000000000001111111111
(lldb) memory read -f x -s 4 -c 1 *map+15
0x17b027209ed8: 0x082003ff
```

So we know that a Map instance is a pointer allocated by the Heap and with a specific 
size. Fields are accessed using indexes (remember there are no member fields in the Map class).
We also know that all HeapObject have a Map. The Map is sometimes referred to as the HiddenClass
and somethime the shape of an object. If two objects have the same properties they would share 
the same Map. This makes sense and I've see blog post that show this but I'd like to verify
this to fully understand it.
I'm going to try to match https://v8project.blogspot.com/2017/08/fast-properties.html with 
the code.

So, lets take a look at adding a property to a JSObject. We start by creating a new Map and then 
use it to create a new JSObject:
```c++
  i::Handle<i::Map> map = factory->NewMap(i::JS_OBJECT_TYPE, 32);
  i::Handle<i::JSObject> js_object = factory->NewJSObjectFromMap(map);

  i::Handle<i::String> prop_name = factory->InternalizeUtf8String("prop_name");
  i::Handle<i::String> prop_value = factory->InternalizeUtf8String("prop_value");
  i::JSObject::AddProperty(js_object, prop_name, prop_value, i::NONE);  
```
Lets take a closer look at `AddProperty` and how it interacts with the Map. This function can be
found in `src/objects.cc`:
```c++
void JSObject::AddProperty(Handle<JSObject> object, Handle<Name> name,
                           Handle<Object> value,
                           PropertyAttributes attributes) {
  LookupIterator it(object, name, object, LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
```
First we have the LookupIterator constructor (`src/lookup.h`) but since this is a new property which 
we know does not exist it will not find any property.
```c++
CHECK(AddDataProperty(&it, value, attributes, kThrowOnError,
                        CERTAINLY_NOT_STORE_FROM_KEYED)
            .IsJust());
```
```c++
  Handle<JSReceiver> receiver = it->GetStoreTarget<JSReceiver>();
  ...
  it->UpdateProtector();
  // Migrate to the most up-to-date map that will be able to store |value|
  // under it->name() with |attributes|.
  it->PrepareTransitionToDataProperty(receiver, value, attributes, store_mode);
  DCHECK_EQ(LookupIterator::TRANSITION, it->state());
  it->ApplyTransitionToDataProperty(receiver);

  // Write the property value.
  it->WriteDataValue(value, true);
```
`PrepareTransitionToDataProperty`:
```c++
  Representation representation = value->OptimalRepresentation();
  Handle<FieldType> type = value->OptimalType(isolate, representation);
  maybe_map = Map::CopyWithField(map, name, type, attributes, constness,
  representation, flag);
```
`Map::CopyWithField`:
```c++
  Descriptor d = Descriptor::DataField(name, index, attributes, constness, representation, wrapped_type);
```
Lets take a closer look the Decriptor which can be found in `src/property.cc`:
```c++
Descriptor Descriptor::DataField(Handle<Name> key, int field_index,
                                 PropertyAttributes attributes,
                                 PropertyConstness constness,
                                 Representation representation,
                                 MaybeObjectHandle wrapped_field_type) {
  DCHECK(wrapped_field_type->IsSmi() || wrapped_field_type->IsWeakHeapObject());
  PropertyDetails details(kData, attributes, kField, constness, representation,
                          field_index);
  return Descriptor(key, wrapped_field_type, details);
}
```
`Descriptor` is declared in `src/property.h` and describes the elements in a instance-descriptor array. These
are returned when calling `map->instance_descriptors()`. Let check some of the arguments:
```console
(lldb) job *key
#prop_name
(lldb) expr attributes
(v8::internal::PropertyAttributes) $27 = NONE
(lldb) expr constness
(v8::internal::PropertyConstness) $28 = kMutable
(lldb) expr representation
(v8::internal::Representation) $29 = (kind_ = '\b')
```
The Descriptor class contains three members:
```c++
 private:
  Handle<Name> key_;
  MaybeObjectHandle value_;
  PropertyDetails details_;
```
Lets take a closer look `PropertyDetails` which only has a single member named `value_`
```c++
  uint32_t value_;
```
It also declares a number of classes the extend BitField, for example:
```c++
class KindField : public BitField<PropertyKind, 0, 1> {};
class LocationField : public BitField<PropertyLocation, KindField::kNext, 1> {};
class ConstnessField : public BitField<PropertyConstness, LocationField::kNext, 1> {};
class AttributesField : public BitField<PropertyAttributes, ConstnessField::kNext, 3> {};
class PropertyCellTypeField : public BitField<PropertyCellType, AttributesField::kNext, 2> {};
class DictionaryStorageField : public BitField<uint32_t, PropertyCellTypeField::kNext, 23> {};

// Bit fields for fast objects.
class RepresentationField : public BitField<uint32_t, AttributesField::kNext, 4> {};
class DescriptorPointer : public BitField<uint32_t, RepresentationField::kNext, kDescriptorIndexBitCount> {};
class FieldIndexField : public BitField<uint32_t, DescriptorPointer::kNext, kDescriptorIndexBitCount> {

enum PropertyKind { kData = 0, kAccessor = 1 };
enum PropertyLocation { kField = 0, kDescriptor = 1 };
enum class PropertyConstness { kMutable = 0, kConst = 1 };
enum PropertyAttributes {
  NONE = ::v8::None,
  READ_ONLY = ::v8::ReadOnly,
  DONT_ENUM = ::v8::DontEnum,
  DONT_DELETE = ::v8::DontDelete,
  ALL_ATTRIBUTES_MASK = READ_ONLY | DONT_ENUM | DONT_DELETE,
  SEALED = DONT_DELETE,
  FROZEN = SEALED | READ_ONLY,
  ABSENT = 64,  // Used in runtime to indicate a property is absent.
  // ABSENT can never be stored in or returned from a descriptor's attributes
  // bitfield.  It is only used as a return value meaning the attributes of
  // a non-existent property.
};
enum class PropertyCellType {
  // Meaningful when a property cell does not contain the hole.
  kUndefined,     // The PREMONOMORPHIC of property cells.
  kConstant,      // Cell has been assigned only once.
  kConstantType,  // Cell has been assigned only one type.
  kMutable,       // Cell will no longer be tracked as constant.
  // Meaningful when a property cell contains the hole.
  kUninitialized = kUndefined,  // Cell has never been initialized.
  kInvalidated = kConstant,     // Cell has been deleted, invalidated or never
                                // existed.
  // For dictionaries not holding cells.
  kNoCell = kMutable,
};


template<class T, int shift, int size>
class BitField : public BitFieldBase<T, shift, size, uint32_t> { };
```
The Type T of KindField will be `PropertyKind`, the `shift` will be 0 , and the `size` 1.
Notice that `LocationField` is using `KindField::kNext` as its shift. This is a static class constant
of type `uint32_t` and is defined as:
```c++
static const U kNext = kShift + kSize;
```
So `LocationField` would get the value from KindField which should be:
```c++
class LocationField : public BitField<PropertyLocation, 1, 1> {};
```

The constructor for PropertyDetails looks like this:
```c++
PropertyDetails(PropertyKind kind, PropertyAttributes attributes, PropertyCellType cell_type, int dictionary_index = 0) {
    value_ = KindField::encode(kind) | LocationField::encode(kField) |
             AttributesField::encode(attributes) |
             DictionaryStorageField::encode(dictionary_index) |
             PropertyCellTypeField::encode(cell_type);
  }
```
So what does KindField::encode(kind) actualy do then?
```console
(lldb) expr static_cast<uint32_t>(kind())
(uint32_t) $36 = 0
(lldb) expr static_cast<uint32_t>(kind()) << 0
(uint32_t) $37 = 0
```
This value is later returned by calling `kind()`:
```c++
PropertyKind kind() const { return KindField::decode(value_); }
```
So we have all this information about this property, its type (Representation), constness, if it is 
read-only, enumerable, deletable, sealed, frozen. After that little detour we are back in `Descriptor::DataField`:
```c++
  return Descriptor(key, wrapped_field_type, details);
```
Here we are using the key (name of the property), the wrapped_field_type, and PropertyDetails we created.
What is `wrapped_field_type` again?  
If we back up a few frames back into `Map::TransitionToDataProperty` we can see that the type passed in
is taken from the following code:
```c++
  Representation representation = value->OptimalRepresentation();
  Handle<FieldType> type = value->OptimalType(isolate, representation);
```
So this is only taking the type of the field:
```console
(lldb) expr representation.kind()
(v8::internal::Representation::Kind) $51 = kHeapObject
```
This makes sense as the map only deals with the shape of the propery and not the value.
Next in `Map::CopyWithField` we have:
```c++
  Handle<Map> new_map = Map::CopyAddDescriptor(map, &d, flag);
```
`CopyAddDescriptor` does:
```c++
  Handle<DescriptorArray> descriptors(map->instance_descriptors());
 
  int nof = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(descriptors, nof, 1);
  new_descriptors->Append(descriptor);
  
  Handle<LayoutDescriptor> new_layout_descriptor =
      FLAG_unbox_double_fields
          ? LayoutDescriptor::New(map, new_descriptors, nof + 1)
          : handle(LayoutDescriptor::FastPointerLayout(), map->GetIsolate());

  return CopyReplaceDescriptors(map, new_descriptors, new_layout_descriptor,
                                flag, descriptor->GetKey(), "CopyAddDescriptor",
                                SIMPLE_PROPERTY_TRANSITION);
```
Lets take a closer look at `LayoutDescriptor`

```console
(lldb) expr new_layout_descriptor->Print()
Layout descriptor: <all tagged>
```
TODO: Take a closer look at LayoutDescritpor

Later when actually adding the value in `Object::AddDataProperty`:
```c++
  it->WriteDataValue(value, true);
```
This call will end up in `src/lookup.cc` and in our case the path will be the following call:
```c++
  JSObject::cast(*holder)->WriteToField(descriptor_number(), property_details_, *value);
```
TODO: Take a closer look at LookupIterator.
`WriteToField` can be found in `src/objects-inl.h`:
```c++
  FieldIndex index = FieldIndex::ForDescriptor(map(), descriptor);
```
`FieldIndex::ForDescriptor` can be found in `src/field-index-inl.h`:
```c++
inline FieldIndex FieldIndex::ForDescriptor(const Map* map, int descriptor_index) {
  PropertyDetails details = map->instance_descriptors()->GetDetails(descriptor_index);
  int field_index = details.field_index();
  return ForPropertyIndex(map, field_index, details.representation());
}
```
Notice that this is calling `instance_descriptors()` on the passed-in map. This as we recall from earlier returns
and DescriptorArray (which is a type of WeakFixedArray). A Descriptor array 

Our DecsriptorArray only has one entry:
```console
(lldb) expr map->instance_descriptors()->number_of_descriptors()
(int) $6 = 1
(lldb) expr map->instance_descriptors()->GetKey(0)->Print()
#prop_name
(lldb) expr map->instance_descriptors()->GetFieldIndex(0)
(int) $11 = 0
```
We can also use `Print` on the DescriptorArray:
```console
lldb) expr map->instance_descriptors()->Print()

  [0]: #prop_name (data field 0:h, p: 0, attrs: [WEC]) @ Any
```
In our case we are accessing the PropertyDetails and then getting the `field_index` which I think tells us
where in the object the value for this property is stored.
The last call in `ForDescriptor` is `ForProperty:
```c++
inline FieldIndex FieldIndex::ForPropertyIndex(const Map* map,
                                               int property_index,
                                               Representation representation) {
  int inobject_properties = map->GetInObjectProperties();
  bool is_inobject = property_index < inobject_properties;
  int first_inobject_offset;
  int offset;
  if (is_inobject) {
    first_inobject_offset = map->GetInObjectPropertyOffset(0);
    offset = map->GetInObjectPropertyOffset(property_index);
  } else {
    first_inobject_offset = FixedArray::kHeaderSize;
    property_index -= inobject_properties;
    offset = FixedArray::kHeaderSize + property_index * kPointerSize;
  }
  Encoding encoding = FieldEncoding(representation);
  return FieldIndex(is_inobject, offset, encoding, inobject_properties,
                    first_inobject_offset);
}
```
I was expecting `inobject_propertis` to be 1 here but it is 0:
```console
(lldb) expr inobject_properties
(int) $14 = 0
```
Why is that, what am I missing?  
These in-object properties are stored directly on the object instance and not do not use
the properties array. All get back to an example of this later to clarify this.
TODO: Add in-object properties example.

Back in `JSObject::WriteToField`:
```c++
  RawFastPropertyAtPut(index, value);
```

```c++
void JSObject::RawFastPropertyAtPut(FieldIndex index, Object* value) {
  if (index.is_inobject()) {
    int offset = index.offset();
    WRITE_FIELD(this, offset, value);
    WRITE_BARRIER(GetHeap(), this, offset, value);
  } else {
    property_array()->set(index.outobject_array_index(), value);
  }
}
```
In our case we know that the index is not inobject()
```console
(lldb) expr index.is_inobject()
(bool) $18 = false
```
So, `property_array()->set()` will be called.
```console
(lldb) expr this
(v8::internal::JSObject *) $21 = 0x00002c31c6a88b59
```
JSObject inherits from JSReceiver which is where the property_array() function is declared.
```c++
  inline PropertyArray* property_array() const;
```
```console
(lldb) expr property_array()->Print()
0x2c31c6a88bb1: [PropertyArray]
 - map: 0x2c31f5603e21 <Map>
 - length: 3
 - hash: 0
           0: 0x2c31f56025a1 <Odd Oddball: uninitialized>
         1-2: 0x2c31f56026f1 <undefined>
(lldb) expr index.outobject_array_index()
(int) $26 = 0
(lldb) expr value->Print()
#prop_value
```
Looking at the above values printed we should see the property be written to entry 0.
```console
(lldb) expr property_array()->get(0)->Print()
#uninitialized
// after call to set
(lldb) expr property_array()->get(0)->Print()
#prop_value
```

```console
(lldb) expr map->instance_descriptors()
(v8::internal::DescriptorArray *) $4 = 0x000039a927082339
```
So a map has an pointer array of instance of DescriptorArray

```console
(lldb) expr map->GetInObjectProperties()
(int) $19 = 1
```
Each Map has int that tells us the number of properties it has. This is the number specified when creating
a new Map, for example:
```console
i::Handle<i::Map> map = i::Map::Create(asInternal(isolate_), 1);
```
But at this stage we don't really have any properties. The value for a property is associated with the actual
instance of the Object. What the Map specifies is index of the value for a particualar property. 

#### Creating a Map instance
Lets take a look at when a map is created.
```console
(lldb) br s -f map_test.cc -l 63
```

```c++
Handle<Map> Factory::NewMap(InstanceType type,
                            int instance_size,
                            ElementsKind elements_kind,
                            int inobject_properties) {
  HeapObject* result = isolate()->heap()->AllocateRawWithRetryOrFail(Map::kSize, MAP_SPACE);
  result->set_map_after_allocation(*meta_map(), SKIP_WRITE_BARRIER);
  return handle(InitializeMap(Map::cast(result), type, instance_size,
                              elements_kind, inobject_properties),
                isolate());
}
```
We can see that the above is calling `AllocateRawWithRetryOrFail` on the heap instance passing a size of `88` and
specifying the `MAP_SPACE`:
```c++
HeapObject* Heap::AllocateRawWithRetryOrFail(int size, AllocationSpace space,
                                             AllocationAlignment alignment) {
  AllocationResult alloc;
  HeapObject* result = AllocateRawWithLigthRetry(size, space, alignment);
  if (result) return result;

  isolate()->counters()->gc_last_resort_from_handles()->Increment();
  CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
  {
    AlwaysAllocateScope scope(isolate());
    alloc = AllocateRaw(size, space, alignment);
  }
  if (alloc.To(&result)) {
    DCHECK(result != exception());
    return result;
  }
  // TODO(1181417): Fix this.
  FatalProcessOutOfMemory("CALL_AND_RETRY_LAST");
  return nullptr;
}
```
The default value for `alignment` is `kWordAligned`. Reading the docs in the header it says that this function
will try to perform an allocation of size `88` in the `MAP_SPACE` and if it fails a full GC will be performed
and the allocation retried.
Lets take a look at `AllocateRawWithLigthRetry`:
```c++
  AllocationResult alloc = AllocateRaw(size, space, alignment);
```
`AllocateRaw` can be found in `src/heap/heap-inl.h`. There are different paths that will be taken depending on the
`space` parameteter. Since it is `MAP_SPACE` in our case we will focus on that path:
```c++
AllocationResult Heap::AllocateRaw(int size_in_bytes, AllocationSpace space, AllocationAlignment alignment) {
  ...
  HeapObject* object = nullptr;
  AllocationResult allocation;
  if (OLD_SPACE == space) {
  ...
  } else if (MAP_SPACE == space) {
    allocation = map_space_->AllocateRawUnaligned(size_in_bytes);
  }
  ...
}
```
`map_space_` is a private member of Heap (src/heap/heap.h):
```c++
MapSpace* map_space_;
```
`AllocateRawUnaligned` can be found in `src/heap/spaces-inl.h`:
```c++
AllocationResult PagedSpace::AllocateRawUnaligned( int size_in_bytes, UpdateSkipList update_skip_list) {
  if (!EnsureLinearAllocationArea(size_in_bytes)) {
    return AllocationResult::Retry(identity());
  }

  HeapObject* object = AllocateLinearly(size_in_bytes);
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object->address(), size_in_bytes);
  return object;
}
```
The default value for `update_skip_list` is `UPDATE_SKIP_LIST`.
So lets take a look at `AllocateLinearly`:
```c++
HeapObject* PagedSpace::AllocateLinearly(int size_in_bytes) {
  Address current_top = allocation_info_.top();
  Address new_top = current_top + size_in_bytes;
  allocation_info_.set_top(new_top);
  return HeapObject::FromAddress(current_top);
}
```
Recall that `size_in_bytes` in our case is `88`.
```console
(lldb) expr current_top
(v8::internal::Address) $5 = 24847457492680
(lldb) expr new_top
(v8::internal::Address) $6 = 24847457492768
(lldb) expr new_top - current_top
(unsigned long) $7 = 88
```
Notice that first the top is set to the new_top and then the current_top is returned and that will be a pointer
to the start of the object in memory (which in this case is of v8::internal::Map which is also of type HeapObject).
I've been wondering why Map (and other HeapObject) don't have any member fields and only/mostly 
getters/setters for the various fields that make up an object. Well the answer is that pointers to instances of
for example Map point to the first memory location of the instance. And the getters/setter functions use indexed
to read/write to memory locations. The indexes are mostly in the form of enum fields that define the memory layout
of the type.

Next, in `AllocateRawUnaligned` we have the `MSAN_ALLOCATED_UNINITIALIZED_MEMORY` macro:
```c++
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object->address(), size_in_bytes);
```
`MSAN_ALLOCATED_UNINITIALIZED_MEMORY` can be found in `src/msan.h` and `ms` stands for `Memory Sanitizer` and 
would only be used if `V8_US_MEMORY_SANITIZER` is defined.
The returned `object` will be used to construct an `AllocationResult` when returned.
Back in `AllocateRaw` we have:
```c++
if (allocation.To(&object)) {
    ...
    OnAllocationEvent(object, size_in_bytes);
  }

  return allocation;
```
This will return us in `AllocateRawWithLightRetry`:
```c++
AllocationResult alloc = AllocateRaw(size, space, alignment);
if (alloc.To(&result)) {
  DCHECK(result != exception());
  return result;
}
```
This will return us back in `AllocateRawWithRetryOrFail`:
```c++
  HeapObject* result = AllocateRawWithLigthRetry(size, space, alignment);
  if (result) return result;
```
And that return will return to `NewMap` in `src/heap/factory.cc`:
```c++
  result->set_map_after_allocation(*meta_map(), SKIP_WRITE_BARRIER);
  return handle(InitializeMap(Map::cast(result), type, instance_size,
                              elements_kind, inobject_properties),
                isolate());
```
`InitializeMap`:
```c++
  map->set_instance_type(type);
  map->set_prototype(*null_value(), SKIP_WRITE_BARRIER);
  map->set_constructor_or_backpointer(*null_value(), SKIP_WRITE_BARRIER);
  map->set_instance_size(instance_size);
  if (map->IsJSObjectMap()) {
    DCHECK(!isolate()->heap()->InReadOnlySpace(map));
    map->SetInObjectPropertiesStartInWords(instance_size / kPointerSize - inobject_properties);
    DCHECK_EQ(map->GetInObjectProperties(), inobject_properties);
    map->set_prototype_validity_cell(*invalid_prototype_validity_cell());
  } else {
    DCHECK_EQ(inobject_properties, 0);
    map->set_inobject_properties_start_or_constructor_function_index(0);
    map->set_prototype_validity_cell(Smi::FromInt(Map::kPrototypeChainValid));
  }
  map->set_dependent_code(DependentCode::cast(*empty_fixed_array()), SKIP_WRITE_BARRIER);
  map->set_weak_cell_cache(Smi::kZero);
  map->set_raw_transitions(MaybeObject::FromSmi(Smi::kZero));
  map->SetInObjectUnusedPropertyFields(inobject_properties);
  map->set_instance_descriptors(*empty_descriptor_array());

  map->set_visitor_id(Map::GetVisitorId(map));
  map->set_bit_field(0);
  map->set_bit_field2(Map::IsExtensibleBit::kMask);
  int bit_field3 = Map::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
                   Map::OwnsDescriptorsBit::encode(true) |
                   Map::ConstructionCounterBits::encode(Map::kNoSlackTracking);
  map->set_bit_field3(bit_field3);
  map->set_elements_kind(elements_kind); //HOLEY_ELEMENTS
  map->set_new_target_is_base(true);
  isolate()->counters()->maps_created()->Increment();
  if (FLAG_trace_maps) LOG(isolate(), MapCreate(map));
  return map;
```

### Context
Context extends `FixedArray` (`src/context.h`). So an instance of this Context is a FixedArray and we can 
use Get(index) etc to get entries in the array.

### V8_EXPORT
This can be found in quite a few places in v8 source code. For example:

    class V8_EXPORT ArrayBuffer : public Object {

What is this?  
It is a preprocessor macro which looks like this:

    #if V8_HAS_ATTRIBUTE_VISIBILITY && defined(V8_SHARED)
    # ifdef BUILDING_V8_SHARED
    #  define V8_EXPORT __attribute__ ((visibility("default")))
    # else
    #  define V8_EXPORT
    # endif
    #else
    # define V8_EXPORT
    #endif 

So we can see that if `V8_HAS_ATTRIBUTE_VISIBILITY`, and `defined(V8_SHARED)`, and also 
if `BUILDING_V8_SHARED`, `V8_EXPORT` is set to `__attribute__ ((visibility("default"))`.
But in all other cases `V8_EXPORT` is empty and the preprocessor does not insert 
anything (nothing will be there come compile time). 
But what about the `__attribute__ ((visibility("default"))` what is this?  

In the GNU compiler collection (GCC) environment, the term that is used for exporting is visibility. As it 
applies to functions and variables in a shared object, visibility refers to the ability of other shared objects 
to call a C/C++ function. Functions with default visibility have a global scope and can be called from other 
shared objects. Functions with hidden visibility have a local scope and cannot be called from other shared objects.

Visibility can be controlled by using either compiler options or visibility attributes.
In your header files, wherever you want an interface or API made public outside the current Dynamic Shared Object (DSO)
, place `__attribute__ ((visibility ("default")))` in struct, class and function declarations you wish to make public.
 With `-fvisibility=hidden`, you are telling GCC that every declaration not explicitly marked with a visibility attribute 
has a hidden visibility. There is such a flag in build/common.gypi


### ToLocalChecked()
You'll see a few of these calls in the hello_world example:

     Local<String> source = String::NewFromUtf8(isolate, js, NewStringType::kNormal).ToLocalChecked();

NewFromUtf8 actually returns a Local<String> wrapped in a MaybeLocal which forces a check to see if 
the Local<> is empty before using it. 
NewStringType is an enum which can be kNormalString (k for constant) or kInternalized.

The following is after running the preprocessor (clang -E src/api.cc):

    # 5961 "src/api.cc"
    Local<String> String::NewFromUtf8(Isolate* isolate,
                                  const char* data,
                                  NewStringType type,
                                  int length) {
      MaybeLocal<String> result; 
      if (length == 0) { 
        result = String::Empty(isolate); 
      } else if (length > i::String::kMaxLength) { 
        result = MaybeLocal<String>(); 
      } else { 
        i::Isolate* i_isolate = reinterpret_cast<internal::Isolate*>(isolate); 
        i::VMState<v8::OTHER> __state__((i_isolate)); 
        i::RuntimeCallTimerScope _runtime_timer( i_isolate, &i::RuntimeCallStats::API_String_NewFromUtf8); 
        LOG(i_isolate, ApiEntryCall("v8::" "String" "::" "NewFromUtf8")); 
        if (length < 0) length = StringLength(data); 
        i::Handle<i::String> handle_result = NewString(i_isolate->factory(), static_cast<v8::NewStringType>(type), i::Vector<const char>(data, length)) .ToHandleChecked(); 
        result = Utils::ToLocal(handle_result); 
     };
     return result.FromMaybe(Local<String>());;
    }

I was wondering where the Utils::ToLocal was defined but could not find it until I found:

    MAKE_TO_LOCAL(ToLocal, String, String)

    #define MAKE_TO_LOCAL(Name, From, To)                                       \
    Local<v8::To> Utils::Name(v8::internal::Handle<v8::internal::From> obj) {   \
      return Convert<v8::internal::From, v8::To>(obj);                          \
    }

The above can be found in src/api.h. The same goes for `Local<Object>, Local<String>` etc.


### Small Integers
Reading through v8.h I came accross `// Tag information for Smi`
Smi stands for small integers. It turns out that ECMA Number is defined as a 64-bit binary double-precision
but internally V8 uses 32-bit to represent all values. How can that work, how can you represent a 64-bit value
using only 32-bits?   

Instead the small integer is represented by the 32 bits plus a pointer to the 64-bit number. V8 needs to
know if a value stored in memory represents a 32-bit integer, or if it is really a 64-bit number, in which
case it has to follow the pointer to get the complete value. This is where the concept of tagging comes in.
Tagging involved borrowing one bit of the 32-bit, making it 31-bit and having the leftover bit represent a 
tag. If the tag is zero then this is a plain value, but if tag is 1 then the pointer must be followed.
This does not only have to be for numbers it could also be used for object (I think)


### Properties/Elements
Take the following object:

    { firstname: "Jon", lastname: "Doe' }

The above object has two named properties. Named properties differ from integer indexed 
which is what you have when you are working with arrays.

Memory layout of JavaScript Object:
```
Properties                  JavaScript Object               Elements
+-----------+              +-----------------+         +----------------+
|property1  |<------+      | HiddenClass     |  +----->|                |
+-----------+       |      +-----------------+  |      +----------------+
|...        |       +------| Properties      |  |      | element1       |<------+
+-----------+              +-----------------+  |      +----------------+       |
|...        |              | Elements        |--+      | ...            |       |
+-----------+              +-----------------+         +----------------+       |
|propertyN  | <---------------------+                  | elementN       |       |
+-----------+                       |                  +----------------+       |
                                    |                                           |
                                    |                                           |
                                    |                                           | 
Named properties:    { firstname: "Jon", lastname: "Doe' } Indexed Properties: {1: "Jon", 2: "Doe"}
```
We can see that properies and elements are stored in different data structures.
Elements are usually implemented as a plain array and the indexes can be used for fast access
to the elements. 
But for the properties this is not the case. Instead there is a mapping between the property names
and the index into the properties.

In `src/objects.h` we can find JSObject:

    class JSObject: public JSReceiver {
    ...
    DECL_ACCESSORS(elements, FixedArrayBase)


And looking a the `DECL_ACCESSOR` macro:

    #define DECL_ACCESSORS(name, type)    \
      inline type* name() const;          \
      inline void set_##name(type* value, \
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

    inline FixedArrayBase* name() const;
    inline void set_elements(FixedArrayBase* value, WriteBarrierMode = UPDATE_WRITE_BARRIER)

Notice that JSObject extends JSReceiver which is extended by all types that can have properties defined on them. I think this includes all JSObjects and JSProxy. It is in JSReceiver that the we find the properties array:

    DECL_ACCESSORS(raw_properties_or_hash, Object)

Now properties (named properties not elements) can be of different kinds internally. These work just
like simple dictionaries from the outside but a dictionary is only used in certain curcumstances
at runtime.

```
Properties                  JSObject                    HiddenClass (Map)
+-----------+              +-----------------+         +----------------+
|property1  |<------+      | HiddenClass     |-------->| bit field1     |
+-----------+       |      +-----------------+         +----------------+
|...        |       +------| Properties      |         | bit field2     |
+-----------+              +-----------------+         +----------------+
|...        |              | Elements        |         | bit field3     |
+-----------+              +-----------------+         +----------------+
|propertyN  |              | property1       |         
+-----------+              +-----------------+         
                           | property2       |
                           +-----------------+
                           | ...             |
                           +-----------------+

```

#### JSObject
Each JSObject has as its first field a pointer to the generated HiddenClass. A hiddenclass contain mappings from property names to indices into the properties data type. When an instance of JSObject is created a `Map` is passed in.
As mentioned earlier JSObject inherits from JSReceiver which inherits from HeapObject

For example,in [jsobject_test.cc](./test/jsobject_test.cc) we first create a new Map using the internal Isolate Factory:

    v8::internal::Handle<v8::internal::Map> map = factory->NewMap(v8::internal::JS_OBJECT_TYPE, 24);
    v8::internal::Handle<v8::internal::JSObject> js_object = factory->NewJSObjectFromMap(map);
    EXPECT_TRUE(js_object->HasFastProperties());

When we call `js_object->HasFastProperties()` this will delegate to the map instance:

    return !map()->is_dictionary_map();

How do you add a property to a JSObject instance?
Take a look at [jsobject_test.cc](./test/jsobject_test.cc) for an example.


### Caching
Are ways to optimize polymorphic function calls in dynamic languages, for example JavaScript.

#### Lookup caches
Sending a message to a receiver requires the runtime to find the correct target method using
the runtime type of the receiver. A lookup cache maps the type of the receiver/message name
pair to methods and stores the most recently used lookup results. The cache is first consulted
and if there is a cache miss a normal lookup is performed and the result stored in the cache.

#### Inline caches
Using a lookup cache as described above still takes a considerable amount of time since the
cache must be probed for each message. It can be observed that the type of the target does often
not vary. If a call to type A is done at a particular call site it is very likely that the next
time it is called the type will also be A.
The method address looked up by the system lookup routine can be cached and the call instruction
can be overwritten. Subsequent calls for the same type can jump directly to the cached method and
completely avoid the lookup. The prolog of the called method must verify that the receivers
type has not changed and do the lookup if it has changed (the type if incorrect, no longer A for
example).

The target methods address is stored in the callers code, or "inline" with the callers code, 
hence the name "inline cache".

If V8 is able to make a good assumption about the type of object that will be passed to a method,
it can bypass the process of figuring out how to access the objects properties, and instead use the stored information from previous lookups to the objects hidden class.

#### Polymorfic Inline cache (PIC)
A polymorfic call site is one where there are many equally likely receiver types (and thus
call targets).

- Monomorfic means there is only one receiver type
- Polymorfic a few receiver types
- Megamorfic very many receiver types

This type of caching extends inline caching to not just cache the last lookup, but cache
all lookup results for a given polymorfic call site using a specially generated stub.
Lets say we have a method that iterates through a list of types and calls a method. If 
all the types are the same (monomorfic) a PIC acts just like an inline cache. The calls will
directly call the target method (with the method prolog followed by the method body).
If a different type exists in the list there will be a cache miss in the prolog and the lookup
routine called. In normal inline caching this would rebind the call, replacing the call to this
types target method. This would happen each time the type changes.

With PIC the cache miss handler will generate a small stub routine and rebinds the call to this
stub. The stub will check if the receiver is of a type that it has seen before and branch to 
the correct targets. Since the type of the target is already known at this point it can directly
branch to the target method body without the need for the prolog.
If the type has not been seen before it will be added to the stub to handle that type. Eventually
the stub will contain all types used and there will be no more cache misses/lookups.

The problem is that we don't have type information so methods cannot be called directly, but 
instead be looked up. In a static language a virtual table might have been used. In JavaScript
there is no inheritance relationship so it is not possible to know a vtable offset ahead of time.
What can be done is to observe and learn about the "types" used in the program. When an object
is seen it can be stored and the target of that method call can be stored and inlined into that
call. Bascially the type will be checked and if that particular type has been seen before the
method can just be invoked directly. But how do we check the type in a dynamic language? The
answer is hidden classes which allow the VM to quickly check an object against a hidden class.

The inline caching source are located in `src/ic`.

## --trace-ic

    $ out/x64.debug/d8 --trace-ic --trace-maps class.js

    before
    [TraceMaps: Normalize from= 0x19a314288b89 to= 0x19a31428aff9 reason= NormalizeAsPrototype ]
    [TraceMaps: ReplaceDescriptors from= 0x19a31428aff9 to= 0x19a31428b051 reason= CopyAsPrototype ]
    [TraceMaps: InitialMap map= 0x19a31428afa1 SFI= 34_Person ]

    [StoreIC in ~Person+65 at class.js:2 (0->.) map=0x19a31428afa1 0x10e68ba83361 <String[4]: name>]
    [TraceMaps: Transition from= 0x19a31428afa1 to= 0x19a31428b0a9 name= name ]
    [StoreIC in ~Person+102 at class.js:3 (0->.) map=0x19a31428b0a9 0x2beaa25abd89 <String[3]: age>]
    [TraceMaps: Transition from= 0x19a31428b0a9 to= 0x19a31428b101 name= age ]
    [TraceMaps: SlowToFast from= 0x19a31428b051 to= 0x19a31428b159 reason= OptimizeAsPrototype ]
    [StoreIC in ~Person+65 at class.js:2 (.->1) map=0x19a31428afa1 0x10e68ba83361 <String[4]: name>]
    [StoreIC in ~Person+102 at class.js:3 (.->1) map=0x19a31428b0a9 0x2beaa25abd89 <String[3]: age>]
    [LoadIC in ~+546 at class.js:9 (0->.) map=0x19a31428b101 0x10e68ba83361 <String[4]: name>]
    [CallIC in ~+571 at class.js:9 (0->1) map=0x0 0x32f481082231 <String[5]: print>]
    Daniel
    [LoadIC in ~+642 at class.js:10 (0->.) map=0x19a31428b101 0x2beaa25abd89 <String[3]: age>]
    [CallIC in ~+667 at class.js:10 (0->1) map=0x0 0x32f481082231 <String[5]: print>]
    41
    [LoadIC in ~+738 at class.js:11 (0->.) map=0x19a31428b101 0x10e68ba83361 <String[4]: name>]
    [CallIC in ~+763 at class.js:11 (0->1) map=0x0 0x32f481082231 <String[5]: print>]
    Tilda
    [LoadIC in ~+834 at class.js:12 (0->.) map=0x19a31428b101 0x2beaa25abd89 <String[3]: age>]
    [CallIC in ~+859 at class.js:12 (0->1) map=0x0 0x32f481082231 <String[5]: print>]
    2
    [CallIC in ~+927 at class.js:13 (0->1) map=0x0 0x32f481082231 <String[5]: print>]
    after

LoadIC (0->.) means that it has transitioned from unititialized state (0) to pre-monomophic state (.)
monomorphic state is specified with a `1`. These states can be found in [src/ic/ic.cc](https://github.com/v8/v8/blob/df1494d69deab472a1a709bd7e688297aa5cc655/src/ic/ic.cc#L33-L52).
What we are doing caching knowledge about the layout of the previously seen object inside the StoreIC/LoadIC calls.

    $ lldb -- out/x64.debug/d8 class.js

#### HeapObject
This class describes heap allocated objects. It is in this class we find information regarding the type of object. This 
information is contained in `v8::internal::Map`.

### v8::internal::Map
`src/objects/map.h`  
* `bit_field1`  
* `bit_field2`
* `bit field3` contains information about the number of properties that this Map has,
a pointer to an DescriptorArray. The DescriptorArray contains information like the name of the 
property, and the posistion where the value is stored in the JSObject.
I noticed that this information available in src/objects/map.h. 

#### DescriptorArray
Can be found in src/objects/descriptor-array.h. This class extends FixedArray and has the following
entries:

```
[0] the number of descriptors it contains  
[1] If uninitialized this will be Smi(0) otherwise an enum cache bridge which is a FixedArray of size 2: 
  [0] enum cache: FixedArray containing all own enumerable keys  
  [1] either Smi(0) or a pointer to a FixedArray with indices  
[2] first key (and internalized String  
[3] first descriptor  
```
### Factory
Each Internal Isolate has a Factory which is used to create instances. This is because all handles needs to be allocated
using the factory (src/factory.h)


### Objects 
All objects extend the abstract class Object (src/objects.h).

### Oddball
This class extends HeapObject and  describes `null`, `undefined`, `true`, and `false` objects.


#### Map
Extends HeapObject and all heap objects have a Map which describes the objects structure.
This is where you can find the size of the instance, access to the inobject_properties.

### Compiler pipeline
When a script is compiled all of the top level code is parsed. These are function declarartions (but not the function
bodies). 

    function f1() {       <- top level code
      console.log('f1');  <- non top level
    }

    function f2() {       <- top level code
      f1();               <- non top level
      console.logg('f2'); <- non top level
    }

    f2();                 <- top level code
    var i = 10;           <- top level code

The non top level code must be pre-parsed to check for syntax errors.
The top level code is parsed and compiles by the full-codegen compiler. This compiler does not perform any optimizations and
it's only task is to generate machine code as quickly as possible (this is pre turbofan)

    Source ------> Parser  --------> Full-codegen ---------> Unoptimized Machine Code

So the whole script is parsed even though we only generated code for the top-level code. The pre-parse (the syntax checking)
was not stored in any way. The functions are lazy stubs that when/if the function gets called the function get compiled. This
means that the function has to be parsed (again, the first time was the pre-parse remember).

If a function is determined to be hot it will be optimized by one of the two optimizing compilers crankshaft for older parts of JavaScript or Turbofan for Web Assembly (WASM) and some of the newer es6 features.

The first time V8 sees a function it will parse it into an AST but not do any further processing of that tree
until that function is used. 

                         +-----> Full-codegen -----> Unoptimized code
                        /                               \/ /\       \
    Parser  ------> AST -------> Cranshaft    -----> Optimized code  |
                        \                                           /
                         +-----> Turbofan     -----> Optimized code

Inline Cachine (IC) is done here which also help to gather type information.
V8 also has a profiler thread which monitors which functions are hot and should be optimized. This profiling
also allows V8 to find out information about types using IC. This type information can then be fed to Crankshaft/Turbofan.
The type information is stored as a 8 bit value. 

When a function is optimized the unoptimized code cannot be thrown away as it might be needed since JavaScript is highly
dynamic the optimzed function migth change and the in that case we fallback to the unoptimzed code. This takes up
alot of memory which may be important for low end devices. Also the time spent in parsing (twice) takes time.

The idea with Ignition is to be an bytecode interpreter and to reduce memory consumption, the bytecode is very consice
compared to native code which can vary depending on the target platform.
The whole source can be parsed and compiled, compared to the current pipeline the has the pre-parse and parse stages mentioned above. So even unused functions will get compiled.
The bytecode becomes the source of truth instead of as before the AST.

    Source ------> Parser  --------> Ignition-codegen ---------> Bytecode ---------> Turbofan ----> Optimized Code ---+
                                                                  /\                                                  |
                                                                   +--------------------------------------------------+

    function bajja(a, b, c) {
      var d = c - 100;
      return a + d * b;
    }

    var result = bajja(2, 2, 150);
    print(result); 

    $ ./d8 test.js --ignition  --print_bytecode

    [generating bytecode for function: bajja]
    Parameter count 4
    Frame size 8
     14 E> 0x2eef8d9b103e @    0 : 7f                StackCheck
     38 S> 0x2eef8d9b103f @    1 : 03 64             LdaSmi [100]   // load 100
     38 E> 0x2eef8d9b1041 @    3 : 2b 02 02          Sub a2, [2]    // a2 is the third argument. a2 is an argument register
           0x2eef8d9b1044 @    6 : 1f fa             Star r0        // r0 is a register for local variables. We only have one which is d
     47 S> 0x2eef8d9b1046 @    8 : 1e 03             Ldar a1        // LoaD accumulator from Register argument a1 which is b
     60 E> 0x2eef8d9b1048 @   10 : 2c fa 03          Mul r0, [3]    // multiply that is our local variable in r0
     56 E> 0x2eef8d9b104b @   13 : 2a 04 04          Add a0, [4]    // add that to our argument register 0 which is a 
     65 S> 0x2eef8d9b104e @   16 : 83                Return         // return the value in the accumulator?


### Abstract Syntax Tree (AST)
In src/ast/ast.h. You can print the ast using the `--print-ast` option for d8.

Lets take the following javascript and look at the ast:

    const msg = 'testing';
    console.log(msg);

```
$ d8 --print-ast simple.js
[generating interpreter code for user-defined function: ]
--- AST ---
FUNC at 0
. KIND 0
. SUSPEND COUNT 0
. NAME ""
. INFERRED NAME ""
. DECLS
. . VARIABLE (0x7ffe5285b0f8) (mode = CONST) "msg"
. BLOCK NOCOMPLETIONS at -1
. . EXPRESSION STATEMENT at 12
. . . INIT at 12
. . . . VAR PROXY context[4] (0x7ffe5285b0f8) (mode = CONST) "msg"
. . . . LITERAL "testing"
. EXPRESSION STATEMENT at 23
. . ASSIGN at -1
. . . VAR PROXY local[0] (0x7ffe5285b330) (mode = TEMPORARY) ".result"
. . . CALL Slot(0)
. . . . PROPERTY Slot(4) at 31
. . . . . VAR PROXY Slot(2) unallocated (0x7ffe5285b3d8) (mode = DYNAMIC_GLOBAL) "console"
. . . . . NAME log
. . . . VAR PROXY context[4] (0x7ffe5285b0f8) (mode = CONST) "msg"
. RETURN at -1
. . VAR PROXY local[0] (0x7ffe5285b330) (mode = TEMPORARY) ".result"
```
You can find the declaration of EXPRESSION in ast.h.

### Bytecode
Can be found in src/interpreter/bytecodes.h

* StackCheck checks that stack limits are not exceeded to guard against overflow.
* `Star` Store content in accumulator regiser in register (the operand).
* Ldar   LoaD accumulator from Register argument a1 which is b

The registers are not machine registers, apart from the accumlator as I understand it, but would instead be stack allocated.


#### Parsing
Parsing is the parsing of the JavaScript and the generation of the abstract syntax tree. That tree is then visited and 
bytecode generated from it. This section tries to figure out where in the code these operations are performed.

For example, take the script example.

    $ make run-script
    $ lldb -- run-script
    (lldb) br s -n main
    (lldb) r

Lets take a look at the following line:

    Local<Script> script = Script::Compile(context, source).ToLocalChecked();

This will land us in `api.cc`

    ScriptCompiler::Source script_source(source);
    return ScriptCompiler::Compile(context, &script_source);

    MaybeLocal<Script> ScriptCompiler::Compile(Local<Context> context, Source* source, CompileOptions options) {
    ...
    auto isolate = context->GetIsolate();
    auto maybe = CompileUnboundInternal(isolate, source, options);

`CompileUnboundInternal` will call `GetSharedFunctionInfoForScript` (in src/compiler.cc):

    result = i::Compiler::GetSharedFunctionInfoForScript(
          str, name_obj, line_offset, column_offset, source->resource_options,
          source_map_url, isolate->native_context(), NULL, &script_data, options,
          i::NOT_NATIVES_CODE);

    (lldb) br s -f compiler.cc -l 1259

    LanguageMode language_mode = construct_language_mode(FLAG_use_strict);
    (lldb) p language_mode
    (v8::internal::LanguageMode) $10 = SLOPPY

`LanguageMode` can be found in src/globals.h and it is an enum with three values:

    enum LanguageMode : uint32_t { SLOPPY, STRICT, LANGUAGE_END };

`SLOPPY` mode, I assume, is the mode when there is no `"use strict";`. Remember that this can go inside a function and does not
have to be at the top level of the file.

    ParseInfo parse_info(script);

There is a [unit test](./test/ast_test.cc) that shows how a ParseInfo instance can be created
and inspected.

This will call ParseInfo's constructor (in src/parsing/parse-info.cc), and which will call `ParseInfo::InitFromIsolate`:

    DCHECK_NOT_NULL(isolate);
    set_hash_seed(isolate->heap()->HashSeed());
    set_stack_limit(isolate->stack_guard()->real_climit());
    set_unicode_cache(isolate->unicode_cache());
    set_runtime_call_stats(isolate->counters()->runtime_call_stats());
    set_ast_string_constants(isolate->ast_string_constants());

I was curious about these ast_string_constants:

    (lldb) p *ast_string_constants_
    (const v8::internal::AstStringConstants) $58 = {
      zone_ = {
        allocation_size_ = 1312
        segment_bytes_allocated_ = 8192
        position_ = 0x0000000105052538 <no value available>
        limit_ = 0x0000000105054000 <no value available>
        allocator_ = 0x0000000103e00080
        segment_head_ = 0x0000000105052000
        name_ = 0x0000000101623a70 "../../src/ast/ast-value-factory.h:365"
        sealed_ = false
      }
      string_table_ = {
        v8::base::TemplateHashMapImpl<void *, void *, v8::base::HashEqualityThenKeyMatcher<void *, bool (*)(void *, void *)>, v8::base::DefaultAllocationPolicy> = {
          map_ = 0x0000000105054000
          capacity_ = 64
          occupancy_ = 41
          match_ = {
            match_ = 0x000000010014b260 (libv8.dylib`v8::internal::AstRawString::Compare(void*, void*) at ast-value-factory.cc:122)
          }
        }
      }
      hash_seed_ = 500815076
      anonymous_function_string_ = 0x0000000105052018
      arguments_string_ = 0x0000000105052038
      async_string_ = 0x0000000105052058
      await_string_ = 0x0000000105052078
      boolean_string_ = 0x0000000105052098
      constructor_string_ = 0x00000001050520b8
      default_string_ = 0x00000001050520d8
      done_string_ = 0x00000001050520f8
      dot_string_ = 0x0000000105052118
      dot_for_string_ = 0x0000000105052138
      dot_generator_object_string_ = 0x0000000105052158
      dot_iterator_string_ = 0x0000000105052178
      dot_result_string_ = 0x0000000105052198
      dot_switch_tag_string_ = 0x00000001050521b8
      dot_catch_string_ = 0x00000001050521d8
      empty_string_ = 0x00000001050521f8
      eval_string_ = 0x0000000105052218
      function_string_ = 0x0000000105052238
      get_space_string_ = 0x0000000105052258
      length_string_ = 0x0000000105052278
      let_string_ = 0x0000000105052298
      name_string_ = 0x00000001050522b8
      native_string_ = 0x00000001050522d8
      new_target_string_ = 0x00000001050522f8
      next_string_ = 0x0000000105052318
      number_string_ = 0x0000000105052338
      object_string_ = 0x0000000105052358
      proto_string_ = 0x0000000105052378
      prototype_string_ = 0x0000000105052398
      return_string_ = 0x00000001050523b8
      set_space_string_ = 0x00000001050523d8
      star_default_star_string_ = 0x00000001050523f8
      string_string_ = 0x0000000105052418
      symbol_string_ = 0x0000000105052438
      this_string_ = 0x0000000105052458
      this_function_string_ = 0x0000000105052478
      throw_string_ = 0x0000000105052498
      undefined_string_ = 0x00000001050524b8
      use_asm_string_ = 0x00000001050524d8
      use_strict_string_ = 0x00000001050524f8
      value_string_ = 0x0000000105052518
    } 

So these are constants that are set on the new ParseInfo instance using the values from the isolate. Not exactly sure what I 
want with this but I might come back to it later.
So, we are back in ParseInfo's constructor:

    set_allow_lazy_parsing();
    set_toplevel();
    set_script(script);

Script is of type v8::internal::Script which can be found in src/object/script.h

Back now in compiler.cc and the GetSharedFunctionInfoForScript function:

    Zone compile_zone(isolate->allocator(), ZONE_NAME);

    ...
    if (parse_info->literal() == nullptr && !parsing::ParseProgram(parse_info, isolate))

`ParseProgram`:

    Parser parser(info);
    ...
    FunctionLiteral* result = nullptr;
    result = parser.ParseProgram(isolate, info);

`parser.ParseProgram`: 

    Handle<String> source(String::cast(info->script()->source()));


    (lldb) job *source
    "var user1 = new Person('Fletch');\x0avar user2 = new Person('Dr.Rosen');\x0aprint("user1 = " + user1.name);\x0aprint("user2 = " + user2.name);\x0a\x0a"

So here we can see our JavaScript as a String.

    std::unique_ptr<Utf16CharacterStream> stream(ScannerStream::For(source));
    scanner_.Initialize(stream.get(), info->is_module());
    result = DoParseProgram(info);

`DoParseProgram`:

    (lldb) br s -f parser.cc -l 639
    ...

    this->scope()->SetLanguageMode(info->language_mode());
    ParseStatementList(body, Token::EOS, &ok);

This call will land in parser-base.h and its `ParseStatementList` function.

    (lldb) br s -f parser-base.h -l 4695

    StatementT stat = ParseStatementListItem(CHECK_OK_CUSTOM(Return, kLazyParsingComplete));

    result = CompileToplevel(&parse_info, isolate, Handle<SharedFunctionInfo>::null());

This will land in `CompileTopelevel` (in the same file which is src/compiler.cc):

    // Compile the code.
    result = CompileUnoptimizedCode(parse_info, shared_info, isolate);

This will land in `CompileUnoptimizedCode` (in the same file which is src/compiler.cc):

    // Prepare and execute compilation of the outer-most function.
    std::unique_ptr<CompilationJob> outer_job(
       PrepareAndExecuteUnoptimizedCompileJob(parse_info, parse_info->literal(),
                                              shared_info, isolate));


    std::unique_ptr<CompilationJob> job(
        interpreter::Interpreter::NewCompilationJob(parse_info, literal, isolate));
    if (job->PrepareJob() == CompilationJob::SUCCEEDED &&
        job->ExecuteJob() == CompilationJob::SUCCEEDED) {
      return job;
    }

PrepareJobImpl:

    CodeGenerator::MakeCodePrologue(parse_info(), compilation_info(),
                                    "interpreter");
    return SUCCEEDED;

codegen.cc `MakeCodePrologue`:

interpreter.cc ExecuteJobImpl:

    generator()->GenerateBytecode(stack_limit());    

src/interpreter/bytecode-generator.cc

     RegisterAllocationScope register_scope(this);

The bytecode is register based (if that is the correct term) and we had an example previously. I'm guessing 
that this is what this call is about.

VisitDeclarations will iterate over all the declarations in the file which in our case are:

    var user1 = new Person('Fletch');
    var user2 = new Person('Dr.Rosen');

    (lldb) p *variable->raw_name()
    (const v8::internal::AstRawString) $33 = {
       = {
        next_ = 0x000000010600a280
        string_ = 0x000000010600a280
      }
      literal_bytes_ = (start_ = "user1", length_ = 5)
      hash_field_ = 1303438034
      is_one_byte_ = true
      has_string_ = false
    }

    // Perform a stack-check before the body.
    builder()->StackCheck(info()->literal()->start_position());

So that call will output a stackcheck instruction, like in the example above:

    14 E> 0x2eef8d9b103e @    0 : 7f                StackCheck

### Performance
Say you have the expression x + y the full-codegen compiler might produce:

    movq rax, x
    movq rbx, y
    callq RuntimeAdd

If x and y are integers just using the `add` operation would be much quicker:

    movq rax, x
    movq rbx, y
    add rax, rbx


Recall that functions are optimized so if the compiler has to bail out and unoptimize 
part of a function then the whole functions will be affected and it will go back to 
the unoptimized version.

## Bytecode
This section will examine the bytecode for the following JavaScript:

    function beve() {
      const p = new Promise((resolve, reject) => {
        resolve('ok');
      });

      p.then(msg => {
        console.log(msg);
      });
    }

    beve(); 

    $ d8 --print-bytecode promise.js

First have the main function which does not have a name:

    [generating bytecode for function: ]
    (The code that generated this can be found in src/objects.cc BytecodeArray::Dissassemble)
    Parameter count 1
    Frame size 32
           // load what ever the FixedArray[4] is in the constant pool into the accumulator.
           0x34423e7ac19e @    0 : 09 00             LdaConstant [0] 
           // store the FixedArray[4] in register r1
           0x34423e7ac1a0 @    2 : 1e f9             Star r1
           // store zero into the accumulator.
           0x34423e7ac1a2 @    4 : 02                LdaZero
           // store zero (the contents of the accumulator) into register r2.
           0x34423e7ac1a3 @    5 : 1e f8             Star r2
           // 
           0x34423e7ac1a5 @    7 : 1f fe f7          Mov <closure>, r3
           0x34423e7ac1a8 @   10 : 53 96 01 f9 03    CallRuntime [DeclareGlobalsForInterpreter], r1-r3
      0 E> 0x34423e7ac1ad @   15 : 90                StackCheck
    141 S> 0x34423e7ac1ae @   16 : 0a 01 00          LdaGlobal [1], [0]
           0x34423e7ac1b1 @   19 : 1e f9             Star r1
    141 E> 0x34423e7ac1b3 @   21 : 4f f9 03          CallUndefinedReceiver0 r1, [3]
           0x34423e7ac1b6 @   24 : 1e fa             Star r0
    148 S> 0x34423e7ac1b8 @   26 : 94                Return

    Constant pool (size = 2)
    0x34423e7ac149: [FixedArray] in OldSpace
     - map = 0x344252182309 <Map(HOLEY_ELEMENTS)>
     - length: 2
           0: 0x34423e7ac069 <FixedArray[4]>
           1: 0x34423e7abf59 <String[4]: beve>

    Handler Table (size = 16) Load the global with name in constant pool entry <name_index> into the
    // accumulator using FeedBackVector slot <slot> outside of a typeof

* LdaConstant <idx> 
Load the constant at index from the constant pool into the accumulator.  
* Star <dst>
Store the contents of the accumulator register in dst.  
* Ldar <src>
Load accumulator with value from register src.  
* LdaGlobal <idx> <slot>
Load the global with name in constant pool entry idx into the accumulator using FeedBackVector slot  outside of a typeof.
* Mov <closure>, <r3>
Store the value of register  

You can find the declarations for the these instructions in `src/interpreter/interpreter-generator.cc`.


## Unified code generation architecture

## FeedbackVector
Is attached to every function and is responsible for recording and managing all execution feedback, which is information about types enabling. 
You can find the declaration for this class in `src/feedback-vector.h`


## BytecodeGenerator
Is currently the only part of V8 that cares about the AST.

## BytecodeGraphBuilder
Produces high-level IR graph based on interpreter bytecodes.


## TurboFan
Is a compiler backend that gets fed a control flow graph and then does instruction selection, register allocation and code generation. The code generation generates 


### Execution/Runtime
I'm not sure if V8 follows this exactly but I've heard and read that when the engine comes 
across a function declaration it only parses and verifies the syntax and saves a ref
to the function name. The statements inside the function are not checked at this stage
only the syntax of the function declaration (parenthesis, arguments, brackets etc). 


### Function methods
The declaration of Function can be found in `include/v8.h` (just noting this as I've looked for it several times)

## String types
There are a number of different String types in V8 which are optimized for various situations.
If we look in src/objects.h we can see the object hierarchy:
```
    Object
      SMI
      HeapObject    // superclass for every object instans allocated on the heap.
        ...
        Name
          String
            SeqString
              SeqOneByteString
              SeqTwoByteString
            SlicedString
            ConsString
            ThinString
            ExternalString
              ExternalOneByteString
              ExternalTwoByteString
            InternalizedString
              SeqInternalizedString
                SeqOneByteInternalizedString
                SeqTwoByteInternalizedString
              ConsInternalizedString
              ExternalInternalizedString
                ExternalOneByteInternalizedString
                ExternalTwoByteInternalizedString
```

Do note that v8::String is declared in `include/v8.h`.

`Name` as can be seen extends HeapObject and anything that can be used as a property name should extend Name.
Looking at the declaration in include/v8.h we find the following:

    int GetIdentityHash();
    static Name* Cast(Value* obj)



#### String
A String extends `Name` and has a length and content. The content can be made up of 1 or 2 byte characters.
Looking at the declaration in include/v8.h we find the following:

    enum Encoding {
      UNKNOWN_ENCODING = 0x1,
      TWO_BYTE_ENCODING = 0x0,
      ONE_BYTE_ENCODING = 0x8
    };

    int Length() const;
    int Uft8Length const;
    bool IsOneByte() const;

Example usages can be found in [test/string_test.cc](./test/string_test.cc).
Looking at the functions I've seen one that returns the actual bytes
from the String. You can get at the in utf8 format using:

    String::Utf8Value print_value(joined);
    std::cout << *print_value << '\n';

So that is the only string class in include/v8.h, but there are a lot more implementations that we've seen above. There are used for various cases, for example for indexing, concatenation, and slicing).

#### SeqString
Represents a sequence of charaters which (the characters) are either one or two bytes in length

#### ConsString
These are string that are built using:

    const str = "one" + "two";

This would be represented as:
```
         +--------------+
         |              | 
   [str|one|two]     [one|...]   [two|...]
             |                       |
             +-----------------------+
```
So we can see that one and two in str are pointer to existing strings. 


#### ExternalString
These Strings located on the native heap. The ExternalString structure has a pointer to this external location and the usual length field for all Strings.

Looking at `String` I was not able to find any construtor for it, nor the other subtypes.

## Builtins
Are JavaScript functions/objects that are provided by V8. These are built using a C++ DSL and are 
passed through:

    CodeStubAssembler -> CodeAssembler -> RawMachineAssembler.

Builtins need to have bytecode generated for them so that they can be run in TurboFan.

`src/code-stub-assembler.h`

All the builtins are declared in `src/builtins/builtins-definitions.h` by the `BUILTIN_LIST_BASE` macro.
There are different type of builtins (TF = Turbo Fan):
* TFJ 
JavaScript linkage which means it is callable as a JavaScript function  
* TFS
CodeStub linkage. A builtin with stub linkage can be used to extract common code into a separate code object which can
then be used by multiple callers. These is useful because builtins are generated at compile time and
included in the V8 snapshot. This means that they are part of every isolate that is created. Being 
able to share common code for multiple builtins will save space.

* TFC 
CodeStub linkage with custom descriptor

To see how this works in action we first need to disable snapshots. If we don't, we won't be able to
set breakpoints as the the heap will be serialized at compile time and deserialized upon startup of v8.

To find the option to disable snapshots use:

    $ gn args --list out.gn/learning --short | more
    ...
    v8_use_snapshot=true
    $ gn args out.gn/learning
    v8_use_snapshot=false
    $ gn -C out.gn/learning

After building we should be able to set a break point in bootstrapper.cc and its function 
`Genesis::InitializeGlobal`:

    (lldb) br s -f bootstrapper.cc -l 2684

Lets take a look at how the `JSON` object is setup:

    Handle<String> name = factory->InternalizeUtf8String("JSON");
    Handle<JSObject> json_object = factory->NewJSObject(isolate->object_function(), TENURED);

`TENURED` means that this object should be allocated directly in the old generation.

    JSObject::AddProperty(global, name, json_object, DONT_ENUM);

`DONT_ENUM` is checked by some builtin functions and if set this object will be ignored by those
functions.

    SimpleInstallFunction(json_object, "parse", Builtins::kJsonParse, 2, false);

Here we can see that we are installing a function named `parse`, which takes 2 parameters. You can
find the definition in src/builtins/builtins-json.cc.
What does the `SimpleInstallFunction` do?

Lets take `console` as an example which was created using:

    Handle<JSObject> console = factory->NewJSObject(cons, TENURED);
    JSObject::AddProperty(global, name, console, DONT_ENUM);
    SimpleInstallFunction(console, "debug", Builtins::kConsoleDebug, 1, false,
                          NONE);

    V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
      Handle<JSObject> base, 
      const char* name, 
      Builtins::Name call, 
      int len,
      bool adapt, 
      PropertyAttributes attrs = DONT_ENUM,
      BuiltinFunctionId id = kInvalidBuiltinFunctionId) {

So we can see that base is our Handle to a JSObject, and name is "debug".
Buildtins::Name is Builtins:kConsoleDebug. Where is this defined?  
You can find a macro named `CPP` in `src/builtins/builtins-definitions.h`:

   CPP(ConsoleDebug)

What does this macro expand to?  
It is part of the `BUILTIN_LIST_BASE` macro in builtin-definitions.h
We have to look at where BUILTIN_LIST is used which we can find in builtins.cc.
In `builtins.cc` we have an array of `BuiltinMetadata` which is declared as:

    const BuiltinMetadata builtin_metadata[] = {
      BUILTIN_LIST(DECL_CPP, DECL_API, DECL_TFJ, DECL_TFC, DECL_TFS, DECL_TFH, DECL_ASM)
    };

    #define DECL_CPP(Name, ...) { #Name, Builtins::CPP, \
                                { FUNCTION_ADDR(Builtin_##Name) }},

Which will expand to the creation of a BuiltinMetadata struct entry in the array. The
BuildintMetadata struct looks like this which might help understand what is going on:

    struct BuiltinMetadata {
      const char* name;
      Builtins::Kind kind;
      union {
        Address cpp_entry;       // For CPP and API builtins.
        int8_t parameter_count;  // For TFJ builtins.
      } kind_specific_data;
    };

So the `CPP(ConsoleDebug)` will expand to an entry in the array which would look something like
this:

    { ConsoleDebug, 
      Builtings::CPP, 
      {
        reinterpret_cast<v8::internal::Address>(reinterpret_cast<intptr_t>(Builtin_ConsoleDebug))
      }
    },

The third paramter is the creation on the union which might not be obvious.

Back to the question I'm trying to answer which is:  
"Buildtins::Name is is Builtins:kConsoleDebug. Where is this defined?"  
For this we have to look at `builtins.h` and the enum Name:

    enum Name : int32_t {
    #define DEF_ENUM(Name, ...) k##Name,
        BUILTIN_LIST_ALL(DEF_ENUM)
    #undef DEF_ENUM
        builtin_count
     };

This will expand to the complete list of builtins in builtin-definitions.h using the DEF_ENUM
macro. So the expansion for ConsoleDebug will look like:

    enum Name: int32_t {
      ...
      kDebugConole,
      ...
    };

So backing up to looking at the arguments to SimpleInstallFunction which are:

    SimpleInstallFunction(console, "debug", Builtins::kConsoleDebug, 1, false,
                          NONE);

    V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
      Handle<JSObject> base, 
      const char* name, 
      Builtins::Name call, 
      int len,
      bool adapt, 
      PropertyAttributes attrs = DONT_ENUM,
      BuiltinFunctionId id = kInvalidBuiltinFunctionId) {

We know about `Builtins::Name`, so lets look at len which is one, what is this?  
SimpleInstallFunction will call:

    Handle<JSFunction> fun =
      SimpleCreateFunction(base->GetIsolate(), function_name, call, len, adapt);

`len` would be used if adapt was true but it is false in our case. This is what it would 
be used for if adapt was true:

    fun->shared()->set_internal_formal_parameter_count(len);

I'm not exactly sure what adapt is referring to here.

PropertyAttributes is not specified so it will get the default value of `DONT_ENUM`.
The last parameter which is of type BuiltinFunctionId is not specified either so the
default value of `kInvalidBuiltinFunctionId` will be used. This is an enum defined in 
`src/objects.h`.


This [blog](https://v8project.blogspot.se/2017/11/csa.html) provides an example of adding
a function to the String object. 

    $ out.gn/learning/mksnapshot --print-code > output

You can then see the generated code from this. This will produce a code stub that can 
be called through C++. Lets update this to have it be called from JavaScript:

Update builtins/builtins-string-get.cc :

    TF_BUILTIN(GetStringLength, StringBuiltinsAssembler) {
      Node* const str = Parameter(Descriptor::kReceiver);
      Return(LoadStringLength(str));
    }

We also have to update builtins/builtins-definitions.h:

    TFJ(GetStringLength, 0)

And bootstrapper.cc:

    SimpleInstallFunction(prototype, "len", Builtins::kGetStringLength, 0, true);

If you now build using 'ninja -C out.gn/learning' you should be able to run d8 and try this out:

    d8> const s = 'testing'
    undefined
    d8> s.len()
    7

Now lets take a closer look at the code that is generated for this:

    $ out.gn/learning/mksnapshot --print-code > output

Looking at the output generated I was surprised to see two entries for GetStringLength (I changed the name
just to make sure there was not something else generating the second one). Why two?

The following uses Intel Assembly syntax which means that no register/immediate prefixes and the first operand is the 
destination and the second operand the source.
```
--- Code ---
kind = BUILTIN
name = BeveStringLength
compiler = turbofan
Instructions (size = 136)
0x1fafde09b3a0     0  55             push rbp
0x1fafde09b3a1     1  4889e5         REX.W movq rbp,rsp                  // movq rsp into rbp

0x1fafde09b3a4     4  56             push rsi                            // push the value of rsi (first parameter) onto the stack 
0x1fafde09b3a5     5  57             push rdi                            // push the value of rdi (second parameter) onto the stack
0x1fafde09b3a6     6  50             push rax                            // push the value of rax (accumulator) onto the stack

0x1fafde09b3a7     7  4883ec08       REX.W subq rsp,0x8                  // make room for a 8 byte value on the stack
0x1fafde09b3ab     b  488b4510       REX.W movq rax,[rbp+0x10]           // move the value rpm + 10 to rax
0x1fafde09b3af     f  488b58ff       REX.W movq rbx,[rax-0x1]
0x1fafde09b3b3    13  807b0b80       cmpb [rbx+0xb],0x80                // IsString(object). compare byte to zero
0x1fafde09b3b7    17  0f8350000000   jnc 0x1fafde09b40d  <+0x6d>        // jump it carry flag was not set

0x1fafde09b3bd    1d  488b400f       REX.W movq rax,[rax+0xf]
0x1fafde09b3c1    21  4989e2         REX.W movq r10,rsp
0x1fafde09b3c4    24  4883ec08       REX.W subq rsp,0x8
0x1fafde09b3c8    28  4883e4f0       REX.W andq rsp,0xf0
0x1fafde09b3cc    2c  4c891424       REX.W movq [rsp],r10
0x1fafde09b3d0    30  488945e0       REX.W movq [rbp-0x20],rax
0x1fafde09b3d4    34  48be0000000001000000 REX.W movq rsi,0x100000000
0x1fafde09b3de    3e  48bad9c228dfa8090000 REX.W movq rdx,0x9a8df28c2d9    ;; object: 0x9a8df28c2d9 <String[101]: CAST(LoadObjectField(object, offset, MachineTypeOf<T>::value)) at ../../src/code-stub-assembler.h:432>
0x1fafde09b3e8    48  488bf8         REX.W movq rdi,rax
0x1fafde09b3eb    4b  48b830726d0a01000000 REX.W movq rax,0x10a6d7230    ;; external reference (check_object_type)
0x1fafde09b3f5    55  40f6c40f       testb rsp,0xf
0x1fafde09b3f9    59  7401           jz 0x1fafde09b3fc  <+0x5c>
0x1fafde09b3fb    5b  cc             int3l
0x1fafde09b3fc    5c  ffd0           call rax
0x1fafde09b3fe    5e  488b2424       REX.W movq rsp,[rsp]
0x1fafde09b402    62  488b45e0       REX.W movq rax,[rbp-0x20]
0x1fafde09b406    66  488be5         REX.W movq rsp,rbp
0x1fafde09b409    69  5d             pop rbp
0x1fafde09b40a    6a  c20800         ret 0x8

// this is where we jump to if IsString failed
0x1fafde09b40d    6d  48ba71c228dfa8090000 REX.W movq rdx,0x9a8df28c271    ;; object: 0x9a8df28c271 <String[76]\: CSA_ASSERT failed: IsString(object) [../../src/code-stub-assembler.cc:1498]\n>
0x1fafde09b417    77  e8e4d1feff     call 0x1fafde088600     ;; code: BUILTIN
0x1fafde09b41c    7c  cc             int3l
0x1fafde09b41d    7d  cc             int3l
0x1fafde09b41e    7e  90             nop
0x1fafde09b41f    7f  90             nop


Safepoints (size = 8)

RelocInfo (size = 7)
0x1fafde09b3e0  embedded object  (0x9a8df28c2d9 <String[101]: CAST(LoadObjectField(object, offset, MachineTypeOf<T>::value)) at ../../src/code-stub-assembler.h:432>)
0x1fafde09b3ed  external reference (check_object_type)  (0x10a6d7230)
0x1fafde09b40f  embedded object  (0x9a8df28c271 <String[76]\: CSA_ASSERT failed: IsString(object) [../../src/code-stub-assembler.cc:1498]\n>)
0x1fafde09b418  code target (BUILTIN)  (0x1fafde088600)

--- End code --- 
```


### TF_BUILTIN macro
Is a macro to defining Turbofan (TF) builtins and can be found in `builtins/builtins-utils-gen.h`

    TF_BUILTIN(GetStringLength, StringBuiltinsAssembler) {
      Node* const str = Parameter(Descriptor::kReceiver);
      Return(LoadStringLength(str));
    }

Let's take our GetStringLength example from above and see what this will be expanded to after
processing this macro:

    class GetStringLengthAssembler : public StringBuiltinsAssembler {
      public:
       typedef Builtin_GetStringLength_InterfaceDescriptor Descriptor;

       explicit GetStringLengthAssembler(compiler::CodeAssemblerState* state) : AssemblerBase(state) {}

       void GenerateGetStringLengthImpl();

       Node* Parameter(Descriptor::ParameterIndices index) {
         return CodeAssembler::Parameter(static_cast<int>(index));
       }

       Node* Parameter(BuiltinDescriptor::ParameterIndices index) {
         return CodeAssembler::Parameter(static_cast<int>(index));
       }
    };

    void Builtins::Generate_GetStringLength(compiler::CodeAssemblerState* state) {
      GetStringLengthAssembler assembler(state);
      state->SetInitialDebugInformation(GetStringLength, __FILE__, __LINE__);
      assembler.GenerateGetStringLenghtImpl();
    }

    void GetStringLengthAssembler::GenerateGetStringLengthImpl() {
      Node* const str = Parameter(Descriptor::kReceiver);
      Return(LoadStringLength(str));
    }

From the resulting class you can see how `Parameter` can be used from within `TF_BUILTIN` macro.

## Building V8
You'll need to have checked out the Google V8 sources to you local file system and build it by following 
the instructions found [here](https://developers.google.com/v8/build).

### [gclient](https://www.chromium.org/developers/how-tos/depottools) sync

    gclient sync

### [GN](https://chromium.googlesource.com/chromium/src/+/master/tools/gn/docs/quick_start.md)

    $ tools/dev/v8gen.py --help

    $ ./tools/dev/v8gen.py list
    ....
    x64.debug
    x64.optdebug
    x64.release

    $ vi out.gn/learning/args.gn

Generate Ninja files:

    $ gn args out.gn/learning

This will open an editor where you can set configuration options. I've been using the following:

    is_debug = true
    target_cpu = "x64"
    v8_enable_backtrace = true
    v8_enable_slow_dchecks = true
    v8_optimized_debug = false

Note that for lldb command aliases to work `is_debug` must be set to true.

List avaiable build arguments:

    $ gn args --list out.gn/learning

List all available targets:

    $ ninja -C out.gn/learning/ -t targets all

Building:

    $ ninja -C out.gn/learning

Running quickchecks:

    $ ./tools/run-tests.py --outdir=out.gn/learning --quickcheck

You can use `./tools-run-tests.py -h` to list all the opitions that can be passed
to run-tests.

Running cctest:
```console
$ out.gn/learning/cctest test-heap-profiler/HeapSnapshotRetainedObjectInfo
```
To get a list of the available tests:
```console
$ out.gn/learning/cctest --list
```

Running pre-submit checks:

    $ ./tools/presubmit.py

## Building chromium
When making changes to V8 you might need to verify that your changes have not broken anything in Chromium. 

Generate Your Project (gpy) :
You'll have to run this once before building:

    $ gclient sync
    $ gclient runhooks

#### Update the code base

    $ git fetch origin master
    $ git co master
    $ git merge origin/master

### Building using GN

    $ gn args out.gn/learning

### Building using Ninja

    $ ninja -C out.gn/learning 

Building the tests:

    $ ninja -C out.gn/learning chrome/test:unit_tests

An error I got when building the first time:

    traceback (most recent call last):
    File "./gyp-mac-tool", line 713, in <module>
      sys.exit(main(sys.argv[1:]))
    File "./gyp-mac-tool", line 29, in main
      exit_code = executor.Dispatch(args)
    File "./gyp-mac-tool", line 44, in Dispatch
      return getattr(self, method)(*args[1:])
    File "./gyp-mac-tool", line 68, in ExecCopyBundleResource
      self._CopyStringsFile(source, dest)
    File "./gyp-mac-tool", line 134, in _CopyStringsFile
      import CoreFoundation
    ImportError: No module named CoreFoundation
    [6642/20987] CXX obj/base/debug/base.task_annotator.o
    [6644/20987] ACTION base_nacl: build newlib plib_9b4f41e4158ebb93a5d28e6734a13e85
    ninja: build stopped: subcommand failed.

I was able to get around this by:

    $ pip install -U pyobjc

#### Using a specific version of V8
The instructions below work but it is also possible to create a soft link from chromium/src/v8
to local v8 repository and the build/test. 

So, we want to include our updated version of V8 so that we can verify that it builds correctly with our change to V8.
While I'm not sure this is the proper way to do it, I was able to update DEPS in src (chromium) and set
the v8 entry to git@github.com:danbev/v8.git@064718a8921608eaf9b5eadbb7d734ec04068a87:

    "git@github.com:danbev/v8.git@064718a8921608eaf9b5eadbb7d734ec04068a87"

You'll have to run `gclient sync` after this. 

Another way is to not updated the `DEPS` file, which is a version controlled file, but instead update
`.gclientrc` and add a `custom_deps` entry:

    solutions = [{u'managed': False, u'name': u'src', u'url': u'https://chromium.googlesource.com/chromium/src.git', 
    u'custom_deps': {
      "src/v8": "git@github.com:danbev/v8.git@27a666f9be7ca3959c7372bdeeee14aef2a4b7ba"
    }, u'deps_file': u'.DEPS.git', u'safesync_url': u''}]

## Buiding pdfium
You may have to compile this project (in addition to chromium to verify that changes in v8 are not breaking
code in pdfium.

### Create/clone the project

     $ mkdir pdfuim_reop
     $ gclient config --unmanaged https://pdfium.googlesource.com/pdfium.git
     $ gclient sync
     $ cd pdfium

### Building

    $ ninja -C out/Default

#### Using a branch of v8
You should be able to update the .gclient file adding a custom_deps entry:

    solutions = [
    {
      "name"        : "pdfium",
      "url"         : "https://pdfium.googlesource.com/pdfium.git",
      "deps_file"   : "DEPS",
      "managed"     : False,
      "custom_deps" : {
        "v8": "git@github.com:danbev/v8.git@064718a8921608eaf9b5eadbb7d734ec04068a87"
      },
    },
   ]
   cache_dir = None
You'll have to run `gclient sync` after this too.




## Code in this repo

#### hello-world
[hello-world](./hello-world.cc) is heavily commented and show the usage of a static int being exposed and
accessed from JavaScript.

#### instances
[instances](./instances.cc) shows the usage of creating new instances of a C++ class from JavaScript.

#### run-script
[run-script](./run-script.cc) is basically the same as instance but reads an external file, [script.js](./script.js)
and run the script.

#### tests
The test directory contains unit tests for individual classes/concepts in V8 to help understand them.

## Building this projects code

    $ make

## Running

    $ ./hello-world

## Cleaning

    $ make clean

## Contributing a change to V8
1) Create a working branch using `git new-branch name`
2) git cl upload  

See Googles [contributing-code](https://www.chromium.org/developers/contributing-code) for more details.

### Find the current issue number

    $ git cl issue

## Debugging

    $ lldb hello-world
    (lldb) br s -f hello-world.cc -l 27

There are a number of useful functions in `src/objects-printer.cc` which can also be used in lldb.

#### Print value of a Local object

    (lldb) print _v8_internal_Print_Object(*(v8::internal::Object**)(*init_fn))

#### Print stacktrace

    (lldb) p _v8_internal_Print_StackTrace()

#### Creating command aliases in lldb
Create a file named [.lldbinit](./.lldbinit) (in your project director or home directory). This file can now be found in v8's tools directory.



### Using d8
This is the source used for the following examples:

    $ cat class.js
    function Person(name, age) {
      this.name = name;
      this.age = age;
    }

    print("before");
    const p = new Person("Daniel", 41);
    print(p.name);
    print(p.age);
    print("after"); 


### V8_shell startup
What happens when the v8_shell is run?   

    $ lldb -- out/x64.debug/d8 --enable-inspector class.js
    (lldb) breakpoint set --file d8.cc --line 2662
    Breakpoint 1: where = d8`v8::Shell::Main(int, char**) + 96 at d8.cc:2662, address = 0x0000000100015150

First v8::base::debug::EnableInProcessStackDumping() is called followed by some windows specific code guarded
by macros. Next is all the options are set using `v8::Shell::SetOptions`

SetOptions will call `v8::V8::SetFlagsFromCommandLine` which is found in src/api.cc:

    i::FlagList::SetFlagsFromCommandLine(argc, argv, remove_flags);

This function can be found in src/flags.cc. The flags themselves are defined in src/flag-definitions.h

Next a new SourceGroup array is create:
    
    options.isolate_sources = new SourceGroup[options.num_isolates];
    SourceGroup* current = options.isolate_sources;
    current->Begin(argv, 1);
    for (int i = 1; i < argc; i++) {
      const char* str = argv[i];

    (lldb) p str
    (const char *) $6 = 0x00007fff5fbfed4d "manual.js"

There are then checks performed to see if the args is `--isolate` or `--module`, or `-e` and if not (like in our case)

    } else if (strncmp(str, "-", 1) != 0) {
      // Not a flag, so it must be a script to execute.
      options.script_executed = true;

TODO: I'm not exactly sure what SourceGroups are about but just noting this and will revisit later.

This will take us back `int Shell::Main` in src/d8.cc

    ::V8::InitializeICUDefaultLocation(argv[0], options.icu_data_file);

    (lldb) p argv[0]
    (char *) $8 = 0x00007fff5fbfed48 "./d8"

See [ICU](international-component-for-unicode) a little more details.

Next the default V8 platform is initialized:

    g_platform = i::FLAG_verify_predictable ? new PredictablePlatform() : v8::platform::CreateDefaultPlatform();

v8::platform::CreateDefaultPlatform() will be called in our case.

We are then back in Main and have the following lines:

    2685 v8::V8::InitializePlatform(g_platform);
    2686 v8::V8::Initialize();

This is very similar to what I've seen in the [Node.js startup process](https://github.com/danbev/learning-nodejs#startint-argc-char-argv).

We did not specify any natives_blob or snapshot_blob as an option on the command line so the defaults 
will be used:

    v8::V8::InitializeExternalStartupData(argv[0]);

back in src/d8.cc line 2918:

    Isolate* isolate = Isolate::New(create_params);

this call will bring us into api.cc line 8185:

     i::Isolate* isolate = new i::Isolate(false);
So, we are invoking the Isolate constructor (in src/isolate.cc).

    isolate->set_snapshot_blob(i::Snapshot::DefaultSnapshotBlob());

api.cc:

    isolate->Init(NULL);
    
    compilation_cache_ = new CompilationCache(this);
    context_slot_cache_ = new ContextSlotCache();
    descriptor_lookup_cache_ = new DescriptorLookupCache();
    unicode_cache_ = new UnicodeCache();
    inner_pointer_to_code_cache_ = new InnerPointerToCodeCache(this);
    global_handles_ = new GlobalHandles(this);
    eternal_handles_ = new EternalHandles();
    bootstrapper_ = new Bootstrapper(this);
    handle_scope_implementer_ = new HandleScopeImplementer(this);
    load_stub_cache_ = new StubCache(this, Code::LOAD_IC);
    store_stub_cache_ = new StubCache(this, Code::STORE_IC);
    materialized_object_store_ = new MaterializedObjectStore(this);
    regexp_stack_ = new RegExpStack();
    regexp_stack_->isolate_ = this;
    date_cache_ = new DateCache();
    call_descriptor_data_ =
      new CallInterfaceDescriptorData[CallDescriptors::NUMBER_OF_DESCRIPTORS];
    access_compiler_data_ = new AccessCompilerData();
    cpu_profiler_ = new CpuProfiler(this);
    heap_profiler_ = new HeapProfiler(heap());
    interpreter_ = new interpreter::Interpreter(this);
    compiler_dispatcher_ =
      new CompilerDispatcher(this, V8::GetCurrentPlatform(), FLAG_stack_size);


src/builtins/builtins.cc, this is where the builtins are defined.
TODO: sort out what these macros do.

In src/v8.cc we have a couple of checks for if the options passed are for a stress_run but since we 
did not pass in any such flags this code path will be followed which will call RunMain:

    result = RunMain(isolate, argc, argv, last_run);

this will end up calling:

    options.isolate_sources[0].Execute(isolate);

Which will call SourceGroup::Execute(Isolate* isolate)

    // Use all other arguments as names of files to load and run.
    HandleScope handle_scope(isolate);
    Local<String> file_name = String::NewFromUtf8(isolate, arg, NewStringType::kNormal).ToLocalChecked();
    Local<String> source = ReadFile(isolate, arg);
    if (source.IsEmpty()) {
      printf("Error reading '%s'\n", arg);
      Shell::Exit(1);
    }
    Shell::options.script_executed = true;
    if (!Shell::ExecuteString(isolate, source, file_name, false, true)) {
      exception_was_thrown = true;
      break;
    }

    ScriptOrigin origin(name);
    if (compile_options == ScriptCompiler::kNoCompileOptions) {
      ScriptCompiler::Source script_source(source, origin);
      return ScriptCompiler::Compile(context, &script_source, compile_options);
    }

Which will delegate to ScriptCompiler(Local<Context>, Source* source, CompileOptions options):

    auto maybe = CompileUnboundInternal(isolate, source, options);

CompileUnboundInternal

    result = i::Compiler::GetSharedFunctionInfoForScript(
        str, name_obj, line_offset, column_offset, source->resource_options,
        source_map_url, isolate->native_context(), NULL, &script_data, options,
        i::NOT_NATIVES_CODE);

src/compiler.cc

    // Compile the function and add it to the cache.
    ParseInfo parse_info(script);
    Zone compile_zone(isolate->allocator(), ZONE_NAME);
    CompilationInfo info(&compile_zone, &parse_info, Handle<JSFunction>::null());


Back in src/compiler.cc-info.cc:

    result = CompileToplevel(&info);

    (lldb) job *result
    0x17df0df309f1: [SharedFunctionInfo]
     - name = 0x1a7f12d82471 <String[0]: >
     - formal_parameter_count = 0
     - expected_nof_properties = 10
     - ast_node_count = 23
     - instance class name = #Object

     - code = 0x1d8484d3661 <Code: BUILTIN>
     - source code = function bajja(a, b, c) {
      var d = c - 100;
      return a + d * b;
    }

    var result = bajja(2, 2, 150);
    print(result);

     - anonymous expression
     - function token position = -1
     - start position = 0
     - end position = 114
     - no debug info
     - length = 0
     - optimized_code_map = 0x1a7f12d82241 <FixedArray[0]>
     - feedback_metadata = 0x17df0df30d09: [FeedbackMetadata]
     - length: 3
     - slot_count: 11
     Slot #0 LOAD_GLOBAL_NOT_INSIDE_TYPEOF_IC
     Slot #2 kCreateClosure
     Slot #3 LOAD_GLOBAL_NOT_INSIDE_TYPEOF_IC
     Slot #5 CALL_IC
     Slot #7 CALL_IC
     Slot #9 LOAD_GLOBAL_NOT_INSIDE_TYPEOF_IC

     - bytecode_array = 0x17df0df30c61


Back in d8.cc:

    maybe_result = script->Run(realm);


src/api.cc

    auto fun = i::Handle<i::JSFunction>::cast(Utils::OpenHandle(this));

    (lldb) job *fun
    0x17df0df30e01: [Function]
     - map = 0x19cfe0003859 [FastProperties]
     - prototype = 0x17df0df043b1
     - elements = 0x1a7f12d82241 <FixedArray[0]> [FAST_HOLEY_ELEMENTS]
     - initial_map =
     - shared_info = 0x17df0df309f1 <SharedFunctionInfo>
     - name = 0x1a7f12d82471 <String[0]: >
     - formal_parameter_count = 0
     - context = 0x17df0df03bf9 <FixedArray[245]>
     - feedback vector cell = 0x17df0df30ed1 Cell for 0x17df0df30e49 <FixedArray[13]>
     - code = 0x1d8484d3661 <Code: BUILTIN>
     - properties = 0x1a7f12d82241 <FixedArray[0]> {
        #length: 0x2c35a5718089 <AccessorInfo> (const accessor descriptor)
        #name: 0x2c35a57180f9 <AccessorInfo> (const accessor descriptor)
        #arguments: 0x2c35a5718169 <AccessorInfo> (const accessor descriptor)
        #caller: 0x2c35a57181d9 <AccessorInfo> (const accessor descriptor)
        #prototype: 0x2c35a5718249 <AccessorInfo> (const accessor descriptor)

      }

    i::Handle<i::Object> receiver = isolate->global_proxy();
    Local<Value> result;
    has_pending_exception = !ToLocal<Value>(i::Execution::Call(isolate, fun, receiver, 0, nullptr), &result);

src/execution.cc

### Zone
Taken directly from src/zone/zone.h:
```
// The Zone supports very fast allocation of small chunks of
// memory. The chunks cannot be deallocated individually, but instead
// the Zone supports deallocating all chunks in one fast
// operation. The Zone is used to hold temporary data structures like
// the abstract syntax tree, which is deallocated after compilation.
```



### V8 flags

    $ ./d8 --help

### d8

    (lldb) br s -f d8.cc -l 2935

    return v8::Shell::Main(argc, argv);

    api.cc:6112
    i::ReadNatives();
    natives-external.cc

### v8::String::NewFromOneByte
So I was a little confused when I first read this function name and thought it
had something to do with the length of the string. But the byte is the type
of the chars that make up the string.
For example, a one byte char would be reinterpreted as uint8_t:

    const char* data

    reinterpret_cast<const uint8_t*>(data)


#### Tasks
* gdbinit has been updated. Check if there is something that should be ported to lldbinit


### Invocation walkthrough 
This section will go through calling a Script to understand what happens in V8.

I'll be using [run-scripts.cc](./run-scripts.cc) as the example for this.

    $ lldb -- ./run-scripts
    (lldb) br s -n main

I'll step through until the following call:

    script->Run(context).ToLocalChecked();

So, Script::Run is defined in api.cc
First things that happens in this function is a macro:

    PREPARE_FOR_EXECUTION_WITH_CONTEXT_IN_RUNTIME_CALL_STATS_SCOPE(
         "v8", 
         "V8.Execute", 
         context, 
         Script, 
         Run, 
         MaybeLocal<Value>(),
         InternalEscapableScope, 
    true);
    TRACE_EVENT_CALL_STATS_SCOPED(isolate, category, name);
    PREPARE_FOR_EXECUTION_GENERIC(isolate, context, class_name, function_name, \
        bailout_value, HandleScopeClass, do_callback);

So, what does the preprocessor replace this with then:

    auto isolate = context.IsEmpty() ? i::Isolate::Current()                               : reinterpret_cast<i::Isolate*>(context->GetIsolate());

I'm skipping TRACE_EVENT_CALL_STATS_SCOPED for now.
`PREPARE_FOR_EXECUTION_GENERIC` will be replaced with:

    if (IsExecutionTerminatingCheck(isolate)) {                        \
      return bailout_value;                                            \
    }                                                                  \
    HandleScopeClass handle_scope(isolate);                            \
    CallDepthScope<do_callback> call_depth_scope(isolate, context);    \
    LOG_API(isolate, class_name, function_name);                       \
    ENTER_V8_DO_NOT_USE(isolate);                                      \
    bool has_pending_exception = false

 


    auto fun = i::Handle<i::JSFunction>::cast(Utils::OpenHandle(this));

    (lldb) job *fun
    0x33826912c021: [Function]
     - map = 0x1d0656c03599 [FastProperties]
     - prototype = 0x338269102e69
     - elements = 0x35190d902241 <FixedArray[0]> [FAST_HOLEY_ELEMENTS]
     - initial_map =
     - shared_info = 0x33826912bc11 <SharedFunctionInfo>
     - name = 0x35190d902471 <String[0]: >
     - formal_parameter_count = 0
     - context = 0x338269102611 <FixedArray[265]>
     - feedback vector cell = 0x33826912c139 <Cell value= 0x33826912c069 <FixedArray[24]>>
     - code = 0x1319e25fcf21 <Code BUILTIN>
     - properties = 0x35190d902241 <FixedArray[0]> {
        #length: 0x2e9d97ce68b1 <AccessorInfo> (const accessor descriptor)
        #name: 0x2e9d97ce6921 <AccessorInfo> (const accessor descriptor)
        #arguments: 0x2e9d97ce6991 <AccessorInfo> (const accessor descriptor)
        #caller: 0x2e9d97ce6a01 <AccessorInfo> (const accessor descriptor)
        #prototype: 0x2e9d97ce6a71 <AccessorInfo> (const accessor descriptor)
     }

The code for i::JSFunction is generated in src/api.h. Lets take a closer look at this.

    #define DECLARE_OPEN_HANDLE(From, To) \
      static inline v8::internal::Handle<v8::internal::To> \
      OpenHandle(const From* that, bool allow_empty_handle = false);

    OPEN_HANDLE_LIST(DECLARE_OPEN_HANDLE)

OPEN_HANDLE_LIST looks like this:

    #define OPEN_HANDLE_LIST(V)                    \
    ....
    V(Script, JSFunction)                        \ 

So lets expand this for JSFunction and it should become:

      static inline v8::internal::Handle<v8::internal::JSFunction> \
        OpenHandle(const Script* that, bool allow_empty_handle = false);

So there will be an function named OpenHandle that will take a const pointer to Script.

A little further down in src/api.h there is another macro which looks like this:

    OPEN_HANDLE_LIST(MAKE_OPEN_HANDLE)

MAKE_OPEN_HANDLE:

    #define MAKE_OPEN_HANDLE(From, To)
      v8::internal::Handle<v8::internal::To> Utils::OpenHandle( 
      const v8::From* that, bool allow_empty_handle) {         
      DCHECK(allow_empty_handle || that != NULL);             
      DCHECK(that == NULL ||                                 
           (*reinterpret_cast<v8::internal::Object* const*>(that))->Is##To());
      return v8::internal::Handle<v8::internal::To>(                         
        reinterpret_cast<v8::internal::To**>(const_cast<v8::From*>(that))); 
      }
And remember that JSFunction is included in the OPEN_HANDLE_LIST so there will
be the following in the source after the preprocessor has processed this header:

      v8::internal::Handle<v8::internal::JSFunction> Utils::OpenHandle( 
        const v8::Script* that, bool allow_empty_handle) {         
          DCHECK(allow_empty_handle || that != NULL);             
          DCHECK(that == NULL ||                                 
           (*reinterpret_cast<v8::internal::Object* const*>(that))->IsJSFunction());
          return v8::internal::Handle<v8::internal::JSFunction>(                               reinterpret_cast<v8::internal::JSFunction**>(const_cast<v8::Script*>(that))); 

So where is JSFunction declared? 
It is defined in objects.h





## Ignition interpreter
User JavaScript also needs to have bytecode generated for them and they also use the C++ DLS
and use the CodeStubAssembler -> CodeAssembler -> RawMachineAssembler just like builtins.

## C++ Domain Specific Language (DLS)

## CodeStubAssembler (CSA)

#### Build failure
After rebasing I've seen the following issue:

    $ ninja -C out/Debug chrome
    ninja: Entering directory `out/Debug'
    ninja: error: '../../chrome/renderer/resources/plugins/plugin_delay.html', needed by 'gen/chrome/grit/renderer_resources.h', missing and no known rule to make it

The "solution" was to remove the out directory and rebuild.

### Tasks
To find suitable task you can use `label:HelpWanted` at [bugs.chromium.org](https://bugs.chromium.org/p/v8/issues/list?can=2&q=label%3AHelpWanted+&colspec=ID+Type+Status+Priority+Owner+Summary+HW+OS+Component+Stars&x=priority&y=owner&cells=ids).


### OpenHandle
What does this call do: 

    Utils::OpenHandle(*(source->source_string));

    OPEN_HANDLE_LIST(MAKE_OPEN_HANDLE)

Which is a macro defined in src/api.h:

    #define MAKE_OPEN_HANDLE(From, To)                                             \
      v8::internal::Handle<v8::internal::To> Utils::OpenHandle(                    \
          const v8::From* that, bool allow_empty_handle) {                         \
      DCHECK(allow_empty_handle || that != NULL);                                \
      DCHECK(that == NULL ||                                                     \
           (*reinterpret_cast<v8::internal::Object* const*>(that))->Is##To()); \
      return v8::internal::Handle<v8::internal::To>(                             \
          reinterpret_cast<v8::internal::To**>(const_cast<v8::From*>(that)));    \
    }

    OPEN_HANDLE_LIST(MAKE_OPEN_HANDLE)

If we take a closer look at the macro is should expand to something like this in our case:

     v8::internal::Handle<v8::internal::To> Utils::OpenHandle(const v8:String* that, false) {
       DCHECK(allow_empty_handle || that != NULL);                                \
       DCHECK(that == NULL ||                                                     \
           (*reinterpret_cast<v8::internal::Object* const*>(that))->IsString()); \
       return v8::internal::Handle<v8::internal::String>(                             \
          reinterpret_cast<v8::internal::String**>(const_cast<v8::String*>(that)));    \
     }

So this is returning a new v8::internal::Handle, the constructor is defined in src/handles.h:95.
     
src/objects.cc
Handle<WeakFixedArray> WeakFixedArray::Add(Handle<Object> maybe_array,
10167                                            Handle<HeapObject> value,
10168                                            int* assigned_index) {
Notice the name of the first parameter `maybe_array` but it is not of type maybe?

### Context
JavaScript provides a set of builtin functions and objects. These functions and objects can be changed by user code. Each context
is separate collection of these objects and functions.

And internal::Context is declared in `deps/v8/src/contexts.h` and extends FixedArray
```console
class Context: public FixedArray {
```

A Context can be create by calling:
```console
const v8::HandleScope handle_scope(isolate_);
Handle<Context> context = Context::New(isolate_,
                                       nullptr,
                                       v8::Local<v8::ObjectTemplate>());
```
`Context::New` can be found in `src/api.cc:6405`:
```c++
Local<Context> v8::Context::New(
    v8::Isolate* external_isolate, v8::ExtensionConfiguration* extensions,
    v8::MaybeLocal<ObjectTemplate> global_template,
    v8::MaybeLocal<Value> global_object,
    DeserializeInternalFieldsCallback internal_fields_deserializer) {
  return NewContext(external_isolate, extensions, global_template,
                    global_object, 0, internal_fields_deserializer);
}
```
The declaration of this function can be found in `include/v8.h`:
```c++
static Local<Context> New(
      Isolate* isolate, ExtensionConfiguration* extensions = NULL,
      MaybeLocal<ObjectTemplate> global_template = MaybeLocal<ObjectTemplate>(),
      MaybeLocal<Value> global_object = MaybeLocal<Value>(),
      DeserializeInternalFieldsCallback internal_fields_deserializer =
          DeserializeInternalFieldsCallback());
```
So we can see the reason why we did not have to specify `internal_fields_deserialize`.
What is `ExtensionConfiguration`?  
This class can be found in `include/v8.h` and only has two members, a count of the extension names 
and an array with the names.

If specified these will be installed by `Boostrapper::InstallExtensions` which will delegate to 
`Genesis::InstallExtensions`, both can be found in `src/boostrapper.cc`.
Where are extensions registered?   
This is done once per process and called from `V8::Initialize()`:
```c++
void Bootstrapper::InitializeOncePerProcess() {
  free_buffer_extension_ = new FreeBufferExtension;
  v8::RegisterExtension(free_buffer_extension_);
  gc_extension_ = new GCExtension(GCFunctionName());
  v8::RegisterExtension(gc_extension_);
  externalize_string_extension_ = new ExternalizeStringExtension;
  v8::RegisterExtension(externalize_string_extension_);
  statistics_extension_ = new StatisticsExtension;
  v8::RegisterExtension(statistics_extension_);
  trigger_failure_extension_ = new TriggerFailureExtension;
  v8::RegisterExtension(trigger_failure_extension_);
  ignition_statistics_extension_ = new IgnitionStatisticsExtension;
  v8::RegisterExtension(ignition_statistics_extension_);
}
```
The extensions can be found in `src/extensions`. You register your own extensions and an example of this
can be found in [test/context_test.cc](./test/context_test.cc).


```console
(lldb) br s -f node.cc -l 4439
(lldb) expr context->length()
(int) $522 = 281
```
This output was taken

Creating a new Context is done by `v8::CreateEnvironment`
```console
(lldb) br s -f api.cc -l 6565
```
```c++
InvokeBootstrapper<ObjectType> invoke;
   6635    result =
-> 6636        invoke.Invoke(isolate, maybe_proxy, proxy_template, extensions,
   6637                      context_snapshot_index, embedder_fields_deserializer);
```
This will later end up in `Snapshot::NewContextFromSnapshot`:
```c++
Vector<const byte> context_data =
      ExtractContextData(blob, static_cast<uint32_t>(context_index));
  SnapshotData snapshot_data(context_data);

  MaybeHandle<Context> maybe_result = PartialDeserializer::DeserializeContext(
      isolate, &snapshot_data, can_rehash, global_proxy,
      embedder_fields_deserializer);
```
So we can see here that the Context is deserialized from the snapshot. What does the Context contain at this stage:
```console
(lldb) expr result->length()
(int) $650 = 281
(lldb) expr result->Print()
// not inlcuding the complete output
```
Lets take a look at an entry:
```console
(lldb) expr result->get(0)->Print()
0xc201584331: [Function] in OldSpace
 - map = 0xc24c002251 [FastProperties]
 - prototype = 0xc201584371
 - elements = 0xc2b2882251 <FixedArray[0]> [HOLEY_ELEMENTS]
 - initial_map =
 - shared_info = 0xc2b2887521 <SharedFunctionInfo>
 - name = 0xc2b2882441 <String[0]: >
 - formal_parameter_count = -1
 - kind = [ NormalFunction ]
 - context = 0xc201583a59 <FixedArray[281]>
 - code = 0x2df1f9865a61 <Code BUILTIN>
 - source code = () {}
 - properties = 0xc2b2882251 <FixedArray[0]> {
    #length: 0xc2cca83729 <AccessorInfo> (const accessor descriptor)
    #name: 0xc2cca83799 <AccessorInfo> (const accessor descriptor)
    #arguments: 0xc201587fd1 <AccessorPair> (const accessor descriptor)
    #caller: 0xc201587fd1 <AccessorPair> (const accessor descriptor)
    #constructor: 0xc201584c29 <JSFunction Function (sfi = 0xc2b28a6fb1)> (const data descriptor)
    #apply: 0xc201588079 <JSFunction apply (sfi = 0xc2b28a7051)> (const data descriptor)
    #bind: 0xc2015880b9 <JSFunction bind (sfi = 0xc2b28a70f1)> (const data descriptor)
    #call: 0xc2015880f9 <JSFunction call (sfi = 0xc2b28a7191)> (const data descriptor)
    #toString: 0xc201588139 <JSFunction toString (sfi = 0xc2b28a7231)> (const data descriptor)
    0xc2b28bc669 <Symbol: Symbol.hasInstance>: 0xc201588179 <JSFunction [Symbol.hasInstance] (sfi = 0xc2b28a72d1)> (const data descriptor)
 }

 - feedback vector: not available
```
So we can see that this is of type `[Function]` which we can cast using:
```
(lldb) expr JSFunction::cast(result->get(0))->code()->Print()
0x2df1f9865a61: [Code]
kind = BUILTIN
name = EmptyFunction
```

```console
(lldb) expr JSFunction::cast(result->closure())->Print()
0xc201584331: [Function] in OldSpace
 - map = 0xc24c002251 [FastProperties]
 - prototype = 0xc201584371
 - elements = 0xc2b2882251 <FixedArray[0]> [HOLEY_ELEMENTS]
 - initial_map =
 - shared_info = 0xc2b2887521 <SharedFunctionInfo>
 - name = 0xc2b2882441 <String[0]: >
 - formal_parameter_count = -1
 - kind = [ NormalFunction ]
 - context = 0xc201583a59 <FixedArray[281]>
 - code = 0x2df1f9865a61 <Code BUILTIN>
 - source code = () {}
 - properties = 0xc2b2882251 <FixedArray[0]> {
    #length: 0xc2cca83729 <AccessorInfo> (const accessor descriptor)
    #name: 0xc2cca83799 <AccessorInfo> (const accessor descriptor)
    #arguments: 0xc201587fd1 <AccessorPair> (const accessor descriptor)
    #caller: 0xc201587fd1 <AccessorPair> (const accessor descriptor)
    #constructor: 0xc201584c29 <JSFunction Function (sfi = 0xc2b28a6fb1)> (const data descriptor)
    #apply: 0xc201588079 <JSFunction apply (sfi = 0xc2b28a7051)> (const data descriptor)
    #bind: 0xc2015880b9 <JSFunction bind (sfi = 0xc2b28a70f1)> (const data descriptor)
    #call: 0xc2015880f9 <JSFunction call (sfi = 0xc2b28a7191)> (const data descriptor)
    #toString: 0xc201588139 <JSFunction toString (sfi = 0xc2b28a7231)> (const data descriptor)
    0xc2b28bc669 <Symbol: Symbol.hasInstance>: 0xc201588179 <JSFunction [Symbol.hasInstance] (sfi = 0xc2b28a72d1)> (const data descriptor)
 }

 - feedback vector: not available
```
So this is the JSFunction associated with the deserialized context. Not sure what this is about as looking at the source code it looks like
an empty function. A function can also be set on the context so I'm guessing that this give access to the function of a context once set.
Where is function set, well it is probably deserialized but we can see it be used in `deps/v8/src/bootstrapper.cc`:
```c++
{
  Handle<JSFunction> function = SimpleCreateFunction(isolate, factory->empty_string(), Builtins::kAsyncFunctionAwaitCaught, 2, false);
  native_context->set_async_function_await_caught(*function);
}
​```console
(lldb) expr isolate()->builtins()->builtin_handle(Builtins::Name::kAsyncFunctionAwaitCaught)->Print()
```

`Context::Scope` is a RAII class used to Enter/Exit a context. Lets take a closer look at `Enter`:
```c++
void Context::Enter() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* isolate = env->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  impl->EnterContext(env);
  impl->SaveContext(isolate->context());
  isolate->set_context(*env);
}
```
So the current context is saved and then the this context `env` is set as the current on the isolate.
`EnterContext` will push the passed-in context (deps/v8/src/api.cc):
```c++
void HandleScopeImplementer::EnterContext(Handle<Context> context) {
  entered_contexts_.push_back(*context);
}
...
DetachableVector<Context*> entered_contexts_;
```
DetachableVector is a delegate/adaptor with some additonaly features on a std::vector.
Handle<Context> context1 = NewContext(isolate);
Handle<Context> context2 = NewContext(isolate);
Context::Scope context_scope1(context1);        // entered_contexts_ [context1], saved_contexts_[isolateContext]
Context::Scope context_scope2(context2);        // entered_contexts_ [context1, context2], saved_contexts[isolateContext, context1]

Now, `SaveContext` is using the current context, not `this` context (`env`) and pushing that to the end of the saved_contexts_ vector.
We can look at this as we entered context_scope2 from context_scope1:


And `Exit` looks like:
```c++
void Context::Exit() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* isolate = env->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  if (!Utils::ApiCheck(impl->LastEnteredContextWas(env),
                       "v8::Context::Exit()",
                       "Cannot exit non-entered context")) {
    return;
  }
  impl->LeaveContext();
  isolate->set_context(impl->RestoreContext());
}
```


#### EmbedderData
A context can have embedder data set on it. Like decsribed above a Context is
internally A FixedArray. `SetEmbedderData` in Context is implemented in `src/api.cc`:
```c++
const char* location = "v8::Context::SetEmbedderData()";
i::Handle<i::FixedArray> data = EmbedderDataFor(this, index, true, location);
i::Handle<i::FixedArray> data(env->embedder_data());
```
`location` is only used for logging and we can ignore it for now.
`EmbedderDataFor`:
```c++
i::Handle<i::Context> env = Utils::OpenHandle(context);
...
i::Handle<i::FixedArray> data(env->embedder_data());
```
We can find `embedder_data` in `src/contexts-inl.h`

```c++
#define NATIVE_CONTEXT_FIELD_ACCESSORS(index, type, name) \
  inline void set_##name(type* value);                    \
  inline bool is_##name(type* value) const;               \
  inline type* name() const;
  NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSORS)
```
And `NATIVE_CONTEXT_FIELDS` in context.h:
```c++
#define NATIVE_CONTEXT_FIELDS(V)                                               \
  V(GLOBAL_PROXY_INDEX, JSObject, global_proxy_object)                         \
  V(EMBEDDER_DATA_INDEX, FixedArray, embedder_data)                            \
...

#define NATIVE_CONTEXT_FIELD_ACCESSORS(index, type, name) \
  void Context::set_##name(type* value) {                 \
    DCHECK(IsNativeContext());                            \
    set(index, value);                                    \
  }                                                       \
  bool Context::is_##name(type* value) const {            \
    DCHECK(IsNativeContext());                            \
    return type::cast(get(index)) == value;               \
  }                                                       \
  type* Context::name() const {                           \
    DCHECK(IsNativeContext());                            \
    return type::cast(get(index));                        \
  }
NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSORS)
#undef NATIVE_CONTEXT_FIELD_ACCESSORS
```
So the preprocessor would expand this to:
```c++
FixedArray embedder_data() const;

void Context::set_embedder_data(FixedArray value) {
  DCHECK(IsNativeContext());
  set(EMBEDDER_DATA_INDEX, value);
}

bool Context::is_embedder_data(FixedArray value) const {
  DCHECK(IsNativeContext());
  return FixedArray::cast(get(EMBEDDER_DATA_INDEX)) == value;
}

FixedArray Context::embedder_data() const {
  DCHECK(IsNativeContext());
  return FixedArray::cast(get(EMBEDDER_DATA_INDEX));
}
```
We can take a look at the initial data:
```console
lldb) expr data->Print()
0x2fac3e896439: [FixedArray] in OldSpace
 - map = 0x2fac9de82341 <Map(HOLEY_ELEMENTS)>
 - length: 3
         0-2: 0x2fac1cb822e1 <undefined>
(lldb) expr data->length()
(int) $5 = 3
```
And after setting:
```console
(lldb) expr data->Print()
0x2fac3e896439: [FixedArray] in OldSpace
 - map = 0x2fac9de82341 <Map(HOLEY_ELEMENTS)>
 - length: 3
           0: 0x2fac20c866e1 <String[7]: embdata>
         1-2: 0x2fac1cb822e1 <undefined>

(lldb) expr v8::internal::String::cast(data->get(0))->Print()
"embdata"
```
This was taken while debugging [ContextTest::EmbedderData](./test/context_test.cc).

### ENTER_V8_FOR_NEW_CONTEXT
This macro is used in `CreateEnvironment` (src/api.cc) and the call in this function looks like this:
```c++
ENTER_V8_FOR_NEW_CONTEXT(isolate);
```


### Factory::NewMap
This section will take a look at the following call:
```c++
i::Handle<i::Map> map = factory->NewMap(i::JS_OBJECT_TYPE, 24);
```

Lets take a closer look at this function which can be found in `src/factory.cc`:
```
Handle<Map> Factory::NewMap(InstanceType type, int instance_size,
                            ElementsKind elements_kind,
                            int inobject_properties) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateMap(type, instance_size, elements_kind,
                                     inobject_properties),
      Map);
}

```
If we take a look at factory.h we can see the default values for elements_kind and inobject_properties:
```c++
Handle<Map> NewMap(InstanceType type, int instance_size,
                     ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
                     int inobject_properties = 0);
```
If we expand the CALL_HEAP_FUNCTION macro we will get:
```c++
    AllocationResult __allocation__ = isolate()->heap()->AllocateMap(type,
                                                                     instance_size,
                                                                     elements_kind,
                                                                     inobject_properties),
    Object* __object__ = nullptr;
    RETURN_OBJECT_UNLESS_RETRY(isolate(), Map)
    /* Two GCs before panicking.  In newspace will almost always succeed. */
    for (int __i__ = 0; __i__ < 2; __i__++) {
      (isolate())->heap()->CollectGarbage(
          __allocation__.RetrySpace(),
          GarbageCollectionReason::kAllocationFailure);
      __allocation__ = FUNCTION_CALL;
      RETURN_OBJECT_UNLESS_RETRY(isolate, Map)
    }
    (isolate())->counters()->gc_last_resort_from_handles()->Increment();
    (isolate())->heap()->CollectAllAvailableGarbage(
        GarbageCollectionReason::kLastResort);
    {
      AlwaysAllocateScope __scope__(isolate());
    t __allocation__ = isolate()->heap()->AllocateMap(type,
                                                      instance_size,
                                                      elements_kind,
                                                      inobject_properties),
    }
    RETURN_OBJECT_UNLESS_RETRY(isolate, Map)
    /* TODO(1181417): Fix this. */
    v8::internal::Heap::FatalProcessOutOfMemory("CALL_AND_RETRY_LAST", true);
    return Handle<Map>();
```
So, lets take a look at `isolate()->heap()->AllocateMap` in 'src/heap/heap.cc':
```c++
  HeapObject* result = nullptr;
  AllocationResult allocation = AllocateRaw(Map::kSize, MAP_SPACE);
```
`AllocateRaw` can be found in src/heap/heap-inl.h:
```c++
  bool large_object = size_in_bytes > kMaxRegularHeapObjectSize;
  HeapObject* object = nullptr;
  AllocationResult allocation;
  if (NEW_SPACE == space) {
    if (large_object) {
      space = LO_SPACE;
    } else {
      allocation = new_space_->AllocateRaw(size_in_bytes, alignment);
      if (allocation.To(&object)) {
        OnAllocationEvent(object, size_in_bytes);
      }
      return allocation;
    }
  }
 } else if (MAP_SPACE == space) {
    allocation = map_space_->AllocateRawUnaligned(size_in_bytes);
 }

```
```console
(lldb) expr large_object
(bool) $3 = false
(lldb) expr size_in_bytes
(int) $5 = 80
(lldb) expr map_space_
(v8::internal::MapSpace *) $6 = 0x0000000104700f60
```
`AllocateRawUnaligned` can be found in `src/heap/spaces-inl.h`
```c++
  HeapObject* object = AllocateLinearly(size_in_bytes);
```

### v8::internal::Object
Is an abstract super class for all classes in the object hierarch and both Smi and HeapObject
are subclasses of Object so there are no data members in object only functions.
For example:
```
  bool IsObject() const { return true; }
  INLINE(bool IsSmi() const
  INLINE(bool IsLayoutDescriptor() const
  INLINE(bool IsHeapObject() const
  INLINE(bool IsPrimitive() const
  INLINE(bool IsNumber() const
  INLINE(bool IsNumeric() const
  INLINE(bool IsAbstractCode() const
  INLINE(bool IsAccessCheckNeeded() const
  INLINE(bool IsArrayList() const
  INLINE(bool IsBigInt() const
  INLINE(bool IsUndefined() const
  INLINE(bool IsNull() const
  INLINE(bool IsTheHole() const
  INLINE(bool IsException() const
  INLINE(bool IsUninitialized() const
  INLINE(bool IsTrue() const
  INLINE(bool IsFalse() const
  ...
```

### v8::internal::Smi
Extends v8::internal::Object and are not allocated on the heap. There are no members as the
pointer itself is used to store the information.

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

### Heap
Lets take a look when the heap is constructed by using test/heap_test and setting a break
point in Heap's constructor. 
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

The Isolate class extends HiddenFactory:
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

```console
Young Generation                  Old Generation
+-------------+------------+  +-----------+-------------+
| scavenger   |            |  | map space | old object  |
+-------------+------------+  +-----------+-------------+
```
The `map space` contains only map objects.
The old and map spaces consist of list of pages. 
The Page class can be found in `src/heap/spaces.h`
```c++
class Page : public MemoryChunk {
}
```


In our case the calling v8::Isolate::New which is done by the test fixture: 
```c++
virtual void SetUp() {
  isolate_ = v8::Isolate::New(create_params_);
}
```
This will call: 
```c++
Isolate* Isolate::New(const Isolate::CreateParams& params) {
  Isolate* isolate = Allocate();
  Initialize(isolate, params);
  return isolate;
}
```
In `Isolate::Initialize` we'll call `i::Snapshot::Initialize(i_isolate)`:
```c++
if (params.entry_hook || !i::Snapshot::Initialize(i_isolate)) {
  ...
```
Which will call:
```c++
bool success = isolate->Init(&deserializer);
```
Before this call all the roots are uninitialized. Reading this [blog](https://v8project.blogspot.com/) it says that
the Isolate class contains a roots table. It looks to me that the Heap contains this data structure but perhaps that
is what they meant. 
```console
(lldb) bt 3
* thread #1, queue = 'com.apple.main-thread', stop reason = step over
  * frame #0: 0x0000000101584f43 libv8.dylib`v8::internal::StartupDeserializer::DeserializeInto(this=0x00007ffeefbfe200, isolate=0x000000010481cc00) at startup-deserializer.cc:39
    frame #1: 0x0000000101028bb6 libv8.dylib`v8::internal::Isolate::Init(this=0x000000010481cc00, des=0x00007ffeefbfe200) at isolate.cc:3036
    frame #2: 0x000000010157c682 libv8.dylib`v8::internal::Snapshot::Initialize(isolate=0x000000010481cc00) at snapshot-common.cc:54
```
In `startup-deserializer.cc` we can find `StartupDeserializer::DeserializeInto`:
```c++
  DisallowHeapAllocation no_gc;
  isolate->heap()->IterateSmiRoots(this);
  isolate->heap()->IterateStrongRoots(this, VISIT_ONLY_STRONG);
```
After 
If we take a look in `src/roots.h` we can find the read-only roots in Heap. If we take the 10 value, which is:
```c++
V(String, empty_string, empty_string)                                        \
```
we can then inspect this value:
```console
(lldb) expr roots_[9]
(v8::internal::Object *) $32 = 0x0000152d30b82851
(lldb) expr roots_[9]->IsString()
(bool) $30 = true
(lldb) expr roots_[9]->Print()
#
```
So this entry is a pointer to objects on the managed heap which have been deserialized from the snapshot.

The heap class has a lot of members that are initialized during construction by the body of the constructor looks like this:
```c++
{
  // Ensure old_generation_size_ is a multiple of kPageSize.
  DCHECK_EQ(0, max_old_generation_size_ & (Page::kPageSize - 1));

  memset(roots_, 0, sizeof(roots_[0]) * kRootListLength);
  set_native_contexts_list(nullptr);
  set_allocation_sites_list(Smi::kZero);
  set_encountered_weak_collections(Smi::kZero);
  // Put a dummy entry in the remembered pages so we can find the list the
  // minidump even if there are no real unmapped pages.
  RememberUnmappedPage(nullptr, false);
}
```
We can see that roots_ is filled with 0 values. We can inspect `roots_` using:
```console
(lldb) expr roots_
(lldb) expr RootListIndex::kRootListLength
(int) $16 = 509
```
Now they are all 0 at this stage, so when will this array get populated?  
These will happen in `Isolate::Init`:
```c++
  heap_.SetUp()
  if (!create_heap_objects) des->DeserializeInto(this);

void StartupDeserializer::DeserializeInto(Isolate* isolate) {
-> 17    Initialize(isolate);
startup-deserializer.cc:37

isolate->heap()->IterateSmiRoots(this);
```

This will delegate to `ConfigureHeapDefaults()` which will call Heap::ConfigureHeap:
```c++
enum RootListIndex {
  kFreeSpaceMapRootIndex,
  kOnePointerFillerMapRootIndex,
  ...
}
```

```console
(lldb) expr heap->RootListIndex::kFreeSpaceMapRootIndex
(int) $3 = 0
(lldb) expr heap->RootListIndex::kOnePointerFillerMapRootIndex
(int) $4 = 1
```

### MemoryChunk
Found in `src/heap/spaces.h` an instace of a MemoryChunk represents a region in memory that is 
owned by a specific space.


### Embedded builtins
In the [blog post](https://v8project.blogspot.com/) explains how the builtins are embedded into the executable
in to the .TEXT section which is readonly and therefore can be shared amoung multiple processes. We know that
builtins are compiled and stored in the snapshot but now it seems that the are instead placed in to `out.gn/learning/gen/embedded.cc` and the combined with the object files from the compile to produce the libv8.dylib.
V8 has a configuration option named `v8_enable_embedded_builtins` which which case `embedded.cc` will be added 
to the list of sources. This is done in `BUILD.gn` and the `v8_snapshot` target. If `v8_enable_embedded_builtins` is false then `src/snapshot/embedded-empty.cc` will be included instead. Both of these files have the following functions:
```c++
const uint8_t* DefaultEmbeddedBlob()
uint32_t DefaultEmbeddedBlobSize()

#ifdef V8_MULTI_SNAPSHOTS
const uint8_t* TrustedEmbeddedBlob()
uint32_t TrustedEmbeddedBlobSize()
#endif
```
These functions are used by `isolate.cc` and declared `extern`:
```c++
extern const uint8_t* DefaultEmbeddedBlob();
extern uint32_t DefaultEmbeddedBlobSize();
```
And the usage of `DefaultEmbeddedBlob` can be see in Isolate::Isolate where is sets the embedded blob:
```c++
SetEmbeddedBlob(DefaultEmbeddedBlob(), DefaultEmbeddedBlobSize());
```
Lets set a break point there and see if this is empty of not.
```console
(lldb) expr v8_embedded_blob_size_
(uint32_t) $0 = 4021088
```
So we can see that we are not using the empty one. Isolate::SetEmbeddedBlob

We can see in `src/snapshot/deserializer.cc` (line 552) we have a check for the embedded_blob():
```c++
  CHECK_NOT_NULL(isolate->embedded_blob());
  EmbeddedData d = EmbeddedData::FromBlob();
  Address address = d.InstructionStartOfBuiltin(builtin_index);
```
`EmbeddedData can be found in `src/snapshot/snapshot.h` and the implementation can be found in snapshot-common.cc.
```c++
Address EmbeddedData::InstructionStartOfBuiltin(int i) const {
  const struct Metadata* metadata = Metadata();
  const uint8_t* result = RawData() + metadata[i].instructions_offset;
  return reinterpret_cast<Address>(result);
}
```
```console
(lldb) expr *metadata
(const v8::internal::EmbeddedData::Metadata) $7 = (instructions_offset = 0, instructions_length = 1464)
```
```c++
  struct Metadata {
    // Blob layout information.
    uint32_t instructions_offset;
    uint32_t instructions_length;
  };
```
```console
(lldb) expr *this
(v8::internal::EmbeddedData) $10 = (data_ = "\xffffffdc\xffffffc0\xffffff88'"y[\xffffffd6", size_ = 4021088)
(lldb) expr metadata[i]
(const v8::internal::EmbeddedData::Metadata) $8 = (instructions_offset = 0, instructions_length = 1464)
```
So, is it possible for us to verify that this information is in the .text section?
```console
(lldb) expr result
(const uint8_t *) $13 = 0x0000000101b14ee0 "UH\x89�jH\x83�(H\x89U�H�\x16H\x89}�H�u�H�E�H\x89U�H\x83�
(lldb) image lookup --address 0x0000000101b14ee0 --verbose
      Address: libv8.dylib[0x00000000019cdee0] (libv8.dylib.__TEXT.__text + 27054464)
      Summary: libv8.dylib`v8_Default_embedded_blob_ + 7072
       Module: file = "/Users/danielbevenius/work/google/javascript/v8/out.gn/learning/libv8.dylib", arch = "x86_64"
       Symbol: id = {0x0004b596}, range = [0x0000000101b13340-0x0000000101ee8ea0), name="v8_Default_embedded_blob_"
```
So what we have is a pointer to the .text segment which is returned:
```console
(lldb) memory read -f x -s 1 -c 13 0x0000000101b14ee0
0x101b14ee0: 0x55 0x48 0x89 0xe5 0x6a 0x18 0x48 0x83
0x101b14ee8: 0xec 0x28 0x48 0x89 0x55
```
And we can compare this with `out.gn/learning/gen/embedded.cc`:
```c++
V8_EMBEDDED_TEXT_HEADER(v8_Default_embedded_blob_)
__asm__(
  ...
  ".byte 0x55,0x48,0x89,0xe5,0x6a,0x18,0x48,0x83,0xec,0x28,0x48,0x89,0x55\n"
  ...
);
```
The macro `V8_EMBEDDED_TEXT_HEADER` can be found `src/snapshot/macros.h`:
```c++
#define V8_EMBEDDED_TEXT_HEADER(LABEL)         \
  __asm__(V8_ASM_DECLARE(#LABEL)               \
          ".csect " #LABEL "[DS]\n"            \
          #LABEL ":\n"                         \
          ".llong ." #LABEL ", TOC[tc0], 0\n"  \
          V8_ASM_TEXT_SECTION                  \
          "." #LABEL ":\n");

define V8_ASM_DECLARE(NAME) ".private_extern " V8_ASM_MANGLE_LABEL NAME "\n"
#define V8_ASM_MANGLE_LABEL "_"
#define V8_ASM_TEXT_SECTION ".csect .text[PR]\n"
```
And would be expanded by the preprocessor into:
```c++
  __asm__(".private_extern " _ v8_Default_embedded_blob_ "\n"
          ".csect " v8_Default_embedded_blob_ "[DS]\n"
          v8_Default_embedded_blob_ ":\n"
          ".llong ." v8_Default_embedded_blob_ ", TOC[tc0], 0\n"
          ".csect .text[PR]\n"
          "." v8_Default_embedded_blob_ ":\n");
  __asm__(
    ...
    ".byte 0x55,0x48,0x89,0xe5,0x6a,0x18,0x48,0x83,0xec,0x28,0x48,0x89,0x55\n"
    ...
  );

```

Back in `src/snapshot/deserialzer.cc` we are on this line:
```c++
  Address address = d.InstructionStartOfBuiltin(builtin_index);
  CHECK_NE(kNullAddress, address);
  if (RelocInfo::OffHeapTargetIsCodedSpecially()) {
    // is false in our case so skipping the code here
  } else {
    MaybeObject* o = reinterpret_cast<MaybeObject*>(address);
    UnalignedCopy(current, &o);
    current++;
  }
  break;
```
