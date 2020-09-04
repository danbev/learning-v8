### Node.js Yarn issue on ARM 64
This section documents the troubleshooting of an issue with Node.js v12.16.1
and yarn.

Backtrace:
```console
(gdb) bt 10
#0  0x0000aaaaab4d07f4 in std::_Rb_tree<unsigned long, unsigned long, std::_Identity<unsigned long>, std::less<unsigned long>, std::allocator<unsigned long> >::_M_get_insert_unique_pos (__k=<synthetic pointer>: <optimized out>, this=0x18)
    at /usr/include/c++/8/bits/stl_tree.h:2101
#1  std::_Rb_tree<unsigned long, unsigned long, std::_Identity<unsigned long>, std::less<unsigned long>, std::allocator<unsigned long> >::_M_insert_unique<unsigned long const&> (__v=<synthetic pointer>: <optimized out>, this=0x18)
    at /usr/include/c++/8/bits/stl_tree.h:2109
#2  std::set<unsigned long, std::less<unsigned long>, std::allocator<unsigned long> >::insert (
    __x=<synthetic pointer>: <optimized out>, this=0x18) at /usr/include/c++/8/bits/stl_set.h:511
#3  v8::internal::CodeObjectRegistry::RegisterNewlyAllocatedCodeObject (this=0x0, code=code@entry=1301151744)
    at ../deps/v8/src/heap/spaces.cc:620
#4  0x0000aaaaab47ea90 in v8::internal::Heap::AllocateRaw (alignment=v8::internal::kWordAligned, 
    origin=v8::internal::AllocationOrigin::kRuntime, type=-80, size_in_bytes=129152, this=0xaaaaacbbaab0)
    at ../deps/v8/src/objects/heap-object.h:108
#5  v8::internal::Heap::AllocateRawWithLightRetry (this=this@entry=0xaaaaacbbaab0, size=size@entry=129152, 
    allocation=allocation@entry=v8::internal::AllocationType::kCode, 
    origin=origin@entry=v8::internal::AllocationOrigin::kRuntime, alignment=alignment@entry=v8::internal::kWordAligned)
    at ../deps/v8/src/heap/heap.cc:4886
#6  0x0000aaaaab47ec38 in v8::internal::Heap::AllocateRawWithRetryOrFail (this=0xaaaaacbbaab0, size=129152, 
    allocation=v8::internal::AllocationType::kCode, origin=v8::internal::AllocationOrigin::kRuntime, 
    alignment=v8::internal::kWordAligned) at ../deps/v8/src/heap/heap.cc:4908
#7  0x0000aaaaab451fe0 in v8::internal::Heap::AllocateRawWithRetryOrFail (alignment=v8::internal::kWordAligned, 
    allocation=v8::internal::AllocationType::kCode, size=129152, this=0xaaaaacbbaab0) at ../deps/v8/src/heap/heap.h:1762
#8  v8::internal::Factory::CodeBuilder::BuildInternal (this=this@entry=0xffffffff96b0, 
    retry_allocation_or_fail=retry_allocation_or_fail@entry=true) at ../deps/v8/src/heap/factory.cc:121
#9  0x0000aaaaab452380 in v8::internal::Factory::CodeBuilder::Build (this=this@entry=0xffffffff96b0)
    at ../deps/v8/src/heap/factory.cc:205
```
Lets take a look at `AllocateRawWithLightRetry`:
```c++
4882 HeapObject Heap::AllocateRawWithLightRetry(int size, AllocationType allocation, 
4883                                            AllocationOrigin origin,             
4884                                            AllocationAlignment alignment) {     
4885   HeapObject result;                                                            
4886   AllocationResult alloc = AllocateRaw(size, allocation, origin, alignment);
```
The next frame down is:
```
(gdb) down
#4  0x0000aaaaab47ea90 in v8::internal::Heap::AllocateRaw (alignment=v8::internal::kWordAligned, 
    origin=v8::internal::AllocationOrigin::kRuntime, type=-80, size_in_bytes=129152, this=0xaaaaacbbaab0)
    at ../deps/v8/src/objects/heap-object.h:108
108	  inline Address address() const { return ptr() - kHeapObjectTag; }
```
Lets set a conditional break point heap.cc:4886:
```console
(gdb) br heap.cc:4886 if size == 129152
Breakpoint 5 at 0xaaaaab47e700: file ../deps/v8/src/heap/heap.cc, line 4886.
```
This will take us into heap-inl.h:
```c++
206   } else if (AllocationType::kCode == type) {                                   
207     if (size_in_bytes <= code_space()->AreaSize() && !large_object) {           
208       allocation = code_space_->AllocateRawUnaligned(size_in_bytes);            
209     } else {                                                                    
210       allocation = code_lo_space_->AllocateRaw(size_in_bytes);                  
211     }                                                         
```
In our case line 210 will be executed. This will take us into `spaces.cc`
```c++
4386 AllocationResult CodeLargeObjectSpace::AllocateRaw(int object_size) {           
4387   return LargeObjectSpace::AllocateRaw(object_size, EXECUTABLE);                
4388 } 
```
In `heap-inl.h` and `AllocateRaw` following code towards the end of the function:
```c++
227   if (allocation.To(&object)) {                                                 
228     if (AllocationType::kCode == type) {                                        
229       // Unprotect the memory chunk of the object if it was not unprotected     
230       // already.                                                               
231       UnprotectAndRegisterMemoryChunk(object);                                  
232       ZapCodeObject(object.address(), size_in_bytes);                           
233       if (!large_object) {                                                      
234         MemoryChunk::FromHeapObject(object)                                     
235             ->GetCodeObjectRegistry()                                           
236             ->RegisterNewlyAllocatedCodeObject(object.address());               
237       }                                                                         
238     }                                                                           
239     OnAllocationEvent(object, size_in_bytes);                                   
240   }                                                                             
241                                                                                 
242   return allocation;  
```
This will call
MemoryChunk::FromHeapObject(object)->GetCodeObjectRegistry()->RegisterNewlyAllocatedCodeObject(object.address()). 

