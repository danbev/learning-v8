### Learning Google V8
The sole purpose of this project is to aid me in leaning Google's V8 JavaScript engine.

## Prerequisites
You'll need to have checked out the Google V8 sources to you local file system and build it by following 
the instructions found [here](https://developers.google.com/v8/build).

### [gclient](https://www.chromium.org/developers/how-tos/depottools) sync

    gclient sync

What I've been using the following target:

    $ make x64.debug

To run the tests:

    $ make x64.check

After this has been done you can set an environment variable named `V8_HOME` which points to the the checked
out v8 directory. For example, :

    $ export V8_HOME=~/work/google/javascript/v8

## Building

    $ make

## Running

    $ ./hello-world

## Cleaning

    $ make clean

## Contributing a change
1) Create a working branch as usual and fix/build/test etc.  
2) Login to https://codereview.chromium.org/mine  
3) depot-tools-auth login https://codereview.chromium.org  
3) git cl upload  

See Googles [contributing-code](https://www.chromium.org/developers/contributing-code) for more details.

### Find the current issue number

    $ git cl issue


## Debugging

    $ lldb hello-world
    (lldb) breatpoint set --file hello-world.cc --line 27

### Local<String>

```c++
Local<String> script_name = ...;
```
So what is script_name. Well it is an object reference that is managed by the v8 GC.
The GC needs to be able to move things (pointers around) and also track if things should be GC'd

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

So we can see that if V8_HAS_ATTRIBUTE_VISIBILITY and defined(V8_SHARD) and also 
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
Smi stands for small integers. It turns out that ECMA Number is defined as 64-bit binary double-precision
but internally v8 uses 32-bit to represent all values. How can that work, you can represent a 64-bit value
using only 32-bits right?   
Instead the small integer is represented by the 32 bits plus a pointer to the 64-bit number. v8 needs to
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


## Building chromium
When making changes to V8 you might need to verify that your changes have not broken anything in Chromium. 

Generate Your Project (gpy) :
You'll have to run this once before building:

    $ gclient sync
    $ gclient runhooks

GN bulid:

    $ gn gen out/Debug

#### Building

    $ ninja -C out/Debug chrome

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


#### Testing

    $ out/Default/unit_tests --gtest_filter="PushClientTest.*"


#### Using a specific version of V8
So, we want to include our updated version of V8 so that we can verify that it builds correctly with our change to V8.
While I'm not sure this is the proper way to do it, I was able to update DEPS in src (chromium) and set
the v8 entry to git@github.com:danbev/v8.git@064718a8921608eaf9b5eadbb7d734ec04068a87:

    "git@github.com:danbev/v8.git@064718a8921608eaf9b5eadbb7d734ec04068a87"

You'll have to run `gclient sync` after this. 


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
Using a lookup cache as described about still takes a considerable amount of time since the
cache must be probed for each message. It can be observed that the type of the target does often
not vary. If a call to type A is done at a particular call site it is very likely that the next
time it is called the type will also be A.

The method address looked up by the system lookup routine can be cached and the call instruction
can be overwritten. Subsequent call for the same type can jump directly to the cached method and
completely avoid the lookup. The prolog of the called method must verify that the receivers
type has not changed do the lookup if it has changed (the type if incorrect, no longer A for
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
If a differenty type exists in the list there will be a cache miss in the prolog and the lookup
routine called. In normal inline caching this would rebind the call, replace the call to this
types target method. This would happen each time the type changes.

With PIC the cache miss handler will generate a small stub routine and rebinds the call to this
stub. The stub will check if the receiver is of a type that it was seen before and branch to 
the correct targets. Since the type of the target is already know at this point it can directly
branch to the target method body without the need for the prolog.
If the type has not been seen before it will be added to the stub to handle that type. Eventually
the stub will contain all types used and there will be no more cache misses/lookups.

The problem is that we don't have type information so methods cannot be called directly, but 
instead be looked up. In a static language a virtual table might have been used. In JavaScript
is no inheritance relationship so it is not possible to know a vtable offset ahead of time.
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


#### Configuration

#### Building the project



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
