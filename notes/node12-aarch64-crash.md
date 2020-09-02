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
`MemoryChunk::FromHeapObject(object)->GetCodeObjectRegistry()->RegisterNewlyAllocatedCodeObject(object.address())`. 

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
So we now understant the cause of the segmentation fault. But we still need to
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
I've attempted to reproduce this in
https://github.com/danbev/learning-v8/blob/f3aae7a4e073f5a3183199aab89b0a8abe09680f/test/heap_test.cc#L48

In newer versions of Node, which contain a newer version of V8, the above
if statement also includes an additional check:
```c++
if (allocation.To(&object)) {
    if (AllocationType::kCode == type && !V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {       
      // Unprotect the memory chunk of the object if it was not unprotected        
      // already.
      UnprotectAndRegisterMemoryChunk(object);
      ZapCodeObject(object.address(), size_in_bytes);
      if (!large_object) {
        MemoryChunk::FromHeapObject(object)
            ->GetCodeObjectRegistry()
            ->RegisterNewlyAllocatedCodeObject(object.address());
      }
    }
    OnAllocationEvent(object, size_in_bytes);
  }
```
This flag was added in Commit 8e8fe4750522e6368a3c055de5b33a9089e64b5b
("heap] Introduce third-party heap interface").

Work in progress...
