### Backingstore issue
This document contains notes for an issue that was discovered in Node.js.

Node.js issue: https://github.com/nodejs/node/issues/35669

The following address sanitizer error is generated then this
[test](https://github.com/nodejs/node/blob/11f1ad939f902ddbeb406350a6162359e04c13ec/test/cctest/test_environment.cc#L349):
```console
$ out/Release/cctest --gtest_filter=EnvironmentTest.BufferWithFreeCallbackIsDetached
Running main() from ../test/cctest/gtest/gtest_main.cc
Note: Google Test filter = EnvironmentTest.BufferWithFreeCallbackIsDetached
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from EnvironmentTest
[ RUN      ] EnvironmentTest.BufferWithFreeCallbackIsDetached
6
=================================================================
==2773765==ERROR: AddressSanitizer: new-delete-type-mismatch on 0x60400001af50 in thread T0:
  object passed to delete has wrong type:
  size of the allocated type:   48 bytes;
  size of the deallocated type: 1 bytes.
    #0 0x7f0a3a306175 in operator delete(void*, unsigned long) (/lib64/libasan.so.5+0x111175)
    #1 0x38328c2 in v8::internal::JSArrayBuffer::Detach(bool) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x38328c2)
    #2 0x29bf88a in v8::ArrayBuffer::Detach() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x29bf88a)
    #3 0x151adae in node::Buffer::(anonymous namespace)::CallbackInfo::CleanupHook(void*) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x151adae)
    #4 0x144a5ff in node::Environment::RunCleanup() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x144a5ff)
    #5 0x131a6f7 in node::FreeEnvironment(node::Environment*) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x131a6f7)
    #6 0x11d7730 in EnvironmentTest_BufferWithFreeCallbackIsDetached_Test::TestBody() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x11d7730)
    #7 0x1114cfd in testing::Test::Run() [clone .part.0] (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x1114cfd)
    #8 0x1115af0 in testing::TestInfo::Run() [clone .part.0] (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x1115af0)
    #9 0x11162c9 in testing::TestSuite::Run() [clone .part.0] (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x11162c9)
    #10 0x1137831 in testing::internal::UnitTestImpl::RunAllTests() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x1137831)
    #11 0x11383bf in testing::UnitTest::Run() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x11383bf)
    #12 0xa4d5b3 in main (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0xa4d5b3)
    #13 0x7f0a39ccf1a2 in __libc_start_main (/lib64/libc.so.6+0x271a2)
    #14 0xa7f09d in _start (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0xa7f09d)

0x60400001af50 is located 0 bytes inside of 48-byte region [0x60400001af50,0x60400001af80)
allocated by thread T0 here:
    #0 0x7f0a3a304a97 in operator new(unsigned long) (/lib64/libasan.so.5+0x10fa97)
    #1 0x34cb1a8 in v8::internal::BackingStore::WrapAllocation(void*, unsigned long, void (*)(void*, unsigned long, void*), void*, v8::internal::SharedFlag) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x34cb1a8)
    #2 0x29c18d5 in v8::ArrayBuffer::NewBackingStore(void*, unsigned long, void (*)(void*, unsigned long, void*), void*) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x29c18d5)
    #3 0x1547193 in node::Buffer::New(node::Environment*, char*, unsigned long, void (*)(char*, void*), void*) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x1547193)
    #4 0x1548e95 in node::Buffer::New(v8::Isolate*, char*, unsigned long, void (*)(char*, void*), void*) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x1548e95)
    #5 0x11d7536 in EnvironmentTest_BufferWithFreeCallbackIsDetached_Test::TestBody() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x11d7536)
    #6 0x1114cfd in testing::Test::Run() [clone .part.0] (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x1114cfd)
    #7 0x1115af0 in testing::TestInfo::Run() [clone .part.0] (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x1115af0)
    #8 0x11162c9 in testing::TestSuite::Run() [clone .part.0] (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x11162c9)
    #9 0x1137831 in testing::internal::UnitTestImpl::RunAllTests() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x1137831)
    #10 0x11383bf in testing::UnitTest::Run() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x11383bf)
    #11 0xa4d5b3 in main (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0xa4d5b3)
    #12 0x7f0a39ccf1a2 in __libc_start_main (/lib64/libc.so.6+0x271a2)

SUMMARY: AddressSanitizer: new-delete-type-mismatch (/lib64/libasan.so.5+0x111175) in operator delete(void*, unsigned long)
==2773765==HINT: if you don't care about these errors you may set ASAN_OPTIONS=new_delete_type_mismatch=0
==2773765==ABORTING

(lldb) AddressSanitizer report breakpoint hit. Use 'thread info -s' to get extended information about the report.
Process 3712019 stopped
* thread #1, name = 'cctest', stop reason = Deallocation size different from allocation size

```
Now this is somewhat hard to follow with the Node testing fixture mixed in but
There is a standalone example in [backingstore_test.cc](../test/backingstore_test.cc).
We will be focusing on the following parts of that example:
```c++
  Local<ArrayBuffer> ab;
  {
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        data, sizeof(data), BackingStoreDeleter, &deleter_called);
    ab = ArrayBuffer::New(isolate_, std::move(bs));
    std::cout << "ArrayBuffer created\n";
  }
  ab->Detach();
```
If we take a look at v8::ArrayBuffer::NewBackingStore in src/api/api.cc we have
the following:
```c++
std::unique_ptr<v8::BackingStore> v8::ArrayBuffer::NewBackingStore(
    void* data, size_t byte_length, v8::BackingStore::DeleterCallback deleter,
    void* deleter_data) {
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::WrapAllocation(data, byte_length, deleter, deleter_data,
                                      i::SharedFlag::kNotShared);
  return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
}
```
Notice that the `unique_ptr` is of type v8::internal::BackingStoreBase, and
WrapAllocation returns `std::unique_ptr<v8::internal::BackingStore>`:
```c++
std::unique_ptr<BackingStore> BackingStore::WrapAllocation(
    void* allocation_base, size_t allocation_length,
    v8::BackingStore::DeleterCallback deleter, void* deleter_data,
    SharedFlag shared) {
  bool is_empty_deleter = (deleter == v8::BackingStore::EmptyDeleter);
  auto result = new BackingStore(allocation_base,    // start
                                 allocation_length,  // length
                                 allocation_length,  // capacity
                                 shared,             // shared
                                 false,              // is_wasm_memory
                                 true,               // free_on_destruct
                                 false,              // has_guard_regions
                                 true,               // custom_deleter
                                 is_empty_deleter);  // empty_deleter
  result->type_specific_data_.deleter = {deleter, deleter_data};
  TRACE_BS("BS:wrap   bs=%p mem=%p (length=%zu)\n", result,
           result->buffer_start(), result->byte_length());
  return std::unique_ptr<BackingStore>(result);
}
```
WrapAllocation creates a new `v8::internalBackingStore` which is then returned
in a unique_ptr:
```console
(lldb) expr backing_store
(std::unique_ptr<v8::internal::BackingStoreBase, std::default_delete<v8::internal::BackingStoreBase> >) $1 = 0x51fe70 {
  pointer = 0x000000000051fe70
}
```
This is then returned by `ArrayBuffer::NewBackingStore` with a cast to
`v8::BackingStore`:
```c++
return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
```
Next, the returned `v8::BackingStore` will be passed into ArrayBuffer::New:
```c++
  Local<ArrayBuffer> ab;
  {
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        data, data_len, BackingStoreDeleter, nullptr);
    ab = ArrayBuffer::New(isolate_, std::move(bs));
```
The backingstore is then moved into ArrayBuffer::New (in src/api/api.cc):
```c++
  std::shared_ptr<i::BackingStore> i_backing_store(ToInternal(std::move(backing_store)));
  i::Handle<i::JSArrayBuffer> obj = i_isolate->factory()->NewJSArrayBuffer(std::move(i_backing_store));
```
This will call js-array-buffer.cc `Attach`:
```c++
void JSArrayBuffer::Attach(std::shared_ptr<BackingStore> backing_store) {
  Isolate* isolate = GetIsolate();
  set_backing_store(isolate, backing_store->buffer_start());
  set_byte_length(backing_store->byte_length());
  if (backing_store->is_wasm_memory()) set_is_detachable(false);
  if (!backing_store->free_on_destruct()) set_is_external(true);
  Heap* heap = isolate->heap();
  ArrayBufferExtension* extension = EnsureExtension();
  size_t bytes = backing_store->PerIsolateAccountingLength();
  extension->set_accounting_length(bytes);
  extension->set_backing_store(std::move(backing_store));
  heap->AppendArrayBufferExtension(*this, extension);
}
```

Notice that the type of pointer is `v8::BackingStore`. Now, at some later point
ab->Detach() will be called and this will end up in `src/objects/js-array-buffer.cc`:
```c++
void JSArrayBuffer::Detach(bool force_for_wasm_memory) {
  ...
  Isolate* const isolate = GetIsolate();
  if (backing_store()) {
     std::shared_ptr<BackingStore> backing_store;
     backing_store = RemoveExtension();
    CHECK_IMPLIES(force_for_wasm_memory, backing_store->is_wasm_memory());
  }
  ...
}
```
`backing_store` here is of type `v8::internal::BackingStore`:
```console
(lldb) br s -f js-array-buffer.cc -l 89
(lldb) expr backing_store
(std::shared_ptr<v8::internal::BackingStore>) $1 = std::__shared_ptr<v8::internal::BackingStore, __gnu_cxx::_S_atomic>::element_type @ 0x0000604000008150 {
  _M_ptr = 0x0000604000008150
}
```
When this pointer goes out of scope and the use_count is 0 it will be deallocated
and `v8::BackingStore::~BackingStore` destructor will be called:
```c++
v8::BackingStore::~BackingStore() {
  auto i_this = reinterpret_cast<const i::BackingStore*>(this);
  i_this->~BackingStore();  // manually call internal destructor
}
```
Now, this might seem confusing since we checked the type of the pointer and
it is `v8::internal::BackingStore`. But that is the type of the pointer and how
the compiler interprets what is at the memory location, and what is actually 
there an instance of `v8::BackingStore` which done by as cast in ArrayBuffer::New.
Now, the destructor of v8::BackingStore is not inherited so it's implementation
will be run.

And this will land in `backing-store.cc` in `v8::internal::~BackingStore`:
```c++
if (custom_deleter_) {
    DCHECK(free_on_destruct_);
    TRACE_BS("BS:custome deleter bs=%p mem=%p (length=%zu, capacity=%zu)\n",
             this, buffer_start_, byte_length(), byte_capacity_);
    type_specific_data_.deleter.callback(buffer_start_, byte_length_,
                                         type_specific_data_.deleter.data);
    Clear();
    return;
  }
```
This is where the deleter callback will be called. After this function returns,
`v8::internal::~BackingStoreBase` destructor will be called (if there is one that
is), and for testing I've added one so I can step into it. 
After it returns we will be back in `v8::BackingStore::~BackingStore` which will
also call `v8::internal::~BackingStoreBase` since `v8::BackingStore` also extends
`v8::internal::BackingStoreBase`.

Ok, to sort this out, in `include/v8.h` we have:
```c++
class V8_EXPORT BackingStore : public v8::internal::BackingStoreBase {
 public:
  ~BackingStore();
  ...
};
```
In `src/objects/backing-store.h` we have:
```c++
class V8_EXPORT_PRIVATE BackingStore : public BackingStoreBase {
 public:
  ~BackingStore();
  ...
};
```
And we have the definition of `v8::internal::BackingStoreBase` in
`include/v8-internal.h`, which I've added a destructor which prints out the this
pointer to help with debugging this issue:
```c++
class BackingStoreBase {
  public:
    ~BackingStoreBase() {
      std::cout << "~BackingStoreBase. bs=" << this << '\n';
    }
};
```
So first `~BackingStoreBase()` will be called via the call to 
`i_this->~BackingStore()` and then again after `v8::BackingStore::~BackingStore()`
is run as it also extends `v8::internal::BackingStoreBase`.

A standalone example of this issue can be found in 
[virtual-desctructor.cc](https://github.com/danbev/learning-cpp/blob/master/src/fundamentals/virtual-desctructor.cc).
It is interesting to note that this will report the asan error when compiled
with gcc but not when compiled with clang.

Since `v8::~BackingStore` is just casting itself into `v8::internal::BackingStore`
and both extend `v8::internal::BackingStoreBase` if we added a virtual destructor
to `v8::internal::BackingStoreBase` it would be used.

But what I missed was the following line in the debugger:
```console
* thread #1, name = 'cctest', stop reason = Deallocation size different from allocation size
```
So asan is saying that the sizes of the types being deleted miss match which
is correct. We can avoid this error for our test by setting the following:
```c++
extern "C" const char* __asan_default_options() {
  return "new_delete_type_mismatch=0";
}
```

I'm trying the following patch:
```console
diff --git a/include/v8-internal.h b/include/v8-internal.h
index 06846d7005..8e71a04027 100644
--- a/include/v8-internal.h
+++ b/include/v8-internal.h
@@ -452,7 +452,10 @@ V8_INLINE void PerformCastCheck(T* data) {

 // A base class for backing stores, which is needed due to vagaries of
 // how static casts work with std::shared_ptr.
-class BackingStoreBase {};
+class BackingStoreBase {
+ public:
+  virtual ~BackingStoreBase() {}
+};

 }  // namespace internal
 }  // namespace v8
```
Which worked. I'm going to try this patch on Node upstream/master and see
if this works there too. It worked there too. 
