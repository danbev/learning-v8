### Tasks and micro tasks
This document takes a closer look at both tasks and microtasks.

### Tasks
As mentioned in the [introduction](./intro.md) a task is something that is
placed on the task/callback queue and will be picked up by the runtime and
placed onto the execution stack.
For example, if a function calls `setTimeout` that will place the function to be
executed on the task/callback queue once the timer has expired.

Let's take a look at a concrete [example](../lib/task.js) using node.js
```console
$ env NODE_DEBUG=timer node lib/task.js 
main...
TIMER 3924762: no 1 list was found in insert, creating a new one
main...done
TIMER 3924762: process timer lists 145
TIMER 3924762: timeout callback 1
something
TIMER 3924762: 1 list empty
```
In node.js `setTimeout` is implemented using `libuv`'s `uv_timer_start`

### Microtasks
The [introduction](./intro.md) has some background information about microtasks.
This page is going to look into the internals. 

There is an example of running a microtask in [microtask_test](../test/microtask_test.c).

The Isolate class exposes a number of functions related to enqueueing and running
microtasks:
```c++
class V8_EXPORT Isolate {                                                       
 public:     
 ...
  V8_DEPRECATE_SOON("Use PerformMicrotaskCheckpoint.")                          
  void RunMicrotasks() { PerformMicrotaskCheckpoint(); }  
  void PerformMicrotaskCheckpoint();
  void EnqueueMicrotask(Local<Function> microtask);
  void EnqueueMicrotask(MicrotaskCallback callback, void* data = nullptr);
};
```

Lets take a look at what happens when we enqueue a microtask function:
```console
$ lldb -- ./test/microtask_test
(lldb) br s -f microtask_test.cc -l 33
(lldb) r
```
`Isolate::EnqueueMicrotask` can be found in `src/api/api.cc`
The actual call will end up in `microtask-queue.cc` which can be found in
`src/execution/microtask-queue.cc`:
```c++
void MicrotaskQueue::EnqueueMicrotask(v8::Isolate* v8_isolate,
                                      v8::Local<Function> function) {
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
  HandleScope scope(isolate);
  Handle<CallableTask> microtask = isolate->factory()->NewCallableTask(
      Utils::OpenHandle(*function), isolate->native_context());
  EnqueueMicrotask(*microtask);
}
```
NewCallableTask can be found in `src/heap/factory.cc`.

