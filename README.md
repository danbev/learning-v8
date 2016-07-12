### Learning Google V8
The sole purpose of this project is to aid me in leaning Google's V8 JavaScript engine

## Prerequisites
You'll need to have checked out the Google V8 Sources to you local file system and build it by following 
the instructions found [here](https://developers.google.com/v8/build).

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
It is a preprocessor macro with look like this:

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
if BUILDING_V8_SHARED V8_EXPORT is set to __attribute__ ((visibility("default")).
But in all other cases V8_EXPORT is empty and the preprocessor does not insert 
anything (nothing will be there come compile time). 
But what about the `__attribute__ ((visibility("default"))` what is this?  

In the GNU compiler collection (GCC) environment, the term that is used for exporting is visibility. As it 
applies to functions and variables in a shared object, visibility refers to the ability of other shared objects 
to call a C/C++ function. Functions with default visibility have a global scope and can be called from other 
shared objects. Functions with hidden visibility have a local scope and cannot be called from other shared objects.

Visibility can be controlled by using either compiler options or visibility attributes.
In your header files, wherever you want an interface or API made public outside the current Dynamic Shared Object (DSO)
, place __attribute__ ((visibility ("default"))) in struct, class and function declarations you wish to make public
 With -fvisibility=hidden, you are telling GCC that every declaration not explicitly marked with a visibility attribute 
has a hidden visibility. There is such a flag in build/common.gypi



### ToLocalChecked()
You'll see a few of these calls in the hello_world example:

     Local<String> source = String::NewFromUtf8(isolate, js, NewStringType::kNormal).ToLocalChecked();

NewFromUtf8 actually returns a The Local<String> wrapped in a MaybeLocal which fores a check to see if 
the Local<> is empty before using it. 
NewStringType is an enum which can be k (for constant) normal or internalized.

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

### Contributing a change
1) Create a working branch as usual and fix/build/test etc.
2) Login to https://codereview.chromium.org/mine
3) depot-tools-auth login https://codereview.chromium.org
3) git cl upload


### Small Integers
Reading through v8.h I came accross `// Tag information for Smi`
Smi stands for small integers. It turns out that ECMA Number is defined as 64-bit binary double-precision
but internaly v8 uses 32-bit to represent all values. How can that work, you can represent a 64-bit value
using only 32-bits rigth? 
Instead the small integer is represented by the 32 bits plus a pointer to the 64-bit number. v8 needs to
know if a value stored in memory represents a 32-bit integer, or if it is really a 64-bit number, in which
case it has to follow the pointer to get the complete value. This is where the concept of tagging comes in.
Tagging involved borrowing one bit of the 32-bit, making it 31-bit and having the leftover bit represent a 
tag. If the tag is zero then this is a plain value, but if tag is 1 then the pointer must be followed.
This does not only have to be for numbers it could also be used for object (I think)

### v8::PersistentObject 

### Tasks 
4740 // TODO(dcarney): mark V8_WARN_UNUSED_RESULT
2741   Maybe<bool> Delete(Local<Context> context, Local<Value> key); 
and 4747, and 2764

3384     V8_DEPRECATE_SOON("Use maybe version", void Resolve(Local<Value> value));
3385     // TODO(dcarney): mark V8_WARN_UNUSED_RESULT
3386     Maybe<bool> Resolve(Local<Context> context, Local<Value> value);
3387
3388     V8_DEPRECATE_SOON("Use maybe version", void Reject(Local<Value> value));
3389     // TODO(dcarney): mark V8_WARN_UNUSED_RESULT
3390     Maybe<bool> Reject(Local<Context> context, Local<Value> value);


The formatting here looks a little strange:
3135 template<typename T>
3136 class ReturnValue {
3137  public:
3138   template <class S> V8_INLINE ReturnValue(const ReturnValue<S>& that)
3139       : value_(that.value_) {
3140     TYPE_CHECK(T, S);
3141   }


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


### Building chromium
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

I was ableo to get around this by:

    $ pip install -U pyobjc


#### Testing

    $ out/Default/unit_tests --gtest_filter="PushClientTest.*"


#### Using a specific version of V8
So, we want to include our updated version of V8 so that we can verify that it builds correctly.
