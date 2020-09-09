### Heap
This document will try to describe the Heap in V8 and the related classes and
functions.

When a V8 Isolate is created it is usually done by something like this:
```c++
  isolate_ = v8::Isolate::New(create_params_);
```
The first thing that happens in v8::Isolate::New is that the Isolate is
allocated:
```c++
Isolate* Isolate::New(const Isolate::CreateParams& params) {
  Isolate* isolate = Allocate()
  ...
}
```
And allocate calls v8::internal::Isolate::New and casts it back to v8::Isolate:
```c++
Isolate* Isolate::Allocate() {
  return reinterpret_cast<Isolate*>(i::Isolate::New());
}
```

### IsolateAllocator
Each Isolate has an IsolateAllocator instance associated with it which is passes
into the constructor:
```c++
Isolate* Isolate::New(IsolateAllocationMode mode) {
  std::unique_ptr<IsolateAllocator> isolate_allocator =
      std::make_unique<IsolateAllocator>(mode);
  void* isolate_ptr = isolate_allocator->isolate_memory();
  Isolate* isolate = new (isolate_ptr) Isolate(std::move(isolate_allocator));
  ...
  return isolate;
}
```
Notice the above code is using placement new and specifying the memory address
where the new Isolate should be constructed (isolate_ptr).

The definition of IsolateAllocator can be found in `src/init/isolate-allocator.h`
and below are a few of the methods and fields that I found interesting:
```c++
class V8_EXPORT_PRIVATE IsolateAllocator final {
 public:
  void* isolate_memory() const { return isolate_memory_; }
  v8::PageAllocator* page_allocator() const { return page_allocator_; }

 private:
  Address InitReservation();
  void CommitPagesForIsolate(Address heap_reservation_address);

  // The allocated memory for Isolate instance.
  void* isolate_memory_ = nullptr;
  v8::PageAllocator* page_allocator_ = nullptr;
  std::unique_ptr<base::BoundedPageAllocator> page_allocator_instance_;
  VirtualMemory reservation_;
};
```

An Isolate has a `page_allocator()` method that returns the v8::PageAllocator
from IsolateAllocator:
```
v8::PageAllocator* Isolate::page_allocator() {
  return isolate_allocator_->page_allocator();
}
```
So, we are in `Isolate::New(IsolationAllocationMode)` and the next line is:
```c++
  std::unique_ptr<IsolateAllocator> isolate_allocator =
      std::make_unique<IsolateAllocator>(mode);
}
```

The default allocation mode will be `IsolateAllocationMode::kInV8Heap`:
```c++
  static Isolate* New(
      IsolateAllocationMode mode = IsolateAllocationMode::kDefault);
```
And the possible options for mode are:
```c++
  // Allocate Isolate in C++ heap using default new/delete operators.
  kInCppHeap,

  // Allocate Isolate in a committed region inside V8 heap reservation.
  kInV8Heap,

#ifdef V8_COMPRESS_POINTERS
  kDefault = kInV8Heap,
#else
  kDefault = kInCppHeap,
#endif
};
```

The constructor takes an AllocationMode as mentioned above.
```c++
IsolateAllocator::IsolateAllocator(IsolateAllocationMode mode) {
#if V8_TARGET_ARCH_64_BIT
  if (mode == IsolateAllocationMode::kInV8Heap) {
    Address heap_reservation_address = InitReservation();
    CommitPagesForIsolate(heap_reservation_address);
    return;
  }
#endif  // V8_TARGET_ARCH_64_BIT
```
Lets take a closer look at InitReservation (`src/init/isolate-alloctor.cc`).
```c++
Address IsolateAllocator::InitReservation() {
  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();
  ...
   VirtualMemory padded_reservation(platform_page_allocator,
                                    reservation_size * 2,
                                    reinterpret_cast<void*>(hint));

  ...
  reservation_ = std::move(reservation);
```
`GetPlatformPageAllocator()` can be found in `src/utils/allocation.cc`:
```c++
v8::PageAllocator* GetPlatformPageAllocator() {
  DCHECK_NOT_NULL(GetPageTableInitializer()->page_allocator());
  return GetPageTableInitializer()->page_allocator();
}
```
Now, GetPageTableInitializer is also defined in allocation.cc but is defined
using a macro:
```c++
DEFINE_LAZY_LEAKY_OBJECT_GETTER(PageAllocatorInitializer,
                                GetPageTableInitializer)
```
This macro can be found in `src/base/lazy-instance.h`:
```c++
#define DEFINE_LAZY_LEAKY_OBJECT_GETTER(T, FunctionName, ...) \
  T* FunctionName() {                                         \
    static ::v8::base::LeakyObject<T> object{__VA_ARGS__};    \
    return object.get();                                      \
  }
```
So the preprocess would expand the usage above to:
```c++
  PageAllocatorInitializer* GetPageTableInitializer() {
    static ::v8::base::LeakyObject<PageAllocatorInitializer> object{};
    return object.get();
  }
```
```c++
PageAllocatorInitializer() {
    page_allocator_ = V8::GetCurrentPlatform()->GetPageAllocator();
    if (page_allocator_ == nullptr) {
      static base::LeakyObject<base::PageAllocator> default_page_allocator;
      page_allocator_ = default_page_allocator.get();
    }
  }
```
Notice that this is calling V8::GetCurrentPlatform() and recall that a
platform is created and initialized before an Isolate can be created:
```c++
    platform_ = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform_.get());
    v8::V8::Initialize();
```
Looking again at PageAllocator's constructor we see that it retreives the
PageAllocator from the platform. How/when is this PageAllocator created?

