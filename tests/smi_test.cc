#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"

/*
 * Small integer (SMI) example. These are tagged values.
 * more coming soon...
 */
TEST(Smi, IntToSmi) {
  v8::internal::Object* obj = v8::internal::IntToSmi<31>(2);
  EXPECT_EQ(2, *obj);
}
