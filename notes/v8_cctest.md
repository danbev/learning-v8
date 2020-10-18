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

### Address sanitizer
To enable address sanitizer, first install asan:
```console
$ sudo dnf install libasan libasan-static
```
And then add `is_asan=true` to the build args, which in my case is
out/learning_v8/args.gn:
```console
is_asan = true
is_clang = true
```
Notice that `is_clang` also has to be set or there will be error:
```console
[1/1] Regenerating ninja files
FAILED: build.ninja
../../buildtools/linux64/gn --root=../.. -q gen .
ERROR at //build/config/sanitizers/sanitizers.gni:195:1: Assertion failed.
assert(!using_sanitizer || is_clang,
^-----
Sanitizers (is_*san) require setting is_clang = true in 'gn args'
See //build/config/v8_target_cpu.gni:5:1: whence it was imported.
```

#### Parameterized tests
This section shows an example of a parameterized test in V8 and also notes
a compiler warning that I'm seeing with gcc.

The test in question is TEST_P(InstructionSelectorChangeInt32ToInt64Test, ChangeInt32ToInt64WithLoad)
  in test/unittests/compiler/x64/instruction-selector-x64-unittest.cc:
```c++
namespace {
struct LoadWithToInt64Extension {
  MachineType type;
  ArchOpcode expected_opcode;
};

static const LoadWithToInt64Extension kLoadWithToInt64Extensions[] = {
    {MachineType::Int8(), kX64Movsxbq},
    {MachineType::Uint8(), kX64Movzxbq},
    {MachineType::Int16(), kX64Movsxwq},
    {MachineType::Uint16(), kX64Movzxwq},
    {MachineType::Int32(), kX64Movsxlq}};

}  // namespace

using InstructionSelectorChangeInt32ToInt64Test =
    InstructionSelectorTestWithParam<LoadWithToInt64Extension>;

TEST_P(InstructionSelectorChangeInt32ToInt64Test, ChangeInt32ToInt64WithLoad) {
  const LoadWithToInt64Extension extension = GetParam();
  StreamBuilder m(this, MachineType::Int64(), MachineType::Pointer());
  m.Return(m.ChangeInt32ToInt64(m.Load(extension.type, m.Parameter(0))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(extension.expected_opcode, s[0]->arch_opcode());
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest,
                         InstructionSelectorChangeInt32ToInt64Test,
                         ::testing::ValuesIn(kLoadWithToInt64Extensions));
```
Notice the `using` statement and we can find InstructionSelectorTestWithParam in
test/unittests/compiler/backend/instruction-selector-unittest.h:
```c++
template <typename T>
class InstructionSelectorTestWithParam
    : public InstructionSelectorTest,
      public ::testing::WithParamInterface<T> {};
```
And also notice that `T` is `LoadWithToInt64Extension` and that it is in the
annonymous namespace (more about this later).
`INSTANTIATE_TEST_SUITE_P` will instantiate tests From InstructorSelectorTest.

There is an example in [parameterized_test.cc)](test/parameterized_test.cc).

Now, back to the anonymous namespace. The following compiler warning is currently
being generated:
```console
In file included from ../../third_party/googletest/src/googletest/include/gtest/gtest-param-test.h:181,
                 from ../../third_party/googletest/src/googletest/include/gtest/gtest.h:67,
                 from ../../testing/gtest/include/gtest/gtest.h:10,
                 from ../../testing/gtest-support.h:8,
                 from ../../test/unittests/test-utils.h:20,
                 from ../../test/unittests/compiler/backend/instruction-selector-unittest.h:15,
                 from ../../test/unittests/compiler/x64/instruction-selector-x64-unittest.cc:9:
../../third_party/googletest/src/googletest/include/gtest/internal/gtest-param-util.h: In instantiation of ‘class testing::internal::ParameterizedTestFactory<v8::internal::compiler::InstructionSelectorChangeInt32ToInt64Test_ChangeInt32ToInt64WithLoad_Test>’:
../../third_party/googletest/src/googletest/include/gtest/internal/gtest-param-util.h:439:12:   required from ‘testing::internal::TestFactoryBase* testing::internal::TestMetaFactory<TestSuite>::CreateTestFactory(testing::internal::TestMetaFactory<TestSuite>::ParamType) [with TestSuite = v8::internal::compiler::InstructionSelectorChangeInt32ToInt64Test_ChangeInt32ToInt64WithLoad_Test; testing::internal::TestMetaFactory<TestSuite>::ParamType = v8::internal::compiler::{anonymous}::LoadWithToInt64Extension]’
../../third_party/googletest/src/googletest/include/gtest/internal/gtest-param-util.h:438:20:   required from here
../../third_party/googletest/src/googletest/include/gtest/internal/gtest-param-util.h:394:7: warning: ‘testing::internal::ParameterizedTestFactory<v8::internal::compiler::InstructionSelectorChangeInt32ToInt64Test_ChangeInt32ToInt64WithLoad_Test>’ has a field ‘testing::internal::ParameterizedTestFactory<v8::internal::compiler::InstructionSelectorChangeInt32ToInt64Test_ChangeInt32ToInt64WithLoad_Test>::parameter_’ whose type uses the anonymous namespace [-Wsubobject-linkage]
  394 | class ParameterizedTestFactory : public TestFactoryBase {
      |       ^~~~~~~~~~~~~~~~~~~~~~~~
```
The issue here is that InstructionSelectorChangeInt32ToInt64Test is using a type
that is in the anonymous namespace but the class generated by TEST_P is not.
In this case it will only be used in the single translation unit and the
warning about subobject-linkage could be avoided by placing the class generated
by TEST_P in it as well.


To see what the gtest macros expand to the following command can be used:
```console
$ g++ -I./deps/googletest/googletest/include -E test/parameterized_test.cc
```
```c++
class SomeParamTest_Something_Test : public SomeParamTest { 
  public: 
    SomeParamTest_Something_Test() {} 
    virtual void TestBody(); 
  private: 
    static int AddToRegistry() { 
      ::testing::UnitTest::GetInstance()->parameterized_test_registry()
	.GetTestCasePatternHolder<SomeParamTest>( "SomeParamTest",
	  ::testing::internal::CodeLocation( "test/parameterized_test.cc", 16))->AddTestPattern( "SomeParamTest", "Something",
	  new ::testing::internal::TestMetaFactory< SomeParamTest_Something_Test>());
      return 0; 
    }
    static int gtest_registering_dummy_ __attribute__ ((unused));
    SomeParamTest_Something_Test(SomeParamTest_Something_Test const &) = delete;
    void operator=(SomeParamTest_Something_Test const &) = delete;
};
    int SomeParamTest_Something_Test::gtest_registering_dummy_ = SomeParamTest_Something_Test::AddToRegistry();

    void SomeParamTest_Something_Test::TestBody() {
      std::cout << "Something: param: " << GetParam() << '\n';
    }
