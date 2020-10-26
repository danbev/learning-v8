## Introduction
V8 basically consists of the memory management of the heap and the execution
stack (very simplified but helps make my point). Things like the callback queue,
the event loop and other things like the WebAPIs (DOM, ajax, setTimeout etc) are
found inside Chrome or in the case of Node the APIs are Node.js APIs:
```
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
| |     Event loop      |     |          Task/Callback queue          |                    |
| |                     |     |                                       |                    |
| +---------------------+     +---------------------------------------+                    |
|                             +---------------------------------------+                    |
|                             |          Microtask queue              |                    |
|                             |                                       |                    |
|                             +---------------------------------------+                    |
|                                                                                          |
|                                                                                          |
+------------------------------------------------------------------------------------------+
```
The execution stack is a stack of frame pointers. For each function called, that
function will be pushed onto the stack. When that function returns it will be
removed. If that function calls other functions they will be pushed onto the
stack. When they have all returned execution can proceed from the returned to 
point. If one of the functions performs an operation that takes time progress
will not be made until it completes as the only way to complete is that the
function returns and is popped off the stack. This is what happens when you have
a single threaded programming language.

So that describes synchronous functions, what about asynchronous functions?  
Lets take for example that you call setTimeout, the setTimeout function will be
pushed onto the call stack and executed. This is where the callback queue comes
into play and the event loop. The setTimeout function can add functions to the
callback queue. This queue will be processed by the event loop when the call
stack is empty.

### Task
A task is a function that can be scheduled by placing the task on the callback
queue. This is done be WebAPIs like `setTimeout` and `setInterval`.
When the event loop starts executing tasks it will run all the tasks that
are currently in the task queue. Any new tasks that get scheduled by WebAPI
function calls are only pushed onto the queue but will not be executed until
the next iteration of the event loop.

When the execution stack is empty all the tasks in the microtask queue will be
run, and if any of these tasks add tasks to the microtask queue that will also
be run which is different compared with how the task queue handles this situation.

In Node.js `setTimeout` and `setInterval`...


### Micro task
Is a function that is executed after current function has run after all the
other functions that are currently on the call stack.

Microtasks internals info can be found in [microtasks](./microtasks.md).


#### Microtask queue
When a promise is created it will execute right away and if it has been resovled
you can call `then` on it.