It is created in DefaultPlatform's constructor(src/libplatform/default-platform.cc)
which is called by `NewDefaultPlatform`:
```c++
```
DefaultPlatform::DefaultPlatform(
    int thread_pool_size, IdleTaskSupport idle_task_support,
    std::unique_ptr<v8::TracingController> tracing_controller)
    : thread_pool_size_(GetActualThreadPoolSize(thread_pool_size)),
      idle_task_support_(idle_task_support),
      tracing_controller_(std::move(tracing_controller)),
      page_allocator_(std::make_unique<v8::base::PageAllocator>()) {
  ...
}
```
PageAllocators constructor can be found in `src/base/page-allocator.cc`:
```c++
PageAllocator::PageAllocator()
    : allocate_page_size_(base::OS::AllocatePageSize()),
      commit_page_size_(base::OS::CommitPageSize()) {}
```
The allocate_page_size_ is what would be retrieved by calling (on linux):
```c++
size_t OS::AllocatePageSize() {
  return static_cast<size_t>(sysconf(_SC_PAGESIZE));
}
```
This is the number of fixed block of bytes that is the unit that mmap works with.
(_SC is a prefix for System Config parameters for the sysconf function).

commit_page_size_ is implemented like this:
```c++
size_t OS::CommitPageSize() {
  static size_t page_size = getpagesize();
  return page_size;
}
```
Accourding to the man page of getpagesize, portable solutions should use
sysconf(_SC_PAGESIZE) instead (which is what AllocatePageSize does). I'm a little
confused about this. TODO: take a closer look at this.

The internal PageAllocator extends the public one that is declared in
`include/v8-platform.h`.
```c++
class V8_BASE_EXPORT PageAllocator
    : public NON_EXPORTED_BASE(::v8::PageAllocator) {
 private:
   const size_t allocate_page_size_;
   const size_t commit_page_size_;
```
Om my machine these are:
```console
$ ./test/heap_test --gtest_filter=HeapTest.PageAllocator
AllocatePageSize: 4096
CommitPageSize: 4096
```



Notice that a VirtualMemory instance is created and the arguments passed. This
is actually part of a retry loop but I've excluded that above.
VirtualMemory's constructor can be found in `src/utils/allocation.cc`:
```c++
VirtualMemory::VirtualMemory(v8::PageAllocator* page_allocator, size_t size,
                             void* hint, size_t alignment)
    : page_allocator_(page_allocator) {
  size_t page_size = page_allocator_->AllocatePageSize();
  alignment = RoundUp(alignment, page_size);
  Address address = reinterpret_cast<Address>(
      AllocatePages(page_allocator_, hint, RoundUp(size, page_size), alignment,
                    PageAllocator::kNoAccess));
  ...
}
```
And `AllocatePages` can be found in `src/base/page-allocator.cc`:
```c++
void* PageAllocator::AllocatePages(void* hint, size_t size, size_t alignment,
                                   PageAllocator::Permission access) {
  return base::OS::Allocate(hint, size, alignment,
                            static_cast<base::OS::MemoryPermission>(access));
}
```
Which will lead us to `base::OS::Allocate` which is declared in `src/base/platform/platform.h`
and the concrete version on my system (linux) can be found in platform-posix.cc
which can be found in `src/base/platform/platform-posix.cc`:
```c++
void* Allocate(void* hint, size_t size, OS::MemoryPermission access, PageType page_type) {
  int prot = GetProtectionFromMemoryPermission(access);
  int flags = GetFlagsForMemoryPermission(access, page_type);
  void* result = mmap(hint, size, prot, flags, kMmapFd, kMmapFdOffset);
  if (result == MAP_FAILED) return nullptr;
  return result;
}
```
Notice the call to `mmap` which is a system call. 
I've written a small example of using
[mmap](https://github.com/danbev/learning-linux-kernel/blob/master/mmap.c) and
[notes](https://github.com/danbev/learning-linux-kernel#mmap-sysmmanh) which
might help to look at mmap in isolation and better understand what `Allocate`
is doing.

`prot` is the memory protection wanted for this mapping and is retrived by
calling `GetProtectionFromMemoryPermission`:
```c++
int GetProtectionFromMemoryPermission(OS::MemoryPermission access) {
  switch (access) {
    case OS::MemoryPermission::kNoAccess:
      return PROT_NONE;
    case OS::MemoryPermission::kRead:
      return PROT_READ;
    case OS::MemoryPermission::kReadWrite:
      return PROT_READ | PROT_WRITE;
    case OS::MemoryPermission::kReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    case OS::MemoryPermission::kReadExecute:
      return PROT_READ | PROT_EXEC;
  }
  UNREACHABLE();
}
```
In our case this will return `PROT_NONE` which means no access. I believe this
is so that the memory mapping can be further subdivied and different permissions
can be given later to a region using mprotect.

Next we get the `flags` to be used in the mmap call:
```c++
enum class PageType { kShared, kPrivate };

int GetFlagsForMemoryPermission(OS::MemoryPermission access,
                                PageType page_type) {
  int flags = MAP_ANONYMOUS;
  flags |= (page_type == PageType::kShared) ? MAP_SHARED : MAP_PRIVATE;
  ...
  return flags;
```
PageType in our case is `kPrivate` so this will add the `MAP_PRIVATE` flag to
the flags, in addition to `MAP_ANONYMOUS`.

So the following values are used in the call to mmap:
```c++
  void* result = mmap(hint, size, prot, flags, kMmapFd, kMmapFdOffset);
```
```console
(lldb) expr hint
(void *) $20 = 0x00001b9100000000
(lldb) expr request_size/1073741824
(unsigned long) $24 = 8
(lldb) expr kMmapFd
(const int) $17 = -1
```
`kMmapFdOffset` seems to be optimized out but we can see that it is `0x0` and
passed in register `r9d` (as it is passed as the sixth argument):
```console
(lldb) disassemble
->  0x7ffff39ba4ca <+53>:  mov    ecx, dword ptr [rbp - 0xc]
    0x7ffff39ba4cd <+56>:  mov    edx, dword ptr [rbp - 0x10]
    0x7ffff39ba4d0 <+59>:  mov    rsi, qword ptr [rbp - 0x20]
    0x7ffff39ba4d4 <+63>:  mov    rax, qword ptr [rbp - 0x18]
    0x7ffff39ba4d8 <+67>:  mov    r9d, 0x0
    0x7ffff39ba4de <+73>:  mov    r8d, 0xffffffff
    0x7ffff39ba4e4 <+79>:  mov    rdi, rax
    0x7ffff39ba4e7 <+82>:  call   0x7ffff399f000            ; symbol stub for: mmap64
```
So if I've understood this correctly this is requesting a mapping of 8GB, with
a memory protection of `PROT_NONE` which can be used to protect that mapping space
and also allow it to be subdivied by calling mmap later with specific addresses
in this mapping. These could then be called with other memory protections depending
on the type of contents that should be stored there, for example for code objects
it would be executable.

So that will return the address to the new mapping, this will return us to:
```c++
void* OS::Allocate(void* hint, size_t size, size_t alignment,
                   MemoryPermission access) {
  ...
  void* result = base::Allocate(hint, request_size, access, PageType::kPrivate);
  if (result == nullptr) return nullptr;

  uint8_t* base = static_cast<uint8_t*>(result);
  uint8_t* aligned_base = reinterpret_cast<uint8_t*>(
      RoundUp(reinterpret_cast<uintptr_t>(base), alignment));
  if (aligned_base != base) {
    DCHECK_LT(base, aligned_base);
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    CHECK(Free(base, prefix_size));
    request_size -= prefix_size;
  }
  // Unmap memory allocated after the potentially unaligned end.
  if (size != request_size) {
    DCHECK_LT(size, request_size);
    size_t suffix_size = request_size - size;
    CHECK(Free(aligned_base + size, suffix_size));
    request_size -= suffix_size;
  }

  DCHECK_EQ(size, request_size);
  return static_cast<void*>(aligned_base);
}
```
TODO: figure out what why this unmapping is done.
This return will return to `AllocatePages` which will just return to
VirtualMemory's constructor where the address will be casted to an Address.
```c++
 Address address = reinterpret_cast<Address>(
      AllocatePages(page_allocator_, hint, RoundUp(size, page_size), alignment,
                    PageAllocator::kNoAccess));
  if (address != kNullAddress) {
    DCHECK(IsAligned(address, alignment));
    region_ = base::AddressRegion(address, size);
  }
}
```
`AddressRegion` can be found in `src/base/address-region.h` and is a helper
class to manage region with a start `address` and a `size`.

After this call the VirtualMemory instance will look like this:
```console
(lldb) expr *this
(v8::internal::VirtualMemory) $35 = {
  page_allocator_ = 0x00000000004936b0
  region_ = (address_ = 30309584207872, size_ = 8589934592)
}
```

After `InitReservation` returns, we will be in `IsolateAllocator::IsolateAllocator`:
```c++
  if (mode == IsolateAllocationMode::kInV8Heap) {
    Address heap_reservation_address = InitReservation();
    CommitPagesForIsolate(heap_reservation_address);
    return;
  }
```

In CommitPagesForIsolate we have :
```c++
void IsolateAllocator::CommitPagesForIsolate(Address heap_reservation_address) {
  v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();

  Address isolate_root = heap_reservation_address + kIsolateRootBiasPageSize;


  size_t page_size = RoundUp(size_t{1} << kPageSizeBits,
                             platform_page_allocator->AllocatePageSize());

  page_allocator_instance_ = std::make_unique<base::BoundedPageAllocator>(
      platform_page_allocator, isolate_root, kPtrComprHeapReservationSize,
      page_size);
  page_allocator_ = page_allocator_instance_.get();
```
```
(lldb) expr isolate_root
(v8::internal::Address) $54 = 30309584207872
```
And notice that the `VirtualMemory` that we allocated above has the same memory
address. 

So we are creating a new instance of BoundedPageAllocator and setting this to
page_allocator_instance_ (smart pointer) and also setting the pointer
page_allocator_.

```c++
  Address isolate_address = isolate_root - Isolate::isolate_root_bias();
  Address isolate_end = isolate_address + sizeof(Isolate);
  {
    Address reserved_region_address = isolate_root;
    size_t reserved_region_size =
        RoundUp(isolate_end, page_size) - reserved_region_address;

    CHECK(page_allocator_instance_->AllocatePagesAt(
        reserved_region_address, reserved_region_size,
        PageAllocator::Permission::kNoAccess));
  }
```
`AllocatePagesAt` will call `BoundedPageAllocator::AllocatePagesAt`:
```c++
bool BoundedPageAllocator::AllocatePagesAt(Address address, size_t size,
                                           PageAllocator::Permission access) {
  if (!region_allocator_.AllocateRegionAt(address, size)) {
    return false;
  }
  CHECK(page_allocator_->SetPermissions(reinterpret_cast<void*>(address), size,
                                        access));
  return true;
}
```
`SetPermissions` can be found in `platform-posix.cc` and looks like this:
```c++
bool OS::SetPermissions(void* address, size_t size, MemoryPermission access) {
  ...
  int prot = GetProtectionFromMemoryPermission(access);
  int ret = mprotect(address, size, prot);
```
`mprotect` is a system call that can change a memory mappings protection.


Later in `Isolate::Init` the heap will be set up:
```c++
heap_.SetUp();
```
```c++
  memory_allocator_.reset(
      new MemoryAllocator(isolate_, MaxReserved(), code_range_size_));
```
In MemoryAllocator's constructor we have a the following function call:
```c++
MemoryAllocator::MemoryAllocator(Isolate* isolate, size_t capacity,
                                 size_t code_range_size)
    : isolate_(isolate),
      data_page_allocator_(isolate->page_allocator()),
      code_page_allocator_(nullptr),
      capacity_(RoundUp(capacity, Page::kPageSize)),
      size_(0),
      size_executable_(0),
      lowest_ever_allocated_(static_cast<Address>(-1ll)),
      highest_ever_allocated_(kNullAddress),
      unmapper_(isolate->heap(), this) {
  InitializeCodePageAllocator(data_page_allocator_, code_range_size);
```
Notice that the `data_page_allocator_` is set from `isolate->page_allocator`.
Now, a MemoryAllocator will use the data_page_allocator_ which will be either
PageAllocator or a BoundedPageAllocator if pointer compression is enabled.
A MemoryAllocator instance will also have a code_page_allocator.

When an object is to be created it is done so using one of the Space implementations
that are available in the Heap class. These spaces are create by 
`Heap::SetupSpaces`, which is called from `Isolate::Init`:
```c++
void Heap::SetUpSpaces() {
  space_[NEW_SPACE] = new_space_ =
      new NewSpace(this, memory_allocator_->data_page_allocator(),
                   initial_semispace_size_, max_semi_space_size_);
  space_[OLD_SPACE] = old_space_ = new OldSpace(this);
  space_[CODE_SPACE] = code_space_ = new CodeSpace(this);
  space_[MAP_SPACE] = map_space_ = new MapSpace(this);
  space_[LO_SPACE] = lo_space_ = new OldLargeObjectSpace(this);
  space_[NEW_LO_SPACE] = new_lo_space_ =
      new NewLargeObjectSpace(this, new_space_->Capacity());
  space_[CODE_LO_SPACE] = code_lo_space_ = new CodeLargeObjectSpace(this);
  ...
```

### Spaces
A space refers to parts of the heap that are handled in different ways with
regards to garbage collection:

```console
+----------------------- -----------------------------------------------------------+
|   Young Generation                  Old Generation          Large Object space    |
|  +-------------+--------------+  +-----------+-------------+ +------------------+ |
|  |        NEW_SPACE           |  | MAP_SPACE | OLD_SPACE   | | LO_SPACE         | |
|  +-------------+--------------+  +-----------+-------------+ +------------------+ |
|  |to_SemiSpace |from_SemiSpace|                                                   |
|  +-------------+--------------+                                                   |
|  +-------------+                 +-----------+               +------------------+ |
|  | NEW_LO_SPACE|                 | CODE_SPACE|               | CODE_LO_SPACE    | |
|  +-------------+                 +-----------+               +------------------+ |
|                                                                                   |
|   Read-only                                                                       |
|  +--------------+                                                                 |
|  | RO_SPACE     |                                                                 |
|  +--------------+                                                                 |
+-----------------------------------------------------------------------------------+
```
The `Map space` contains only map objects and they are non-movable.

The base class for all spaces is BaseSpace.


#### BaseSpace
BaseSpace is a super class of all Space classes in V8 which are spaces that are
not read-only (sealed).

What are the spaces that are defined?  
* NewSpace
* PagedSpace
* SemiSpace
* LargeObjectSpace
* others ?

BaseSpace extends Malloced:
```c++
class V8_EXPORT_PRIVATE BaseSpace : public Malloced {
```
`Malloced` which can be found in in `src/utils/allocation.h`:
```c++
// Superclass for classes managed with new & delete.
class V8_EXPORT_PRIVATE Malloced {
 public:
  static void* operator new(size_t size);
  static void operator delete(void* p);
};
```
And we can find the implemention in `src/utils/allocation.cc`:
```c++
void* Malloced::operator new(size_t size) {
  void* result = AllocWithRetry(size);
  if (result == nullptr) {
    V8::FatalProcessOutOfMemory(nullptr, "Malloced operator new");
  }
  return result;
}

void* AllocWithRetry(size_t size) {
  void* result = nullptr;
  for (int i = 0; i < kAllocationTries; ++i) {
    result = malloc(size);
    if (result != nullptr) break;
    if (!OnCriticalMemoryPressure(size)) break;
  }
  return result;
}

void Malloced::operator delete(void* p) { free(p); }
```
So for all classes of type BaseSpace when those are created using the `new`
operator, the `Malloced::operator new` version will be called. This is like
other types of operator overloading in c++. Here is a standalone example
[new-override.cc](https://github.com/danbev/learning-cpp/blob/master/src/fundamentals/new-override.cc). 


The spaces are create by `Heap::SetupSpaces`, which is called from
`Isolate::Init`:
```c++
void Heap::SetUpSpaces() {
  space_[NEW_SPACE] = new_space_ =
      new NewSpace(this, memory_allocator_->data_page_allocator(),
                   initial_semispace_size_, max_semi_space_size_);
  space_[OLD_SPACE] = old_space_ = new OldSpace(this);
  space_[CODE_SPACE] = code_space_ = new CodeSpace(this);
  space_[MAP_SPACE] = map_space_ = new MapSpace(this);
  space_[LO_SPACE] = lo_space_ = new OldLargeObjectSpace(this);
  space_[NEW_LO_SPACE] = new_lo_space_ =
      new NewLargeObjectSpace(this, new_space_->Capacity());
  space_[CODE_LO_SPACE] = code_lo_space_ = new CodeLargeObjectSpace(this);
  ...
```
Notice that this use the `new` operator and that they all extend Malloced so
these instance will be allocated by Malloced.

An instance of BaseSpace has a Heap associated with it, an AllocationSpace, and
committed and uncommited counters.

`AllocationSpace` is a enum found in `src/common/globals.h`:
```c++
enum AllocationSpace {
  RO_SPACE,      // Immortal, immovable and immutable objects,
  NEW_SPACE,     // Young generation semispaces for regular objects collected with
                 // Scavenger.
  OLD_SPACE,     // Old generation regular object space.
  CODE_SPACE,    // Old generation code object space, marked executable.
  MAP_SPACE,     // Old generation map object space, non-movable.
  LO_SPACE,      // Old generation large object space.
  CODE_LO_SPACE, // Old generation large code object space.
  NEW_LO_SPACE,  // Young generation large object space.

  FIRST_SPACE = RO_SPACE,
  LAST_SPACE = NEW_LO_SPACE,
  FIRST_MUTABLE_SPACE = NEW_SPACE,
  LAST_MUTABLE_SPACE = NEW_LO_SPACE,
  FIRST_GROWABLE_PAGED_SPACE = OLD_SPACE,
  LAST_GROWABLE_PAGED_SPACE = MAP_SPACE
};
```
There is an abstract superclass that extends BaseSpace named `Space`.

### NewSpace
A NewSpace contains two SemiSpaces, one name `to_space_` and the other
`from_space`.

Class NewSpace extends SpaceWithLinearArea (can be found in `src/heap/spaces.h`).

When a NewSpace instance is created it is passed a page allocator:
```c++
  space_[NEW_SPACE] = new_space_ =
      new NewSpace(this, memory_allocator_->data_page_allocator(),
                   initial_semispace_size_, max_semi_space_size_);
```


### BasicMemoryChunk
A BasicMemoryChunk has a size, a start address, and an end address. It also has
flags that describe the memory chunk, like if it is executable, pinned (cannot be
moved etc). 

A BasicMemoryChunk is created by specifying the size and the start and end
address:
```c++
BasicMemoryChunk::BasicMemoryChunk(size_t size, Address area_start,
                                   Address area_end) {
  size_ = size;
  area_start_ = area_start;
  area_end_ = area_end;
}
```
So there is nothing else to creating an instance of a BasicMemoryChunk.

But note that BasicMemoryChunk also has a Heap member which was not specified
in the above constructor.
There is a static function named `Initialize` that does take a pointer to a
Heap though:
```c++
 static BasicMemoryChunk* Initialize(Heap* heap, Address base, size_t size,
                                      Address area_start, Address area_end,
                                      BaseSpace* owner,
                                      VirtualMemory reservation);
```


### MemoryChunk
Represents a memory area that is owned by a Space.

Is a struct defined in `src/heap/memory-chunk.h` (there is another MemoryChunk
struct in the namespace `v8::heap_internal` just in case you find it instead).
```c++
class MemoryChunk : public BasicMemoryChunk {

};
```

### Page
Is a class that extends MemoryChunk and has a size of 256K.

The `Page` class can be found in `src/heap/spaces.h`
```c++
class Page : public MemoryChunk {
}
```
### Heap
A heap instance has the following members:
```c++
  NewSpace* new_space_ = nullptr;
  OldSpace* old_space_ = nullptr;
  CodeSpace* code_space_ = nullptr;
  MapSpace* map_space_ = nullptr;
  OldLargeObjectSpace* lo_space_ = nullptr;
  CodeLargeObjectSpace* code_lo_space_ = nullptr;
  NewLargeObjectSpace* new_lo_space_ = nullptr;
  ReadOnlySpace* read_only_space_ = nullptr;
```

### AllocateRaw
This is a method declared in `src/heap/heap.h` and will allocate an uninitialized
object:
```c++
 V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime,
      AllocationAlignment alignment = kWordAligned);
```
So we have the size of the object in bytes. And the other types are described
in separate sections below.

The implemementation for this method can be found in `src/heap/heap-inl.h`.
```c++
  ...
  HeapObject object;
  AllocationResult allocation;
```
This is then followed by checks or the AllocationType 
```c++
  if (AllocationType::kYoung == type) {
    ...
    } else if (AllocationType::kCode == type) {
      if (size_in_bytes <= code_space()->AreaSize() && !large_object) {
        allocation = code_space_->AllocateRawUnaligned(size_in_bytes);
      } else {
        allocation = code_lo_space_->AllocateRaw(size_in_bytes);
      }

  }
```
Lets take a closer look at `code_lo_space` which is a field in the Heap class.
```c++
 private:
  CodeLargeObjectSpace* code_lo_space_ = nullptr;
```
And it's `AllocateRaw` function looks like this:
```c++
AllocationResult CodeLargeObjectSpace::AllocateRaw(int object_size) {
  return OldLargeObjectSpace::AllocateRaw(object_size, EXECUTABLE);
}
```
OldLargeObjectSpace's AllocateRaw can be found in `src/heap/large-spaces.cc`.

There is an example of AllocateRaw in [heap_test.cc](./test/heap_test.cc).


### AllocationType
```c++
enum class AllocationType : uint8_t {
  kYoung,    // Regular object allocated in NEW_SPACE or NEW_LO_SPACE
  kOld,      // Regular object allocated in OLD_SPACE or LO_SPACE
  kCode,     // Code object allocated in CODE_SPACE or CODE_LO_SPACE
  kMap,      // Map object allocated in MAP_SPACE
  kReadOnly  // Object allocated in RO_SPACE
};
```

### AllocationOrigin
```c++
enum class AllocationOrigin {
  kGeneratedCode = 0,
  kRuntime = 1,
  kGC = 2,
  kFirstAllocationOrigin = kGeneratedCode,
  kLastAllocationOrigin = kGC,
  kNumberOfAllocationOrigins = kLastAllocationOrigin + 1
};
```

### AllocationAlignement
```c++
enum AllocationAlignment {
  kWordAligned,
  kDoubleAligned,
  kDoubleUnaligned,
  kCodeAligned
};
```

### MemoryAllocator
A memory allocator is what requires chunks of memory from the OS.
Is defined in `src/heap/memory-allocator.h` and is created in Heap::SetUpg

I want to know/understand where memory is allocated.

When a MemoryAllocator is created it is passes an internal isolate, a capacity,
and a code_range_size.
Each instance has a PageAllocator for non-executable pages which is called
data_page_allocator, and another that is for executable pages called
code_page_allocator.

The constructor can be found in `src/heap/memory-allocator.cc`:
```c++
MemoryAllocator::MemoryAllocator(Isolate* isolate, size_t capacity,
                                 size_t code_range_size)
    : isolate_(isolate),
      data_page_allocator_(isolate->page_allocator()),
      code_page_allocator_(nullptr),
      capacity_(RoundUp(capacity, Page::kPageSize)),
      size_(0),
      size_executable_(0),
      lowest_ever_allocated_(static_cast<Address>(-1ll)),
      highest_ever_allocated_(kNullAddress),
      unmapper_(isolate->heap(), this) {
  InitializeCodePageAllocator(data_page_allocator_, code_range_size);
}
```
We can see that the data_page_allocator_ is set to the isolates page_allocator.

Notice that the above constructor calls `InitializeCodePageAllocator` which
will set the `code_page_allocator`:
```c++
void MemoryAllocator::InitializeCodePageAllocator(
    v8::PageAllocator* page_allocator, size_t requested) {

  code_page_allocator_ = page_allocator;
  ...
  code_page_allocator_instance_ = std::make_unique<base::BoundedPageAllocator>(
      page_allocator, aligned_base, size,
      static_cast<size_t>(MemoryChunk::kAlignment));
  code_page_allocator_ = code_page_allocator_instance_.get();
```

### PageAlloctor
Is responsible for allocating pages from the underlying OS. This class is a
abstract class (has virtual members that subclasses must implement).
Can be found in `src/base/page-alloctor.h` and notice that it is in the
`v8::base` namespace and it extends `v8::PageAllocator` which can be found
in `include/v8-platform.h`.


BoundedPageAllocator is 

A Platform as a PageAllocator as a member. For the DefaultPlatform an instance
of v8::base::PageAllocator is created in the constructor.

src/base/page-allocator.h

+-----------------+
|  DefaltPlatform |
+-----------------+         +-----------------------+
| GetPageAllocator|-------> |v8::base::PageAllocator|
+-----------------+         +-----------------------+      +------------------+
			    |AllocatePages          |----->|base::OS::Allocate|
			    +-----------------------+      +------------------+



Now, lets see what goes on when we create a new Object, say of type String.
```console
(lldb) bt
* thread #1, name = 'string_test', stop reason = step in
  * frame #0: 0x00007ffff633d55a libv8.so`v8::internal::FactoryBase<v8::internal::Factory>::AllocateRawWithImmortalMap(this=0x00001d1000000000, size=20, allocation=kYoung, map=Map @ 0x00007fffffffcab8, alignment=kWordAligned) at factory-base.cc:817:3
    frame #1: 0x00007ffff633c2f1 libv8.so`v8::internal::FactoryBase<v8::internal::Factory>::NewRawOneByteString(this=0x00001d1000000000, length=6, allocation=kYoung) at factory-base.cc:533:14
    frame #2: 0x00007ffff634771e libv8.so`v8::internal::Factory::NewStringFromOneByte(this=0x00001d1000000000, string=0x00007fffffffcbd0, allocation=kYoung) at factory.cc:607:3
    frame #3: 0x00007ffff5fc1c54 libv8.so`v8::(anonymous namespace)::NewString(factory=0x00001d1000000000, type=kNormal, string=(start_ = "bajja", length_ = 6)) at api.cc:6381:46
    frame #4: 0x00007ffff5fc2284 libv8.so`v8::String::NewFromOneByte(isolate=0x00001d1000000000, data="bajja", type=kNormal, length=6) at api.cc:6439:3
    frame #5: 0x0000000000413fa4 string_test`StringTest_create_Test::TestBody(this=0x000000000048dff0) at string_test.cc:18:8
```
If we look at the call `AllocateRawWithImmortalMap` we find:
```c++
  HeapObject result = AllocateRaw(size, allocation, alignment);
```
AllocateRaw will end up in FactoryBase<Impl>::AllocateRaw:
```c++
HeapObject FactoryBase<Impl>::AllocateRaw(int size, AllocationType allocation,
                                          AllocationAlignment alignment) {
  return impl()->AllocateRaw(size, allocation, alignment);
}
```
Which will end up in `Factory::AllocateRaw`:
```c++
HeapObject Factory::AllocateRaw(int size, AllocationType allocation,
                                AllocationAlignment alignment) {
  return isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(
      size, allocation, AllocationOrigin::kRuntime, alignment);
  
}
```

```c++
   case kRetryOrFail:
-> 297 	      return AllocateRawWithRetryOrFailSlowPath(size, allocation, origin,
   298 	                                                alignment);
```
```c++
 5165	  AllocationResult alloc;
   5166	  HeapObject result =
-> 5167	      AllocateRawWithLightRetrySlowPath(size, allocation, origin, alignment);
```

```c++
-> 5143	  HeapObject result;
   5144	  AllocationResult alloc = AllocateRaw(size, allocation, origin, alignment);
```

```c++
-> 210 	        allocation = new_space_->AllocateRaw(size_in_bytes, alignment, origin);
```
```c++
-> 95  	    result = AllocateFastUnaligned(size_in_bytes, origin);
```

```c++
AllocationResult NewSpace::AllocateFastUnaligned(int size_in_bytes,
                                                 AllocationOrigin origin) {
  Address top = allocation_info_.top();

  HeapObject obj = HeapObject::FromAddress(top);
  allocation_info_.set_top(top + size_in_bytes);
```
Where `allocation_info_` is of type `LinearAllocationArea`. Notice that a
HeapObject is created using the value of top. This is a pointer to the top
of the address of mapped memory that this space manages. And after this top
is increased by the size of the object. 

`AllocateRawWithImmortalMap`:
```c++
   818 	  HeapObject result = AllocateRaw(size, allocation, alignment);
-> 819 	  result.set_map_after_allocation(map, SKIP_WRITE_BARRIER);
   820 	  return result;
```



Now, every Space will have a region of mapped memory that it can manage. For
example the new space would have it and it would manage it with a pointer
to the top of allocated memory. To create an object it could return the top address
and increase the top with the size of the object in question. Functions that
set values on the object would have to write to the field specified by the
layout of the object. TODO: verify if that is actually what is happening.


