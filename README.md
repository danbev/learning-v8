## Learning Google V8
The sole purpose of this project is to aid me in leaning Google's V8 JavaScript engine.


### Contents
1. [Introduction](./notes/intro.md)
1. [Address](#address)
1. [TaggedImpl](#taggedimpl)
1. [Object](#object)
1. [Handle](#handle)
1. [FunctionTemplate](#functiontemplate)
1. [ObjectTemplate](#objecttemplate)
1. [Small Integers](#small-integers)
1. [String types](./notes/string.md)
1. [Roots](#roots)
1. [Heap](./notes/heap.md)
1. [Builtins](#builtins)
1. [Compiler pipeline](#compiler-pipeline)
1. [CodeStubAssembler](#codestubassembler)
1. [Torque](#torque)
1. [WebAssembly](#webassembly)
1. [Promises](./notes/promises.md)
1. [Snapshots](./notes/snapshots.md)
1. [V8 Build artifacts](#v8-build-artifacts)
1. [V8 Startup walkthrough](#startup-walk-through)
1. [Building V8](#building-v8)
1. [Contributing a change](#contributing-a-change)
1. [Debugging](#debugging)
1. [Building chromium](#building-chromium)
1. [Goma chromium](#goma)
1. [EcmaScript notes](./notes/ecmaspec.md)
1. [GN notes](./notes/gn.md)

### Isolate
An Isolate is an independant copy of the V8 runtime which includes its own heap.
Two different Isolates can run in parallel and can be seen as entirely different
sandboxed instances of a V8 runtime.

### Context
To allow separate JavaScript applications to run in the same isolate a context
must be specified for each one.  This is to avoid them interfering with each
other, for example by changing the builtin objects provided.

### Template
This is the super class of both ObjecTemplate and FunctionTemplate. Remember
that in JavaScript a function can have fields just like objects.

```c++
class V8_EXPORT Template : public Data {
 public:
  void Set(Local<Name> name, Local<Data> value,
           PropertyAttribute attributes = None);
  void SetPrivate(Local<Private> name, Local<Data> value,
                  PropertyAttribute attributes = None);
  V8_INLINE void Set(Isolate* isolate, const char* name, Local<Data> value);

  void SetAccessorProperty(
     Local<Name> name,
     Local<FunctionTemplate> getter = Local<FunctionTemplate>(),
     Local<FunctionTemplate> setter = Local<FunctionTemplate>(),
     PropertyAttribute attribute = None,
     AccessControl settings = DEFAULT);
```
The `Set` function can be used to have an name and a value set on an instance
created from this template.
The `SetAccessorProperty` is for properties that are get/set using functions.

```c++
enum PropertyAttribute {
  /** None. **/
  None = 0,
  /** ReadOnly, i.e., not writable. **/
  ReadOnly = 1 << 0,
  /** DontEnum, i.e., not enumerable. **/
  DontEnum = 1 << 1,
  /** DontDelete, i.e., not configurable. **/
  DontDelete = 1 << 2
};

enum AccessControl {
  DEFAULT               = 0,
  ALL_CAN_READ          = 1,
  ALL_CAN_WRITE         = 1 << 1,
  PROHIBITS_OVERWRITING = 1 << 2
};
```


### ObjectTemplate
These allow you to create JavaScript objects without a dedicated constructor.
When an instance is create using an ObjectTemplate the new instance will have
the properties and functions configured on the ObjectTemplate.

This would be something like:
```js
const obj = {};
```
This class is declared in include/v8.h and extends Template:
```c++
class V8_EXPORT ObjectTemplate : public Template { 
  ...
}
class V8_EXPORT Template : public Data {
  ...
}
class V8_EXPORT Data {
 private:                                                                       
  Data();                                                                       
};
```
We create an instance of ObjectTemplate and we can add properties to it that
all instance created using this ObjectTemplate instance will have. This is done
by calling `Set` which is member of the `Template` class. You specify a
Local<Name> for the property. `Name` is a superclass for `Symbol` and `String`
which can be both be used as names for a property.

The implementation for `Set` can be found in `src/api/api.cc`:
```c++
void Template::Set<v8::Local<Name> name, v8::Local<Data> value, v8::PropertyAttribute attribute) {
  ...

  i::ApiNatives::AddDataProperty(isolate, templ, Utils::OpenHandle(*name),           
                                 value_obj,                                     
                                 static_cast<i::PropertyAttributes>(attribute));
}
```

There is an example in [objecttemplate_test.cc](./test/objecttemplate_test.cc)

### FunctionTemplate
Is a template that is used to create functions and like ObjectTemplate it inherits
from Template:
```c++
class V8_EXPORT FunctionTemplate : public Template {
}
```
Rememeber that a function in javascript can have properties just like object.

There is an example in [functionttemplate_test.cc](./test/functiontemplate_test.cc)

An instance of a function template can be created using:
```c++
  Local<FunctionTemplate> ft = FunctionTemplate::New(isolate_, function_callback, data);
  Local<Function> function = ft->GetFunction(context).ToLocalChecked();
```
And the function can be called using:
```c++
  MaybeLocal<Value> ret = function->Call(context, recv, 0, nullptr);
```
Function::Call can be found in `src/api/api.cc`: 
```c++
  bool has_pending_exception = false;
  auto self = Utils::OpenHandle(this);                                               
  i::Handle<i::Object> recv_obj = Utils::OpenHandle(*recv);                          
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);   
  Local<Value> result;                                                               
  has_pending_exception = !ToLocal<Value>(                                           
      i::Execution::Call(isolate, self, recv_obj, argc, args), &result);
```
Notice that the return value of `Call` which is a `MaybeHandle<Object>` will be
passed to ToLocal<Value> which is defined in `api.h`:
```c++
template <class T>                                                              
inline bool ToLocal(v8::internal::MaybeHandle<v8::internal::Object> maybe,      
                    Local<T>* local) {                                          
  v8::internal::Handle<v8::internal::Object> handle;                            
  if (maybe.ToHandle(&handle)) {                                                   
    *local = Utils::Convert<v8::internal::Object, T>(handle);                   
    return true;                                                                
  }                                                                                
  return false;                                                                 
```
So lets take a look at `Execution::Call` which can be found in `execution/execution.cc`
and it calls:
```c++
return Invoke(isolate, InvokeParams::SetUpForCall(isolate, callable, receiver, argc, argv));
```
`SetUpForCall` will return an `InvokeParams`.
TODO: Take a closer look at InvokeParams.
```c++
V8_WARN_UNUSED_RESULT MaybeHandle<Object> Invoke(Isolate* isolate,              
                                                 const InvokeParams& params) {
```
```c++
Handle<Object> receiver = params.is_construct                             
                                    ? isolate->factory()->the_hole_value()         
                                    : params.receiver; 
```
In our case `is_construct` is false as we are not using `new` and the receiver,
the `this` in the function should be set to the receiver that we passed in. After
that we have `Builtins::InvokeApiFunction` 
```c++
auto value = Builtins::InvokeApiFunction(                                 
          isolate, params.is_construct, function, receiver, params.argc,        
          params.argv, Handle<HeapObject>::cast(params.new_target)); 
```

```c++
result = HandleApiCallHelper<false>(isolate, function, new_target,        
                                    fun_data, receiver, arguments);
```

`api-arguments-inl.h` has:
```c++
FunctionCallbackArguments::Call(CallHandlerInfo handler) {
  ...
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                  
  FunctionCallbackInfo<v8::Value> info(values_, argv_, argc_);                  
  f(info);
  return GetReturnValue<Object>(isolate);
}
```
The call to f(info) is what invokes the callback, which is just a normal
function call. 

Back in `HandleApiCallHelper` we have:
```c++
Handle<Object> result = custom.Call(call_data);                             
                                                                                
RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
```
`RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION` expands to:
```c++
Handle<Object> result = custom.Call(call_data);                             
do { 
  Isolate* __isolate__ = (isolate); 
  ((void) 0); 
  if (__isolate__->has_scheduled_exception()) { 
    __isolate__->PromoteScheduledException(); 
    return MaybeHandle<Object>(); 
  }
} while (false);
```
Notice that if there was an exception an empty object is returned.
Later in `Invoke` in `execution.cc`a:
```c++
  auto value = Builtins::InvokeApiFunction(                                 
          isolate, params.is_construct, function, receiver, params.argc,        
          params.argv, Handle<HeapObject>::cast(params.new_target));            
  bool has_exception = value.is_null();                                     
  if (has_exception) {                                                      
    if (params.message_handling == Execution::MessageHandling::kReport) {   
      isolate->ReportPendingMessages();                                     
    }                                                                       
    return MaybeHandle<Object>();                                           
  } else {                                                                  
    isolate->clear_pending_message();                                       
  }                                                                         
  return value;                         
```
Looking at this is looks like passing back an empty object will cause an 
exception to be triggered?

### Address
`Address` can be found in `include/v8-internal.h`:

```c++
typedef uintptr_t Address;
```
`uintptr_t` is an optional type specified in `cstdint` and is capable of storing
a data pointer. It is an unsigned integer type that any valid pointer to void
can be converted to this type (and back).

### TaggedImpl
This class is declared in `src/objects/tagged-impl.h and has a single private
member which is declared as:
```c++
 public
  constexpr StorageType ptr() const { return ptr_; }
 private:
  StorageType ptr_;
```
An instance can be created using:
```c++
  i::TaggedImpl<i::HeapObjectReferenceType::STRONG, i::Address>  tagged{};
```
Storage type can also be `Tagged_t` which is defined in globals.h:
```c++
 using Tagged_t = uint32_t;
```
It looks like it can be a different value when using pointer compression.

See [tagged_test.cc](./test/tagged_test.cc) for an example.

### Object
This class extends TaggedImpl:
```c++
class Object : public TaggedImpl<HeapObjectReferenceType::STRONG, Address> {       
```
An Object can be created using the default constructor, or by passing in an 
Address which will delegate to TaggedImpl constructors. Object itself does
not have any members (apart from `ptr_` which is inherited from TaggedImpl that is). 
So if we create an Object on the stack this is like a pointer/reference to
an object: 
```
+------+
|Object|
|------|
|ptr_  |---->
+------+
```
Now, `ptr_` is a StorageType so it could be a `Smi` in which case it would just
contains the value directly, for example a small integer:
```
+------+
|Object|
|------|
|  18  |
+------+
```
See [object_test.cc](./test/object_test.cc) for an example.

### ObjectSlot
```c++
  i::Object obj{18};
  i::FullObjectSlot slot{&obj};
```

```
+----------+      +---------+
|ObjectSlot|      | Object  |
|----------|      |---------|
| address  | ---> |   18    |
+----------+      +---------+
```
See [objectslot_test.cc](./test/objectslot_test.cc) for an example.

### Maybe
A Maybe is like an optional which can either hold a value or nothing.
```c++
template <class T>                                                              
class Maybe {
 public:
  V8_INLINE bool IsNothing() const { return !has_value_; }                      
  V8_INLINE bool IsJust() const { return has_value_; }
  ...

 private:
  bool has_value_;                                                              
  T value_; 
}
```
I first thought that name `Just` was a little confusing but if you read this
like:
```c++
  bool cond = true;
  Maybe<int> maybe = cond ? Just<int>(10) : Nothing<int>();
```
I think it makes more sense. There are functions that check if the Maybe is
nothing and crash the process if so. You can also check and return the value
by using `FromJust`. 

The usage of Maybe is where api calls can fail and returning Nothing is a way
of signaling this.

See [maybe_test.cc](./test/maybe_test.cc) for an example.

### MaybeLocal
```c++
template <class T>                                                              
class MaybeLocal {
 public:                                                                        
  V8_INLINE MaybeLocal() : val_(nullptr) {} 
  V8_INLINE Local<T> ToLocalChecked();
  V8_INLINE bool IsEmpty() const { return val_ == nullptr; }
  template <class S>                                                            
  V8_WARN_UNUSED_RESULT V8_INLINE bool ToLocal(Local<S>* out) const {           
    out->val_ = IsEmpty() ? nullptr : this->val_;                               
    return !IsEmpty();                                                          
  }    

 private:
  T* val_;
```
`ToLocalChecked` will crash the process if `val_` is a nullptr. If you want to
avoid a crash one can use `ToLocal`.

See [maybelocal_test.cc](./test/maybelocal_test.cc) for an example.

### Data
Is the super class of all objects that can exist the V8 heap:
```c++
class V8_EXPORT Data {                                                          
 private:                                                                       
  Data();                                                                       
};
```

### Value
Value extends `Data` and adds a number of methods that check if a Value
is of a certain type, like `IsUndefined()`, `IsNull`, `IsNumber` etc.
It also has useful methods to convert to a Local<T>, for example:
```c++
V8_WARN_UNUSED_RESULT MaybeLocal<Number> ToNumber(Local<Context> context) const;
V8_WARN_UNUSED_RESULT MaybeLocal<String> ToNumber(Local<String> context) const;
...
```


### Handle
A Handle is similar to a Object and ObjectSlot in that it also contains
an Address member (called `location_` and declared in `HandleBase`), but with the
difference is that Handles acts as a layer of abstraction and can be relocated
by the garbage collector.
Can be found in `src/handles/handles.h`.

```c++
class HandleBase {  
 ...
 protected:
  Address* location_; 
}
template <typename T>                                                           
class Handle final : public HandleBase {
  ...
}
```

```
+----------+                  +--------+         +---------+
|  Handle  |                  | Object |         |   int   |
|----------|      +-----+     |--------|         |---------|
|*location_| ---> |&ptr_| --> | ptr_   | ----->  |     5   |
+----------+      +-----+     +--------+         +---------+
```
```console
(gdb) p handle
$8 = {<v8::internal::HandleBase> = {location_ = 0x7ffdf81d60c0}, <No data fields>}
```
Notice that `location_` contains a pointer:
```console
(gdb) p /x *(int*)0x7ffdf81d60c0
$9 = 0xa9d330
```
And this is the same as the value in obj:
```console
(gdb) p /x obj.ptr_
$14 = 0xa9d330
```
And we can access the int using any of the pointers:
```console
(gdb) p /x *value
$16 = 0x5
(gdb) p /x *obj.ptr_
$17 = 0x5
(gdb) p /x *(int*)0x7ffdf81d60c0
$18 = 0xa9d330
(gdb) p /x *(*(int*)0x7ffdf81d60c0)
$19 = 0x5
```

See [handle_test.cc](./test/handle_test.cc) for an example.

### HandleScope
Contains a number of Local/Handle's (think pointers to objects but is managed
by V8) and will take care of deleting the Local/Handles for us. HandleScopes
are stack allocated

When ~HandleScope is called all handles created within that scope are removed
from the stack maintained by the HandleScope which makes objects to which the
handles point being eligible for deletion from the heap by the GC.

A HandleScope only has three members:
```c++
  internal::Isolate* isolate_;
  internal::Address* prev_next_;
  internal::Address* prev_limit_;
```

Lets take a closer look at what happens when we construct a HandleScope:
```c++
  v8::HandleScope handle_scope{isolate_};
```
The constructor call will end up in `src/api/api.cc` and the constructor simply
delegates to `Initialize`:
```c++
HandleScope::HandleScope(Isolate* isolate) { Initialize(isolate); }

void HandleScope::Initialize(Isolate* isolate) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ...
  i::HandleScopeData* current = internal_isolate->handle_scope_data();
  isolate_ = internal_isolate;
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
}
```
Every `v8::internal::Isolate` has member of type HandleScopeData:
```c++
HandleScopeData* handle_scope_data() { return &handle_scope_data_; }
HandleScopeData handle_scope_data_;
```
HandleScopeData is a struct defined in `src/handles/handles.h`:
```c++
struct HandleScopeData final {
  Address* next;
  Address* limit;
  int level;
  int sealed_level;
  CanonicalHandleScope* canonical_scope;

  void Initialize() {
    next = limit = nullptr;
    sealed_level = level = 0;
    canonical_scope = nullptr;
  }
};
```
Notice that there are two pointers (Address*) to next and a limit. When a 
HandleScope is Initialized the current handle_scope_data will be retrieved 
from the internal isolate. The HandleScope instance that is getting created
stores the next/limit pointers of the current isolate so that they can be restored
when this HandleScope is closed (see CloseScope).

So with a HandleScope created, how does a Local<T> interact with this instance?  

When a Local<T> is created this will/might go through FactoryBase::NewStruct
which will allocate a new Map and then create a Handle for the InstanceType
being created:
```c++
Handle<Struct> str = handle(Struct::cast(result), isolate()); 
```
This will land in the constructor Handle<T>src/handles/handles-inl.h
```c++
template <typename T>                                                           
Handle<T>::Handle(T object, Isolate* isolate): HandleBase(object.ptr(), isolate) {}

HandleBase::HandleBase(Address object, Isolate* isolate)                        
    : location_(HandleScope::GetHandle(isolate, object)) {}
```
Notice that `object.ptr()` is used to pass the Address to HandleBase.
And also notice that HandleBase sets its location_ to the result of HandleScope::GetHandle.

```c++
Address* HandleScope::GetHandle(Isolate* isolate, Address value) {              
  DCHECK(AllowHandleAllocation::IsAllowed());                                   
  HandleScopeData* data = isolate->handle_scope_data();                         
  CanonicalHandleScope* canonical = data->canonical_scope;                      
  return canonical ? canonical->Lookup(value) : CreateHandle(isolate, value);   
}
```
Which will call `CreateHandle` in this case and this function will retrieve the
current isolate's handle_scope_data:
```c++
  HandleScopeData* data = isolate->handle_scope_data();                         
  Address* result = data->next;                                                 
  if (result == data->limit) {                                                  
    result = Extend(isolate);                                                   
  }     
```
In this case both next and limit will be 0x0 so Extend will be called.
Extend will also get the isolates handle_scope_data and check the current level
and after that get the isolates HandleScopeImplementer:
```c++
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();           
```
`HandleScopeImplementer` is declared in `src/api/api.h`

HandleScope:CreateHandle will get the handle_scope_data from the isolate:
```c++
Address* HandleScope::CreateHandle(Isolate* isolate, Address value) {
  HandleScopeData* data = isolate->handle_scope_data();
  if (result == data->limit) {
    result = Extend(isolate);
  }
  // Update the current next field, set the value in the created handle,        
  // and return the result.
  data->next = reinterpret_cast<Address*>(reinterpret_cast<Address>(result) + sizeof(Address));
  *result = value;
  return result;
}                         
```
Notice that `data->next` is set to the address passed in + the size of an
Address.


The destructor for HandleScope will call CloseScope.
See [handlescope_test.cc](./test/handlescope_test.cc) for an example.

### EscapableHandleScope
Local handles are located on the stack and are deleted when the appropriate
destructor is called. If there is a local HandleScope then it will take care
of this when the scope returns. When there are no references left to a handle
it can be garbage collected. This means if a function has a HandleScope and
wants to return a handle/local it will not be available after the function
returns. This is what EscapableHandleScope is for, it enable the value to be
placed in the enclosing handle scope to allow it to survive. When the enclosing
HandleScope goes out of scope it will be cleaned up.

```c++
class V8_EXPORT EscapableHandleScope : public HandleScope {                        
 public:                                                                           
  explicit EscapableHandleScope(Isolate* isolate);
  V8_INLINE ~EscapableHandleScope() = default;
  template <class T>
  V8_INLINE Local<T> Escape(Local<T> value) {
    internal::Address* slot = Escape(reinterpret_cast<internal::Address*>(*value));
    return Local<T>(reinterpret_cast<T*>(slot));
  }

  template <class T>
  V8_INLINE MaybeLocal<T> EscapeMaybe(MaybeLocal<T> value) {
    return Escape(value.FromMaybe(Local<T>()));
  }

 private:
  ...
  internal::Address* escape_slot_;
};
```

From `api.cc`
```c++
EscapableHandleScope::EscapableHandleScope(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  escape_slot_ = CreateHandle(isolate, i::ReadOnlyRoots(isolate).the_hole_value().ptr());
  Initialize(v8_isolate);
}
```
So when an EscapableHandleScope is created it will create a handle with the
hole value and store it in the `escape_slot_` which is of type Address. This
Handle will be created in the current HandleScope, and EscapableHandleScope
can later set a value for that pointer/address which it want to be escaped.
Later when that HandleScope goes out of scope it will be cleaned up.
It then calls Initialize just like a normal HandleScope would.

```c++
i::Address* HandleScope::CreateHandle(i::Isolate* isolate, i::Address value) {
  return i::HandleScope::CreateHandle(isolate, value);
}
```
From `handles-inl.h`:
```c++
Address* HandleScope::CreateHandle(Isolate* isolate, Address value) {
  DCHECK(AllowHandleAllocation::IsAllowed());
  HandleScopeData* data = isolate->handle_scope_data();
  Address* result = data->next;
  if (result == data->limit) {
    result = Extend(isolate);
  }
  // Update the current next field, set the value in the created handle,
  // and return the result.
  DCHECK_LT(reinterpret_cast<Address>(result),
            reinterpret_cast<Address>(data->limit));
  data->next = reinterpret_cast<Address*>(reinterpret_cast<Address>(result) +
                                          sizeof(Address));
  *result = value;
  return result;
}
```

When Escape is called the following happens (v8.h):
```c++
template <class T>
  V8_INLINE Local<T> Escape(Local<T> value) {
    internal::Address* slot = Escape(reinterpret_cast<internal::Address*>(*value));
    return Local<T>(reinterpret_cast<T*>(slot));
  }
```
An the EscapeableHandleScope::Escape (api.cc):
```c++
i::Address* EscapableHandleScope::Escape(i::Address* escape_value) {
  i::Heap* heap = reinterpret_cast<i::Isolate*>(GetIsolate())->heap();
  Utils::ApiCheck(i::Object(*escape_slot_).IsTheHole(heap->isolate()),
                  "EscapableHandleScope::Escape", "Escape value set twice");
  if (escape_value == nullptr) {
    *escape_slot_ = i::ReadOnlyRoots(heap).undefined_value().ptr();
    return nullptr;
  }
  *escape_slot_ = *escape_value;
  return escape_slot_;
}
```
If the escape_value is null, the `escape_slot` that is a pointer into the 
parent HandleScope is set to the undefined_value() instead of the hole value
which is was previously, and nullptr will be returned. This returned 
address/pointer will then be returned after being casted to T*.
Next, we take a look at what happens when the EscapableHandleScope goes out of
scope. This will call HandleScope::~HandleScope which makes sense as any other
Local handles should be cleaned up.

`Escape` copies the value of its argument into the enclosing scope, deletes alli
its local handles, and then gives back the new handle copy which can safely be
returned.

### HeapObject
TODO:

### Local
Has a single member `val_` which is of type pointer to `T`:
```c++
template <class T> class Local { 
...
 private:
  T* val_
}
```
Notice that this is a pointer to T. We could create a local using:
```c++
  v8::Local<v8::Value> empty_value;
```

So a Local contains a pointer to type T. We can access this pointer using
`operator->` and `operator*`.

We can cast from a subtype to a supertype using Local::Cast:
```c++
v8::Local<v8::Number> nr = v8::Local<v8::Number>(v8::Number::New(isolate_, 12));
v8::Local<v8::Value> val = v8::Local<v8::Value>::Cast(nr);
```
And there is also the 
```c++
v8::Local<v8::Value> val2 = nr.As<v8::Value>();
```

See [local_test.cc](./test/local_test.cc) for an example.

### PrintObject
Using _v8_internal_Print_Object from c++:
```console
$ nm -C libv8_monolith.a | grep Print_Object
0000000000000000 T _v8_internal_Print_Object(void*)
```
Notice that this function does not have a namespace.
We can use this as:
```c++
extern void _v8_internal_Print_Object(void* object);

_v8_internal_Print_Object(*((v8::internal::Object**)(*global)));
```
Lets take a closer look at the above:
```c++
  v8::internal::Object** gl = ((v8::internal::Object**)(*global));
```
We use the dereference operator to get the value of a Local (*global), which is
just of type `T*`, a pointer to the type the Local:
```c++
template <class T>
class Local {
  ...
 private:
  T* val_;
}
```

We are then casting that to be of type pointer-to-pointer to Object.
```
  gl**        Object*         Object
+-----+      +------+      +-------+
|     |----->|      |----->|       |
+-----+      +------+      +-------+
```
An instance of `v8::internal::Object` only has a single data member which is a
field named `ptr_` of type `Address`:

`src/objects/objects.h`:
```c++
class Object : public TaggedImpl<HeapObjectReferenceType::STRONG, Address> {
 public:
  constexpr Object() : TaggedImpl(kNullAddress) {}
  explicit constexpr Object(Address ptr) : TaggedImpl(ptr) {}

#define IS_TYPE_FUNCTION_DECL(Type) \
  V8_INLINE bool Is##Type() const;  \
  V8_INLINE bool Is##Type(const Isolate* isolate) const;
  OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  IS_TYPE_FUNCTION_DECL(HashTableBase)
  IS_TYPE_FUNCTION_DECL(SmallOrderedHashTable)
#undef IS_TYPE_FUNCTION_DECL
  V8_INLINE bool IsNumber(ReadOnlyRoots roots) const;
}
```
Lets take a look at one of these functions and see how it is implemented. For
example in the OBJECT_TYPE_LIST we have:
```c++
#define OBJECT_TYPE_LIST(V) \
  V(LayoutDescriptor)       \
  V(Primitive)              \
  V(Number)                 \
  V(Numeric)
```
So the object class will have a function that looks like:
```c++
inline bool IsNumber() const;
inline bool IsNumber(const Isolate* isolate) const;
```
And in src/objects/objects-inl.h we will have the implementations:
```c++
bool Object::IsNumber() const {
  return IsHeapObject() && HeapObject::cast(*this).IsNumber();
}
```
`IsHeapObject` is defined in TaggedImpl:
```c++
  constexpr inline bool IsHeapObject() const { return IsStrong(); }

  constexpr inline bool IsStrong() const {
#if V8_HAS_CXX14_CONSTEXPR
    DCHECK_IMPLIES(!kCanBeWeak, !IsSmi() == HAS_STRONG_HEAP_OBJECT_TAG(ptr_));
#endif
    return kCanBeWeak ? HAS_STRONG_HEAP_OBJECT_TAG(ptr_) : !IsSmi();
  }
```

The macro can be found in src/common/globals.h:
```c++
#define HAS_STRONG_HEAP_OBJECT_TAG(value)                          \
  (((static_cast<i::Tagged_t>(value) & ::i::kHeapObjectTagMask) == \
    ::i::kHeapObjectTag))
```
So we are casting `ptr_` which is of type Address into type `Tagged_t` which
is defined in src/common/global.h and can be different depending on if compressed
pointers are used or not. If they are  not supported it is the same as Address:
``` 
using Tagged_t = Address;
```

`src/objects/tagged-impl.h`:
```c++
template <HeapObjectReferenceType kRefType, typename StorageType>
class TaggedImpl {

  StorageType ptr_;
}
```
The HeapObjectReferenceType can be either WEAK or STRONG. And the storage type
is `Address` in this case. So Object itself only has one member that is inherited
from its only super class and this is `ptr_`.

So the following is telling the compiler to treat the value of our Local,
`*global`, as a pointer (which it already is) to a pointer that points to
a memory location that adhers to the layout of an `v8::internal::Object` type,
which we know now has a `prt_` member. And we want to dereference it and pass
it into the function.
```c++
_v8_internal_Print_Object(*((v8::internal::Object**)(*global)));
```

### ObjectTemplate
But I'm still missing the connection between ObjectTemplate and object.
When we create it we use:
```c++
Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
```
In `src/api/api.cc` we have:
```c++
static Local<ObjectTemplate> ObjectTemplateNew(
    i::Isolate* isolate, v8::Local<FunctionTemplate> constructor,
    bool do_not_cache) {
  i::Handle<i::Struct> struct_obj = isolate->factory()->NewStruct(
      i::OBJECT_TEMPLATE_INFO_TYPE, i::AllocationType::kOld);
  i::Handle<i::ObjectTemplateInfo> obj = i::Handle<i::ObjectTemplateInfo>::cast(struct_obj);
  InitializeTemplate(obj, Consts::OBJECT_TEMPLATE);
  int next_serial_number = 0;
  if (!constructor.IsEmpty())
    obj->set_constructor(*Utils::OpenHandle(*constructor));
  obj->set_data(i::Smi::zero());
  return Utils::ToLocal(obj);
}
```
What is a `Struct` in this context?  
`src/objects/struct.h`
```c++
#include "torque-generated/class-definitions-tq.h"

class Struct : public TorqueGeneratedStruct<Struct, HeapObject> {
 public:
  inline void InitializeBody(int object_size);
  void BriefPrintDetails(std::ostream& os);
  TQ_OBJECT_CONSTRUCTORS(Struct)
```
Notice that the include is specifying `torque-generated` include which can be
found `out/x64.release_gcc/gen/torque-generated/class-definitions-tq`. So, somewhere
there must be an call to the `torque` executable which generates the Code Stub
Assembler C++ headers and sources before compiling the main source files. There is
and there is a section about this in `Building V8`.
The macro `TQ_OBJECT_CONSTRUCTORS` can be found in `src/objects/object-macros.h`
and expands to:
```c++
  constexpr Struct() = default;

 protected:
  template <typename TFieldType, int kFieldOffset>
  friend class TaggedField;

  inline explicit Struct(Address ptr);
```

So what does the TorqueGeneratedStruct look like?
```
template <class D, class P>
class TorqueGeneratedStruct : public P {
 public:
```
Where D is Struct and P is HeapObject in this case. But the above is the declartion
of the type but what we have in the .h file is what was generated. 

This type is defined in `src/objects/struct.tq`:
```
@abstract                                                                       
@generatePrint                                                                  
@generateCppClass                                                               
extern class Struct extends HeapObject {                                        
} 
```

`NewStruct` can be found in `src/heap/factory-base.cc`
```c++
template <typename Impl>
HandleFor<Impl, Struct> FactoryBase<Impl>::NewStruct(
    InstanceType type, AllocationType allocation) {
  Map map = Map::GetStructMap(read_only_roots(), type);
  int size = map.instance_size();
  HeapObject result = AllocateRawWithImmortalMap(size, allocation, map);
  HandleFor<Impl, Struct> str = handle(Struct::cast(result), isolate());
  str->InitializeBody(size);
  return str;
}
```
Every object that is stored on the v8 heap has a Map (`src/objects/map.h`) that
describes the structure of the object being stored.
```c++
class Map : public HeapObject {
```

```console
1725	  return Utils::ToLocal(obj);
(gdb) p obj
$6 = {<v8::internal::HandleBase> = {location_ = 0x30b5160}, <No data fields>}
```
So this is the connection, what we see as a Local<ObjectTemplate> is a HandleBase.
TODO: dig into this some more when I have time.


```console
(lldb) expr gl
(v8::internal::Object **) $0 = 0x00000000020ee160
(lldb) memory read -f x -s 8 -c 1 gl
0x020ee160: 0x00000aee081c0121

(lldb) memory read -f x -s 8 -c 1 *gl
0xaee081c0121: 0x0200000002080433
```


You can reload `.lldbinit` using the following command:
```console
(lldb) command source ~/.lldbinit
```
This can be useful when debugging a lldb command. You can set a breakpoint
and break at that location and make updates to the command and reload without
having to restart lldb.

Currently, the lldb-commands.py that ships with v8 contains an extra operation
of the parameter pased to `ptr_arg_cmd`:
```python
def ptr_arg_cmd(debugger, name, param, cmd):                                    
  if not param:                                                                 
    print("'{}' requires an argument".format(name))                             
    return                                                                      
  param = '(void*)({})'.format(param)                                           
  no_arg_cmd(debugger, cmd.format(param)) 
```
Notice that `param` is the object that we want to print, for example lets say
it is a local named obj:
```
param = "(void*)(obj)"
```
This will then be "passed"/formatted into the command string:
```
"_v8_internal_Print_Object(*(v8::internal::Object**)(*(void*)(obj))")
```

#### Threads
V8 is single threaded (the execution of the functions of the stack) but there
are supporting threads used for garbage collection, profiling (IC, and perhaps
other things) (I think).
Lets see what threads there are:

    $ LD_LIBRARY_PATH=../v8_src/v8/out/x64.release_gcc/ lldb ./hello-world 
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

### Local

```c++
Local<String> script_name = ...;
```
So what is script_name. Well it is an object reference that is managed by the v8 GC.
The GC needs to be able to move things (pointers around) and also track if
things should be GC'd. Local handles as opposed to persistent handles are light
weight and mostly used local operations. These handles are managed by
HandleScopes so you must have a handlescope on the stack and the local is only
valid as long as the handlescope is valid. This uses Resource Acquisition Is
Initialization (RAII) so when the HandleScope instance goes out of scope it
will remove all the Local instances.

The `Local` class (in `include/v8.h`) only has one member which is of type
pointer to the type `T`. So for the above example it would be:
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

The handle stack is not part of the C++ call stack, but the handle scopes are
embedded in the C++ stack. Handle scopes can only be stack-allocated, not
allocated with new.

### Persistent
https://v8.dev/docs/embed:
Persistent handles provide a reference to a heap-allocated JavaScript Object, 
just like a local handle. There are two flavors, which differ in the lifetime
management of the reference they handle. Use a persistent handle when you need
to keep a reference to an object for more than one function call, or when handle
lifetimes do not correspond to C++ scopes. Google Chrome, for example, uses
persistent handles to refer to Document Object Model (DOM) nodes.

A persistent handle can be made weak, using PersistentBase::SetWeak, to trigger
a callback from the garbage collector when the only references to an object are
from weak persistent handles.


A UniquePersistent<SomeType> handle relies on C++ constructors and destructors
to manage the lifetime of the underlying object.
A Persistent<SomeType> can be constructed with its constructor, but must be
explicitly cleared with Persistent::Reset.

So how is a persistent object created?  
Let's write a test and find out (`test/persistent-object_text.cc`):
```console
$ make test/persistent-object_test
$ ./test/persistent-object_test --gtest_filter=PersistentTest.value
```
Now, to create an instance of Persistent we need a Local<T> instance or the
Persistent instance will just be empty.
```c++
Local<Object> o = Local<Object>::New(isolate_, Object::New(isolate_));
```
`Local<Object>::New` can be found in `src/api/api.cc`:
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
The first thing that happens is that the public Isolate pointer is cast to an
pointer to the internal `Isolate` type.
`LOG_API` is a macro in the same source file (src/api/api.cc):
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

First, `i_isolate->object_function()` is called and the result passed to
`NewJSObject`. `object_function` is generated by a macro named 
`NATIVE_CONTEXT_FIELDS`:
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
So an HeapObject contains a pointer to a Map, or rather has a function that 
returns a pointer to Map. I can't see any member map in the HeapObject class.

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
We can see that the above is calling `AllocateRawWithRetryOrFail` on the heap 
instance passing a size of `88` and specifying the `MAP_SPACE`:
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
and sometimes the shape of an object. If two objects have the same properties they would share 
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

In the GNU compiler collection (GCC) environment, the term that is used for
exporting is visibility. As it applies to functions and variables in a shared
object, visibility refers to the ability of other shared objects to call a
C/C++ function. Functions with default visibility have a global scope and can
be called from other shared objects. Functions with hidden visibility have a
local scope and cannot be called from other shared objects.

Visibility can be controlled by using either compiler options or visibility attributes.
In your header files, wherever you want an interface or API made public outside
the current Dynamic Shared Object (DSO) , place
`__attribute__ ((visibility ("default")))` in struct, class and function
declarations you wish to make public.  With `-fvisibility=hidden`, you are
telling GCC that every declaration not explicitly marked with a visibility
attribute has a hidden visibility. There is such a flag in build/common.gypi


### ToLocalChecked()
You'll see a few of these calls in the hello_world example:
```c++
  Local<String> source = String::NewFromUtf8(isolate, js, NewStringType::kNormal).ToLocalChecked();
```

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

The above can be found in `src/api.h`. The same goes for `Local<Object>,
Local<String>` etc.


### Small Integers
Reading through v8.h I came accross `// Tag information for Smi`
Smi stands for small integers.

A pointer is really just a integer that is treated like a memory address. We can
use that memory address to get the start of the data located in that memory slot.
But we can also just store an normal value like 18 in it. There might be cases
where it does not make sense to store a small integer somewhere in the heap and
have a pointer to it, but instead store the value directly in the pointer itself.
But that only works for small integers so there needs to be away to know if the
value we want is stored in the pointer or if we should follow the value stored to
the heap to get the value.

A word on a 64 bit machine is 8 bytes (64 bits) and all of the pointers need to
be aligned to multiples of 8. So a pointer could be:
```
1000       = 8
10000      = 16
11000      = 24
100000     = 32
1000000000 = 512
```
Remember that we are talking about the pointers and not the values store at
the memory location they point to. We can see that there are always three bits
that are zero in the pointers. So we can use them for something else and just
mask them out when using them as pointers.

Tagging involves borrowing one bit of the 32-bit, making it 31-bit and having
the leftover bit represent a tag. If the tag is zero then this is a plain value,
but if tag is 1 then the pointer must be followed.
This does not only have to be for numbers it could also be used for object (I think)

Instead the small integer is represented by the 32 bits plus a pointer to the
64-bit number. V8 needs to know if a value stored in memory represents a 32-bit
integer, or if it is really a 64-bit number, in which case it has to follow the
pointer to get the complete value. This is where the concept of tagging comes in.



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

In `src/objects/objects.h` we can find JSObject:

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
This class describes heap allocated objects. It is in this class we find
information regarding the type of object. This information is contained in
`v8::internal::Map`.

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
Each Internal Isolate has a Factory which is used to create instances. This is
because all handles needs to be allocated using the factory (src/heap/factory.h)


### Objects 
All objects extend the abstract class Object (src/objects/objects.h).

### Oddball
This class extends HeapObject and  describes `null`, `undefined`, `true`, and
`false` objects.


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
Can be found in `src/interpreter/bytecodes.h`

* StackCheck checks that stack limits are not exceeded to guard against overflow.
* `Star` Store content in accumulator regiser in register (the operand).
* Ldar   LoaD accumulator from Register argument a1 which is b

The registers are not machine registers, apart from the accumlator as I
understand it, but would instead be stack allocated.


#### Parsing
Parsing is the parsing of the JavaScript and the generation of the abstract
syntax tree. That tree is then visited and bytecode generated from it. This
section tries to figure out where in the code these operations are performed.

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

### Symbol
The declarations for the Symbol class can be found in `v8.h` and the internal
implementation in `src/api/api.cc`.

The well known Symbols are generated using macros so you won't find the just
by searching using the static function names like 'GetToPrimitive`.
```c++
#define WELL_KNOWN_SYMBOLS(V)                 \
  V(AsyncIterator, async_iterator)            \
  V(HasInstance, has_instance)                \
  V(IsConcatSpreadable, is_concat_spreadable) \
  V(Iterator, iterator)                       \
  V(Match, match)                             \
  V(Replace, replace)                         \
  V(Search, search)                           \
  V(Split, split)                             \
  V(ToPrimitive, to_primitive)                \
  V(ToStringTag, to_string_tag)               \
  V(Unscopables, unscopables)

#define SYMBOL_GETTER(Name, name)                                   \
  Local<Symbol> v8::Symbol::Get##Name(Isolate* isolate) {           \
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate); \
    return Utils::ToLocal(i_isolate->factory()->name##_symbol());   \
  }
```
So GetToPrimitive would become:
```c++
Local<Symbol> v8::Symbol::GeToPrimitive(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  return Utils::ToLocal(i_isolate->factory()->to_primitive_symbol());
}

```



There is an example in [symbol-test.cc](./test/symbol-test.cc).

## Builtins
Are JavaScript functions/objects that are provided by V8. These are built using a
C++ DSL and are passed through:

    CodeStubAssembler -> CodeAssembler -> RawMachineAssembler.

Builtins need to have bytecode generated for them so that they can be run in TurboFan.

`src/code-stub-assembler.h`

All the builtins are declared in `src/builtins/builtins-definitions.h` by the
`BUILTIN_LIST_BASE` macro. 
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
Builtins::Name is Builtins:kConsoleDebug. Where is this defined?  
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
      Builtins::CPP, 
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
      kDebugConsole,
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
`src/objects/objects.h`.


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

If you now build using 'ninja -C out.gn/learning_v8' you should be able to run d8 and try this out:

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

If we take a look at the file src/builtins/builtins-bigint-gen.cc and the following
function:
```c++
TF_BUILTIN(BigIntToI64, CodeStubAssembler) {                                       
  if (!Is64()) {                                                                   
    Unreachable();                                                                 
    return;                                                                        
  }                                                                                
                                                                                   
  TNode<Object> value = CAST(Parameter(Descriptor::kArgument));                    
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));                  
  TNode<BigInt> n = ToBigInt(context, value);                                      
                                                                                   
  TVARIABLE(UintPtrT, var_low);                                                    
  TVARIABLE(UintPtrT, var_high);                                                   
                                                                                   
  BigIntToRawBytes(n, &var_low, &var_high);                                        
  Return(var_low.value());                                                         
}
```
Let's take our GetStringLength example from above and see what this will be expanded to after
processing this macro:
```console
$ clang++ --sysroot=build/linux/debian_sid_amd64-sysroot -isystem=./buildtools/third_party/libc++/trunk/include -isystem=buildtools/third_party/libc++/trunk/include -I. -E src/builtins/builtins-bigint-gen.cc > builtins-bigint-gen.cc.pp
```
```c++
static void Generate_BigIntToI64(compiler::CodeAssemblerState* state);

class BigIntToI64Assembler : public CodeStubAssembler { 
 public:
  using Descriptor = Builtin_BigIntToI64_InterfaceDescriptor; 
  explicit BigIntToI64Assembler(compiler::CodeAssemblerState* state) : CodeStubAssembler(state) {} 
  void GenerateBigIntToI64Impl(); 
  Node* Parameter(Descriptor::ParameterIndices index) {
    return CodeAssembler::Parameter(static_cast<int>(index));
  }
}; 

void Builtins::Generate_BigIntToI64(compiler::CodeAssemblerState* state) {
  BigIntToI64Assembler assembler(state);
  state->SetInitialDebugInformation("BigIntToI64", "src/builtins/builtins-bigint-gen.cc", 14);
  if (Builtins::KindOf(Builtins::kBigIntToI64) == Builtins::TFJ) {
    assembler.PerformStackCheck(assembler.GetJSContextParameter());
  }
  assembler.GenerateBigIntToI64Impl();
} 
void BigIntToI64Assembler::GenerateBigIntToI64Impl() {
 if (!Is64()) {                                                                
   Unreachable();                                                              
   return;                                                                     
 }                                                                             
                                                                                
 TNode<Object> value = Cast(Parameter(Descriptor::kArgument));                 
 TNode<Context> context = Cast(Parameter(Descriptor::kContext));                
 TNode<BigInt> n = ToBigInt(context, value);                                   
                                                                               
 TVariable<UintPtrT> var_low(this);                                            
 TVariable<UintPtrT> var_high(this);                                           
                                                                                
 BigIntToRawBytes(n, &var_low, &var_high);                                     
 Return(var_low.value());                                                      
} 
```

From the resulting class you can see how `Parameter` can be used from within `TF_BUILTIN` macro.

## Building V8
You'll need to have checked out the Google V8 sources to you local file system
and build it by following the instructions found [here](https://developers.google.com/v8/build).

### Configure v8 build for learning-v8
There is a make target that can generate a build configuration for V8 that is
specific to this project. It can be run using the following command:
```
$ make configure_v8
```
Then to compile this configuration:
``` console
$ make compile_v8
```


### [gclient](https://www.chromium.org/developers/how-tos/depottools) sync
```console
$ gclient sync
```

#### Troubleshooting build:
```console
/v8_src/v8/out/x64.release/obj/libv8_monolith.a(eh-frame.o):eh-frame.cc:function v8::internal::EhFrameWriter::WriteEmptyEhFrame(std::__1::basic_ostream<char, std::__1::char_traits<char> >&): error: undefined reference to 'std::__1::basic_ostream<char, std::__1::char_traits<char> >::write(char const*, long)'
clang: error: linker command failed with exit code 1 (use -v to see invocation)
```
`-stdlib=libc++` is llvm's C++ runtime. This runtime has a `__1` namespace.
I looks like the static library above was compiled with clangs/llvm's `libc++`
as we are seeing the `__1` namespace.

-stdlib=libstdc++ is GNU's C++ runtime

So we can see that the namespace `std::__1` is used which we now
know is the namespace that libc++ which is clangs libc++ library.
I guess we could go about this in two ways, either we can change v8 build of
to use glibc++ when compiling so that the symbols are correct when we want to
link against it, or we can update our linker (ld) to use libc++.

We need to include the correct libraries to link with during linking, 
which means specifying:
```
-stdlib=libc++ -Wl,-L$(v8_build_dir)
```
If we look in $(v8_build_dir) we find `libc++.so`. We also need to this library
to be found at runtime by the dynamic linker using `LD_LIBRARY_PATH`:
```console
$ LD_LIBRARY_PATH=../v8_src/v8/out/x64.release/ ./hello-world
```
Notice that this is using `ld` from our path. We can tell clang to use a different
search path with the `-B` option:
```console
$ clang++ --help | grep -- '-B'
  -B <dir>                Add <dir> to search path for binaries and object files used implicitly
```

`libgcc_s` is GCC low level runtime library. I've been confusing this with
glibc++ libraries for some reason but they are not the same.

Running cctest:
```console
$ out.gn/learning/cctest test-heap-profiler/HeapSnapshotRetainedObjectInfo
```
To get a list of the available tests:
```console
$ out.gn/learning/cctest --list
```

Checking formating/linting:
```
$ git cl format
```
You can then `git diff` and see the changes.

Running pre-submit checks:
```console
$ git cl presubmit
```

Then upload using:
```console
$ git cl upload
```

#### Build details
So when we run gn it will generate Ninja build file. GN itself is written in 
C++ but has a python wrapper around it. 

A group in gn is just a collection of other targets which enables them to have
a name.

So when we run gn there will be a number of .ninja files generated. If we look
in the root of the output directory we find two .ninja files:
```console
build.ninja  toolchain.ninja
```
By default ninja will look for `build.ninja' and when we run ninja we usually
specify the `-C out/dir`. If no targets are specified on the command line ninja
will execute all outputs unless there is one specified as default. V8 has the 
following default target:
```
default all

build all: phony $
    ./bytecode_builtins_list_generator $                                        
    ./d8 $                                                                      
    obj/fuzzer_support.stamp $                                                  
    ./gen-regexp-special-case $                                                 
    obj/generate_bytecode_builtins_list.stamp $                                 
    obj/gn_all.stamp $                                                          
    obj/json_fuzzer.stamp $                                                     
    obj/lib_wasm_fuzzer_common.stamp $                                          
    ./mksnapshot $                                                              
    obj/multi_return_fuzzer.stamp $                                             
    obj/parser_fuzzer.stamp $                                                   
    obj/postmortem-metadata.stamp $                                             
    obj/regexp_builtins_fuzzer.stamp $                                          
    obj/regexp_fuzzer.stamp $                                                   
    obj/run_gen-regexp-special-case.stamp $                                     
    obj/run_mksnapshot_default.stamp $                                          
    obj/run_torque.stamp $                                                      
    ./torque $                                                                  
    ./torque-language-server $                                                  
    obj/torque_base.stamp $                                                     
    obj/torque_generated_definitions.stamp $                                    
    obj/torque_generated_initializers.stamp $                                   
    obj/torque_ls_base.stamp $                                                  
    ./libv8.so.TOC $                                                            
    obj/v8_archive.stamp $
    ...
```
A `phony` rule can be used to create an alias for other targets. 
The `$` in ninja is an escape character so in the case of the all target it
escapes the new line, like using \ in a shell script.

Lets take a look at `bytecode_builtins_list_generator`: 
```
build $:bytecode_builtins_list_generator: phony ./bytecode_builtins_list_generator
```
The format of the ninja build statement is:
```
build outputs: rulename inputs
```
We are again seeing the `$` ninja escape character but this time it is escaping
the colon which would otherwise be interpreted as separating file names. The output
in this case is bytecode_builtins_list_generator. And I'm guessing, as I can't
find a connection between `./bytecode_builtins_list_generator` and 

The default `target_out_dir` in this case is //out/x64.release_gcc/obj.
The executable in BUILD.gn which generates this does not specify any output
directory so I'm assuming that it the generated .ninja file is place in the 
target_out_dir in this case where we can find `bytecode_builtins_list_generator.ninja` 
This file has a label named:
```
label_name = bytecode_builtins_list_generator                                   
```
Hmm, notice that in build.ninja there is the following command:
```
subninja toolchain.ninja
```
And in `toolchain.ninja` we have:
```
subninja obj/bytecode_builtins_list_generator.ninja
```
This is what is making `./bytecode_builtins_list_generator` available.

```console
$ ninja -C out/x64.release_gcc/ -t targets all  | grep bytecode_builtins_list_generator
$ rm out/x64.release_gcc/bytecode_builtins_list_generator 
$ ninja -C out/x64.release_gcc/ bytecode_builtins_list_generator
ninja: Entering directory `out/x64.release_gcc/'
[1/1] LINK ./bytecode_builtins_list_generator
```

Alright, so I'd like to understand when in the process torque is run to
generate classes like TorqueGeneratedStruct:
```c++
class Struct : public TorqueGeneratedStruct<Struct, HeapObject> {
```
```
./torque $                                                                  
./torque-language-server $                                                  
obj/torque_base.stamp $                                                     
obj/torque_generated_definitions.stamp $                                    
obj/torque_generated_initializers.stamp $                                   
obj/torque_ls_base.stamp $  
```
Like before we can find that obj/torque.ninja in included by the subninja command
in toolchain.ninja:
```
subninja obj/torque.ninja
```
So this is building the executable `torque`, but it has not been run yet.
```console
$ gn ls out/x64.release_gcc/ --type=action
//:generate_bytecode_builtins_list
//:postmortem-metadata
//:run_gen-regexp-special-case
//:run_mksnapshot_default
//:run_torque
//:v8_dump_build_config
//src/inspector:protocol_compatibility
//src/inspector:protocol_generated_sources
//tools/debug_helper:gen_heap_constants
//tools/debug_helper:run_mkgrokdump
```
Notice the `run_torque` target
```console
$ gn desc out/x64.release_gcc/ //:run_torque
```
If we look in toolchain.ninja we have a rule named `___run_torque___build_toolchain_linux_x64__rule`
```console
command = python ../../tools/run.py ./torque -o gen/torque-generated -v8-root ../.. 
  src/builtins/array-copywithin.tq
  src/builtins/array-every.tq
  src/builtins/array-filter.tq
  src/builtins/array-find.tq
  ...
```
And there is a build that specifies the .h and cc files in gen/torque-generated
which has this rule in it if they change.


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
```c++
    #define MAKE_OPEN_HANDLE(From, To)
      v8::internal::Handle<v8::internal::To> Utils::OpenHandle( 
      const v8::From* that, bool allow_empty_handle) {         
      return v8::internal::Handle<v8::internal::To>(                         
        reinterpret_cast<v8::internal::Address*>(const_cast<v8::From*>(that))); 
      }
```
And remember that JSFunction is included in the `OPEN_HANDLE_LIST` so there will
be the following in the source after the preprocessor has processed this header:
A concrete example would look like this:
```c++
v8::internal::Handle<v8::internal::JSFunction> Utils::OpenHandle(
    const v8::Script* that, bool allow_empty_handle) {
  return v8::internal::Handle<v8::internal::JSFunction>(
      reinterpret_cast<v8::internal::Address*>(const_cast<v8::Script*>(that))); }
```

You can inspect the output of the preprocessor using:
```console
$ clang++ -I./out/x64.release/gen -I. -I./include -E src/api/api-inl.h > api-inl.output
```


So where is JSFunction declared? 
It is defined in objects.h





## Ignition interpreter
User JavaScript also needs to have bytecode generated for them and they also use
the C++ DLS and use the CodeStubAssembler -> CodeAssembler -> RawMachineAssembler
just like builtins.

## C++ Domain Specific Language (DLS)


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
JavaScript provides a set of builtin functions and objects. These functions and
objects can be changed by user code. Each context is separate collection of
these objects and functions.

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
```c++
DetachableVector is a delegate/adaptor with some additonaly features on a std::vector.
Handle<Context> context1 = NewContext(isolate);
Handle<Context> context2 = NewContext(isolate);
Context::Scope context_scope1(context1);        // entered_contexts_ [context1], saved_contexts_[isolateContext]
Context::Scope context_scope2(context2);        // entered_contexts_ [context1, context2], saved_contexts[isolateContext, context1]
```

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

### print-code
```console
$ ./d8 -print-bytecode  -print-code sample.js 
[generated bytecode for function:  (0x2a180824ffbd <SharedFunctionInfo>)]
Parameter count 1
Register count 5
Frame size 40
         0x2a1808250066 @    0 : 12 00             LdaConstant [0]
         0x2a1808250068 @    2 : 26 f9             Star r2
         0x2a180825006a @    4 : 27 fe f8          Mov <closure>, r3
         0x2a180825006d @    7 : 61 32 01 f9 02    CallRuntime [DeclareGlobals], r2-r3
         0x2a1808250072 @   12 : 0b                LdaZero 
         0x2a1808250073 @   13 : 26 fa             Star r1
         0x2a1808250075 @   15 : 0d                LdaUndefined 
         0x2a1808250076 @   16 : 26 fb             Star r0
         0x2a1808250078 @   18 : 00 0c 10 27       LdaSmi.Wide [10000]
         0x2a180825007c @   22 : 69 fa 00          TestLessThan r1, [0]
         0x2a180825007f @   25 : 9a 1c             JumpIfFalse [28] (0x2a180825009b @ 53)
         0x2a1808250081 @   27 : a7                StackCheck 
         0x2a1808250082 @   28 : 13 01 01          LdaGlobal [1], [1]
         0x2a1808250085 @   31 : 26 f9             Star r2
         0x2a1808250087 @   33 : 0c 02             LdaSmi [2]
         0x2a1808250089 @   35 : 26 f7             Star r4
         0x2a180825008b @   37 : 5e f9 fa f7 03    CallUndefinedReceiver2 r2, r1, r4, [3]
         0x2a1808250090 @   42 : 26 fb             Star r0
         0x2a1808250092 @   44 : 25 fa             Ldar r1
         0x2a1808250094 @   46 : 4c 05             Inc [5]
         0x2a1808250096 @   48 : 26 fa             Star r1
         0x2a1808250098 @   50 : 8a 20 00          JumpLoop [32], [0] (0x2a1808250078 @ 18)
         0x2a180825009b @   53 : 25 fb             Ldar r0
         0x2a180825009d @   55 : ab                Return 
Constant pool (size = 2)
0x2a1808250035: [FixedArray] in OldSpace
 - map: 0x2a18080404b1 <Map>
 - length: 2
           0: 0x2a180824ffe5 <FixedArray[2]>
           1: 0x2a180824ff61 <String[#9]: something>
Handler Table (size = 0)
Source Position Table (size = 0)
[generated bytecode for function: something (0x2a180824fff5 <SharedFunctionInfo something>)]
Parameter count 3
Register count 0
Frame size 0
         0x2a18082501ba @    0 : 25 02             Ldar a1
         0x2a18082501bc @    2 : 34 03 00          Add a0, [0]
         0x2a18082501bf @    5 : ab                Return 
Constant pool (size = 0)
Handler Table (size = 0)
Source Position Table (size = 0)
--- Raw source ---
function something(x, y) {
  return x + y
}
for (let i = 0; i < 10000; i++) {
  something(i, 2);
}


--- Optimized code ---
optimization_id = 0
source_position = 0
kind = OPTIMIZED_FUNCTION
stack_slots = 14
compiler = turbofan
address = 0x108400082ae1

Instructions (size = 536)
0x108400082b20     0  488d1df9ffffff REX.W leaq rbx,[rip+0xfffffff9]
0x108400082b27     7  483bd9         REX.W cmpq rbx,rcx
0x108400082b2a     a  7418           jz 0x108400082b44  <+0x24>
0x108400082b2c     c  48ba6800000000000000 REX.W movq rdx,0x68
0x108400082b36    16  49bae0938c724b560000 REX.W movq r10,0x564b728c93e0  (Abort)    ;; off heap target
0x108400082b40    20  41ffd2         call r10
0x108400082b43    23  cc             int3l
0x108400082b44    24  8b59d0         movl rbx,[rcx-0x30]
0x108400082b47    27  4903dd         REX.W addq rbx,r13
0x108400082b4a    2a  f6430701       testb [rbx+0x7],0x1
0x108400082b4e    2e  740d           jz 0x108400082b5d  <+0x3d>
0x108400082b50    30  49bae0f781724b560000 REX.W movq r10,0x564b7281f7e0  (CompileLazyDeoptimizedCode)    ;; off heap target
0x108400082b5a    3a  41ffe2         jmp r10
0x108400082b5d    3d  55             push rbp
0x108400082b5e    3e  4889e5         REX.W movq rbp,rsp
0x108400082b61    41  56             push rsi
0x108400082b62    42  57             push rdi
0x108400082b63    43  48ba4200000000000000 REX.W movq rdx,0x42
0x108400082b6d    4d  4c8b15c4ffffff REX.W movq r10,[rip+0xffffffc4]
0x108400082b74    54  41ffd2         call r10
0x108400082b77    57  cc             int3l
0x108400082b78    58  4883ec18       REX.W subq rsp,0x18
0x108400082b7c    5c  488975a0       REX.W movq [rbp-0x60],rsi
0x108400082b80    60  488b4dd0       REX.W movq rcx,[rbp-0x30]
0x108400082b84    64  f6c101         testb rcx,0x1
0x108400082b87    67  0f8557010000   jnz 0x108400082ce4  <+0x1c4>
0x108400082b8d    6d  81f9204e0000   cmpl rcx,0x4e20
0x108400082b93    73  0f8c0b000000   jl 0x108400082ba4  <+0x84>
0x108400082b99    79  488b45d8       REX.W movq rax,[rbp-0x28]
0x108400082b9d    7d  488be5         REX.W movq rsp,rbp
0x108400082ba0    80  5d             pop rbp
0x108400082ba1    81  c20800         ret 0x8
0x108400082ba4    84  493b6560       REX.W cmpq rsp,[r13+0x60] (external value (StackGuard::address_of_jslimit()))
0x108400082ba8    88  0f8669000000   jna 0x108400082c17  <+0xf7>
0x108400082bae    8e  488bf9         REX.W movq rdi,rcx
0x108400082bb1    91  d1ff           sarl rdi, 1
0x108400082bb3    93  4c8bc7         REX.W movq r8,rdi
0x108400082bb6    96  4183c002       addl r8,0x2
0x108400082bba    9a  0f8030010000   jo 0x108400082cf0  <+0x1d0>
0x108400082bc0    a0  83c701         addl rdi,0x1
0x108400082bc3    a3  0f8033010000   jo 0x108400082cfc  <+0x1dc>
0x108400082bc9    a9  e921000000     jmp 0x108400082bef  <+0xcf>
0x108400082bce    ae  6690           nop
0x108400082bd0    b0  488bcf         REX.W movq rcx,rdi
0x108400082bd3    b3  83c102         addl rcx,0x2
0x108400082bd6    b6  0f802c010000   jo 0x108400082d08  <+0x1e8>
0x108400082bdc    bc  4c8bc7         REX.W movq r8,rdi
0x108400082bdf    bf  4183c001       addl r8,0x1
0x108400082be3    c3  0f802b010000   jo 0x108400082d14  <+0x1f4>
0x108400082be9    c9  498bf8         REX.W movq rdi,r8
0x108400082bec    cc  4c8bc1         REX.W movq r8,rcx
0x108400082bef    cf  81ff10270000   cmpl rdi,0x2710
0x108400082bf5    d5  0f8d0b000000   jge 0x108400082c06  <+0xe6>
0x108400082bfb    db  493b6560       REX.W cmpq rsp,[r13+0x60] (external value (StackGuard::address_of_jslimit()))
0x108400082bff    df  77cf           ja 0x108400082bd0  <+0xb0>
0x108400082c01    e1  e943000000     jmp 0x108400082c49  <+0x129>
0x108400082c06    e6  498bc8         REX.W movq rcx,r8
0x108400082c09    e9  4103c8         addl rcx,r8
0x108400082c0c    ec  0f8061000000   jo 0x108400082c73  <+0x153>
0x108400082c12    f2  488bc1         REX.W movq rax,rcx
0x108400082c15    f5  eb86           jmp 0x108400082b9d  <+0x7d>
0x108400082c17    f7  33c0           xorl rax,rax
0x108400082c19    f9  48bef50c240884100000 REX.W movq rsi,0x108408240cf5    ;; object: 0x108408240cf5 <NativeContext[261]>
0x108400082c23   103  48bb101206724b560000 REX.W movq rbx,0x564b72061210    ;; external reference (Runtime::StackGuard)
0x108400082c2d   10d  488bf8         REX.W movq rdi,rax
0x108400082c30   110  4c8bc6         REX.W movq r8,rsi
0x108400082c33   113  49ba2089a3724b560000 REX.W movq r10,0x564b72a38920  (CEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit)    ;; off heap target
0x108400082c3d   11d  41ffd2         call r10
0x108400082c40   120  488b4dd0       REX.W movq rcx,[rbp-0x30]
0x108400082c44   124  e965ffffff     jmp 0x108400082bae  <+0x8e>
0x108400082c49   129  48897da8       REX.W movq [rbp-0x58],rdi
0x108400082c4d   12d  488b1dd1ffffff REX.W movq rbx,[rip+0xffffffd1]
0x108400082c54   134  33c0           xorl rax,rax
0x108400082c56   136  48bef50c240884100000 REX.W movq rsi,0x108408240cf5    ;; object: 0x108408240cf5 <NativeContext[261]>
0x108400082c60   140  4c8b15ceffffff REX.W movq r10,[rip+0xffffffce]
0x108400082c67   147  41ffd2         call r10
0x108400082c6a   14a  488b7da8       REX.W movq rdi,[rbp-0x58]
0x108400082c6e   14e  e95dffffff     jmp 0x108400082bd0  <+0xb0>
0x108400082c73   153  48b968ea2f744b560000 REX.W movq rcx,0x564b742fea68    ;; external reference (Heap::NewSpaceAllocationTopAddress())
0x108400082c7d   15d  488b39         REX.W movq rdi,[rcx]
0x108400082c80   160  4c8d4f0c       REX.W leaq r9,[rdi+0xc]
0x108400082c84   164  4c8945b0       REX.W movq [rbp-0x50],r8
0x108400082c88   168  49bb70ea2f744b560000 REX.W movq r11,0x564b742fea70    ;; external reference (Heap::NewSpaceAllocationLimitAddress())
0x108400082c92   172  4d390b         REX.W cmpq [r11],r9
0x108400082c95   175  0f8721000000   ja 0x108400082cbc  <+0x19c>
0x108400082c9b   17b  ba0c000000     movl rdx,0xc
0x108400082ca0   180  49ba200282724b560000 REX.W movq r10,0x564b72820220  (AllocateRegularInYoungGeneration)    ;; off heap target
0x108400082caa   18a  41ffd2         call r10
0x108400082cad   18d  488d78ff       REX.W leaq rdi,[rax-0x1]
0x108400082cb1   191  488b0dbdffffff REX.W movq rcx,[rip+0xffffffbd]
0x108400082cb8   198  4c8b45b0       REX.W movq r8,[rbp-0x50]
0x108400082cbc   19c  4c8d4f0c       REX.W leaq r9,[rdi+0xc]
0x108400082cc0   1a0  4c8909         REX.W movq [rcx],r9
0x108400082cc3   1a3  488d4f01       REX.W leaq rcx,[rdi+0x1]
0x108400082cc7   1a7  498bbd40010000 REX.W movq rdi,[r13+0x140] (root (heap_number_map))
0x108400082cce   1ae  8979ff         movl [rcx-0x1],rdi
0x108400082cd1   1b1  c4c1032ac0     vcvtlsi2sd xmm0,xmm15,r8
0x108400082cd6   1b6  c5fb114103     vmovsd [rcx+0x3],xmm0
0x108400082cdb   1bb  488bc1         REX.W movq rax,rcx
0x108400082cde   1be  e9bafeffff     jmp 0x108400082b9d  <+0x7d>
0x108400082ce3   1c3  90             nop
0x108400082ce4   1c4  49c7c500000000 REX.W movq r13,0x0
0x108400082ceb   1cb  e850f30300     call 0x1084000c2040     ;; eager deoptimization bailout
0x108400082cf0   1d0  49c7c501000000 REX.W movq r13,0x1
0x108400082cf7   1d7  e844f30300     call 0x1084000c2040     ;; eager deoptimization bailout
0x108400082cfc   1dc  49c7c502000000 REX.W movq r13,0x2
0x108400082d03   1e3  e838f30300     call 0x1084000c2040     ;; eager deoptimization bailout
0x108400082d08   1e8  49c7c503000000 REX.W movq r13,0x3
0x108400082d0f   1ef  e82cf30300     call 0x1084000c2040     ;; eager deoptimization bailout
0x108400082d14   1f4  49c7c504000000 REX.W movq r13,0x4
0x108400082d1b   1fb  e820f30300     call 0x1084000c2040     ;; eager deoptimization bailout
0x108400082d20   200  49c7c505000000 REX.W movq r13,0x5
0x108400082d27   207  e814f30700     call 0x108400102040     ;; lazy deoptimization bailout
0x108400082d2c   20c  49c7c506000000 REX.W movq r13,0x6
0x108400082d33   213  e808f30700     call 0x108400102040     ;; lazy deoptimization bailout

Source positions:
 pc offset  position
        f7         0

Inlined functions (count = 1)
 0x10840824fff5 <SharedFunctionInfo something>

Deoptimization Input Data (deopt points = 7)
 index  bytecode-offset    pc
     0               22    NA 
     1                2    NA 
     2               46    NA 
     3                2    NA 
     4               46    NA 
     5               27   120 
     6               27   14a 

Safepoints (size = 50)
0x108400082c40     120   200  10000010000000 (sp -> fp)       5
0x108400082c6a     14a   20c  10000000000000 (sp -> fp)       6
0x108400082cad     18d    NA  00000000000000 (sp -> fp)  <none>

RelocInfo (size = 34)
0x108400082b38  off heap target
0x108400082b52  off heap target
0x108400082c1b  full embedded object  (0x108408240cf5 <NativeContext[261]>)
0x108400082c25  external reference (Runtime::StackGuard)  (0x564b72061210)
0x108400082c35  off heap target
0x108400082c58  full embedded object  (0x108408240cf5 <NativeContext[261]>)
0x108400082c75  external reference (Heap::NewSpaceAllocationTopAddress())  (0x564b742fea68)
0x108400082c8a  external reference (Heap::NewSpaceAllocationLimitAddress())  (0x564b742fea70)
0x108400082ca2  off heap target
0x108400082cec  runtime entry  (eager deoptimization bailout)
0x108400082cf8  runtime entry  (eager deoptimization bailout)
0x108400082d04  runtime entry  (eager deoptimization bailout)
0x108400082d10  runtime entry  (eager deoptimization bailout)
0x108400082d1c  runtime entry  (eager deoptimization bailout)
0x108400082d28  runtime entry  (lazy deoptimization bailout)
0x108400082d34  runtime entry  (lazy deoptimization bailout)

--- End code ---
$ 

```

### Building Google Test
```console
$ mkdir lib
$ mkdir deps ; cd deps
$ git clone git@github.com:google/googletest.git
$ cd googletest/googletest
$ /usr/bin/clang++ --std=c++14 -Iinclude -I. -pthread -c src/gtest-all.cc
$ ar -rv libgtest-linux.a gtest-all.o 
$ cp libgtest-linux.a ../../../../lib/gtest
```

Linking issue:
```console
./lib/gtest/libgtest-linux.a(gtest-all.o):gtest-all.cc:function testing::internal::BoolFromGTestEnv(char const*, bool): error: undefined reference to 'std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >::c_str() const'
```
```
$ nm lib/gtest/libgtest-linux.a | grep basic_string | c++filt 
....
```
There are a lot of symbols listed above but the point is that in the object
file of `libgtest-linux.a` these symbols were compiled in. Now, when we compile
v8 and the tests we are using `-std=c++14` and we have to use the same when compiling
gtest. Lets try that. Just adding that does not help in this case. We need to
check which c++ headers are being used:
```console
$ /usr/bin/clang++ -print-search-dirs
programs: =/usr/bin:/usr/bin/../lib/gcc/x86_64-redhat-linux/9/../../../../x86_64-redhat-linux/bin
libraries: =/usr/lib64/clang/9.0.0:
            /usr/bin/../lib/gcc/x86_64-redhat-linux/9:
            /usr/bin/../lib/gcc/x86_64-redhat-linux/9/../../../../lib64:
            /usr/bin/../lib64:
            /lib/../lib64:
            /usr/lib/../lib64:
            /usr/bin/../lib/gcc/x86_64-redhat-linux/9/../../..:
            /usr/bin/../lib:
            /lib:/usr/lib
$ 
```
Lets search for the `string` header and inspect the namespace in that header:
```console
$ find /usr/ -name string
/usr/include/c++/9/debug/string
/usr/include/c++/9/experimental/string
/usr/include/c++/9/string
/usr/src/debug/gcc-9.2.1-1.fc31.x86_64/obj-x86_64-redhat-linux/x86_64-redhat-linux/libstdc++-v3/include/string
```
```console
$ vi /usr/include/c++/9/string
```
So this looks alright and thinking about this a little more I've been bitten
by the linking with different libc++ symbols issue (again). When we compile using Make we
are using the c++ headers that are shipped with v8 (clang libc++). Take the
string header for example in v8/buildtools/third_party/libc++/trunk/include/string
which is from clang's c++ library which does not use namespaces (__11 or __14 etc).

But when I compiled gtest did not specify the istystem include path and the
default would be used adding symbols with __11 into them. When the linker tries
to find these symbols it fails as it does not have any such symbols in the libraries
that it searches.

Create a simple test linking with the standard build of gtest to see if that
compiles and runs:
```console
$ /usr/bin/clang++ -std=c++14 -I./deps/googletest/googletest/include  -L$PWD/lib -g -O0 -o test/simple_test test/main.cc test/simple.cc lib/libgtest.a -lpthread
```
That worked and does not segfault. 

But when I run the version that is built using the makefile I get:
```console
lldb) target create "./test/persistent-object_test"
Current executable set to './test/persistent-object_test' (x86_64).
(lldb) r
Process 1024232 launched: '/home/danielbevenius/work/google/learning-v8/test/persistent-object_test' (x86_64)
warning: (x86_64) /lib64/libgcc_s.so.1 unsupported DW_FORM values: 0x1f20 0x1f21

[ FATAL ] Process 1024232 stopped
* thread #1, name = 'persistent-obje', stop reason = signal SIGSEGV: invalid address (fault address: 0x33363658)
    frame #0: 0x00007ffff7c0a7b0 libc.so.6`__GI___libc_free + 32
libc.so.6`__GI___libc_free:
->  0x7ffff7c0a7b0 <+32>: mov    rax, qword ptr [rdi - 0x8]
    0x7ffff7c0a7b4 <+36>: lea    rsi, [rdi - 0x10]
    0x7ffff7c0a7b8 <+40>: test   al, 0x2
    0x7ffff7c0a7ba <+42>: jne    0x7ffff7c0a7f0            ; <+96>
(lldb) bt
* thread #1, name = 'persistent-obje', stop reason = signal SIGSEGV: invalid address (fault address: 0x33363658)
  * frame #0: 0x00007ffff7c0a7b0 libc.so.6`__GI___libc_free + 32
    frame #1: 0x000000000042bb58 persistent-object_test`std::__1::basic_stringbuf<char, std::__1::char_traits<char>, std::__1::allocator<char> >::~basic_stringbuf(this=0x000000000046e908) at iosfwd:130:32
    frame #2: 0x000000000042ba4f persistent-object_test`std::__1::basic_stringstream<char, std::__1::char_traits<char>, std::__1::allocator<char> >::~basic_stringstream(this=0x000000000046e8f0, vtt=0x000000000044db28) at iosfwd:139:32
    frame #3: 0x0000000000420176 persistent-object_test`std::__1::basic_stringstream<char, std::__1::char_traits<char>, std::__1::allocator<char> >::~basic_stringstream(this=0x000000000046e8f0) at iosfwd:139:32
    frame #4: 0x000000000042bacc persistent-object_test`std::__1::basic_stringstream<char, std::__1::char_traits<char>, std::__1::allocator<char> >::~basic_stringstream(this=0x000000000046e8f0) at iosfwd:139:32
    frame #5: 0x0000000000427f4e persistent-object_test`testing::internal::scoped_ptr<std::__1::basic_stringstream<char, std::__1::char_traits<char>, std::__1::allocator<char> > >::reset(this=0x00007fffffffcee8, p=0x0000000000000000) at gtest-port.h:1216:9
    frame #6: 0x0000000000427ee9 persistent-object_test`testing::internal::scoped_ptr<std::__1::basic_stringstream<char, std::__1::char_traits<char>, std::__1::allocator<char> > >::~scoped_ptr(this=0x00007fffffffcee8) at gtest-port.h:1201:19
    frame #7: 0x000000000041f265 persistent-object_test`testing::Message::~Message(this=0x00007fffffffcee8) at gtest-message.h:89:18
    frame #8: 0x00000000004235ec persistent-object_test`std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > testing::internal::StreamableToString<int>(streamable=0x00007fffffffcf9c) at gtest-message.h:247:3
    frame #9: 0x000000000040d2bd persistent-object_test`testing::internal::FormatFileLocation(file="/home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest-internal-inl.h", line=663) at gtest-port.cc:946:28
    frame #10: 0x000000000041b7e2 persistent-object_test`testing::internal::GTestLog::GTestLog(this=0x00007fffffffd060, severity=GTEST_FATAL, file="/home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest-internal-inl.h", line=663) at gtest-port.cc:972:18
    frame #11: 0x000000000042242c persistent-object_test`testing::internal::UnitTestImpl::AddTestInfo(this=0x000000000046e480, set_up_tc=(persistent-object_test`testing::Test::SetUpTestCase() at gtest.h:427), tear_down_tc=(persistent-object_test`testing::Test::TearDownTestCase() at gtest.h:435), test_info=0x000000000046e320)(), void (*)(), testing::TestInfo*) at gtest-internal-inl.h:663:7
    frame #12: 0x000000000040d04f persistent-object_test`testing::internal::MakeAndRegisterTestInfo(test_case_name="Persistent", name="object", type_param=0x0000000000000000, value_param=0x0000000000000000, code_location=<unavailable>, fixture_class_id=0x000000000046d748, set_up_tc=(persistent-object_test`testing::Test::SetUpTestCase() at gtest.h:427), tear_down_tc=(persistent-object_test`testing::Test::TearDownTestCase() at gtest.h:435), factory=0x000000000046e300)(), void (*)(), testing::internal::TestFactoryBase*) at gtest.cc:2599:22
    frame #13: 0x00000000004048b8 persistent-object_test`::__cxx_global_var_init() at persistent-object_test.cc:5:1
    frame #14: 0x00000000004048e9 persistent-object_test`_GLOBAL__sub_I_persistent_object_test.cc at persistent-object_test.cc:0
    frame #15: 0x00000000004497a5 persistent-object_test`__libc_csu_init + 69
    frame #16: 0x00007ffff7ba512e libc.so.6`__libc_start_main + 126
    frame #17: 0x0000000000404eba persistent-object_test`_start + 42
```

### Google test (gtest) linking issue
This issue came up when linking a unit test with gtest:
```console
/usr/bin/ld: ./lib/gtest/libgtest-linux.a(gtest-all.o): in function `testing::internal::BoolFromGTestEnv(char const*, bool)':
/home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest-port.cc:1259: undefined reference to `std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::~basic_string()'
```
So this indicated that the object files in `libgtest-linux.a` where infact using
headers from libc++ and not libstc++. This was a really stupig mistake on my
part, I'd not specified the output file explicitly (-o) so this was getting
added into the current working directory, but the file included in the archive
was taken from within deps/googltest/googletest/ directory which was old and
compiled using libc++.

### Peristent cast-function-type
This issue was seen in Node.js when compiling with GCC. It can also been see
if building V8 using GCC and also enabling `-Wcast-function-type` in BUILD.gn:
```
      "-Wcast-function-type",
```
There are unit tests in V8 that also produce this warning, for example
`test/cctest/test-global-handles.cc`:
Original:
```console
g++ -MMD -MF obj/test/cctest/cctest_sources/test-global-handles.o.d -DV8_INTL_SUPPORT -DUSE_UDEV -DUSE_AURA=1 -DUSE_GLIB=1 -DUSE_NSS_CERTS=1 -DUSE_X11=1 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -DCR_SYSROOT_HASH=9c905c99558f10e19cc878b5dca1d4bd58c607ae -D_DEBUG -DDYNAMIC_ANNOTATIONS_ENABLED=1 -DENABLE_DISASSEMBLER -DV8_TYPED_ARRAY_MAX_SIZE_IN_HEAP=64 -DENABLE_GDB_JIT_INTERFACE -DENABLE_MINOR_MC -DOBJECT_PRINT -DV8_TRACE_MAPS -DV8_ENABLE_ALLOCATION_TIMEOUT -DV8_ENABLE_FORCE_SLOW_PATH -DV8_ENABLE_DOUBLE_CONST_STORE_CHECK -DV8_INTL_SUPPORT -DENABLE_HANDLE_ZAPPING -DV8_SNAPSHOT_NATIVE_CODE_COUNTERS -DV8_CONCURRENT_MARKING -DV8_ENABLE_LAZY_SOURCE_POSITIONS -DV8_CHECK_MICROTASKS_SCOPES_CONSISTENCY -DV8_EMBEDDED_BUILTINS -DV8_WIN64_UNWINDING_INFO -DV8_ENABLE_REGEXP_INTERPRETER_THREADED_DISPATCH -DV8_SNAPSHOT_COMPRESSION -DV8_ENABLE_CHECKS -DV8_COMPRESS_POINTERS -DV8_31BIT_SMIS_ON_64BIT_ARCH -DV8_DEPRECATION_WARNINGS -DV8_IMMINENT_DEPRECATION_WARNINGS -DV8_TARGET_ARCH_X64 -DV8_HAVE_TARGET_OS -DV8_TARGET_OS_LINUX -DDEBUG -DDISABLE_UNTRUSTED_CODE_MITIGATIONS -DV8_ENABLE_CHECKS -DV8_COMPRESS_POINTERS -DV8_31BIT_SMIS_ON_64BIT_ARCH -DV8_DEPRECATION_WARNINGS -DV8_IMMINENT_DEPRECATION_WARNINGS -DU_USING_ICU_NAMESPACE=0 -DU_ENABLE_DYLOAD=0 -DUSE_CHROMIUM_ICU=1 -DU_STATIC_IMPLEMENTATION -DICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_FILE -DUCHAR_TYPE=uint16_t -I../.. -Igen -I../../include -Igen/include -I../.. -Igen -I../../third_party/icu/source/common -I../../third_party/icu/source/i18n -I../../include -I../../tools/debug_helper -fno-strict-aliasing --param=ssp-buffer-size=4 -fstack-protector -funwind-tables -fPIC -pipe -B../../third_party/binutils/Linux_x64/Release/bin -pthread -m64 -march=x86-64 -Wno-builtin-macro-redefined -D__DATE__= -D__TIME__= -D__TIMESTAMP__= -Wall -Wno-unused-local-typedefs -Wno-maybe-uninitialized -Wno-deprecated-declarations -Wno-comments -Wno-packed-not-aligned -Wno-missing-field-initializers -Wno-unused-parameter -fno-omit-frame-pointer -g2 -Wno-strict-overflow -Wno-return-type -Wcast-function-type -O3 -fno-ident -fdata-sections -ffunction-sections -fvisibility=default -std=gnu++14 -Wno-narrowing -Wno-class-memaccess -fno-exceptions -fno-rtti --sysroot=../../build/linux/debian_sid_amd64-sysroot -c ../../test/cctest/test-global-handles.cc -o obj/test/cctest/cctest_sources/test-global-handles.o
In file included from ../../include/v8-inspector.h:14,
                 from ../../src/execution/isolate.h:15,
                 from ../../src/api/api.h:10,
                 from ../../src/api/api-inl.h:8,
                 from ../../test/cctest/test-global-handles.cc:28:
../../include/v8.h: In instantiation of ‘void v8::PersistentBase<T>::SetWeak(P*, typename v8::WeakCallbackInfo<P>::Callback, v8::WeakCallbackType) [with P = v8::Global<v8::Object>; T = v8::Object; typename v8::WeakCallbackInfo<P>::Callback = void (*)(const v8::WeakCallbackInfo<v8::Global<v8::Object> >&)]’:
../../test/cctest/test-global-handles.cc:292:47:   required from here
../../include/v8.h:10750:16: warning: cast between incompatible function types from ‘v8::WeakCallbackInfo<v8::Global<v8::Object> >::Callback’ {aka ‘void (*)(const v8::WeakCallbackInfo<v8::Global<v8::Object> >&)’} to ‘Callback’ {aka ‘void (*)(const v8::WeakCallbackInfo<void>&)’} [-Wcast-function-type]
10750 |                reinterpret_cast<Callback>(callback), type);
      |                ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
../../include/v8.h: In instantiation of ‘void v8::PersistentBase<T>::SetWeak(P*, typename v8::WeakCallbackInfo<P>::Callback, v8::WeakCallbackType) [with P = v8::internal::{anonymous}::FlagAndGlobal; T = v8::Object; typename v8::WeakCallbackInfo<P>::Callback = void (*)(const v8::WeakCallbackInfo<v8::internal::{anonymous}::FlagAndGlobal>&)]’:
../../test/cctest/test-global-handles.cc:493:53:   required from here
../../include/v8.h:10750:16: warning: cast between incompatible function types from ‘v8::WeakCallbackInfo<v8::internal::{anonymous}::FlagAndGlobal>::Callback’ {aka ‘void (*)(const v8::WeakCallbackInfo<v8::internal::{anonymous}::FlagAndGlobal>&)’} to ‘Callback’ {aka ‘void (*)(const v8::WeakCallbackInfo<void>&)’} [-Wcast-function-type]
```
Formatted for git commit message:
```console
g++ -MMD -MF obj/test/cctest/cctest_sources/test-global-handles.o.d 
...
In file included from ../../include/v8-inspector.h:14,
                 from ../../src/execution/isolate.h:15,
                 from ../../src/api/api.h:10,
                 from ../../src/api/api-inl.h:8,
                 from ../../test/cctest/test-global-handles.cc:28:
../../include/v8.h:
In instantiation of ‘void v8::PersistentBase<T>::SetWeak(
    P*,
    typename v8::WeakCallbackInfo<P>::Callback,
    v8::WeakCallbackType)
[with 
  P = v8::Global<v8::Object>; 
  T = v8::Object;
  typename v8::WeakCallbackInfo<P>::Callback =
  void (*)(const v8::WeakCallbackInfo<v8::Global<v8::Object> >&)
]’:
../../test/cctest/test-global-handles.cc:292:47:   required from here
../../include/v8.h:10750:16: warning:
cast between incompatible function types from
‘v8::WeakCallbackInfo<v8::Global<v8::Object> >::Callback’ {aka
‘void (*)(const v8::WeakCallbackInfo<v8::Global<v8::Object> >&)’} to 
‘Callback’ {aka ‘void (*)(const v8::WeakCallbackInfo<void>&)’}
[-Wcast-function-type]
10750 |                reinterpret_cast<Callback>(callback), type);
      |                ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
```
This commit suggests adding a pragma specifically for GCC to suppress
this warning. The motivation for this is that there were quite a few
of these warnings in the Node.js build, but these have been suppressed
by adding a similar pragma but around the include of v8.h [1].

[1] https://github.com/nodejs/node/blob/331d63624007be4bf49d6d161bdef2b5e540affa/src/node.h#L63-L70

```console
$ 
In file included from persistent-obj.cc:8:
/home/danielbevenius/work/google/v8_src/v8/include/v8.h: In instantiation of ‘void v8::PersistentBase<T>::SetWeak(P*, typename v8::WeakCallbackInfo<P>::Callback, v8::WeakCallbackType) [with P = Something; T = v8::Object; typename v8::WeakCallbackInfo<P>::Callback = void (*)(const v8::WeakCallbackInfo<Something>&)]’:

persistent-obj.cc:57:38:   required from here
/home/danielbevenius/work/google/v8_src/v8/include/v8.h:10750:16: warning: cast between incompatible function types from ‘v8::WeakCallbackInfo<Something>::Callback’ {aka ‘void (*)(const v8::WeakCallbackInfo<Something>&)’} to ‘Callback’ {aka ‘void (*)(const v8::WeakCallbackInfo<void>&)’} [-Wcast-function-type]
10750 |                reinterpret_cast<Callback>(callback), type);
      |                ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
```
Currently, we have added a pragma to avoid this warning in node.js but we'd like
to add this in v8 and closer to the actual code that is causing it. In node we
have to set the praga on the header.
```c++
template <class T>
template <typename P>
V8_INLINE void PersistentBase<T>::SetWeak(
    P* parameter,
    typename WeakCallbackInfo<P>::Callback callback,
    WeakCallbackType type) {
  typedef typename WeakCallbackInfo<void>::Callback Callback;
  V8::MakeWeak(reinterpret_cast<internal::Address*>(this->val_), parameter,
               reinterpret_cast<Callback>(callback), type);
}
```
Notice the second parameter is `typename WeakCallbackInfo<P>::Callback` which is
a typedef:
```c++
  typedef void (*Callback)(const WeakCallbackInfo<T>& data);
```
This is a function declaration for `Callback` which is a function that takes
a reference to a const WeakCallbackInfo<T> and returns void. So we could define
it like this:
```c++
void WeakCallback(const v8::WeakCallbackInfo<Something>& data) {
  Something* obj = data.GetParameter();
  std::cout << "in make weak callback..." << '\n';
}
```
And the trying to cast it into:
```c++
  typedef typename v8::WeakCallbackInfo<void>::Callback Callback;
  Callback cb = reinterpret_cast<Callback>(WeakCallback);
```
This is done as V8::MakeWeak has the following signature:
```c++
void V8::MakeWeak(i::Address* location, void* parameter,
                  WeakCallbackInfo<void>::Callback weak_callback,
                  WeakCallbackType type) {
  i::GlobalHandles::MakeWeak(location, parameter, weak_callback, type);
}
```


### gdb warnings
```console
warning: Could not find DWO CU obj/v8_compiler/common-node-cache.dwo(0x42b8adb87d74d56b) referenced by CU at offset 0x206f7 [in module /home/danielbevenius/work/google/learning-v8/hello-world]
```
This can be worked around by specifying the `--cd` argument to gdb:
```console
$ gdb --cd=/home/danielbevenius/work/google/v8_src/v8/out/x64.release --args /home/danielbevenius/work/google/learning-v8/hello-world
```

### Building with g++
Update args.gn to include:
```
is_clang = false
```
Next I got the following error when trying to compile:
```console
$ ninja -v -C out/x64.release/ obj/test/cctest/cctest_sources/test-global-handles.o
ux/debian_sid_amd64-sysroot -fexceptions -frtti -c ../../src/torque/instance-type-generator.cc -o obj/torque_base/instance-type-generator.o
In file included from /usr/include/c++/9/bits/stl_algobase.h:59,
                 from /usr/include/c++/9/memory:62,
                 from ../../src/torque/implementation-visitor.h:8,
                 from ../../src/torque/instance-type-generator.cc:5:
/usr/include/c++/9/x86_64-redhat-linux/bits/c++config.h:3:10: fatal error: bits/wordsize.h: No such file or directory
    3 | #include <bits/wordsize.h>
      |          ^~~~~~~~~~~~~~~~~
compilation terminated.
ninja: build stopped: subcommand failed.
```
```console
$ export CPATH=/usr/include
```

```console
third_party/binutils/Linux_x64/Release/bin/ld.gold: error: cannot open /usr/lib64/libatomic.so.1.2.0: No such file or directory
```
```console
$ sudo dnf install -y libatomic
```
I still got an error because of a warning but I'm trying to build using:
```
treat_warnings_as_errors = false
```
Lets see how that works out. I also had to use gnus linker by disableing 
gold:
```console
use_gold = false
```

### CodeStubAssembler
This history of this is that JavaScript builtins used be written in assembly
which gave very good performance but made porting V8 to different architectures
more difficult as these builtins had to have specific implementations for each
supported architecture, so it dit not scale very well. With the addition of features
to the JavaScript specifications having to support new features meant having to
implement them for all platforms which made it difficult to keep up and deliver
these new features.

The goal is to have the perfomance of handcoded assembly but not have to write
it for every platform. So a portable assembly language was build on top of
Tubofans backend. This is an API that generates Turbofan's machine-level IR.
This IR can be used by Turbofan to produce very good machine code on all platforms.
So one "only" has to implement one component/function/feature (not sure what to
call this) and then it can be made available to all platforms. They no longer
have to maintain all that handwritten assembly.

Just to be clear CSA is a C++ API that is used to generate IR which is then
compiled in to machine code for the target instruction set architectur.

### Torque
[Torque](https://v8.dev/docs/torque) is a DLS language to avoid having to use
the CodeStubAssembler directly (it is still used behind the scene). This language
is statically typed, garbage collected, and compatible with JavaScript.

The JavaScript standard library was implemented in V8 previously using hand
written assembly. But as we mentioned in the previous section this did not scale.

It could have been written in JavaScript too, and I think this was done in the
past but this has some issues as builtins would need warmup time to become
optimized, there were also issues with monkey-patching and exposing VM internals
unintentionally.

Is torque run a build time, I'm thinking yes as it would have to generate the
c++ code.

There is a main function in torque.cc which will be built into an executable
```console
$ ./out/x64.release_gcc/torque --help
Unexpected command-line argument "--help", expected a .tq file.
```

The files that are processed by torque are defined in BUILD.gc in the 
`torque_files` section. There is also a template named `run_torque`.
I've noticed that this template and others in GN use the script  `tools/run.py`.
This is apperently because GN can only execute scripts at the moment and what this
script does is use python to create a subprocess with the passed in argument:
```console
$ gn help action
```
And a template is way to reuse code in GN.


There is a make target that shows what is generated by torque:
```console
$ make torque-example
```
This will create a directory in the current directory named `gen/torque-generated`.
Notice that this directory contains c++ headers and sources.

It take [torque-example.tq](./torque-example.tq) as input. For this file the
following header will be generated:
```c++
#ifndef V8_GEN_TORQUE_GENERATED_TORQUE_EXAMPLE_TQ_H_                            
#define V8_GEN_TORQUE_GENERATED_TORQUE_EXAMPLE_TQ_H_                            
                                                                                
#include "src/builtins/builtins-promise.h"                                      
#include "src/compiler/code-assembler.h"                                        
#include "src/codegen/code-stub-assembler.h"                                    
#include "src/utils/utils.h"                                                    
#include "torque-generated/field-offsets-tq.h"                                  
#include "torque-generated/csa-types-tq.h"                                      
                                                                                
namespace v8 {                                                                  
namespace internal {                                                            
                                                                                
void HelloWorld_0(compiler::CodeAssemblerState* state_);                        

}  // namespace internal                                                        
}  // namespace v8                                                              
                                                                                
#endif  // V8_GEN_TORQUE_GENERATED_TORQUE_EXAMPLE_TQ_H_

```
This is only to show the generated files and make it clear that torque will
generate these file which will then be compiled during the v8 build. So, lets
try copying `example-torque.tq` to v8/src/builtins directory.
```console
$ cp torque-example.tq ../v8_src/v8/src/builtins/
```
This is not enough to get it included in the build, we have to update BUILD.gn
and add this file to the `torque_files` list. After running the build we can
see that there is a file named `src/builtins/torque-example-tq-csa.h` generated
along with a .cc.

To understand how this works I'm going to use https://v8.dev/docs/torque-builtins
as a starting point:
```
  transitioning javascript builtin                                              
  MathIs42(js-implicit context: NativeContext, receiver: JSAny)(x: JSAny): Boolean {
    const number: Number = ToNumber_Inline(x);                                  
    typeswitch (number) {                                                       
      case (smi: Smi): {                                                        
        return smi == 42 ? True : False;                                        
      }                                                                         
      case (heapNumber: HeapNumber): {                                          
        return Convert<float64>(heapNumber) == 42 ? True : False;               
      }                                                                         
    }                                                                           
  }                   
```
This has been updated to work with the latest V8 version.

Next, we need to update `src/init/bootstrappers.cc` to add/install this function
on the math object:
```c++
  SimpleInstallFunction(isolate_, math, "is42", Builtins::kMathIs42, 1, true);
```
After this we need to rebuild v8:
```console
$ env CPATH=/usr/include ninja -v -C out/x64.release_gcc
```
```console
$ d8
d8> Math.is42(42)
true
d8> Math.is42(2)
false
```

If we look at the generated code that Torque has produced in 
`out/x64.release_gcc/gen/torque-generated/src/builtins/math-tq-csa.cc` (we can
run it through the preprocessor using):
```console
$ clang++ --sysroot=build/linux/debian_sid_amd64-sysroot -isystem=./buildtools/third_party/libc++/trunk/include -isystem=buildtools/third_party/libc++/trunk/include -I. -E out/x64.release_gcc/gen/torque-generated/src/builtins/math-tq-csa.cc > math.cc.pp
```
If we open math.cc.pp and search for `Is42` we can find:
```c++
class MathIs42Assembler : public CodeStubAssembler {                            
 public:                                                                        
  using Descriptor = Builtin_MathIs42_InterfaceDescriptor;                      
  explicit MathIs42Assembler(compiler::CodeAssemblerState* state) : CodeStubAssembler(state) {}
  void GenerateMathIs42Impl();                                                  
  Node* Parameter(Descriptor::ParameterIndices index) {                         
    return CodeAssembler::Parameter(static_cast<int>(index));                   
  }                                                                             
};                                                                              
                                                                                
void Builtins::Generate_MathIs42(compiler::CodeAssemblerState* state) {         
  MathIs42Assembler assembler(state);                                           
  state->SetInitialDebugInformation("MathIs42", "out/x64.release_gcc/gen/torque-generated/src/builtins/math-tq-csa.cc", 2121);
  if (Builtins::KindOf(Builtins::kMathIs42) == Builtins::TFJ) {                 
    assembler.PerformStackCheck(assembler.GetJSContextParameter());             
  }                                                                             
  assembler.GenerateMathIs42Impl();                                             
}                                                                               
                                                                                
void MathIs42Assembler::GenerateMathIs42Impl() {     
  ...
```
So this is what gets generated by the Torque compiler and what we see
above is CodeStubAssemble class. 

If we take a look in out/x64.release_gcc/gen/torque-generated/builtin-definitions-tq.h
we can find the following line that has been generated:
```c++
TFJ(MathIs42, 1, kReceiver, kX) \                                               
```
Now, there is a section about the [TF_BUILTIN](#tf_builtin) macro, and it will
create function declarations, and function and class definitions:

Now, in src/builtins/builtins.h we have the following macros:
```c++
class Builtins {
 public:

  enum Name : int32_t {
#define DEF_ENUM(Name, ...) k##Name,                                            
    BUILTIN_LIST(DEF_ENUM, DEF_ENUM, DEF_ENUM, DEF_ENUM, DEF_ENUM, DEF_ENUM,    
                 DEF_ENUM)                                                      
#undef DEF_ENUM 
    ...
  }

#define DECLARE_TF(Name, ...) \                                                 
  static void Generate_##Name(compiler::CodeAssemblerState* state);             
                                                                                
  BUILTIN_LIST(IGNORE_BUILTIN, DECLARE_TF, DECLARE_TF, DECLARE_TF, DECLARE_TF,  
               IGNORE_BUILTIN, DECLARE_ASM)
```
And `BUILTINS_LIST` is declared in src/builtins/builtins-definitions.h and this
file includes:
```c++
#include "torque-generated/builtin-definitions-tq.h"

#define BUILTIN_LIST(CPP, TFJ, TFC, TFS, TFH, BCH, ASM)  \                          
  BUILTIN_LIST_BASE(CPP, TFJ, TFC, TFS, TFH, ASM)        \                          
  BUILTIN_LIST_FROM_TORQUE(CPP, TFJ, TFC, TFS, TFH, ASM) \                          
  BUILTIN_LIST_INTL(CPP, TFJ, TFS)                       \                          
  BUILTIN_LIST_BYTECODE_HANDLERS(BCH)     
```
Notice `BUILTIN_LIST_FROM_TORQUE`, this is how our MathIs42 gets included from
builtin-definitions-tq.h. This is in turn included by builtins.h.

If we take a look at the this header after it has gone through the preprocessor
we can see what has been generated for MathIs42:
```console
$ clang++ --sysroot=build/linux/debian_sid_amd64-sysroot -isystem=./buildtools/third_party/libc++/trunk/include -isystem=buildtools/third_party/libc++/trunk/include -I. -I./out/x64.release_gcc/gen/ -E src/builtins/builtins.h > builtins.h.pp
```
First MathIs42 will be come a member in the Name enum of the Builtins class:
```c++
class Builtins {
 public:

  enum Name : int32_t { 
    ...
    kMathIs42,
  };

  static void Generate_MathIs42(compiler::CodeAssemblerState* state); 
```
We should also take a look in `src/builtins/builtins-descriptors.h` as the BUILTIN_LIST
is used there two and specifically to our current example there is a
`DEFINE_TFJ_INTERFACE_DESCRIPTOR` macro used:
```c++
BUILTIN_LIST(IGNORE_BUILTIN, DEFINE_TFJ_INTERFACE_DESCRIPTOR,
             DEFINE_TFC_INTERFACE_DESCRIPTOR, DEFINE_TFS_INTERFACE_DESCRIPTOR,
             DEFINE_TFH_INTERFACE_DESCRIPTOR, IGNORE_BUILTIN,
             DEFINE_ASM_INTERFACE_DESCRIPTOR)

#define DEFINE_TFJ_INTERFACE_DESCRIPTOR(Name, Argc, ...)                \
  struct Builtin_##Name##_InterfaceDescriptor {                         \
    enum ParameterIndices {                                             \
      kJSTarget = compiler::CodeAssembler::kTargetParameterIndex,       \
      ##__VA_ARGS__,                                                    \
      kJSNewTarget,                                                     \
      kJSActualArgumentsCount,                                          \
      kContext,                                                         \
      kParameterCount,                                                  \
    };                                                                  \
  }; 
```
So the above will generate the following code but this time for builtins.cc:
```console
$ clang++ --sysroot=build/linux/debian_sid_amd64-sysroot -isystem=./buildtools/third_party/libc++/trunk/include -isystem=buildtools/third_party/libc++/trunk/include -I. -I./out/x64.release_gcc/gen/ -E src/builtins/builtins.cc > builtins.cc.pp
```

```c++
struct Builtin_MathIs42_InterfaceDescriptor { 
  enum ParameterIndices { 
    kJSTarget = compiler::CodeAssembler::kTargetParameterIndex,
    kReceiver,
    kX,
    kJSNewTarget,
    kJSActualArgumentsCount,
    kContext,
    kParameterCount,
  };

const BuiltinMetadata builtin_metadata[] = {
  ...
  {"MathIs42", Builtins::TFJ, {1, 0}}
  ...
};
```
BuiltinMetadata is a struct defined in builtins.cc and in our case the name 
is passed, then the type, and the last struct is specifying the number of parameters
and the last 0 is unused as far as I can tell and only there make it different
from the constructor that takes an Address parameter.

So, where is `Generate_MathIs42` used:
```c++
void SetupIsolateDelegate::SetupBuiltinsInternal(Isolate* isolate) {
  Code code;
  ...
  code = BuildWithCodeStubAssemblerJS(isolate, index, &Builtins::Generate_MathIs42, 1, "MathIs42");
  AddBuiltin(builtins, index++, code);
  ...
```
`BuildWithCodeStubAssemblerJS` can be found in `src/builtins/setup-builtins-internal.cc`
```c++
Code BuildWithCodeStubAssemblerJS(Isolate* isolate, int32_t builtin_index,
                                  CodeAssemblerGenerator generator, int argc,
                                  const char* name) {
  Zone zone(isolate->allocator(), ZONE_NAME);
  const int argc_with_recv = (argc == kDontAdaptArgumentsSentinel) ? 0 : argc + 1;
  compiler::CodeAssemblerState state(
      isolate, &zone, argc_with_recv, Code::BUILTIN, name,
      PoisoningMitigationLevel::kDontPoison, builtin_index);
  generator(&state);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(
      &state, BuiltinAssemblerOptions(isolate, builtin_index));
  return *code;
```
Lets add a conditional break point so that we can stop in this function when
`MathIs42` is passed in:
```console
(gdb) br setup-builtins-internal.cc:161
(gdb) cond 1 ((int)strcmp(name, "MathIs42")) == 0
```
We can see that we first create a new `CodeAssemblerState`, which we say previously
was that type that the `Generate_MathIs42` function takes. TODO: look into this class
a litte more.
After this `generator` will be called with the newly created state passed in:
```console 
(gdb) p generator
$8 = (v8::internal::(anonymous namespace)::CodeAssemblerGenerator) 0x5619fd61b66e <v8::internal::Builtins::Generate_MathIs42(v8::internal::compiler::CodeAssemblerState*)>
```
TODO: Take a closer look at generate and how that code works. 
After generate returns we will have the following call:
```c++
  generator(&state);                                                               
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(                       
      &state, BuiltinAssemblerOptions(isolate, builtin_index));                    
  return *code;
```
Then next thing that will happen is the code returned will be added to the builtins
by calling `SetupIsolateDelegate::AddBuiltin`:
```c++
void SetupIsolateDelegate::AddBuiltin(Builtins* builtins, int index, Code code) {
  builtins->set_builtin(index, code);                                           
} 
```
`set_builtins` can be found in src/builtins/builtins.cc` and looks like this:
```c++
void Builtins::set_builtin(int index, Code builtin) {                           
  isolate_->heap()->set_builtin(index, builtin);                                
}
```
And Heap::set_builtin does:
```c++
 void Heap::set_builtin(int index, Code builtin) {
  isolate()->builtins_table()[index] = builtin.ptr();
}
```
So this is how the builtins_table is populated.
 
And when is `SetupBuiltinsInternal` called?  
It is called from `SetupIsolateDelegat::SetupBuiltins` which is called from Isolate::Init.

Just to recap before I loose track of what is going on...We have math.tq, which
is the torque source file. This is parsed by the torque compiler/parser and it
will generate c++ headers and source files, one of which will be a
CodeStubAssembler class for our MathI42 function. It will also generate the 
"torque-generated/builtin-definitions-tq.h. 
After this has happened the sources need to be compiled into object files. After
that if a snapshot is configured to be created, mksnapshot will create a new
Isolate and in that process the MathIs42 builtin will get added. Then a context will
be created and saved. The snapshot can then be deserialized into an Isoalte as
some later point.

Alright, so we have seen what gets generated for the function MathIs42 but how
does this get "hooked" but to enable us to call `Math.is42(11)`?  

In bootstrapper.cc we can see a number of lines:
```c++
 SimpleInstallFunction(isolate_, math, "trunc", Builtins::kMathTrunc, 1, true); 
```
And we are going to add a line like the following:
```c++
 SimpleInstallFunction(isolate_, math, "is42", Builtins::kMathIs42, 1, true);
```
The signature for `SimpleInstallFunction` looks like this
```c++
V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
    Isolate* isolate, Handle<JSObject> base, const char* name,
    Builtins::Name call, int len, bool adapt,
    PropertyAttributes attrs = DONT_ENUM) {
  Handle<String> internalized_name = isolate->factory()->InternalizeUtf8String(name);
  Handle<JSFunction> fun = SimpleCreateFunction(isolate, internalized_name, call, len, adapt);       
  JSObject::AddProperty(isolate, base, internalized_name, fun, attrs);          
  return fun;                                                                   
} 
```
So we see that the function is added as a property to the Math object.
Notice that we also have to add `kMathIs42` to the Builtins class which is now
part of the builtins_table_ array which we went through above.

#### Transitioning/Transient
In torgue source files we can sometimes see types declared as `transient`, and
functions that have a `transitioning` specifier. In V8 HeapObjects can change
at runtime (I think an example of this would be deleting an element in an array
which would transition it to a different type of array HoleyElementArray or
something like that. TODO: verify and explain this). And a function that calls
JavaScript which cause such a transition is marked with transitioning.


#### Callables
Are like functions is js/c++ but have some additional capabilities and there
are several different types of callables:

##### macro callables
These correspond to generated CodeStubAssebler C++ that will be inlined at
the callsite.

##### builtin callables
These will become V8 builtins with info added to builtin-definitions.h (via
the include of torque-generated/builtin-definitions-tq.h). There is only one
copy of this and this will be a call instead of being inlined as is the case
with macros.

##### runtime callables

##### intrinsic callables

#### Explicit parameters
macros and builtins can have parameters. For example:
```
@export
macro HelloWorld1(msg: JSAny) {
  Print(msg);
}
```
And we can call this from another macro like this:
```
@export
macro HelloWorld() {
  HelloWorld1('Hello World');
}
```

#### Implicit parameters
In the previous section we showed explicit parameters but we can also have
implicit parameters:
```
@export
macro HelloWorld2(implicit msg: JSAny)() {
  Print(msg);
}
@export
macro HelloWorld() {
  const msg = 'Hello implicit';
  HelloWorld2();
}
```

### Troubleshooting
Compilation error when including `src/objects/objects-inl.h:
```console
/home/danielbevenius/work/google/v8_src/v8/src/objects/object-macros.h:263:14: error: no declaration matches ‘bool v8::internal::HeapObject::IsJSCollator() const’
```
Does this need i18n perhaps?
```console
$ gn args --list out/x64.release_gcc | grep i18n
v8_enable_i18n_support
```

```console
usr/bin/ld: /tmp/ccJOrUMl.o: in function `v8::internal::MaybeHandle<v8::internal::Object>::Check() const':
/home/danielbevenius/work/google/v8_src/v8/src/handles/maybe-handles.h:44: undefined reference to `V8_Fatal(char const*, ...)'
collect2: error: ld returned 1 exit status
```
V8_Fatal is referenced but not defined in v8_monolith.a:
```console
$ nm libv8_monolith.a | grep V8_Fatal | c++filt 
...
U V8_Fatal(char const*, int, char const*, ...)
```
And I thought it might be defined in libv8_libbase.a but it is the same there.
Actually, I was looking at the wrong symbol. This was not from the logging.o 
object file. If we look at it we find:
```console
v8_libbase/logging.o:
...
0000000000000000 T V8_Fatal(char const*, int, char const*, ...)
```
In out/x64.release/obj/logging.o we can find it defined:
```console
$ nm -C  libv8_libbase.a | grep -A 50 logging.o | grep V8_Fatal
0000000000000000 T V8_Fatal(char const*, int, char const*, ...)
```
`T` means that the symbol is in the text section.
So if the linker is able to find libv8_libbase.a it should be able to resolve
this.

So we need to make sure the linker can find the directory where the libraries
are located ('-Wl,-Ldir'), and also that it will include the library ('-Wl,-llibname')

With this in place I can see that the linker can open the archive:
```console
attempt to open /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/obj/libv8_libbase.so failed
attempt to open /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/obj/libv8_libbase.a succeeded
/home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/obj/libv8_libbase.a
```
But I'm still getting the same linking error. If we look closer at the error message
we can see that it is maybe-handles.h that is complaining. Could it be that the
order is incorrect when linking. libv8_libbase.a needs to come after libv8_monolith
Something I noticed is that even though the library libv8_libbase.a is found it
does not look like the linker actually reads the object files. I can see that it
does this for libv8_monolith.a:
```console
(/home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/obj/libv8_monolith.a)common-node-cache.o
```
Hmm, actually looking at the signature of the function it is V8_Fatal(char const*, ...)
and not char const*, int, char const*, ...)

For a debug build it will be:
```
    void V8_Fatal(const char* file, int line, const char* format, ...);
```
And else
```
    void V8_Fatal(const char* format, ...);
```
So it looks like I need to set debug to false. With this the V8_Fatal symbol
in logging.o is:
```console
$ nm -C out/x64.release_gcc/obj/v8_libbase/logging.o | grep V8_Fatal
0000000000000000 T V8_Fatal(char const*, ...)
```


### V8 Build artifacts
What is actually build when you specify 
v8_monolithic:
When this type is chosen the build cannot be a component build, there is an
assert for this. In this case a static library build:
```
if (v8_monolithic) {                                                            
  # A component build is not monolithic.                                        
  assert(!is_component_build)                                                   
                                                                                
  # Using external startup data would produce separate files.                   
  assert(!v8_use_external_startup_data)                                         
  v8_static_library("v8_monolith") {                                            
    deps = [                                                                    
      ":v8",                                                                    
      ":v8_libbase",                                                            
      ":v8_libplatform",                                                        
      ":v8_libsampler",                                                         
      "//build/win:default_exe_manifest",                                       
    ]                                                                           
                                                                                
    configs = [ ":internal_config" ]                                            
  }                                                                             
}
```
Notice that the builtin function is called `static_library` so is a template
that can be found in `gni/v8.gni` 

v8_static_library:
This will use source_set instead of creating a static library when compiling.
When set to false, the object files that would be included in the linker command.
The can speed up the build as the creation of the static libraries is skipped.
But this does not really help when linking to v8 externally as from this project.

is_component_build:
This will compile targets declared as components as shared libraries.
All the v8_components in BUILD.gn will be built as .so files in the output
director (not the obj directory which is the case for static libraries).

So the only two options are the v8_monolith or is_component_build where it
might be an advantage of being able to build a single component and not have
to rebuild the whole monolith at times.

### wee8
`libwee8` can be produced which is a library which only supports WebAssembly
and does not support JavaScript.
```console
$ ninja -C out/wee8 wee8
```


### V8 Internal Isolate
`src/execution/isolate.h` is where you can find the v8::internal::Isolate.
```c++
class V8_EXPORT_PRIVATE Isolate final : private HiddenFactory {

```
And HiddenFactory is just to allow Isolate to inherit privately from Factory
which can be found in src/heap/factory.h.

### Startup Walk through
This section will walk through the start up on V8 by using the hello_world example
in this project:
```console
$ LD_LIBRARY_PATH=../v8_src/v8/out/x64.release_gcc/ lldb ./hello-world
(lldb) br s -n main
Breakpoint 1: where = hello-world`main + 25 at hello-world.cc:41:38, address = 0x0000000000402821
```
```console
    V8::InitializeExternalStartupData(argv[0]);
```
This call will land in `api.cc` which will just delegate the call to and internal
(internal namespace that is). If you try to step into this function you will
just land on the next line in hello_world. This is because we compiled v8 without
external start up data so this function will be empty:
```console
$ objdump -Cd out/x64.release_gcc/obj/v8_base_without_compiler/startup-data-util.o
Disassembly of section .text._ZN2v88internal37InitializeExternalStartupDataFromFileEPKc:

0000000000000000 <v8::internal::InitializeExternalStartupDataFromFile(char const*)>:
   0:	c3                   	retq
```
Next, we have:
```console
    std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
```
This will land in `src/libplatform/default-platform.cc` which will create a new
DefaultPlatform.

```c++
Isolate* isolate = Isolate::New(create_params);
```
This will call Allocate: 
```c++
Isolate* isolate = Allocate();
```
```c++
Isolate* Isolate::Allocate() {
  return reinterpret_cast<Isolate*>(i::Isolate::New());
}
```

Remember that the internal Isolate can be found in `src/execution/isolate.h`.
In `src/execution/isolate.cc` we find `Isolate::New`
```c++
Isolate* Isolate::New(IsolateAllocationMode mode) {
  std::unique_ptr<IsolateAllocator> isolate_allocator = std::make_unique<IsolateAllocator>(mode);
  void* isolate_ptr = isolate_allocator->isolate_memory();
  Isolate* isolate = new (isolate_ptr) Isolate(std::move(isolate_allocator));
```
So we first create an IsolateAllocator instance which will allocate memory for
a single Isolate instance. This is then passed into the Isolate constructor,
notice the usage of `new` here, this is just a normal heap allocation. 

The default new operator has been deleted and an override provided that takes
a void pointer, which is just returned: 
```c++
  void* operator new(size_t, void* ptr) { return ptr; }
  void* operator new(size_t) = delete;
  void operator delete(void*) = delete;
```
In this case it just returns the memory allocateed by isolate-memory().
The reason for doing this is that using the new operator not only invokes the
new operator but the compiler will also add a call the types constructor passing
in the address of the allocated memory.
```c++
Isolate::Isolate(std::unique_ptr<i::IsolateAllocator> isolate_allocator)
    : isolate_data_(this),
      isolate_allocator_(std::move(isolate_allocator)),
      id_(isolate_counter.fetch_add(1, std::memory_order_relaxed)),
      allocator_(FLAG_trace_zone_stats
                     ? new VerboseAccountingAllocator(&heap_, 256 * KB)
                     : new AccountingAllocator()),
      builtins_(this),
      rail_mode_(PERFORMANCE_ANIMATION),
      code_event_dispatcher_(new CodeEventDispatcher()),
      jitless_(FLAG_jitless),
#if V8_SFI_HAS_UNIQUE_ID
      next_unique_sfi_id_(0),
#endif
      cancelable_task_manager_(new CancelableTaskManager()) {
```
Notice that  `isolate_data_` will be populated by calling the constructor which
takes an pointer to an Isolate.
```c++
class IsolateData final {
 public:
  explicit IsolateData(Isolate* isolate) : stack_guard_(isolate) {}
```

Back in Isolate's constructor we have:
```c++
#define ISOLATE_INIT_LIST(V)                                                   \
  /* Assembler state. */                                                       \
  V(FatalErrorCallback, exception_behavior, nullptr)                           \
  ...

#define ISOLATE_INIT_EXECUTE(type, name, initial_value) \                           
  name##_ = (initial_value);                                                        
  ISOLATE_INIT_LIST(ISOLATE_INIT_EXECUTE)                                           
#undef ISOLATE_INIT_EXECUTE
```
So lets expand the first entry to understand what is going on:
```c++
   exception_behavior_ = (nullptr);
   oom_behavior_ = (nullptr);
   event_logger_ = (nullptr);
   allow_code_gen_callback_ = (nullptr);
   modify_code_gen_callback_ = (nullptr);
   allow_wasm_code_gen_callback_ = (nullptr);
   wasm_module_callback_ = (&NoExtension);
   wasm_instance_callback_ = (&NoExtension);
   wasm_streaming_callback_ = (nullptr);
   wasm_threads_enabled_callback_ = (nullptr);
   wasm_load_source_map_callback_ = (nullptr);
   relocatable_top_ = (nullptr);
   string_stream_debug_object_cache_ = (nullptr);
   string_stream_current_security_token_ = (Object());
   api_external_references_ = (nullptr);
   external_reference_map_ = (nullptr);
   root_index_map_ = (nullptr);
   default_microtask_queue_ = (nullptr);
   turbo_statistics_ = (nullptr);
   code_tracer_ = (nullptr);
   per_isolate_assert_data_ = (0xFFFFFFFFu);
   promise_reject_callback_ = (nullptr);
   snapshot_blob_ = (nullptr);
   code_and_metadata_size_ = (0);
   bytecode_and_metadata_size_ = (0);
   external_script_source_size_ = (0);
   is_profiling_ = (false);
   num_cpu_profilers_ = (0);
   formatting_stack_trace_ = (false);
   debug_execution_mode_ = (DebugInfo::kBreakpoints);
   code_coverage_mode_ = (debug::CoverageMode::kBestEffort);
   type_profile_mode_ = (debug::TypeProfileMode::kNone);
   last_stack_frame_info_id_ = (0);
   last_console_context_id_ = (0);
   inspector_ = (nullptr);
   next_v8_call_is_safe_for_termination_ = (false);
   only_terminate_in_safe_scope_ = (false);
   detailed_source_positions_for_profiling_ = (FLAG_detailed_line_info);
   embedder_wrapper_type_index_ = (-1);
   embedder_wrapper_object_index_ = (-1);
```
So all of the entries in this list will become private members of the
Isolate class after the preprocessor is finished. There will also be public
assessor to get and set these initial values values (which is the last entry
in the ISOLATE_INIT_LIST above.

Back in isolate.cc constructor we have:
```c++
#define ISOLATE_INIT_ARRAY_EXECUTE(type, name, length) \
  memset(name##_, 0, sizeof(type) * length);
  ISOLATE_INIT_ARRAY_LIST(ISOLATE_INIT_ARRAY_EXECUTE)
#undef ISOLATE_INIT_ARRAY_EXECUTE
#define ISOLATE_INIT_ARRAY_LIST(V)                                             \
  /* SerializerDeserializer state. */                                          \
  V(int32_t, jsregexp_static_offsets_vector, kJSRegexpStaticOffsetsVectorSize) \
  ...

  InitializeDefaultEmbeddedBlob();
  MicrotaskQueue::SetUpDefaultMicrotaskQueue(this);
```
After that we have created a new Isolate, we were in this function call:
```c++
  Isolate* isolate = new (isolate_ptr) Isolate(std::move(isolate_allocator));
```
After this we will be back in `api.cc`:
```c++
  Initialize(isolate, params);
```
```c++
void Isolate::Initialize(Isolate* isolate,
                         const v8::Isolate::CreateParams& params) {
```
We are not using any external snapshot data so the following will be false:
```c++
  if (params.snapshot_blob != nullptr) {
    i_isolate->set_snapshot_blob(params.snapshot_blob);
  } else {
    i_isolate->set_snapshot_blob(i::Snapshot::DefaultSnapshotBlob());
```
```console
(gdb) p snapshot_blob_
$7 = (const v8::StartupData *) 0x0
(gdb) n
(gdb) p i_isolate->snapshot_blob_
$8 = (const v8::StartupData *) 0x7ff92d7d6cf0 <v8::internal::blob>
```
`snapshot_blob_` is also one of the members that was set up with ISOLATE_INIT_LIST.
So we are setting up the Isolate instance for creation. 

```c++
Isolate::Scope isolate_scope(isolate);                                        
if (!i::Snapshot::Initialize(i_isolate)) { 
```
In `src/snapshot/snapshot-common.cc` we find 
```c++
bool Snapshot::Initialize(Isolate* isolate) {
  ...
  const v8::StartupData* blob = isolate->snapshot_blob();
  Vector<const byte> startup_data = ExtractStartupData(blob);
  Vector<const byte> read_only_data = ExtractReadOnlyData(blob);
  SnapshotData startup_snapshot_data(MaybeDecompress(startup_data));
  SnapshotData read_only_snapshot_data(MaybeDecompress(read_only_data));
  StartupDeserializer startup_deserializer(&startup_snapshot_data);
  ReadOnlyDeserializer read_only_deserializer(&read_only_snapshot_data);
  startup_deserializer.SetRehashability(ExtractRehashability(blob));
  read_only_deserializer.SetRehashability(ExtractRehashability(blob));

  bool success = isolate->InitWithSnapshot(&read_only_deserializer, &startup_deserializer);
```
So we get the blob and create deserializers for it which are then passed to
`isolate->InitWithSnapshot` which delegated to `Isolate::Init`. The blob will
have be create previously using `mksnapshot` (more on this can be found later).

This will use a `FOR_EACH_ISOLATE_ADDRESS_NAME` macro to assign to the
`isolate_addresses_` field:
```c++
isolate_addresses_[IsolateAddressId::kHandlerAddress] = reinterpret_cast<Address>(handler_address());
isolate_addresses_[IsolateAddressId::kCEntryFPAddress] = reinterpret_cast<Address>(c_entry_fp_address());
isolate_addresses_[IsolateAddressId::kCFunctionAddress] = reinterpret_cast<Address>(c_function_address());
isolate_addresses_[IsolateAddressId::kContextAddress] = reinterpret_cast<Address>(context_address());
isolate_addresses_[IsolateAddressId::kPendingExceptionAddress] = reinterpret_cast<Address>(pending_exception_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerContextAddress] = reinterpret_cast<Address>(pending_handler_context_address());
 isolate_addresses_[IsolateAddressId::kPendingHandlerEntrypointAddress] = reinterpret_cast<Address>(pending_handler_entrypoint_address());
 isolate_addresses_[IsolateAddressId::kPendingHandlerConstantPoolAddress] = reinterpret_cast<Address>(pending_handler_constant_pool_address());
 isolate_addresses_[IsolateAddressId::kPendingHandlerFPAddress] = reinterpret_cast<Address>(pending_handler_fp_address());
 isolate_addresses_[IsolateAddressId::kPendingHandlerSPAddress] = reinterpret_cast<Address>(pending_handler_sp_address());
 isolate_addresses_[IsolateAddressId::kExternalCaughtExceptionAddress] = reinterpret_cast<Address>(external_caught_exception_address());
 isolate_addresses_[IsolateAddressId::kJSEntrySPAddress] = reinterpret_cast<Address>(js_entry_sp_address());
```
After this we have a number of members that are assigned to:
```c++
  compilation_cache_ = new CompilationCache(this);
  descriptor_lookup_cache_ = new DescriptorLookupCache();
  inner_pointer_to_code_cache_ = new InnerPointerToCodeCache(this);
  global_handles_ = new GlobalHandles(this);
  eternal_handles_ = new EternalHandles();
  bootstrapper_ = new Bootstrapper(this);
  handle_scope_implementer_ = new HandleScopeImplementer(this);
  load_stub_cache_ = new StubCache(this);
  store_stub_cache_ = new StubCache(this);
  materialized_object_store_ = new MaterializedObjectStore(this);
  regexp_stack_ = new RegExpStack();
  regexp_stack_->isolate_ = this;
  date_cache_ = new DateCache();
  heap_profiler_ = new HeapProfiler(heap());
  interpreter_ = new interpreter::Interpreter(this);
  compiler_dispatcher_ =
      new CompilerDispatcher(this, V8::GetCurrentPlatform(), FLAG_stack_size);
```
After this we have:
```c++
isolate_data_.external_reference_table()->Init(this);
```
This will land in `src/codegen/external-reference-table.cc` where we have:
```c++
void ExternalReferenceTable::Init(Isolate* isolate) {                              
  int index = 0;                                                                   
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

void ExternalReferenceTable::Add(Address address, int* index) {                 
ref_addr_[(*index)++] = address;                                                
} 

Address ref_addr_[kSize];
```

Now, lets take a look at `AddReferences`: 
```c++
Add(ExternalReference::abort_with_reason().address(), index); 
```
What are ExternalReferences?   
They represent c++ addresses used in generated code.

After that we have AddBuiltins:
```c++
static const Address c_builtins[] = {                                         
      (reinterpret_cast<v8::internal::Address>(&Builtin_HandleApiCall)), 
      ...

Address Builtin_HandleApiCall(int argc, Address* args, Isolate* isolate);
```
I can see that the function declaration is in external-reference.h but the
implementation is not there. Instead this is defined in `src/builtins/builtins-api.cc`:
```c++
BUILTIN(HandleApiCall) {                                                           
(will expand to:)

V8_WARN_UNUSED_RESULT static Object Builtin_Impl_HandleApiCall(
      BuiltinArguments args, Isolate* isolate);

V8_NOINLINE static Address Builtin_Impl_Stats_HandleApiCall(
      int args_length, Address* args_object, Isolate* isolate) {
    BuiltinArguments args(args_length, args_object);
    RuntimeCallTimerScope timer(isolate,
                                RuntimeCallCounterId::kBuiltin_HandleApiCall);
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.runtime"), "V8.Builtin_HandleApiCall");
    return CONVERT
}
V8_WARN_UNUSED_RESULT Address Builtin_HandleApiCall(
      int args_length, Address* args_object, Isolate* isolate) {
    DCHECK(isolate->context().is_null() || isolate->context().IsContext());
    if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled())) {
      return Builtin_Impl_Stats_HandleApiCall(args_length, args_object, isolate);
    }
    BuiltinArguments args(args_length, args_object);
    return CONVERT_OBJECT(Builtin_Impl_HandleApiCall(args, isolate));
  }

  V8_WARN_UNUSED_RESULT static Object Builtin_Impl_HandleApiCall(
      BuiltinArguments args, Isolate* isolate) {
    HandleScope scope(isolate);                                                      
    Handle<JSFunction> function = args.target();                                  
    Handle<Object> receiver = args.receiver();                                    
    Handle<HeapObject> new_target = args.new_target();                               
    Handle<FunctionTemplateInfo> fun_data(function->shared().get_api_func_data(), 
                                        isolate);                                  
    if (new_target->IsJSReceiver()) {                                                
      RETURN_RESULT_OR_FAILURE(                                                   
          isolate, HandleApiCallHelper<true>(isolate, function, new_target,          
                                             fun_data, receiver, args));             
    } else {                                                                         
      RETURN_RESULT_OR_FAILURE(                                                      
          isolate, HandleApiCallHelper<false>(isolate, function, new_target,         
                                            fun_data, receiver, args));            
    }
  }
``` 
The `BUILTIN` macro can be found in `src/builtins/builtins-utils.h`:
```c++
#define BUILTIN(name)                                                       \
  V8_WARN_UNUSED_RESULT static Object Builtin_Impl_##name(                  \
      BuiltinArguments args, Isolate* isolate);
```

```c++
  if (setup_delegate_ == nullptr) {                                                 
    setup_delegate_ = new SetupIsolateDelegate(create_heap_objects);            
  } 

  if (!setup_delegate_->SetupHeap(&heap_)) {                                    
    V8::FatalProcessOutOfMemory(this, "heap object creation");                  
    return false;                                                               
  }    
```
This does nothing in the current code path and the code comment says that the
heap will be deserialized from the snapshot and true will be returned.

```c++
InitializeThreadLocal();
startup_deserializer->DeserializeInto(this);
```
```c++
DisallowHeapAllocation no_gc;                                               
isolate->heap()->IterateSmiRoots(this);                                     
isolate->heap()->IterateStrongRoots(this, VISIT_FOR_SERIALIZATION);         
Iterate(isolate, this);                                                     
isolate->heap()->IterateWeakRoots(this, VISIT_FOR_SERIALIZATION);           
DeserializeDeferredObjects();                                               
RestoreExternalReferenceRedirectors(accessor_infos());                      
RestoreExternalReferenceRedirectors(call_handler_infos());
```
In `heap.cc` we find IterateSmiRoots` which takes a pointer to a `RootVistor`.
RootVisitor is used for visiting and modifying (optionally) the pointers contains
in roots. This is used in garbage collection and also in serializing and deserializing
snapshots.

### Roots
RootVistor:
```c++
class RootVisitor {
 public:
  virtual void VisitRootPointers(Root root, const char* description,
                                 FullObjectSlot start, FullObjectSlot end) = 0;

  virtual void VisitRootPointer(Root root, const char* description,
                                FullObjectSlot p) {
    VisitRootPointers(root, description, p, p + 1);
  }
 
  static const char* RootName(Root root);
```
Root is an enum in `src/object/visitors.h`. This enum is generated by a macro
and expands to:
```c++
enum class Root {                                                               
  kStringTable,
  kExternalStringsTable,
  kReadOnlyRootList,
  kStrongRootList,
  kSmiRootList,
  kBootstrapper,
  kTop,
  kRelocatable,
  kDebug,
  kCompilationCache,
  kHandleScope,
  kBuiltins,
  kGlobalHandles,
  kEternalHandles,
  kThreadManager,
  kStrongRoots,
  kExtensions,
  kCodeFlusher,
  kPartialSnapshotCache,
  kReadOnlyObjectCache,
  kWeakCollections,
  kWrapperTracing,
  kUnknown,
  kNumberOfRoots                                                            
}; 
```
These can be displayed using:
```console
$ ./test/roots_test --gtest_filter=RootsTest.visitor_roots
```
Just to keep things clear for myself here, these visitor roots are only used
for GC and serialization/deserialization (at least I think so) and should not
be confused with the RootIndex enum in `src/roots/roots.h`.

Lets set a break point in `mksnapshot` and see if we can find where one of the
above Root enum elements is used to make it a little more clear what these are
used for.
```console
$ lldb ../v8_src/v8/out/x64.debug/mksnapshot 
(lldb) target create "../v8_src/v8/out/x64.debug/mksnapshot"
Current executable set to '../v8_src/v8/out/x64.debug/mksnapshot' (x86_64).
(lldb) br s -n main
Breakpoint 1: where = mksnapshot`main + 42, address = 0x00000000009303ca
(lldb) r
```
What this does is that it creates an V8 environment (Platform, Isolate, Context)
 and then saves it to a file, either a binary file on disk but it can also save
it to a .cc file that can be used in programs in which case the binary is a byte array.
It does this in much the same way as the hello-world example create a platform
and then initializes it, and the creates and initalizes a new Isolate. 
After the Isolate a new Context will be create using the Isolate. If there was
an embedded-src flag passed to mksnaphot it will be run.

StartupSerializer will use the Root enum elements for example and the deserializer
will use the same enum elements.

Adding a script to a snapshot:
```
$ gdb ../v8_src/v8/out/x64.release_gcc/mksnapshot --embedded-src="$PWD/embed.js"
```

TODO: Look into CreateOffHeapTrampolines.

So the VisitRootPointers function takes one of these Root's and visits all those
roots.  In our case the first Root to be visited is Heap::IterateSmiRoots:
```c++
void Heap::IterateSmiRoots(RootVisitor* v) {                                        
  ExecutionAccess access(isolate());                                                
  v->VisitRootPointers(Root::kSmiRootList, nullptr,                                 
                       roots_table().smi_roots_begin(),                             
                       roots_table().smi_roots_end());                              
  v->Synchronize(VisitorSynchronization::kSmiRootList);                             
}
```
And here we can see that it is using `Root::kSmiRootList`, and passing nullptr
for the description argument (I wonder what this is used for?). Next, comes
the start and end arguments. 
```console
(lldb) p roots_table().smi_roots_begin()
(v8::internal::FullObjectSlot) $5 = {
  v8::internal::SlotBase<v8::internal::FullObjectSlot, unsigned long, 8> = (ptr_ = 50680614097760)
}
```
We can list all the values of roots_table using:
```console
(lldb) expr -A -- roots_table()
```
In `src/snapshot/deserializer.cc` we can find VisitRootPointers:
```c++
void Deserializer::VisitRootPointers(Root root, const char* description,
                                     FullObjectSlot start, FullObjectSlot end)
  ReadData(FullMaybeObjectSlot(start), FullMaybeObjectSlot(end),
           SnapshotSpace::kNew, kNullAddress);
```
Notice that description is never used. `ReadData`is in the same source file:

The class SnapshotByteSource has a `data` member that is initialized upon construction
from a const char* or a Vector<const byte>. Where is this done?  
This was done back in `Snapshot::Initialize`:
```c++
  const v8::StartupData* blob = isolate->snapshot_blob();                       
  Vector<const byte> startup_data = ExtractStartupData(blob);                   
  Vector<const byte> read_only_data = ExtractReadOnlyData(blob);                
  SnapshotData startup_snapshot_data(MaybeDecompress(startup_data));            
  SnapshotData read_only_snapshot_data(MaybeDecompress(read_only_data));        
  StartupDeserializer startup_deserializer(&startup_snapshot_data); 
```
```console
(lldb) expr *this
(v8::internal::SnapshotByteSource) $30 = (data_ = "`\x04", length_ = 125752, position_ = 1)
```

All the roots in a heap are declared in src/roots/roots.h. You can access the
roots using RootsTable via the Isolate using isolate_data->roots() or by using
isolate->roots_table. The roots_ field is an array of Address elements:
```c++
class RootsTable {                                                              
 public:
  static constexpr size_t kEntriesCount = static_cast<size_t>(RootIndex::kRootListLength);
  ...
 private:
  Address roots_[kEntriesCount];                                                
  static const char* root_names_[kEntriesCount]; 
```
RootIndex is generated by a macro
```c++
enum class RootIndex : uint16_t {
```
The complete enum can be displayed using:
```console
$ ./test/roots_test --gtest_filter=RootsTest.list_root_index
```

Lets take a look at an entry:
```console
(lldb) p roots_[(uint16_t)RootIndex::kError_string]
(v8::internal::Address) $1 = 42318447256121
```
Now, there are functions in factory which can be used to retrieve these addresses,
like factory->Error_string():
```console
(lldb) expr *isolate->factory()->Error_string()
(v8::internal::String) $9 = {
  v8::internal::TorqueGeneratedString<v8::internal::String, v8::internal::Name> = {
    v8::internal::Name = {
      v8::internal::TorqueGeneratedName<v8::internal::Name, v8::internal::PrimitiveHeapObject> = {
        v8::internal::PrimitiveHeapObject = {
          v8::internal::TorqueGeneratedPrimitiveHeapObject<v8::internal::PrimitiveHeapObject, v8::internal::HeapObject> = {
            v8::internal::HeapObject = {
              v8::internal::Object = {
                v8::internal::TaggedImpl<v8::internal::HeapObjectReferenceType::STRONG, unsigned long> = (ptr_ = 42318447256121)
              }
            }
          }
        }
      }
    }
  }
}
(lldb) expr $9.length()
(int32_t) $10 = 5
(lldb) expr $9.Print()
#Error
```
These accessor functions declarations are generated by the
`ROOT_LIST(ROOT_ACCESSOR))` macros:
```c++
#define ROOT_ACCESSOR(Type, name, CamelName) inline Handle<Type> name();           
  ROOT_LIST(ROOT_ACCESSOR)                                                         
#undef ROOT_ACCESSOR
```
And the definitions can be found in `src/heap/factory-inl.h` and look like this
The implementations then look like this:
```c++
String ReadOnlyRoots::Error_string() const { 
  return  String::unchecked_cast(Object(at(RootIndex::kError_string)));
} 

Handle<String> ReadOnlyRoots::Error_string_handle() const {
  return Handle<String>(&at(RootIndex::kError_string)); 
}
```
The unit test [roots_test](./test/roots_test.cc) shows and example of this.

This shows the usage of root entries but where are the roots added to this
array. `roots_` is a member of `IsolateData` in `src/execution/isolate-data.h`:
```
  RootsTable roots_;
```
We can inspect the roots_ content by using the interal Isolate:
```
(lldb) f
frame #0: 0x00007ffff6261cdf libv8.so`v8::Isolate::Initialize(isolate=0x00000eb900000000, params=0x00007fffffffd0d0) at api.cc:8269:31
   8266	void Isolate::Initialize(Isolate* isolate,
   8267	                         const v8::Isolate::CreateParams& params) {

(lldb) expr i_isolate->isolate_data_.roots_
(v8::internal::RootsTable) $5 = {
  roots_ = {
    [0] = 0
    [1] = 0
    [2] = 0
```
So we can see that the roots are intially zero:ed out. And the type of `roots_`
is an array of `Address`'s.
```console
    frame #3: 0x00007ffff6c33d58 libv8.so`v8::internal::Deserializer::VisitRootPointers(this=0x00007fffffffcce0, root=kReadOnlyRootList, description=0x0000000000000000, start=FullObjectSlot @ 0x00007fffffffc530, end=FullObjectSlot @ 0x00007fffffffc528) at deserializer.cc:94:11
    frame #4: 0x00007ffff6b6212f libv8.so`v8::internal::ReadOnlyRoots::Iterate(this=0x00007fffffffc5c8, visitor=0x00007fffffffcce0) at roots.cc:21:29
    frame #5: 0x00007ffff6c46fee libv8.so`v8::internal::ReadOnlyDeserializer::DeserializeInto(this=0x00007fffffffcce0, isolate=0x00000f7500000000) at read-only-deserializer.cc:41:18
    frame #6: 0x00007ffff66af631 libv8.so`v8::internal::ReadOnlyHeap::DeseralizeIntoIsolate(this=0x000000000049afb0, isolate=0x00000f7500000000, des=0x00007fffffffcce0) at read-only-heap.cc:85:23
    frame #7: 0x00007ffff66af5de libv8.so`v8::internal::ReadOnlyHeap::SetUp(isolate=0x00000f7500000000, des=0x00007fffffffcce0) at read-only-heap.cc:78:53
```
This will land us in `roots.cc` ReadOnlyRoots::Iterate(RootVisitor* visitor):
```c++
void ReadOnlyRoots::Iterate(RootVisitor* visitor) {                                
  visitor->VisitRootPointers(Root::kReadOnlyRootList, nullptr,                     
                             FullObjectSlot(read_only_roots_),                     
                             FullObjectSlot(&read_only_roots_[kEntriesCount])); 
  visitor->Synchronize(VisitorSynchronization::kReadOnlyRootList);                 
} 
```
Deserializer::VisitRootPointers calls `Deserializer::ReadData` and the roots_
array is still zero:ed out when we enter this function.

```c++
void Deserializer::VisitRootPointers(Root root, const char* description,
                                     FullObjectSlot start, FullObjectSlot end) {
  ReadData(FullMaybeObjectSlot(start), FullMaybeObjectSlot(end),
           SnapshotSpace::kNew, kNullAddress);
```
Notice that we called VisitRootPointer and pased in `Root:kReadOnlyRootList`, 
nullptr (the description), and start and end addresses as FullObjectSlots. The
signature of `VisitRootPointers` looks like this:
```c++
virtual void VisitRootPointers(Root root, const char* description,            
                                 FullObjectSlot start, FullObjectSlot end)
```
In our case we are using the address of `read_only_roots_` from `src/roots/roots.h`
and the end is found by using the static member of ReadOnlyRoots::kEntrysCount.

The switch statement in `ReadData` is generated by macros so lets take a look at
an expanded snippet to understand what is going on:
```c++
template <typename TSlot>
bool Deserializer::ReadData(TSlot current, TSlot limit,
                            SnapshotSpace source_space,
                            Address current_object_address) {
  Isolate* const isolate = isolate_;
  ...
  while (current < limit) {                                                     
    byte data = source_.Get();                                                  
```
So current is the start address of the read_only_list and limit the end. `source_`
is a member of `ReadOnlyDeserializer` and is of type SnapshotByteSource.

`source_` got populated back in Snapshot::Initialize(internal_isolate):
```
const v8::StartupData* blob = isolate->snapshot_blob();
Vector<const byte> read_only_data = ExtractReadOnlyData(blob);
ReadOnlyDeserializer read_only_deserializer(&read_only_snapshot_data);
```
And `ReadOnlyDeserializer` extends `Deserialier` (src/snapshot/deserializer.h)
which has a constructor that sets the source_ member to data->Payload().
So `source_` is will be pointer to an instance of `SnapshotByteSource` which
can be found in `src/snapshot-source-sink.h`:
```c++
class SnapshotByteSource final {
 public:
  SnapshotByteSource(const char* data, int length)
      : data_(reinterpret_cast<const byte*>(data)),
        length_(length),
        position_(0) {}

  byte Get() {                                                                  
    return data_[position_++];                                                  
  }
  ...
 private:
  const byte* data_;
  int length_;
  int posistion_;
```
Alright, so we are calling source_.Get() which we can see returns the current
entry from the byte array data_ and increment the position. So with that in
mind lets take closer look at the switch statment:
```c++
  while (current < limit) {                                                     
    byte data = source_.Get();                                                  
    switch (data) {                                                             
      case kNewObject + static_cast<int>(SnapshotSpace::kNew):
        current = ReadDataCase<TSlot, kNewObject, SnapshotSpace::kNew>(isolate, current, current_object_address, data, write_barrier_needed);
        break;
      case kNewObject + static_cast<int>(SnapshotSpace::kOld):
        [[clang::fallthrough]];
      case kNewObject + static_cast<int>(SnapshotSpace::kCode):
        [[clang::fallthrough]];
      case kNewObject + static_cast<int>(SnapshotSpace::kMap):
        static_assert((static_cast<int>(SnapshotSpace::kMap) & ~kSpaceMask) == 0, "(static_cast<int>(SnapshotSpace::kMap) & ~kSpaceMask) == 0");
        [[clang::fallthrough]];
      ...
```
We can see that switch statement will assign the passed-in `current` with a new
instance of `ReadDataCase`.
```c++
  current = ReadDataCase<TSlot, kNewObject, SnapshotSpace::kNew>(isolate,
      current, current_object_address, data, write_barrier_needed);
```
Notice that kNewObject is the type of SerializerDeserliazer::Bytecode that is
to be read (I think), this enum can be found in `src/snapshot/serializer-common.h`.
`TSlot` I think stands for the "Type of Slot", which in our case is a FullMaybyObjectSlot.
```c++
  HeapObject heap_object;
  if (bytecode == kNewObject) {                                                 
    heap_object = ReadObject(space);   
```
ReadObject is also in deserializer.cc :
```c++
Address address = allocator()->Allocate(space, size);
HeapObject obj = HeapObject::FromAddress(address);
isolate_->heap()->OnAllocationEvent(obj, size);

Alright, lets set a watch point on the roots_ array to see when the first entry
is populated and try to figure this out that way:
```console
(lldb) watch set variable  isolate->isolate_data_.roots_.roots_[0]
Watchpoint created: Watchpoint 5: addr = 0xf7500000080 size = 8 state = enabled type = w
    declare @ '/home/danielbevenius/work/google/v8_src/v8/src/heap/read-only-heap.cc:28'
    watchpoint spec = 'isolate->isolate_data_.roots_.roots_[0]'
    new value: 0
(lldb) r

Watchpoint 5 hit:
old value: 0
new value: 16995320070433
Process 1687448 stopped
* thread #1, name = 'hello-world', stop reason = watchpoint 5
    frame #0: 0x00007ffff664e5b1 libv8.so`v8::internal::FullMaybeObjectSlot::store(this=0x00007fffffffc3b0, value=MaybeObject @ 0x00007fffffffc370) const at slots-inl.h:74:1
   71  	
   72  	void FullMaybeObjectSlot::store(MaybeObject value) const {
   73  	  *location() = value.ptr();
-> 74  	}
   75 
```
We can verify that location actually contains the address of `roots_[0]`:
```console
(lldb) expr -f hex -- this->ptr_
(v8::internal::Address) $164 = 0x00000f7500000080
(lldb) expr -f hex -- &this->isolate_->isolate_data_.roots_.roots_[0]
(v8::internal::Address *) $171 = 0x00000f7500000080

(lldb) expr -f hex -- value.ptr()
(unsigned long) $184 = 0x00000f7508040121
(lldb) expr -f hex -- isolate_->isolate_data_.roots_.roots_[0]
(v8::internal::Address) $183 = 0x00000f7508040121
```
The first entry is free_space_map.
```console
(lldb) expr v8::internal::Map::unchecked_cast(v8::internal::Object(value->ptr()))
(v8::internal::Map) $185 = {
  v8::internal::HeapObject = {
    v8::internal::Object = {
      v8::internal::TaggedImpl<v8::internal::HeapObjectReferenceType::STRONG, unsigned long> = (ptr_ = 16995320070433)
    }
  }
```
Next, we will go through the while loop again:
```console
(lldb) expr -f hex -- isolate_->isolate_data_.roots_.roots_[1]
(v8::internal::Address) $191 = 0x0000000000000000
(lldb) expr -f hex -- &isolate_->isolate_data_.roots_.roots_[1]
(v8::internal::Address *) $192 = 0x00000f7500000088
(lldb) expr -f hex -- location()
(v8::internal::SlotBase<v8::internal::FullMaybeObjectSlot, unsigned long, 8>::TData *) $194 = 0x00000f7500000088
```
Notice that in Deserializer::Write we have:
```c++
  dest.store(value);
  return dest + 1;
```
And it's current value is:
```console
(v8::internal::Address) $197 = 0x00000f7500000088
```
Which is the same address as roots_[1] that we just wrote to.

If we know the type that an Address points to we can use the Type::cast(Object obj)
to cast it into a pointer of that type. I think this works will all types.
```console
(lldb) expr -A -f hex  -- v8::internal::Oddball::cast(v8::internal::Object(isolate_->isolate_data_.roots_.roots_[4]))
(v8::internal::Oddball) $258 = {
  v8::internal::TorqueGeneratedOddball<v8::internal::Oddball, v8::internal::PrimitiveHeapObject> = {
    v8::internal::PrimitiveHeapObject = {
      v8::internal::TorqueGeneratedPrimitiveHeapObject<v8::internal::PrimitiveHeapObject, v8::internal::HeapObject> = {
        v8::internal::HeapObject = {
          v8::internal::Object = {
            v8::internal::TaggedImpl<v8::internal::HeapObjectReferenceType::STRONG, unsigned long> = (ptr_ = 0x00000f750804030d)
          }
        }
      }
    }
  }
}
```
You can also just cast it to an object and try printing it:
```console
(lldb) expr -A -f hex  -- v8::internal::Object(isolate_->isolate_data_.roots_.roots_[4]).Print()
#undefined
```
This is actually the Oddball UndefinedValue so it makes sense in this case I think.
With this value in the roots_ array we can use the function ReadOnlyRoots::undefined_value():
```console
(lldb) expr v8::internal::ReadOnlyRoots(&isolate_->heap_).undefined_value()
(v8::internal::Oddball) $265 = {
  v8::internal::TorqueGeneratedOddball<v8::internal::Oddball, v8::internal::PrimitiveHeapObject> = {
    v8::internal::PrimitiveHeapObject = {
      v8::internal::TorqueGeneratedPrimitiveHeapObject<v8::internal::PrimitiveHeapObject, v8::internal::HeapObject> = {
        v8::internal::HeapObject = {
          v8::internal::Object = {
            v8::internal::TaggedImpl<v8::internal::HeapObjectReferenceType::STRONG, unsigned long> = (ptr_ = 16995320070925)
          }
        }
      }
    }
  }
}
```
So how are these roots used, take the above `undefined_value` for example?  
Well most things (perhaps all) that are needed go via the Factory which the
internal Isolate is a type of. In factory we can find:
```c++
Handle<Oddball> Factory::undefined_value() {
  return Handle<Oddball>(&isolate()->roots_table()[RootIndex::kUndefinedValue]);
}
```
Notice that this is basically what we did in the debugger before but here
it is wrapped in Handle so that it can be tracked by the GC.

The unit test [isolate_test](./test/isolate_test.cc) explores the internal 
isolate and has example of usages of the above mentioned methods.

InitwithSnapshot will call Isolate::Init:
```c++
bool Isolate::Init(ReadOnlyDeserializer* read_only_deserializer,
                   StartupDeserializer* startup_deserializer) {

#define ASSIGN_ELEMENT(CamelName, hacker_name)                  \
  isolate_addresses_[IsolateAddressId::k##CamelName##Address] = \
      reinterpret_cast<Address>(hacker_name##_address());
  FOR_EACH_ISOLATE_ADDRESS_NAME(ASSIGN_ELEMENT)
#undef ASSIGN_ELEMENT
```
```c++
  Address isolate_addresses_[kIsolateAddressCount + 1] = {};
```
```console
(gdb) p isolate_addresses_
$16 = {0 <repeats 13 times>}
```

Lets take a look at the expanded code in Isolate::Init:
```console
$ clang++ -I./out/x64.release/gen -I. -I./include -E src/execution/isolate.cc > output
```
```c++
isolate_addresses_[IsolateAddressId::kHandlerAddress] = reinterpret_cast<Address>(handler_address());
isolate_addresses_[IsolateAddressId::kCEntryFPAddress] = reinterpret_cast<Address>(c_entry_fp_address());
isolate_addresses_[IsolateAddressId::kCFunctionAddress] = reinterpret_cast<Address>(c_function_address());
isolate_addresses_[IsolateAddressId::kContextAddress] = reinterpret_cast<Address>(context_address());
isolate_addresses_[IsolateAddressId::kPendingExceptionAddress] = reinterpret_cast<Address>(pending_exception_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerContextAddress] = reinterpret_cast<Address>(pending_handler_context_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerEntrypointAddress] = reinterpret_cast<Address>(pending_handler_entrypoint_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerConstantPoolAddress] = reinterpret_cast<Address>(pending_handler_constant_pool_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerFPAddress] = reinterpret_cast<Address>(pending_handler_fp_address());
isolate_addresses_[IsolateAddressId::kPendingHandlerSPAddress] = reinterpret_cast<Address>(pending_handler_sp_address());
isolate_addresses_[IsolateAddressId::kExternalCaughtExceptionAddress] = reinterpret_cast<Address>(external_caught_exception_address());
isolate_addresses_[IsolateAddressId::kJSEntrySPAddress] = reinterpret_cast<Address>(js_entry_sp_address());
```
Then functions, like handler_address() are implemented as:
```c++ 
inline Address* handler_address() { return &thread_local_top()->handler_; }   
```
```console
(gdb) x/x isolate_addresses_[0]
0x1a3500003240:	0x00000000
```
At this point in the program we have only set the entries to point contain
the addresses specified in ThreadLocalTop, At the time there are initialized
the will mostly be initialized to `kNullAddress`:
```c++
static const Address kNullAddress = 0;
```
And notice that the functions above return pointers so later these pointers can
be updated to point to something. What/when does this happen?  Lets continue and
find out...

Back in Isolate::Init we have:
```c++
  compilation_cache_ = new CompilationCache(this);
  descriptor_lookup_cache_ = new DescriptorLookupCache();
  inner_pointer_to_code_cache_ = new InnerPointerToCodeCache(this);
  global_handles_ = new GlobalHandles(this);
  eternal_handles_ = new EternalHandles();
  bootstrapper_ = new Bootstrapper(this);
  handle_scope_implementer_ = new HandleScopeImplementer(this);
  load_stub_cache_ = new StubCache(this);
  store_stub_cache_ = new StubCache(this);
  materialized_object_store_ = new MaterializedObjectStore(this);
  regexp_stack_ = new RegExpStack();
  regexp_stack_->isolate_ = this;
  date_cache_ = new DateCache();
  heap_profiler_ = new HeapProfiler(heap());
  interpreter_ = new interpreter::Interpreter(this);

  compiler_dispatcher_ =
      new CompilerDispatcher(this, V8::GetCurrentPlatform(), FLAG_stack_size);

  // SetUp the object heap.
  DCHECK(!heap_.HasBeenSetUp());
  heap_.SetUp();

  ...
  InitializeThreadLocal();
```
Lets take a look at `InitializeThreadLocal`

```c++
void Isolate::InitializeThreadLocal() {
  thread_local_top()->Initialize(this);
  clear_pending_exception();
  clear_pending_message();
  clear_scheduled_exception();
}
```
```c++
void Isolate::clear_pending_exception() {
  DCHECK(!thread_local_top()->pending_exception_.IsException(this));
  thread_local_top()->pending_exception_ = ReadOnlyRoots(this).the_hole_value();
}
```
ReadOnlyRoots 
```c++
#define ROOT_ACCESSOR(Type, name, CamelName) \
  V8_INLINE class Type name() const;         \
  V8_INLINE Handle<Type> name##_handle() const;

  READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR
```
This will expand to a number of function declarations that looks like this:
```console
$ clang++ -I./out/x64.release/gen -I. -I./include -E src/roots/roots.h > output
```
```c++
inline __attribute__((always_inline)) class Map free_space_map() const;
inline __attribute__((always_inline)) Handle<Map> free_space_map_handle() const;
```
The Map class is what all HeapObject use to describe their structure. Notice
that there is also a Handle<Map> declared.
These are generated by a macro in roots-inl.h:
```c++
Map ReadOnlyRoots::free_space_map() const { 
  ((void) 0);
  return Map::unchecked_cast(Object(at(RootIndex::kFreeSpaceMap)));
} 

Handle<Map> ReadOnlyRoots::free_space_map_handle() const {
  ((void) 0);
  return Handle<Map>(&at(RootIndex::kFreeSpaceMap));
}
```
Notice that this is using the RootIndex enum that was mentioned earlier:
```c++
  return Map::unchecked_cast(Object(at(RootIndex::kFreeSpaceMap)));
```
In object/map.h there is the following line:
```c++
  DECL_CAST(Map)
```
Which can be found in objects/object-macros.h:
```c++
#define DECL_CAST(Type)                                 \
  V8_INLINE static Type cast(Object object);            \
  V8_INLINE static Type unchecked_cast(Object object) { \
    return bit_cast<Type>(object);                      \
  }
```
This will expand to something like
```c++
  static Map cast(Object object);
  static Map unchecked_cast(Object object) {
    return bit_cast<Map>(object);
  }
```
And the `Object` part is the Object contructor that takes an Address: 
```c++
  explicit constexpr Object(Address ptr) : TaggedImpl(ptr) {}
```
That leaves the at function which is a private function in ReadOnlyRoots:
```c++
  V8_INLINE Address& at(RootIndex root_index) const;
```

So we are now back in Isolate::Init after the call to InitializeThreadLocal we
have:
```c++
setup_delegate_->SetupBuiltins(this);
```

In the following line in api.cc, where does `i::OBJECT_TEMPLATE_INFO_TYPE` come from:
```c++
  i::Handle<i::Struct> struct_obj = isolate->factory()->NewStruct(
      i::OBJECT_TEMPLATE_INFO_TYPE, i::AllocationType::kOld);
```

### InstanceType
The enum `InstanceType` is defined in `src/objects/instance-type.h`:
```c++
#include "torque-generated/instance-types-tq.h" 

enum InstanceType : uint16_t {
  ...   
#define MAKE_TORQUE_INSTANCE_TYPE(TYPE, value) TYPE = value,                    
  TORQUE_ASSIGNED_INSTANCE_TYPES(MAKE_TORQUE_INSTANCE_TYPE)                     
#undef MAKE_TORQUE_INSTANCE_TYPE 
  ...
};
```
And in `gen/torque-generated/instance-types-tq.h` we can find:
```c++
#define TORQUE_ASSIGNED_INSTANCE_TYPES(V) \                                     
  ...
  V(OBJECT_TEMPLATE_INFO_TYPE, 79) \                                      
  ...
```
There is list in `src/objects/objects-definitions.h`:
```c++
#define STRUCT_LIST_GENERATOR_BASE(V, _)                                      \
  ...
  V(_, OBJECT_TEMPLATE_INFO_TYPE, ObjectTemplateInfo, object_template_info)   \
  ...
```
```c++
template <typename Impl>
Handle<Struct> FactoryBase<Impl>::NewStruct(InstanceType type,
                                            AllocationType allocation) {
  Map map = Map::GetInstanceTypeMap(read_only_roots(), type);
```
If we look in `Map::GetInstanceTypeMap` in map.cc we find:
```c++
  Map map;
  switch (type) {
#define MAKE_CASE(TYPE, Name, name) \
  case TYPE:                        \
    map = roots.name##_map();       \
    break;
    STRUCT_LIST(MAKE_CASE)
#undef MAKE_CASE
```
Now, we know that our type is:
```console
(gdb) p type
$1 = v8::internal::OBJECT_TEMPLATE_INFO_TYPE
```
```c++
    map = roots.object_template_info_map();       \
```
And we can inspect the output of the preprocessor of roots.cc and find:
```c++
Map ReadOnlyRoots::object_template_info_map() const { 
  ((void) 0);
  return Map::unchecked_cast(Object(at(RootIndex::kObjectTemplateInfoMap)));
}
```
And this is something we have seen before. 

One things I ran into was wanting to print the InstanceType using the overloaded
<< operator which is defined for the InstanceType in objects.cc.
```c++
std::ostream& operator<<(std::ostream& os, InstanceType instance_type) {
  switch (instance_type) {
#define WRITE_TYPE(TYPE) \
  case TYPE:             \
    return os << #TYPE;
    INSTANCE_TYPE_LIST(WRITE_TYPE)
#undef WRITE_TYPE
  }
  UNREACHABLE();
}
```
The code I'm using is the followig:
```c++
  i::InstanceType type = map.instance_type();
  std::cout << "object_template_info_map type: " << type << '\n';
```
This will cause the `UNREACHABLE()` function to be called and a Fatal error
thrown. But note that the following line works:
```c++
  std::cout << "object_template_info_map type: " << v8::internal::OBJECT_TEMPLATE_INFO_TYPE << '\n';
```
And prints
```console
object_template_info_map type: OBJECT_TEMPLATE_INFO_TYPE
```
In the switch/case block above the case for this value is:
```c++
  case OBJECT_TEMPLATE_INFO_TYPE:
    return os << "OBJECT_TEMPLATE_INFO_TYPE"
```
When map.instance_type() is called, it returns a value of `1023` but the value
of OBJECT_TEMPLATE_INFO_TYPE is:
```c++
OBJECT_TEMPLATE_INFO_TYPE = 79
```
And we can confirm this using:
```console
  std::cout << "object_template_info_map type: " << static_cast<uint16_t>(v8::internal::OBJECT_TEMPLATE_INFO_TYPE) << '\n';
```
Which will print:
```console
object_template_info_map type: 79
```

### IsolateData

### Context creation
When we create a new context using:
```c++
  Local<ObjectTemplate> global = ObjectTemplate::New(isolate_);
  Local<Context> context = Context::New(isolate_, nullptr, global);
```
The Context class in `include/v8.h` declares New as follows:
```c++
static Local<Context> New(Isolate* isolate,
    ExtensionConfiguration* extensions = nullptr,
    MaybeLocal<ObjectTemplate> global_template = MaybeLocal<ObjectTemplate>(),
    MaybeLocal<Value> global_object = MaybeLocal<Value>(),
    DeserializeInternalFieldsCallback internal_fields_deserializer = DeserializeInternalFieldsCallback(),
    MicrotaskQueue* microtask_queue = nullptr);
```

When a step into Context::New(isolate_, nullptr, global) this will first break
in the constructor of DeserializeInternalFieldsCallback in v8.h which has default
values for the callback function and data_args (both are nullptr). After that
gdb will break in MaybeLocal<Value> and setting val_ to nullptr. Next it will
break in Local::operator* for the value of `global` which is then passed to the
MaybeLocal<v8::ObjectTemplate> constructor. After those break points the break
point will be in api.cc and v8::Context::New. New will call NewContext in api.cc.

There will be some checks and logging/tracing and then a call to CreateEnvironment:
```c++
i::Handle<i::Context> env = CreateEnvironment<i::Context>(                         
    isolate,
    extensions,
    global_template, 
    global_object,                           
    context_snapshot_index, 
    embedder_fields_deserializer, 
    microtask_queue); 
```
The first line in CreateEnironment is:
```c++
ENTER_V8_FOR_NEW_CONTEXT(isolate);
```
Which is a macro defined in api.cc
```c++
i::VMState<v8::OTHER> __state__((isolate)); \                                 
i::DisallowExceptions __no_exceptions__((isolate)) 
```
So the first break point we break on will be the execution/vm-state-inl.h and
VMState's constructor:
```c++
template <StateTag Tag>                                                         
VMState<Tag>::VMState(Isolate* isolate)                                         
    : isolate_(isolate), previous_tag_(isolate->current_vm_state()) {           
  isolate_->set_current_vm_state(Tag);                                          
} 
```
In gdb you'll see this:
```console
(gdb) s
v8::internal::VMState<(v8::StateTag)5>::VMState (isolate=0x372500000000, this=<synthetic pointer>) at ../../src/api/api.cc:6005
6005	      context_snapshot_index, embedder_fields_deserializer, microtask_queue);
(gdb) s
v8::internal::Isolate::current_vm_state (this=0x372500000000) at ../../src/execution/isolate.h:1072
1072	  THREAD_LOCAL_TOP_ACCESSOR(StateTag, current_vm_state)
```
Notice that VMState's constructor sets its `previous_tag_` to isolate->current_vm_state()
which is generated by the macro THREAD_LOCAL_TOP_ACCESSOR.
The next break point will be:
```console
#0  v8::internal::PerIsolateAssertScopeDebugOnly<(v8::internal::PerIsolateAssertType)5, false>::PerIsolateAssertScopeDebugOnly (
    isolate=0x372500000000, this=0x7ffc7b51b500) at ../../src/common/assert-scope.h:107
107	  explicit PerIsolateAssertScopeDebugOnly(Isolate* isolate)
```
We can find that `DisallowExceptions` is defined in src/common/assert-scope.h as:
```c++
using DisallowExceptions =                                                      
    PerIsolateAssertScopeDebugOnly<NO_EXCEPTION_ASSERT, false>;
```
After all that we can start to look at the code in CreateEnvironment.

```c++
    // Create the environment.                                                       
    InvokeBootstrapper<ObjectType> invoke;                                           
    result = invoke.Invoke(isolate, maybe_proxy, proxy_template, extensions,    
                           context_snapshot_index, embedder_fields_deserializer,
                           microtask_queue);  


template <typename ObjectType>                                                  
struct InvokeBootstrapper;                                                        
                                                                                     
template <>                                                                     
struct InvokeBootstrapper<i::Context> {                                         
  i::Handle<i::Context> Invoke(                                                 
      i::Isolate* isolate, i::MaybeHandle<i::JSGlobalProxy> maybe_global_proxy, 
      v8::Local<v8::ObjectTemplate> global_proxy_template,                      
      v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,    
      v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,       
      v8::MicrotaskQueue* microtask_queue) {                                         
    return isolate->bootstrapper()->CreateEnvironment(                               
        maybe_global_proxy, global_proxy_template, extensions,                       
        context_snapshot_index, embedder_fields_deserializer, microtask_queue); 
  }                                                                                  
};
```
Bootstrapper can be found in `src/init/bootstrapper.cc`:
```console
HandleScope scope(isolate_);                                                      
Handle<Context> env;                                                              
  {                                                                                 
    Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template,            
                    context_snapshot_index, embedder_fields_deserializer,           
                    microtask_queue);                                               
    env = genesis.result();                                                         
    if (env.is_null() || !InstallExtensions(env, extensions)) {                     
      return Handle<Context>();                                                     
    }                                                                               
  }                 
```
Notice that the break point will be in the HandleScope constructor. Then a 
new instance of Genesis is created which performs some actions in its constructor.
```c++
global_proxy = isolate->factory()->NewUninitializedJSGlobalProxy(instance_size);
```

This will land in factory.cc:
```c++
Handle<Map> map = NewMap(JS_GLOBAL_PROXY_TYPE, size);
```
`size` will be 16 in this case. `NewMap` is declared in factory.h which has
default values for its parameters:
```c++
  Handle<Map> NewMap(InstanceType type, int instance_size,                      
                     ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,  
                     int inobject_properties = 0);
```

In Factory::InitializeMap we have the following check:
```c++
DCHECK_EQ(map.GetInObjectProperties(), inobject_properties);
```
Remember that I called `Context::New` with the following arguments:
```c++
  Local<ObjectTemplate> global = ObjectTemplate::New(isolate_);
  Local<Context> context = Context::New(isolate_, nullptr, global);
```


### VMState



### TaggedImpl
Has a single private member which is declared as:
```c++
StorageType ptr_;
```
An instance can be created using:
```c++
  i::TaggedImpl<i::HeapObjectReferenceType::STRONG, i::Address>  tagged{};
```
Storage type can also be `Tagged_t` which is defined in globals.h:
```c++
 using Tagged_t = uint32_t;
```
It looks like it can be a different value when using pointer compression.

### Object (internal)
This class extends TaggedImpl:
```c++
class Object : public TaggedImpl<HeapObjectReferenceType::STRONG, Address> {       
```
An Object can be created using the default constructor, or by passing in an 
Address which will delegate to TaggedImpl constructors. Object itself does
not have any members (apart from ptr_ which is inherited from TaggedImpl that is). 
So if we create an Object on the stack this is like a pointer/reference to
an object: 
```
+------+
|Object|
|------|
|ptr_  |---->
+------+
```
Now, `ptr_` is a TaggedImpl so it would be a Smi in which case it would just
contains the value directly, for example a small integer:
```
+------+
|Object|
|------|
|  18  |
+------+
```

### Handle
A Handle is similar to a Object and ObjectSlot in that it also contains
an Address member (called location_ and declared in HandleBase), but with the
difference is that Handles can be relocated by the garbage collector.

### HeapObject


### NewContext
When we create a new context using:
```c++
const v8::Local<v8::ObjectTemplate> obt = v8::Local<v8::ObjectTemplate>();
v8::Handle<v8::Context> context = v8::Context::New(isolate_, nullptr, obt);
```
The above is using the static function New declared in `include/v8.h`
```c++
static Local<Context> New(                                                    
    Isolate* isolate,
    ExtensionConfiguration* extensions = nullptr,           
    MaybeLocal<ObjectTemplate> global_template = MaybeLocal<ObjectTemplate>(),
    MaybeLocal<Value> global_object = MaybeLocal<Value>(),                    
    DeserializeInternalFieldsCallback internal_fields_deserializer = DeserializeInternalFieldsCallback(),                                  
    MicrotaskQueue* microtask_queue = nullptr);
```
The implementation for this function can be found in `src/api/api.cc`
How does a Local become a MaybeLocal in this above case?  
This is because MaybeLocal has a constructor that takes a `Local<S>` and this will
be casted into the `val_` member of the MaybeLocal instance.


### Genesis
TODO


### What is the difference between a Local and a Handle?

Currently, the torque generator will generate Print functions that look like
the following:
```c++
template <>                                                                     
void TorqueGeneratedEnumCache<EnumCache, Struct>::EnumCachePrint(std::ostream& os) {
  this->PrintHeader(os, "TorqueGeneratedEnumCache");
  os << "\n - keys: " << Brief(this->keys());
  os << "\n - indices: " << Brief(this->indices());
  os << "\n";
}
```
Notice the last line where the newline character is printed as a string. This
would just be a char instead `'\n'`.

There are a number of things that need to happen only once upon startup for
each process. These things are placed in `V8::InitializeOncePerProcessImpl` which
can be found in `src/init/v8.cc`. This is called by v8::V8::Initialize().
```c++
  CpuFeatures::Probe(false);                                                    
  ElementsAccessor::InitializeOncePerProcess();                                 
  Bootstrapper::InitializeOncePerProcess();                                     
  CallDescriptors::InitializeOncePerProcess();                                  
  wasm::WasmEngine::InitializeOncePerProcess();
```
ElementsAccessor populates the accessor_array with Elements listed in 
`ELEMENTS_LIST`. TODO: take a closer look at Elements. 

v8::Isolate::Initialize will set up the heap.
```c++
i_isolate->heap()->ConfigureHeap(params.constraints);
```

It is when we create an new Context that Genesis is created. This will call
Snapshot::NewContextFromSnapshot.
So the context is read from the StartupData* blob with ExtractContextData(blob).

What is the global proxy?

### Builtins runtime error
Builtins is a member of Isolate and an instance is created by the Isolate constructor.
We can inspect the value of `initialized_` and that it is false:
```console
(gdb) p *this->builtins()
$3 = {static kNoBuiltinId = -1, static kFirstWideBytecodeHandler = 1248, static kFirstExtraWideBytecodeHandler = 1398, 
  static kLastBytecodeHandlerPlusOne = 1548, static kAllBuiltinsAreIsolateIndependent = true, isolate_ = 0x0, initialized_ = false, 
  js_entry_handler_offset_ = 0}
```
The above is printed form Isolate's constructor and it is not changes in the
contructor.

This is very strange, while I though that the `initialized_` was being updated
it now looks like there might be two instances, one with has this value as false
and the other as true. And also one has a nullptr as the isolate and the other
as an actual value.
For example, when I run the hello-world example:
```console
$4 = (v8::internal::Builtins *) 0x33b20000a248
(gdb) p &builtins_
$5 = (v8::internal::Builtins *) 0x33b20000a248
```
Notice that these are poiting to the same location in memory.
```console
(gdb) p &builtins_
$1 = (v8::internal::Builtins *) 0x25210000a248
(gdb) p builtins()
$2 = (v8::internal::Builtins *) 0x25210000a228
```
Alright, so after looking into this closer I noticed that I was including
internal headers in the test itself.
When I include `src/builtins/builtins.h` I will get an implementation of
isolate->builtins() in the object file which is in the shared library libv8.so,
but the field is part of object file that is part of the cctest. This will be a
different method and not the method that is in libv8_v8.so shared library.

As I'm only interested in exploring v8 internals and my goal is only for each
unit test to verify my understanding I've statically linked those object files
needed, like builtins.o and code.o to the test.

```console
 Fatal error in ../../src/snapshot/read-only-deserializer.cc, line 35
# Debug check failed: !isolate->builtins()->is_initialized().
#
#
#
#FailureMessage Object: 0x7ffed92ceb20
==== C stack trace ===============================

    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8_libbase.so(v8::base::debug::StackTrace::StackTrace()+0x1d) [0x7fabe6c348c1]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8_libplatform.so(+0x652d9) [0x7fabe6cac2d9]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8_libbase.so(V8_Fatal(char const*, int, char const*, ...)+0x172) [0x7fabe6c2416d]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8_libbase.so(v8::base::SetPrintStackTrace(void (*)())+0) [0x7fabe6c23de0]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8_libbase.so(V8_Dcheck(char const*, int, char const*)+0x2d) [0x7fabe6c241b1]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8.so(v8::internal::ReadOnlyDeserializer::DeserializeInto(v8::internal::Isolate*)+0x192) [0x7fabe977c468]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8.so(v8::internal::ReadOnlyHeap::DeseralizeIntoIsolate(v8::internal::Isolate*, v8::internal::ReadOnlyDeserializer*)+0x4f) [0x7fabe91e5a7d]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8.so(v8::internal::ReadOnlyHeap::SetUp(v8::internal::Isolate*, v8::internal::ReadOnlyDeserializer*)+0x66) [0x7fabe91e5a2a]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8.so(v8::internal::Isolate::Init(v8::internal::ReadOnlyDeserializer*, v8::internal::StartupDeserializer*)+0x70b) [0x7fabe90633bb]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8.so(v8::internal::Isolate::InitWithSnapshot(v8::internal::ReadOnlyDeserializer*, v8::internal::StartupDeserializer*)+0x7b) [0x7fabe906299f]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8.so(v8::internal::Snapshot::Initialize(v8::internal::Isolate*)+0x1e9) [0x7fabe978d941]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8.so(v8::Isolate::Initialize(v8::Isolate*, v8::Isolate::CreateParams const&)+0x33d) [0x7fabe8d999e3]
    /home/danielbevenius/work/google/v8_src/v8/out/x64.release_gcc/libv8.so(v8::Isolate::New(v8::Isolate::CreateParams const&)+0x28) [0x7fabe8d99b66]
    ./test/builtins_test() [0x4135a2]
    ./test/builtins_test() [0x43a1b7]
    ./test/builtins_test() [0x434c99]
    ./test/builtins_test() [0x41a3a7]
    ./test/builtins_test() [0x41aafb]
    ./test/builtins_test() [0x41b085]
    ./test/builtins_test() [0x4238e0]
    ./test/builtins_test() [0x43b1aa]
    ./test/builtins_test() [0x435773]
    ./test/builtins_test() [0x422836]
    ./test/builtins_test() [0x412ea4]
    ./test/builtins_test() [0x412e3d]
    /lib64/libc.so.6(__libc_start_main+0xf3) [0x7fabe66b31a3]
    ./test/builtins_test() [0x412d5e]
Illegal instruction (core dumped)
```
The issue here is that I'm including the header in the test, which means that
code will be in the object code of the test, while the implementation part will
be in the linked dynamic library which is why these are pointing to different
areas in memory. The one retreived by the function call will use the

### Goma
I've goma referenced in a number of places so just makeing a note of what it is
here: Goma is googles internal distributed compile service.

### WebAssembly
This section is going to take a closer look at how wasm works in V8.

We can use a wasm module like this:
```js
  const buffer = fixtures.readSync('add.wasm'); 
  const module = new WebAssembly.Module(buffer);                             
  const instance = new WebAssembly.Instance(module);                        
  instance.exports.add(3, 4);
```
Where is the WebAssembly object setup?  We have sen previously that objects and
function are added in `src/init/bootstrapper.cc` and for Wasm there is a function
named Genisis::InstallSpecialObjects which calls:
```c++
  WasmJs::Install(isolate, true);
```
This call will land in `src/wasm/wasm-js.cc` where we can find:
```c++
void WasmJs::Install(Isolate* isolate, bool exposed_on_global_object) {
  ...
  Handle<String> name = v8_str(isolate, "WebAssembly")
  ...
  NewFunctionArgs args = NewFunctionArgs::ForFunctionWithoutCode(               
      name, isolate->strict_function_map(), LanguageMode::kStrict);             
  Handle<JSFunction> cons = factory->NewFunction(args);                         
  JSFunction::SetPrototype(cons, isolate->initial_object_prototype());          
  Handle<JSObject> webassembly =                                                
      factory->NewJSObject(cons, AllocationType::kOld); 
  JSObject::AddProperty(isolate, webassembly, factory->to_string_tag_symbol(),  
                        name, ro_attributes);                                   

  InstallFunc(isolate, webassembly, "compile", WebAssemblyCompile, 1);          
  InstallFunc(isolate, webassembly, "validate", WebAssemblyValidate, 1);            
  InstallFunc(isolate, webassembly, "instantiate", WebAssemblyInstantiate, 1);
  ...
  Handle<JSFunction> module_constructor =                                       
      InstallConstructorFunc(isolate, webassembly, "Module", WebAssemblyModule);
  ...
}
```
And all the rest of the functions that are available on the `WebAssembly` object
are setup in the same function.
```console
(lldb) br s -name Genesis::InstallSpecialObjects
```
Now, lets also set a break point in WebAssemblyModule:
```console
(lldb) br s -n WebAssemblyModule
(lldb) r
```
```c++
  v8::Isolate* isolate = args.GetIsolate();                                         
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);                   
  if (i_isolate->wasm_module_callback()(args)) return;                              
```
Notice the `wasm_module_callback()` function which is a function that is setup
on the internal Isolate in `src/execution/isolate.h`:
```c++
#define ISOLATE_INIT_LIST(V)                                                   \
  ...
  V(ExtensionCallback, wasm_module_callback, &NoExtension)                     \
  V(ExtensionCallback, wasm_instance_callback, &NoExtension)                   \
  V(WasmStreamingCallback, wasm_streaming_callback, nullptr)                   \
  V(WasmThreadsEnabledCallback, wasm_threads_enabled_callback, nullptr)        \
  V(WasmLoadSourceMapCallback, wasm_load_source_map_callback, nullptr) 

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
```
So this would be expanded by the preprocessor into:
```c++
inline ExtensionCallback wasm_module_callback() const {
  ((void) 0);
  return wasm_module_callback_;
}
inline void set_wasm_module_callback(ExtensionCallback value) {
  ((void) 0);
  wasm_module_callback_ = value;
}
```
Also notice that if `wasm_module_callback()` return true the `WebAssemblyModule`
fuction will return and no further processing of the instructions in that function
will be done. `NoExtension` is a function that looks like this:
```c++
bool NoExtension(const v8::FunctionCallbackInfo<v8::Value>&) { return false; }
```
And is set as the default function for module/instance callbacks.

Looking a little further we can see checks for WASM Threads support (TODO: take
a look at this).
And then we have:
```c++
  module_obj = i_isolate->wasm_engine()->SyncCompile(                             
        i_isolate, enabled_features, &thrower, bytes);
```
`SyncCompile` can be found in `src/wasm/wasm-engine.cc` and will call
`DecodeWasmModule` which can be found in `src/wasm/module-decoder.cc`.
```c++
ModuleResult result = DecodeWasmModule(enabled, bytes.start(), bytes.end(),
                                       false, kWasmOrigin, 
                                       isolate->counters(), allocator()); 
```
```c++
ModuleResult DecodeWasmModule(const WasmFeatures& enabled,                      
                              const byte* module_start, const byte* module_end, 
                              bool verify_functions, ModuleOrigin origin,       
                              Counters* counters,                               
                              AccountingAllocator* allocator) {
  ...
  ModuleDecoderImpl decoder(enabled, module_start, module_end, origin);
  return decoder.DecodeModule(counters, allocator, verify_functions);
```
DecodeModuleHeader:
```c++
  uint32_t magic_word = consume_u32("wasm magic");
```
This will land in `src/wasm/decoder.h` consume_little_endian(name):
```c++

```
A wasm module has the following preamble:
```
magic nr: 0x6d736100 
version: 0x1
```
These can be found as a constant in `src/wasm/wasm-constants.h`:
```c++
constexpr uint32_t kWasmMagic = 0x6d736100; 
constexpr uint32_t kWasmVersion = 0x01;
```
After the DecodeModuleHeader the code will iterate of the sections (type,
import, function, table, memory, global, export, start, element, code, data,
custom).
For each section `DecodeSection` will be called:
```c++
DecodeSection(section_iter.section_code(), section_iter.payload(),
              offset, verify_functions);
```
There is an enum named `SectionCode` in `src/wasm/wasm-constants.h` which
contains the various sections which is used in switch statement in DecodeSection
. Depending on the `section_code` there are Decode<Type>Section methods that
will be called. In our case section_code is:
```console
(lldb) expr section_code
(v8::internal::wasm::SectionCode) $5 = kTypeSectionCode
```
And this will match the `kTypeSectionCode` and `DecodeTypeSection` will be
called.

ValueType can be found in `src/wasm/value-type.h` and there are types for
each of the currently supported types:
```c++
constexpr ValueType kWasmI32 = ValueType(ValueType::kI32);                      
constexpr ValueType kWasmI64 = ValueType(ValueType::kI64);                      
constexpr ValueType kWasmF32 = ValueType(ValueType::kF32);                      
constexpr ValueType kWasmF64 = ValueType(ValueType::kF64);                      
constexpr ValueType kWasmAnyRef = ValueType(ValueType::kAnyRef);                
constexpr ValueType kWasmExnRef = ValueType(ValueType::kExnRef);                
constexpr ValueType kWasmFuncRef = ValueType(ValueType::kFuncRef);              
constexpr ValueType kWasmNullRef = ValueType(ValueType::kNullRef);              
constexpr ValueType kWasmS128 = ValueType(ValueType::kS128);                    
constexpr ValueType kWasmStmt = ValueType(ValueType::kStmt);                    
constexpr ValueType kWasmBottom = ValueType(ValueType::kBottom);
```

`FunctionSig` is declared with a `using` statement in value-type.h:
```c++
using FunctionSig = Signature<ValueType>;
```
We can find `Signature` in src/codegen/signature.h:
```c++
template <typename T>
class Signature : public ZoneObject {
 public:
  constexpr Signature(size_t return_count, size_t parameter_count,
                      const T* reps)
      : return_count_(return_count),
        parameter_count_(parameter_count),
        reps_(reps) {}
```
The return count can be zero, one (or greater if multi-value return types are
enabled). The parameter count also makes sense, but reps is not clear to me what
that represents.
```console
(lldb) fr v
(v8::internal::Signature<v8::internal::wasm::ValueType> *) this = 0x0000555555583950
(size_t) return_count = 1
(size_t) parameter_count = 2
(const v8::internal::wasm::ValueType *) reps = 0x0000555555583948
```
Before the call to `Signature`s construtor we have:
```c++
    // FunctionSig stores the return types first.                               
    ValueType* buffer = zone->NewArray<ValueType>(param_count + return_count);  
    uint32_t b = 0;                                                             
    for (uint32_t i = 0; i < return_count; ++i) buffer[b++] = returns[i];           
    for (uint32_t i = 0; i < param_count; ++i) buffer[b++] = params[i];         
                                                                                
    return new (zone) FunctionSig(return_count, param_count, buffer);
```
So `reps_` contains the return (re?) and the params (ps?).


After the DecodeWasmModule has returned in SyncCompile we will have a
ModuleResult. This will be compiled to NativeModule:
```c++
ModuleResult result =                                                         
      DecodeWasmModule(enabled, bytes.start(), bytes.end(), false, kWasmOrigin, 
                       isolate->counters(), allocator());
Handle<FixedArray> export_wrappers;                                           
  std::shared_ptr<NativeModule> native_module =                                 
      CompileToNativeModule(isolate, enabled, thrower,                          
                            std::move(result).value(), bytes, &export_wrappers);
```
`CompileToNativeModule` can be found in `module-compiler.cc`

TODO: CompileNativeModule...

There is an example in [wasm_test.cc](./test/wasm_test.cc).

### ExtensionCallback
Is a typedef defined in `include/v8.h`:
```c++
typedef bool (*ExtensionCallback)(const FunctionCallbackInfo<Value>&); 
```




### JSEntry
TODO: This section should describe the functions calls below.
```console
 * frame #0: 0x00007ffff79a52e4 libv8.so`v8::(anonymous namespace)::WebAssemblyModule(v8::FunctionCallbackInfo<v8::Value> const&) [inlined] v8::FunctionCallbackInfo<v8::Value>::GetIsolate(this=0x00007fffffffc9a0) const at v8.h:11204:40
    frame #1: 0x00007ffff79a52e4 libv8.so`v8::(anonymous namespace)::WebAssemblyModule(args=0x00007fffffffc9a0) at wasm-js.cc:638
    frame #2: 0x00007ffff6fe9e92 libv8.so`v8::internal::FunctionCallbackArguments::Call(this=0x00007fffffffca40, handler=CallHandlerInfo @ 0x00007fffffffc998) at api-arguments-inl.h:158:3
    frame #3: 0x00007ffff6fe7c42 libv8.so`v8::internal::MaybeHandle<v8::internal::Object> v8::internal::(anonymous namespace)::HandleApiCallHelper<true>(isolate=<unavailable>, function=Handle<v8::internal::HeapObject> @ 0x00007fffffffca20, new_target=<unavailable>, fun_data=<unavailable>, receiver=<unavailable>, args=BuiltinArguments @ 0x00007fffffffcae0) at builtins-api.cc:111:36
    frame #4: 0x00007ffff6fe67d4 libv8.so`v8::internal::Builtin_Impl_HandleApiCall(args=BuiltinArguments @ 0x00007fffffffcb20, isolate=0x00000f8700000000) at builtins-api.cc:137:5
    frame #5: 0x00007ffff6fe6319 libv8.so`v8::internal::Builtin_HandleApiCall(args_length=6, args_object=0x00007fffffffcc10, isolate=0x00000f8700000000) at builtins-api.cc:129:1
    frame #6: 0x00007ffff6b2c23f libv8.so`Builtins_CEntry_Return1_DontSaveFPRegs_ArgvOnStack_BuiltinExit + 63
    frame #7: 0x00007ffff68fde25 libv8.so`Builtins_JSBuiltinsConstructStub + 101
    frame #8: 0x00007ffff6daf46d libv8.so`Builtins_ConstructHandler + 1485
    frame #9: 0x00007ffff690e1d5 libv8.so`Builtins_InterpreterEntryTrampoline + 213
    frame #10: 0x00007ffff6904b5a libv8.so`Builtins_JSEntryTrampoline + 90
    frame #11: 0x00007ffff6904938 libv8.so`Builtins_JSEntry + 120
    frame #12: 0x00007ffff716ba0c libv8.so`v8::internal::(anonymous namespace)::Invoke(v8::internal::Isolate*, v8::internal::(anonymous namespace)::InvokeParams const&) [inlined] v8::internal::GeneratedCode<unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, long, unsigned long**>::Call(this=<unavailable>, args=17072495001600, args=<unavailable>, args=17072631376141, args=17072630006049, args=<unavailable>, args=<unavailable>) at simulator.h:142:12
    frame #13: 0x00007ffff716ba01 libv8.so`v8::internal::(anonymous namespace)::Invoke(isolate=<unavailable>, params=0x00007fffffffcf50)::InvokeParams const&) at execution.cc:367
    frame #14: 0x00007ffff716aa10 libv8.so`v8::internal::Execution::Call(isolate=0x00000f8700000000, callable=<unavailable>, receiver=<unavailable>, argc=<unavailable>, argv=<unavailable>) at execution.cc:461:10

```


### CustomArguments
Subclasses of CustomArguments, like PropertyCallbackArguments and 
FunctionCallabackArguments are used for setting up and accessing values
on the stack, and also the subclasses provide methods to call various things
like `CallNamedSetter` for PropertyCallbackArguments and `Call` for
FunctionCallbackArguments.

#### FunctionCallbackArguments
```c++
class FunctionCallbackArguments                                                 
    : public CustomArguments<FunctionCallbackInfo<Value> > {
  FunctionCallbackArguments(internal::Isolate* isolate, internal::Object data,  
                            internal::HeapObject callee,                        
                            internal::Object holder,                            
                            internal::HeapObject new_target,                    
                            internal::Address* argv, int argc);
```
This class is in the namespace v8::internal so I'm curious why the explicit
namespace is used here?

#### BuiltinArguments
This class extends `JavaScriptArguments`
```c++
class BuiltinArguments : public JavaScriptArguments {
 public:
  BuiltinArguments(int length, Address* arguments)
      : Arguments(length, arguments) {

  static constexpr int kNewTargetOffset = 0;
  static constexpr int kTargetOffset = 1;
  static constexpr int kArgcOffset = 2;
  static constexpr int kPaddingOffset = 3;
                                                                                
  static constexpr int kNumExtraArgs = 4;
  static constexpr int kNumExtraArgsWithReceiver = 5;
```
`JavaScriptArguments is declared in `src/common/global.h`:
```c++
using JavaScriptArguments = Arguments<ArgumentsType::kJS>;
```
`Arguments` can be found in `src/execution/arguments.h`and is templated with 
the a type of `ArgumentsType` (in `src/common/globals.h`):
```c++
enum class ArgumentsType {                                                          
  kRuntime,                                                                         
  kJS,                                                                              
}; 
```
An instance of Arguments only has a length which is the number of arguments,
and an Address pointer which points to the first argument. The functions it
provides allows for getting/setting specific arguments and handling various
types (like `Handle<S>`, smi, etc). It also overloads the operator[] allowing
to specify an index and getting back an Object to that argument.
In `BuiltinArguments` the constants specify the index's and provides functions
to get them:
```c++
  inline Handle<Object> receiver() const;                                       
  inline Handle<JSFunction> target() const;                                     
  inline Handle<HeapObject> new_target() const;
```

### NativeContext
Can be found in `src/objects/contexts.h` and has the following definition:
```c++
class NativeContext : public Context {
 public:

  DECL_PRIMITIVE_ACCESSORS(microtask_queue, MicrotaskQueue*)

  V8_EXPORT_PRIVATE void AddOptimizedCode(Code code);
  void SetOptimizedCodeListHead(Object head);
  Object OptimizedCodeListHead();
  void SetDeoptimizedCodeListHead(Object head);
  Object DeoptimizedCodeListHead();
  inline OSROptimizedCodeCache GetOSROptimizedCodeCache();
  void ResetErrorsThrown();
  void IncrementErrorsThrown();
  int GetErrorsThrown();
```

`src/parsing/parser.h` we can find:
```c++
class V8_EXPORT_PRIVATE Parser : public NON_EXPORTED_BASE(ParserBase<Parser>) { 
  ...
  enum CompletionKind {                                                             
    kNormalCompletion,                                                              
    kThrowCompletion,                                                               
    kAbruptCompletion                                                               
  };
```
But I can't find any usages of this enum? 

#### Internal fields/methods
When you see something like [[Notation]] you can think of this as a field in
an object that is not exposed to JavaScript user code but internal to the JavaScript
engine. These can also be used for internal methods.


