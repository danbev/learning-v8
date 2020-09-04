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
$ out/learning_v8/cctest test-spaces/OldLargeObjectSpace
```
