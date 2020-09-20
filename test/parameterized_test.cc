#include "gtest/gtest.h"
#include <iostream>

class SomeTest : public testing::Test {

};

class SomeParamTest : public SomeTest, 
                      public testing::WithParamInterface<const char*> {
};

TEST_F(SomeTest, FixtureTest) {
  std::cout << "Fixture test\n";
}

TEST_P(SomeParamTest, Something) {
  std::cout << "Something: param: " << GetParam() << '\n';
}

INSTANTIATE_TEST_CASE_P(SomeTest,
                        SomeParamTest,
                        testing::Values("one", "two", "three"));
