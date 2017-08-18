### Learning Google V8
The sole purpose of this project is to aid me in leaning Google's V8 JavaScript engine.


## Contents
1. [Building](#building-v8)
2. [Contributing a change](#contributing-a-change)
3. [Debugging](#debugging)
4. [Introduction](#introduction)
5. [Inline caches](#inline-caches)
6. [Small Integers](#small-integers)
7. [Building chromium](#building-chromium)
8. [Compiler pipeline](#compiler-pipeline)

## Building V8
You'll need to have checked out the Google V8 sources to you local file system and build it by following 
the instructions found [here](https://developers.google.com/v8/build).

### [gclient](https://www.chromium.org/developers/how-tos/depottools) sync

    gclient sync

What I've been using the following target:

    $ make x64.debug

You should then be able to find the output in `out/x64.debug/`

To run the tests:

    $ make x64.check

After this has been done you can set an environment variable named `V8_HOME` which points to the the checked
out v8 directory. For example, :

    $ export V8_HOME=~/work/google/javascript/v8

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

    $ gn gen out/Debug

### Building using Ninja

    $ ninja -C out/Debug chrome

Building the tests:

    $ ninja -C out/Debug chrome/test:unit_tests

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

### GN

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

These tests can be run using:

    $ make check

## Building this projects code

    $ make

## Running

    $ ./hello-world

## Cleaning

    $ make clean

## Contributing a change to V8
1) Create a working branch as usual and fix/build/test etc.  
2) Login to https://codereview.chromium.org/mine  
3) depot-tools-auth login https://codereview.chromium.org  
3) git cl upload  

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
Create a file named [.lldbinit](./.lldbinit) (in your project director or home directory)

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
    | |                                 |      |          |                              |     |
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

To see where these bin files are used lets step through and see:

    $ . ./setenv.sh

The above is only required once per shell. TODO: fix this using rpath or something.

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

#### Inline Caches
--trace-ic

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

### Local

```c++
Local<String> script_name = ...;
```
So what is script_name. Well it is an object reference that is managed by the v8 GC.
The GC needs to be able to move things (pointers around) and also track if things should be GC'd. Local handles
as opposed to persistent handles are light weight and mostly used local operations. These handles are managed by
HandleScopes so you must have a handlescope on the stack and the local is only valid as long as the handlescope is
valid. You can find the available operations for a Local in `include/v8.h`.

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

The above can be found in src/api.h. The same goes for Local<Object>, Local<String> etc.


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


### Types
Most types can be found in src/objects.h

    // Formats of Object*:
    //  Smi:        [31 bit signed int] 0
    //  HeapObject: [32 bit direct pointer] (4 byte aligned) | 01

### v8::PersistentObject 


### Hidden classes
There is plenty of information about how these work but I was not aware that d8 could be used
with the `--trace-maps`` flag to show information about hidden classes (which are called maps in V8)

    function Person(name, age) {
      this.name = name;
      this.age = age;
    }

    const p = new Person("Daniel", 41);


    $ ./d8 --trace-maps class.js

    [TraceMaps: InitialMap map= 0x37facd30afa1 SFI= 34_Person ]
    [TraceMaps: Transition from= 0x37facd30afa1 to= 0x37facd30b0a9 name= name ]
    [TraceMaps: Transition from= 0x37facd30b0a9 to= 0x37facd30b101 name= age ]


### Tasks 

No space between these declarations:

    3322   /**
    3323    * Returns zero based line number of function body and
    3324    * kLineOffsetNotFound if no information available.
    3325    */
    3326   int GetScriptLineNumber() const;
    3327   /**
    3328    * Returns zero based column number of function body and
    3329    * kLineOffsetNotFound if no information available.
    3330    */
    3331   int GetScriptColumnNumber() const;


    3435 /**
    3436  * An instance of the built-in Proxy constructor (ECMA-262, 6th Edition,
    3437  * 26.2.1).
    3438  */
    3439 class V8_EXPORT Proxy : public Object {
    3440  public:
    3441   Local<Object> GetTarget();
    3442   Local<Value> GetHandler();
    3443   bool IsRevoked();
    3444   void Revoke();
    3445
    3446   /**
    3447    * Creates a new empty Map. <-- a Map???
    3448    */
    3449   static MaybeLocal<Proxy> New(Local<Context> context,
    3450                                Local<Object> local_target,
    3451                                Local<Object> local_handler);



#### Testing

    $ out/Default/unit_tests --gtest_filter="PushClientTest.*"


#### Using a specific version of V8
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
    
You'll have to run `gclient sync` after this too.


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
    
#### Build failure
After rebasing I've seen the following issue:

    $ ninja -C out/Debug chrome
    ninja: Entering directory `out/Debug'
    ninja: error: '../../chrome/renderer/resources/plugins/plugin_delay.html', needed by 'gen/chrome/grit/renderer_resources.h', missing and no known rule to make it

The "solution" was to remove the out directory and rebuild.

### Tasks
To find suitable task you can use `label:HelpWanted` at [bugs.chromium.org](https://bugs.chromium.org/p/v8/issues/list?can=2&q=label%3AHelpWanted+&x=priority&y=owner&cells=ids).

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
can be overwritten. Subsequent call for the same type can jump directly to the cached method and
completely avoid the lookup. The prolog of the called method must verify that the receivers
type has not changed and do the lookup if it has changed (the type if incorrect, no longer A for
example).

The target methods address is stored in the callers code, or "inline" with the callers code, 
hence the name "inline cache".

#### Polymorfic Inline cache (PIC)
A polymorfic call site is one where there are many equally likely receiver types (and thus
call targets).

- Monomorfic means there is onle one receiver type
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
stub. The stub will check if the receiver is of a type that it was seen before and branch to 
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


### gn
GN is a bulid system that generated Ninja Build files. 
In [src/gn](./src/gn) you can find an example project that uses gn. It is very basic and the 
intention is to have something to "play" with while learning how it works.


##### Could not find checkout in any parent issue
When I first tried to run gn in the src directory I got the following error:

    $ gn gen out/mybuild
    gn.py: Could not find checkout in any parent of the current path.
    This must be run inside a checkout. 


When gn starts it will look for a file named .gn, starting from the current directory and
continuing up the the parent directories. This file indicates the source root.
    
### Ninja


### What is a zone
I noticed that there is a `src/zone.h` and wondered what this is all about. Is this related
to zone.js in any way?  
No, this is not related but instead deals with memory allocations.

### Performance Optimizations
Code is optimized 1 function at a time, without knowledge of what other code is doing


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

### Promises

    (lldb) br s -f builtins-promise.cc -l 842


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
it's only task is to generate machine code as quickly as possible.

    Source ------> Parser  --------> Full-codegen ---------> Unoptimized Machine Code

So the whole script is parsed even though we only generated code for the top-level code. The pre-parse (the syntax checking)
was not stored in any way. The functions are lazy stubs that when/if the function gets called the function get compiled. This
means that the function has to be parsed (again, the first time was the pre-parse remember).
If a function is determined to be hot it will be optimized by one of the two optimizing compilers crankshaft for older parts oof JavaScript or Turbofan for Web Assembly (WASM) and some of the newer es6 features. 

The first time V8 sees a function it will parse it into an AST but not do any further processing of that tree
until that function is used. Processing will be running the full-codegen compiler.

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
     47 S> 0x2eef8d9b1046 @    8 : 1e 03             Ldar a1        // LoaD accumulator from Register argument from a1 which is b
     60 E> 0x2eef8d9b1048 @   10 : 2c fa 03          Mul r0, [3]    // multiply that is our local variable in r0
     56 E> 0x2eef8d9b104b @   13 : 2a 04 04          Add a0, [4]    // add that to our argument register 0 which is a 
     65 S> 0x2eef8d9b104e @   16 : 83                Return         // return the value in the accumulator?


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

 
### Execution/Runtime
I'm not sure if V8 follows this exactly but I've heard and read that when the engine comes 
across a function declaration it only parses and verifies the syntax and saves a ref
to the function name. The statements inside the function are not checked at this stage
only the syntax of the function declaration (parenthesis, arguments, brackets etc). 


### Function methods
The declaration of Function can be found in `include/v8.h` (just noting this as I've looked for it several times)


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

Do not that v8::String is declared in include/v8.h

`Name` as can be seen extends HeapObject and anything that can be used as a property name should extend Name.
Looking at the declaration in include/v8.h we find the following:

    int GetIdentityHash();
    static Name* Cast(Value* obj)

#### String
A String extends Name and has a length and content. The content can be made up of 1 or 2 byte characters.
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
Looking at the functions I've get to see one that returns the actual bytes 
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

         +--------------+
         |              | 
   [str|one|two]     [one|...]   [two|...]
             |                       |
             +-----------------------+

So we can see that one and two in str are pointer so existing strings. 


#### ExternalString
These Strings located on the native heap. The ExternalString structure has a pointer to this external location and the usual length field for all Strings.


Looking at `String` I was not able to find any construtor for it, nor the other subtypes.
