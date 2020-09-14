### V8 unit tests
V8's unit test do not use gtest like this project does and this document
contains notes about how `CcTest` works.

### CcTest
This is the test driver, by which I mean it registers and runs tests, and can 
`test/cctest/cctest.h`:
```c++
class CcTest {
 public:
  using TestFunction = void();
  CcTest(TestFunction* callback, const char* file, const char* name,
         bool enabled, bool initialize);
```
So to register a test the above constructor can be used but there are macros
defined to simply this. For example, to register a test named `DoSomething` one
can simply write:
```c++
TEST(DoSomething) {
  ...
}
```
Which will be expanded by the preprocessor into:
```c++
CcTest register_test_DoSomething(TestDoSomething, __FILE__, DoSomething, true, true) {
  ...
}
```
There are also macros like `DISABLED_TEST`, `UNINITIALIZED_TEST`.

CcTest also contains helper functions to access the internal Isolate, the Heap,
call garbage collection, and more.


### Running a single test
To list all the tests the following command can be used:
```console
$ out/learning_v8/cctest --list
```
From this you can grep for the name and then use the name:
```console
$ out/learning_v8/cctest test-heap/CodeLargeObjectSpace

```

One might have to make certain functions available to tests. For example, I ran
into this when adding a test to test/cctest/heap/test-heap.cc. There is header
test/cctest/heap/heap-tester.h which has the following macro:
```c++
// Tests that should have access to private methods of {v8::internal::Heap}.
// Those tests need to be defined using HEAP_TEST(Name) { ... }.
#define HEAP_TEST_METHODS(V)                                \
  V(CodeLargeObjectSpace)                                   \
  V(CompactionFullAbortedPage)                              \
  ...
```
This will work to compile and run the test manually but it might fail on CI unless
the functions being used are also declared with V8_EXPORT_PRIVATE.

Running test with stress options:
```console
$ out/learning_v8/cctest test-heap/CodeLargeObjectSpace --random-seed=768076620 --stress-incremental-marking --nohard-abort --testing-d8-test-runner
```
