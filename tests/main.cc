#include "gtest/gtest.h"
#include "local_test.cc"
#include "persistent-object_test.cc"
#include "maybe_test.cc"

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
