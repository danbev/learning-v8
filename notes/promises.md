### Promises
A Promise is an object which has a state. For example, we can create a promise
using the following call:

```c++
  i::Handle<i::JSPromise> promise = factory->NewJSPromise();
  print_handle(promise);
```
This will output:
```console
0x3e46080c1cb9: [JSPromise]
 - map: 0x3e46081c0fa9 <Map(HOLEY_ELEMENTS)> [FastProperties]
 - prototype: 0x3e46082063ed <Object map = 0x3e46081c0fd1>
 - elements: 0x3e46080406e9 <FixedArray[0]> [HOLEY_ELEMENTS]
 - status: pending
 - reactions: 0
 - has_handler: 0
 - handled_hint: 0
 - properties: 0x3e46080406e9 <FixedArray[0]> {}
```
The implementation for `JSPromise` can be found in `src/objects/js-promise.h`.

Now, `factory->NewJSPromise` looks like this:
```c++
Handle<JSPromise> Factory::NewJSPromise() {
  Handle<JSPromise> promise = NewJSPromiseWithoutHook();
  isolate()->RunPromiseHook(PromiseHookType::kInit, promise, undefined_value());
  return promise;
}
Handle<JSPromise> Factory::NewJSPromiseWithoutHook() {
  Handle<JSPromise> promise =
      Handle<JSPromise>::cast(NewJSObject(isolate()->promise_function()));
  promise->set_reactions_or_result(Smi::zero());
  promise->set_flags(0);
  ZeroEmbedderFields(promise);
  DCHECK_EQ(promise->GetEmbedderFieldCount(), v8::Promise::kEmbedderFieldCount);
  return promise;
}
```
Notice `isolate->promise_function()` what does it return and where is it
defined:
```console
$ gdb ./test/promise_test
(gdb) b Factory::NewJSPromise
(gdb) r
(gdb) p isolate()->promise_function()
$1 = {<v8::internal::HandleBase> = {location_ = 0x20a7c68}, <No data fields>}
```
That did tell me much. But if we look at `src/execution/isolate-inl.h` we can
find:
```c++
#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name)    \
  Handle<type> Isolate::name() {                            \
    return Handle<type>(raw_native_context().name(), this); \
  }                                                         \
  bool Isolate::is_##name(type value) {                     \
    return raw_native_context().is_##name(value);           \
  }
NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSOR)
#undef NATIVE_CONTEXT_FIELD_ACCESSOR
```
And `NATIVE_CONTEXT_FIELDS` can be found in "src/objects/contexts.h" which is
included by isolate.h (which isolate-inl.h includes). In contexts.h we find:
```c++
#define NATIVE_CONTEXT_FIELDS(V)
  ...
  V(PROMISE_FUNCTION_INDEX, JSFunction, promise_function)
```
So the preprocessor will generate the following functions for promise_function:
```c++
inline Handle<JSFunction> promise_function();
inline bool is_promise_function(JSFunction value);

Handle<JSFunction> Isolate::promise_function() {
  return Handle<JSFunction>(raw_native_context().promise_function(), this);
}
bool Isolate::is_promise_function(JSFunction value) {
  return raw_native_context().is_promise_function(value);
}
```
And the functions that are called from the above (in src/objects/contexts.h):
```c++
inline void set_promise_function(JSFunction value);
inline bool is_promise_function(JSFunction value) const;
inline JSFunction promise_function() const;

void Context::set_promise_function(JSFunction value) {
  ((void) 0);
  set(PROMISE_FUNCTION_INDEX, value);
}
bool Context::is_promise_function(JSFunction value) const {
  ((void) 0);
  return JSFunction::cast(get(PROMISE_FUNCTION_INDEX)) == value;
}
JSFunction Context::promise_function() const {
  ((void) 0);
  return JSFunction::cast(get(PROMISE_FUNCTION_INDEX));
}
```
So that answers where the function is declared and defined and what it returns.


