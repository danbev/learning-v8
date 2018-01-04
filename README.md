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
consuming and affect runtime performance if this has to be done every time. The blobs above are prepared
snapshots that get directly deserialized into the heap to provide an initilized context.

Now this is where the files `natives_blob.bin` and `snapshot_blob.bin` come into play. But what are these bin files?  
If you take a look in src/js you'll find a number of javascript files. These files referenced in src/v8.gyp and are used
by the target `js2c`. This target calls tools/js2c.py which is a tool for converting
JavaScript source code into C-Style char arrays. This target will process all the library_files specified in the variables section.
For a GN build you'll find the configuration in BUILD.GN.

The output of this out/Debug/obj/gen/libraries.cc. So how is this file actually used?
The `js2c` target produces the libraries.cc file which is used by other targets, for example by `v8_snapshot` which produces a 
snapshot_blob.bin file.

    $ lldb hello_world
    (lldb) br s -n main
    (lldb) r

Step through to the following line:

    V8::InitializeExternalStartupData(argv[0]);

This call will land us in src/api.cc:

    void v8::V8::InitializeExternalStartupData(const char* directory_path) {
      i::InitializeExternalStartupData(directory_path);
    }

The implementation of `InitializeExternalStartupData` can be found in src/startup-data-util.cc:

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

So we can see that if V8_HAS_ATTRIBUTE_VISIBILITY and defined(V8_SHARED) and also 
if BUILDING_V8_SHARED V8_EXPORT is set to `__attribute__ ((visibility("default"))`.
But in all other cases V8_EXPORT is empty and the preprocessor does not insert 
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
|propertyN  |              | property1       |         | elementN       |
+-----------+              +-----------------+         +----------------+
                           | property2       |
                           +-----------------+
                           | ...             |
                           +-----------------+

```

#### JSObject
Each JSObject has as its first field a pointer to the generated HiddenClass. A hiddenclass contain mappings from property names to indices into the properties data type. When an instance of JSObject is created a `Map` is passed in. 
As mentioned earlier JSObject inherits from JSReceiver which inherits from HeapObject

For example,in [jsobject_test.cc](./tests/jsobject_test.cc) we first create a new Map using the internal Isolate Factory:

    v8::internal::Handle<v8::internal::Map> map = factory->NewMap(v8::internal::JS_OBJECT_TYPE, 24);
    v8::internal::Handle<v8::internal::JSObject> js_object = factory->NewJSObjectFromMap(map);
    EXPECT_TRUE(js_object->HasFastProperties());

When we call `js_object->HasFastProperties()` this will delegate to the map instance: 

    return !map()->is_dictionary_map();

How do you add a property to a JSObject instance?  
Take a look at [jsobject_test.cc](./tests/jsobject_test.cc) for an example.


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
monomorphic state is specified with a `1. These states can be found in [src/ic/ic.cc](https://github.com/v8/v8/blob/df1494d69deab472a1a709bd7e688297aa5cc655/src/ic/ic.cc#L33-L52).
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

If a function is determined to be hot it will be optimized by one of the two optimizing compilers crankshaft for older parts oof JavaScript or Turbofan for Web Assembly (WASM) and some of the newer es6 features. 

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

There is a [unit test](./tests/ast_test.cc) that shows how a ParseInfo instance can be created
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

First have have the main function which does not have a name:

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

    Handler Table (size = 16)

* LdaConstant <idx> 
Load the constant at index from the constant pool into the accumulator.  
* Star <dst>
Store the contents of the accumulator register in dst.  
* Ldar <src>
Load accumulator with value from register src.  
* LdaGlobal <idx> <slot>
Load the constant at index from the constant pool into the accumulator.  
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

Example usages can be found in [tests/string_test.cc](./tests/string_test.cc).
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

So we can see that base is our Handle to a JSObject, and name is "console".
Buildtins::Name is is Builtins:kConsoleDebug. Where is this defined?  
You can find a macro named `CPP` in `src/builtins/builtins-definitions.h`:

   CPP(ConsoleDebug)

What does this macro expand to?  
It is part of the `BUILTIN_LIST_BASE` macro in builtin-definitions.h
We have to look at where BUILTIN_LIST is used with we can find in builtins.cc.
In builtins.cc we have an array of BuiltinMetadata which is declared as:

    const BuiltinMetadata builtin_metadata[] = {
      BUILTIN_LIST(DECL_CPP, DECL_API, DECL_TFJ, DECL_TFC, DECL_TFS, DECL_TFH,
                  DECL_ASM)
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

So back to looking at the arguments to SimpleInstallFunction which looks like this:

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
src/objects.h.

So we have returned from SimpleInstallFunction and are back in


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
Is a macro to defining Turbofan (TF) builtins and can be found in builtins/builtins-utils-gen.h

    TF_BUILTIN(BeveStringLength, StringBuiltinsAssembler) {
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
The tests directory contains unit tests for individual classes/concepts in V8 to help understand them.

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

