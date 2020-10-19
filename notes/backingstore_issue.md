### Backingstore issue
This document contains notes for an issue that was discovered in Node.js.

The following address sanitizer failure happens:
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
```
Now this is somewhat hard to follow with the Node testing fixture mixed in but
I've tried to reproduce this using [backingstore_test.cc](../test/backingstore_test.cc).

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

```console
(lldb) expr backing_store
(std::unique_ptr<v8::internal::BackingStoreBase, std::default_delete<v8::internal::BackingStoreBase> >) $1 = 0x51fe70 {
  pointer = 0x000000000051fe70
}
```
This is then returned with a cast to `v8::BackingStore`:
```c++
return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
```
Next, the returned backing store will be passed into ArrayBuffer::New:
```c++
  Local<ArrayBuffer> ab;
  {
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        data, data_len, BackingStoreDeleter, nullptr);
    ab = ArrayBuffer::New(isolate_, std::move(bs));
```
The backingstore is then moved into ArrayBuffer::New, also in src/api/api.cc:
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
When this shared pointer goes out of scope its deleter will be called and
`v8::BackingStore` destructor will be called:
```c++
v8::BackingStore::~BackingStore() {
  auto i_this = reinterpret_cast<const i::BackingStore*>(this);
  i_this->~BackingStore();  // manually call internal destructor
}
```
And this will land in backing-store.cc in v8::internal::~BackingStore:
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
This is where the deleter callback will be called. After the return
v8::internal::~BackingStoreBase destructor will be called (if there is one that
is), and for testing I've added one so I can step into it. After this we will
be back in v8::BackingStore::~BackingStore which will also call ~BackingStoreBase.

Ok, to sort this out, in include/v8.h we have:
```c++
class V8_EXPORT BackingStore : public v8::internal::BackingStoreBase {
 public:
  ~BackingStore();
```

In src/objects/backing-store.h we have:
```c++
class V8_EXPORT_PRIVATE BackingStore : public BackingStoreBase {
 public:
  ~BackingStore();
```
And we have the definition of BackingStoreBase in include/v8-internal.h, which
I've added a destructor which prints out the this pointer:
```c++
class BackingStoreBase {
  public:
    ~BackingStoreBase() {
      std::cout << "~BackingStoreBase. bs=" << this << '\n';
    }
};
```
So first ~BackingStoreBase() will be called via the call to i_this->~BackingStore()
and then again after v8::BackingStore::~BackingStore() is run as it also extends
BackingStoreBase.

Now, v8::internal::BackingStoreBase does not have an virtual destructor which
could lead to this error because we are deleting a BackingStoreBase object
through BackingStore pointer. 

Going back to the error
```console
0x60400001af50 is located 0 bytes inside of 48-byte region [0x60400001af50,0x60400001af80)
allocated by thread T0 here:
    #0 0x7f0a3a304a97 in operator new(unsigned long) (/lib64/libasan.so.5+0x10fa97)
    #1 0x34cb1a8 in v8::internal::BackingStore::WrapAllocation(void*, unsigned long, void (*)(void*, unsigned long, void*), void*, v8::internal::SharedFlag) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x34cb1a8)
    #2 0x29c18d5 in v8::ArrayBuffer::NewBackingStore(void*, unsigned long, void (*)(void*, unsigned long, void*), void*) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x29c18d5)
```
We now know that WrapAllocation will create a new v8::internal::BackingStore.
```console
(lldb) expr result
(v8::internal::BackingStore *) $0 = 0x000000000051ee70
(lldb) n
BS:wrap   bs=0x51ee70 mem=0x7fffffffcd69 (length=6
```
And before we return this backingstoe will be released (remember that this only
releasing ownership so the constructor is not called, not like reset()). And
is casted into a v8::BackingStore which will be owned/managed by a unique_ptr.

And asan is complaining later at.
```console
==2773765==ERROR: AddressSanitizer: new-delete-type-mismatch on 0x60400001af50 in thread T0:
  object passed to delete has wrong type:
  size of the allocated type:   48 bytes;
  size of the deallocated type: 1 bytes.
    #0 0x7f0a3a306175 in operator delete(void*, unsigned long) (/lib64/libasan.so.5+0x111175)
    #1 0x38328c2 in v8::internal::JSArrayBuffer::Detach(bool) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x38328c2)
    #2 0x29bf88a in v8::ArrayBuffer::Detach() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x29bf88a)
    #3 0x151adae in node::Buffer::(anonymous namespace)::CallbackInfo::CleanupHook(void*) (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x151adae)
    #4 0x144a5ff in node::Environment::RunCleanup() (/home/danielbevenius/work/nodejs/node/out/Release/cctest+0x144a5ff)
```
So we are again looking at js-array-buffer.cc and its Detach function:
```c++
  if (backing_store()) {
    std::shared_ptr<BackingStore> backing_store;
      backing_store = RemoveExtension();
    CHECK_IMPLIES(force_for_wasm_memory, backing_store->is_wasm_memory());
  }
```
The `RemoveExtension()` function will perform a move of the v8::internal::BackingStore
that v8::internal::ArrayBufferExtension has:
```c++
  std::shared_ptr<BackingStore> RemoveBackingStore() {
    return std::move(backing_store_);
  }
```
After the move the ownership is released and the shared_ptr owns it now.
Now, after the `CHECK_IMPLIES` the scope of backing_store will end and if the
usage count is 0 at this point then the destructor will be called.

So v8::BackingStore::~BackingStore() will be called, which will manually call
v8::internal::~BackingStore(), which will call the custom deleter and then
return. Next, v8::internal::~BackingStoreBase() is called. But v8::BackingStore
also extends v8::internal::BackingStore so it its destructor will be called for
it. 

A standalone example of this issue can be found in 
[virtual-desctructor.cc](https://github.com/danbev/learning-cpp/blob/master/src/fundamentals/virtual-desctructor.cc).
It is interesting to note that this will report the asan error when compiled
with gcc but not when compiled with clang.

And lets make ~BackingStoreBase virtual and we will notice that there will only
be one call to ~BackingStoreBase. Why is that? 
First note/recall that we are dealing with pointers to the same underlying
instance of BackingStoreBase, we have only casted it into a type of
v8::BackingStore. When the destructor is non-virtual, or not defined, in
v8::internal::BackingStoreBase, v8::BackingStore and v8::internal::BackingStore
will have separate destructor implementations. When we add a virtual destructor
polymorphism will be used and the destructor called will depend on the type
of pointer, in our case in Detach the type will be v8::internal::BackingStore
so it's destructor will be run which is what we want.  

```c++
  delete __ptr;
```
```console
(lldb) expr __ptr
(v8::BackingStore *) $0 = 0x0000604000008150
```
So we are using a pointer to v8::BackingStore which is derived from
v8::internal::BackingStoreBase. 

Lets see how asan describes this address:
```console
(lldb) expr (void)__asan_describe_address(0x604000008150)
0x604000008150 is located 0 bytes inside of 48-byte region [0x604000008150,0x604000008180)
allocated by thread T0 here:
    #0 0x7ffff7684a97 in operator new(unsigned long) (/lib64/libasan.so.5+0x10fa97)
    #1 0x7ffff5adf9ff in v8::internal::BackingStore::WrapAllocation(void*, unsigned long, void (*)(void*, unsigned long, void*), void*, v8::internal::SharedFlag) ../../src/objects/backing-store.cc:608
    #2 0x7ffff54ca25c in v8::ArrayBuffer::NewBackingStore(void*, unsigned long, void (*)(void*, unsigned long, void*), void*) ../../src/api/api.cc:7658
    #3 0x415706 in BackingStoreTest_GetBackingStoreWithDeleter_Test::TestBody() test/backingstore_test.cc:39
    #4 0x43f4c8 in void testing::internal::HandleSehExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) /home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest.cc:2443
    #5 0x439ffe in void testing::internal::HandleExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) /home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest.cc:2498
    #6 0x41f8b5 in testing::Test::Run() /home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest.cc:2517
    #7 0x41ffb6 in testing::TestInfo::Run() /home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest.cc:2693
    #8 0x420540 in testing::TestCase::Run() /home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest.cc:2811
    #9 0x428d9b in testing::internal::UnitTestImpl::RunAllTests() /home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest.cc:5177
    #10 0x4404bb in bool testing::internal::HandleSehExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) /home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest.cc:2443
    #11 0x43aad8 in bool testing::internal::HandleExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) /home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest.cc:2498
    #12 0x427cf1 in testing::UnitTest::Run() /home/danielbevenius/work/google/learning-v8/deps/googletest/googletest/src/gtest.cc:4786
    #13 0x4150f5 in RUN_ALL_TESTS() deps/googletest/googletest/include/gtest/gtest.h:2341
    #14 0x415016 in main test/main.cc:5
    #15 0x7ffff29011a2 in __libc_start_main (/lib64/libc.so.6+0x271a2)
```
So this is just the as the output we got originally but this we can print this
before the error happens. 

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

Next step will be to open a pull request against V8 with a test reproducing and
perhaps float that patch in node if the patch is accepted.
