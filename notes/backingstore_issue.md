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
I've tried to reproduce this using [backingstore_test.cc](./test/backingstore_test.cc).

If we take a look at v8::ArrayBuffer::NewBackingStore we have the following:
```c++
std::unique_ptr<v8::BackingStore> v8::ArrayBuffer::NewBackingStore(
    Isolate* isolate, size_t byte_length) {
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::Allocate(i_isolate, byte_length, i::SharedFlag::kNotShared,
                                i::InitializedFlag::kZeroInitialized);
```
Notice that the unique_ptr is of type v8::internal::BackingStoreBase.
This is then returned with a cast:
```c++
return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
```
The usage in the test looks like this:
```c++
  Local<ArrayBuffer> ab;
  {
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        data, data_len, BackingStoreDeleter, nullptr);
    ab = ArrayBuffer::New(isolate_, std::move(bs));
    std::cout << "ArrayBuffer create\n";
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
When this shared pointer goes out of scope its deleter will be called.
Now, v8::internal::BackingStoreBase does not have an virtual destructor which
could lead to this error.

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
if this works there too.
