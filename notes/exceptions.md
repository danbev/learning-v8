### Exception
This document contains notes regarding Exception/Error handling in V8.

### Exceptions
When a function is called how is an exception reported/triggered in the call
chain?  

```c++
  Local<FunctionTemplate> ft = FunctionTemplate::New(isolate_, function_callback);
  Local<Function> function = ft->GetFunction(context).ToLocalChecked();
  Local<Object> recv = Object::New(isolate_);
  MaybeLocal<Value> ret = function->Call(context, recv, 0, nullptr);
```
`function->Call` will end up in `src/api/api.cc` `Function::Call` and will
in turn call `v8::internal::Execution::Call`:
```c++
  has_pending_exception = !ToLocal<Value>(                                           
      i::Execution::Call(isolate, self, recv_obj, argc, args), &result);             
  RETURN_ON_FAILED_EXECUTION(Value);                                                 
  RETURN_ESCAPED(result);            
```
Notice that the result of `Call` which is a `MaybeHandle<Object>` will be
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

Execution::Call will call `Invoke` which
```c++
V8_WARN_UNUSED_RESULT MaybeHandle<Object> Invoke(Isolate* isolate,              
                                                 const InvokeParams& params) {
    ...
    auto value = Builtins::InvokeApiFunction(                                 
          isolate, params.is_construct, function, receiver, params.argc,        
          params.argv, Handle<HeapObject>::cast(params.new_target));            

    bool has_exception = value.is_null();                                     
    DCHECK(has_exception == isolate->has_pending_exception());                
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

```c++
  VMState<EXTERNAL> state(isolate);                                             
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                  
  FunctionCallbackInfo<v8::Value> info(values_, argv_, argc_); 
  f(info);                                                                      
  return GetReturnValue<Object>(isolate);                                       
}
```
`f(info)` is a function pointer to our callback:
```console
(lldb) expr f
(v8::FunctionCallback) $0 = 0x0000000000415fa6 (exceptions_test`function_callback(v8::FunctionCallbackInfo<v8::Value> const&) at exceptions_test.cc:16:65)
```
When our callback calls info.GetReturnValue() this get the ReturnType slot
from the args and we set its value to 8 and then return.
Next, `GetReturnValue` is called and then we return to HandleApiCallHelper
which has the following macro:
```c++
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
```
Which will be expanded by the preprocessor into:
```c++
  do {
    Isolate* __isolate__ = (isolate);
    ((void) 0);
    if (__isolate__->has_scheduled_exception()) {
      __isolate__->PromoteScheduledException();
      return MaybeHandle<Object>();
    }
  } while (false);
```
So in this case `has_scheduled_exception` will be true and notice that
an empty MaybeHandle will be returned to `Invoke`.
```c++
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
    }                                    
```
This will return to v8::Function::Call:
```c++
  has_pending_exception = !ToLocal<Value>(                                           
      i::Execution::Call(isolate, self, recv_obj, argc, args), &result);             
  RETURN_ON_FAILED_EXECUTION(Value);                                                 
  RETURN_ESCAPED(result);            
```
We can expand these macro using the following command:
```console
$ g++ -Iout/learning_v8/gen/ -I./include -I. -E src/api/api.cc
```
```c++
  do {
    if (has_pending_exception) {
      call_depth_scope.Escape();
      return MaybeLocal<Value>(); }
  } while (false);
  return handle_scope.Escape(result);;
```


### Throwing an Exception
When calling a Function one can throw an exception using:
```c++
  isolate->ThrowException(String::NewFromUtf8(isolate, "some error").ToLocalChecked());
```
`ThrowException` can be found in `src/api/api.cc` and what it does is:
```c++
  ENTER_V8_DO_NOT_USE(isolate);                                                 
  // If we're passed an empty handle, we throw an undefined exception           
  // to deal more gracefully with out of memory situations.                     
  if (value.IsEmpty()) {                                                        
    isolate->ScheduleThrow(i::ReadOnlyRoots(isolate).undefined_value());             
  } else {                                                                           
    isolate->ScheduleThrow(*Utils::OpenHandle(*value));                              
  }                                                                                  
  return v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));      
```
Lets take a closer look at `ScheduleThrow`. 
```c++
void Isolate::ScheduleThrow(Object exception) {                                     
  Throw(exception);                                                                 
  PropagatePendingExceptionToExternalTryCatch();                                    
  if (has_pending_exception()) {                                                    
    thread_local_top()->scheduled_exception_ = pending_exception();                 
    thread_local_top()->external_caught_exception_ = false;                         
    clear_pending_exception();                                                      
  }                                                                                 
}
```
`Throw` will end by setting the pending_exception to the exception passed in.
Next, `PropagatePendingExceptionToExternalTryCatch` will be called. This is
where a `TryCatch` handler comes into play. If one had been registered, which as
I'm writing this I did not have one (but will add one to try it out and verify).
The code for this part looks like this:
```c++
  v8::TryCatch* handler = try_catch_handler();
  handler->can_continue_ = true;
  handler->has_terminated_ = false;
  handler->exception_ = reinterpret_cast<void*>(pending_exception().ptr());
  // Propagate to the external try-catch only if we got an actual message.
  if (thread_local_top()->pending_message_obj_.IsTheHole(this)) return true;
  handler->message_obj_ = reinterpret_cast<void*>(thread_local_top()->pending_message_obj_.ptr());
```  
When a TryCatch is created its constructor will call `RegisterTryCatchHandler`
which will set the thread_local_top try_catch_handler which is retrieved above
with the call to `try_catch_handler()`.

Prior to this there will be a call to `IsJavaScriptHandlerOnTop`:
```c++
// For uncatchable exceptions, the JavaScript handler cannot be on top.           
if (!is_catchable_by_javascript(exception)) return false;
```
```c++
  return exception != ReadOnlyRoots(heap()).termination_exception();               
}
```
TODO: I really need to understand this better and the various ways to
catch/handle exceptions (from C++ and JavaScript).

Next (in PropagatePendingExceptionToExternalTryCatch) we have:
```c++
  // Get the top-most JS_ENTRY handler, cannot be on top if it doesn't exist.   
  Address entry_handler = Isolate::handler(thread_local_top());                 
  if (entry_handler == kNullAddress) return false;
```
Next, he have the following:
```c++
  if (!IsExternalHandlerOnTop(exception)) {                                     
    thread_local_top()->external_caught_exception_ = false;                     
    return true;                                                                
  }
```
I'm really confused at the moment with these different handler, we have one
for. 


Now, for a javascript function that is executed using `Run` what would be
used in execution.cc Execution::Call would be:
```c++
Handle<Code> code = JSEntry(isolate, params.execution_target, params.is_construct);
```
```c++
Handle<Code> JSEntry(Isolate* isolate, Execution::Target execution_target, bool is_construct) {
  if (is_construct) {
    return BUILTIN_CODE(isolate, JSConstructEntry);
  } else if (execution_target == Execution::Target::kCallable) {
    return BUILTIN_CODE(isolate, JSEntry);
    isolate->builtins()->builtin_handle(Builtins::kJSEntry)
  } else if (execution_target == Execution::Target::kRunMicrotasks) {
    return BUILTIN_CODE(isolate, JSRunMicrotasksEntry);
  }
  UNREACHABLE();
}
```

```c++
if (params.execution_target == Execution::Target::kCallable) {
  // clang-format off
  // {new_target}, {target}, {receiver}, return value: tagged pointers
  // {argv}: pointer to array of tagged pointers
  using JSEntryFunction = GeneratedCode<Address(
      Address root_register_value, Address new_target, Address target,
      Address receiver, intptr_t argc, Address** argv)>;
  JSEntryFunction stub_entry =
      JSEntryFunction::FromAddress(isolate, code->InstructionStart());
  Address orig_func = params.new_target->ptr();
  Address func = params.target->ptr();
  Address recv = params.receiver->ptr();
  Address** argv = reinterpret_cast<Address**>(params.argv);

  RuntimeCallTimerScope timer(isolate, RuntimeCallCounterId::kJS_Execution);

  value = Object(stub_entry.Call(isolate->isolate_data()->isolate_root(),
                                 orig_func, func, recv, params.argc, argv));
```