MemoryChunk::FromHeapObject is only a reinterpret_cast:
```c++
return reinterpret_cast<MemoryChunk*>(object.ptr() & ~kPageAlignmentMask);

const int kPageSizeBits = 18;
static const intptr_t kPageAlignmentMask = (intptr_t{1} << kPageSizeBits) - 1;
```
And notice `this=0x18` in the backtrace:
```console
0x0000aaaaab4d07f4 in std::_Rb_tree<unsigned long, unsigned long, std::_Identity<unsigned long>, std::less<unsigned long>, std::allocator<unsigned long> >::_M_get_insert_unique_pos (__k=<synthetic pointer>: 716832768, this=0x18)
```
I found it strange when I first saw this and that the segfault was coming from
the std::set::insert but hopefully the following example will clarify things:
```c++
#include <stdint.h>
#include <set>
#include <iostream>

struct something {
  std::set<int> xs;
};

int main(int argc, char** argv) {
  intptr_t i = intptr_t{1} + 17;
  struct something* s = reinterpret_cast<something*>(i);
  std::cout << i << '\n';
  s->xs.insert(2);
  return 0;
}
```
So that explains the strange value of `this` and how the it was possible
that this was 0x18. And it should also explain the how it was possible to call
insert even though this was invalid:
```
auto result = code_object_registry_newly_allocated_.insert(code);
```
So we now understand the cause of the segmentation fault. But we still need to
understand why this is happening.

Lets create a break point so that we can easliy reproduce this:
```console
(gdb) br heap-inl.h:210 if size_in_bytes == 129152
(gdb) br heap-inl.h:233 if size_in_bytes == 129152
```
The first break point is where the `allocation` is created by calling `AllocateRaw'
on the `code_lo_space` (which is of type `CodeLargeObjectSpace` which can be
found in `src/heap/spaces.h`):
```c++
210       allocation = code_lo_space_->AllocateRaw(size_in_bytes);                  
```
`allocation is declared earlier as:
```c++
  AllocationResult allocation;
```
An `AllocationResult` instance has a single member which is of type Object.
There is a function that can cast an object into a pointer of a type:
```c++
template <typename T>                                                             
bool To(T* obj) {                                                                 
  if (IsRetry()) return false;                                                
  *obj = T::cast(object_);                                                    
  return true;                                                                
}
```
This function is called in the following `if` statement:
```c++
  HeapObject object; 
  ...
  if (allocation.To(&object)) {
    if (AllocationType::kCode == type) {
      ...
}
```
The value of the field `object_` is:
```console
(gdb) p allocation.object_
$10 = {<v8::internal::TaggedImpl<(v8::internal::HeapObjectReferenceType)1, unsigned long>> = {static kIsFull = true, 
    static kCanBeWeak = false, ptr_ = 1332346881}, static kHeaderSize = 0}
```
The value of object is unfortunately optimized out:
```console
(gdb) p object
$13 = <optimized out>
```
So this is the object that will be passed to HeapObject::cast(object_). This `To`
function will return true and the the next line that will be executed is
```c++
      UnprotectAndRegisterMemoryChunk(object);
      ZapCodeObject(object.address(), size_in_bytes);
      if (!large_object) {
        MemoryChunk::FromHeapObject(object)
            ->GetCodeObjectRegistry()
            ->RegisterNewlyAllocatedCodeObject(object.address());
      }
```
I've attempted to reproduce this in [heap_test](https://github.com/danbev/learning-v8/blob/f3aae7a4e073f5a3183199aab89b0a8abe09680f/test/heap_test.cc#L48)