We can find the torque source file in `src/builtins/promise-constructor.tq` which
has comments that refer to the emcascript spec. In our case this is
[promise-executor](https://tc39.es/ecma262/#sec-promise-executor)
```c++
  transitioning javascript builtin                                                 
  PromiseConstructor(                                                              
      js-implicit context: NativeContext, receiver: JSAny,                         
      newTarget: JSAny)(executor: JSAny): JSAny {
   // 1. If NewTarget is undefined, throw a TypeError exception.                  
   if (newTarget == Undefined) {
     ThrowTypeError(MessageTemplate::kNotAPromise, newTarget);
   }
```
And for the generated c++ code we can look in `out/x64.release_gcc/gen/torque-generated/src/builtins/promise-constructor-tq-csa.cc'.

Now, if we look at the spec and the torque source we can find the first step 
in the spec is:
```
1. If NewTarget is undefined, throw a TypeError exception.
```
And in the torque source file:
```
  if (newTarget == Undefined) {
      ThrowTypeError(MessageTemplate::kNotAPromise, newTarget);
  }
```
And in the generated CodeStubAssembler c++ source for this:
```c++
TF_BUILTIN(PromiseConstructor, CodeStubAssembler) {                             
  compiler::CodeAssemblerState* state_ = state();
  compiler::CodeAssembler ca_(state());
  TNode<Object> parameter2 = UncheckedCast<Object>(Parameter(Descriptor::kJSNewTarget));
  ...

  TNode<Oddball> tmp0;
  TNode<BoolT> tmp1;
  if (block0.is_used()) {
    ca_.Bind(&block0);
    ca_.SetSourcePosition("../../src/builtins/promise-constructor.tq", 51);
    tmp0 = Undefined_0(state_);
    tmp1 = CodeStubAssembler(state_).TaggedEqual(TNode<Object>{parameter2}, TNode<HeapObject>{tmp0});
    ca_.Branch(tmp1, &block1, std::vector<Node*>{}, &block2, std::vector<Node*>{});
  }             

  if (block1.is_used()) {
    ca_.Bind(&block1);
    ca_.SetSourcePosition("../../src/builtins/promise-constructor.tq", 52);
    CodeStubAssembler(state_).ThrowTypeError(TNode<Context>{parameter0}, MessageTemplate::kNotAPromise, TNode<Object>{parameter2});
  }
```
Now, `TF_BUILTIN` is a macro on `src/builtins/builtins-utils-gen.h`. 
```c++
#define TF_BUILTIN(Name, AssemblerBase)                                 \
  class Name##Assembler : public AssemblerBase {                        \
   public:                                                              \
    using Descriptor = Builtin_##Name##_InterfaceDescriptor;            \
                                                                        \
    explicit Name##Assembler(compiler::CodeAssemblerState* state)       \
        : AssemblerBase(state) {}                                       \
    void Generate##Name##Impl();                                        \
                                                                        \
    Node* Parameter(Descriptor::ParameterIndices index) {               \
      return CodeAssembler::Parameter(static_cast<int>(index));         \
    }                                                                   \
  };                                                                    \
  void Builtins::Generate_##Name(compiler::CodeAssemblerState* state) { \
    Name##Assembler assembler(state);                                   \
    state->SetInitialDebugInformation(#Name, __FILE__, __LINE__);       \
    if (Builtins::KindOf(Builtins::k##Name) == Builtins::TFJ) {         \
      assembler.PerformStackCheck(assembler.GetJSContextParameter());   \
    }                                                                   \
    assembler.Generate##Name##Impl();                                   \
  }                                                                     \
  void Name##Assembler::Generate##Name##Impl()
```

So the above will be expanded by the preprocessor into:
```c++
TF_BUILTIN(PromiseConstructor, CodeStubAssembler) {                             
class PromiseConstructorAssembler : public CodeStubAssembler {
 public:
  using Descriptor = Builtin_PromiseConstructor_InterfaceDescriptor;

  explicit PromiseConstructorAssembler(compiler::CodeAssemblerState* state) :
      CodeStubAssembler(state) {}
  void GeneratePromiseConstructorImpl;

  Node* Parameter(Descriptor::ParameterIndices index) {
    return CodeAssembler::Parameter(static_cast<int>(index));
  }
};

void Builtins::Generate_PromiseConstructor(compiler::CodeAssemblerState* state) {
  PromiseConstructorAssembler assembler(state);
  state->SetInitialDebugInformation(PromiseConstructor, __FILE__, __LINE__);
  if (Builtins::KindOf(Builtins::kPromiseConstructor) == Builtins::TFJ) {
    assembler.PerformStackCheck(assembler.GetJSContextParameter());
  }
  assembler.GeneratePromiseConstructorImpl();
}

void PromiseConstructorAssembler::GeneratePromiseConstructorImpl()
  compiler::CodeAssemblerState* state_ = state();
  compiler::CodeAssembler ca_(state());
  TNode<Object> parameter2 = UncheckedCast<Object>(Parameter(Descriptor::kJSNewTarget));
  ... rest of the content that we already showed above.
}
```
And if we are want to inspect the generated assembler code we can find it
in
```console
$ objdump -d ../v8_src/v8/out/x64.release_gcc/obj/torque_generated_initializers/promise-constructor-tq-csa.o | c++filt
```

And this builtin is hooked up by `src/init/boostraper.cc` in:
```c++
void Genesis::InitializeGlobal(Handle<JSGlobalObject> global_object,
                               Handle<JSFunction> empty_function) {
  ...
  {  // -- P r o m i s e                                                        
    Handle<JSFunction> promise_fun = InstallFunction(
        isolate_, global, "Promise", JS_PROMISE_TYPE,
        JSPromise::kSizeWithEmbedderFields, 0, factory->the_hole_value(),
        Builtins::kPromiseConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, promise_fun, Context::PROMISE_FUNCTION_INDEX); 
```
Install function takes the following parameters:
```c++
V8_NOINLINE Handle<JSFunction> InstallFunction(
    Isolate* isolate, Handle<JSObject> target, const char* name,
    InstanceType type, int instance_size, int inobject_properties,
    Handle<HeapObject> prototype, Builtins::Name call) {
  return InstallFunction(isolate, target,
                         isolate->factory()->InternalizeUtf8String(name), type, 
                         instance_size, inobject_properties, prototype, call);
}
```
The above function will be called when creating the snapshot (if snapshots
are configured) so one way to explore this is to debug `mksnapshot`:
```console
$ cd out/x64_release_gcc/
$ gdb mksnapshot
$ Breakpoint 1 at 0x1da5cc7: file ../../src/init/bootstrapper.cc, line 1409.
(gdb) br bootstrapper.cc:2347
(gdb) r
(gdb) continue
(gdb) p JS_PROMISE_TYPE
$1 = v8::internal::JS_PROMISE_TYPE
```
Details about [InstanceType](#instancetype).

And we can see that we passing in `Builtins::kPromiseConstructor`. This is declared 
in `out/x64.release_gcc/gen/torque-generated/builtin-definitions-tq.h`:
```c++
#define BUILTIN_LIST_FROM_TORQUE(CPP, TFJ, TFC, TFS, TFH, ASM) \
...
TFJ(PromiseConstructor, 1, kReceiver, kExecutor) \ 
```
For full details see the [Torque](#torque) section. We will get the following
in `builtins.h` (after being preprocessed):
```c++
class Builtins {
  enum Name : int32_t {
    ...
    kPromiseConstructor,
  };

  static void Generate_PromiseConstructor(compiler::CodeAssemblerState* state); 

};
``` 
And and in builtins.cc (also after being preprocessed):
```c++
struct Builtin_PromiseConstructor_InterfaceDescriptor {
  enum ParameterIndices {
    kJSTarget = compiler::CodeAssembler::kTargetParameterIndex,
    kReceiver,
    kExecutor,
    kJSNewTarget,
    kJSActualArgumentsCount,
    kContext,
    kParameterCount,
  };
};
const BuiltinMetadata builtin_metadata[] = {
  ...
  {"PromiseConstructor", Builtins::TFJ, {1, 0}},
  ...
};
```
`Generate_PromiseConstructor` is declared as as a static function in Builtins
and recall that we showed above that is defined in
`out/x64.release_gcc/gen/torque-generated/src/builtins/promise-constructor-tq-csa.cc'.

So to recap a little, when we do:
```js
const p1 = new Promise(function(resolve, reject) {
  console.log('Running p1 executor function...');
  resolve("success!");
});
```
This will invoke the builtin function Builtins::kPromiseConstructor that was
installed on the Promise object, which was either done upon startup if snapshots
are disabled, or by mksnapshot if they are enabled. 

TODO: continue exploration...

[Promise Objects](https://tc39.es/ecma262/#sec-promise-objects).

There is an example in [promise_test.cc](../test/promise_test.cc)