I looks like the `code_object_registry_` is not gettting created in this case.
In `MemoryChunk::Initialize` there is the following check
(in deps/v8/src/heap/spaces.cc) which is called by
CodeLargeObjectSpace::AllocateRaw which we showed earlier:
```c++
MemoryChunk* MemoryChunk::Initialize(BasicMemoryChunk* basic_chunk, Heap* heap, 
                                     Executability executable) {   
  ...

  if (chunk->owner()->identity() == CODE_SPACE) {
    chunk->code_object_registry_ = new CodeObjectRegistry();
  } else {
    chunk->code_object_registry_ = nullptr;
  }
```
For example if the owner is of type `CODE_LO_SPACE` there would not be a
CodeObjectRegistry created. And this is the case we are seeing. 

I'd like to try out the following patch:
```console
diff --git a/deps/v8/src/heap/spaces.cc b/deps/v8/src/heap/spaces.cc
index dd8ba30101..3e8f2ec005 100644
--- a/deps/v8/src/heap/spaces.cc
+++ b/deps/v8/src/heap/spaces.cc
@@ -749,7 +749,7 @@ MemoryChunk* MemoryChunk::Initialize(Heap* heap, Address base, size_t size,
 
   chunk->reservation_ = std::move(reservation);
 
-  if (owner->identity() == CODE_SPACE) {
+  if (owner->identity() == CODE_SPACE || owner->identity() == CODE_LO_SPACE) {
     chunk->code_object_registry_ = new CodeObjectRegistry();
   } else {
     chunk->code_object_registry_ = nullptr;
```

I've tried to verify this in the debugger but it has been difficult to do as
there are alot of things that are optimized out.
Now, how about we set the this in gdb to verify.

```console
(gdb) br heap-inl.h:206 if size_in_bytes == 129152
Breakpoint 15 at 0xaaaaab44ac6c: heap-inl.h:206. (3 locations)
(gdb) r
```
When it hits the above break point then set the following breakpoint:
```console
(gdb) br spaces.cc:758                                      
Breakpoint 17 at 0xaaaaab4d0c9c: file ../deps/v8/src/heap/spaces.cc, line 758.
(gdb) continue
Thread 1 "node" hit Breakpoint 17, v8::internal::MemoryChunk::Initialize (heap=0xaaaaacbbaab0, base=<optimized out>, 
    size=<optimized out>, area_start=798883840, area_end=799012992, executable=v8::internal::EXECUTABLE, 
    owner=0xaaaaacb9dbb0, reservation=...) at ../deps/v8/src/heap/spaces.cc:758
758	  return chunk;

(gdb) p chunk->code_object_registry_
$19 = (v8::internal::CodeObjectRegistry *) 0x0
```
So we can see that the `code_object_registry_` is null.

Lets see if we can set it in gdb:
```console
(gdb) call ('v8::internal::CodeObjectRegistry'*) malloc(sizeof('v8::internal::CodeObjectRegistry'()))
$37 = (v8::internal::CodeObjectRegistry *) 0xffffbe809050
(gdb) call (('v8::internal::CodeObjectRegistry'*)0xffffbe809050)
$38 = (v8::internal::CodeObjectRegistry *) 0xffffbe809050
(gdb) p $38->code_object_registry_newly_allocated_
$41 = std::set with 0 elements
(gdb) p $38->code_object_registry_newly_allocated_.size()
$42 = 0
(gdb) set chunk->code_object_registry_ = 0xffffbe809050
(gdb) p chunk->code_object_registry_->code_object_registry_newly_allocated_.size()
$53 = 0
```
So we can now see that it is possible to get the size of the set which was not
possible above. 
Now, will this work if we let the debugger continue:
```console
(gdb) continue
```
Nope, that did not work :( 
```
620  auto result = code_object_registry_newly_allocated_.insert(code);
(gdb) p this
$54 = (v8::internal::CodeObjectRegistry * const) 0xffffbe809050
(gdb) p code_object_registry_newly_allocated_.size()
$56 = 0
```
I'm not completly sure if the above should have worked or not as I'm seeing
a few messages about function being inlined. For example, if I try to call
```console
(gdb) p code_object_registry_newly_allocated_.insert(('v8::internal::Address')10)
Cannot evaluate function -- may be inlined
```

But applying this patch to node 12.16.1 it no longer crashes. Next step is to
add a test to V8 and get some more eyes on this change.
