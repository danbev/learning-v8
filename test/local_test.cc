#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"

TEST(Local, local) {
  v8::Local<v8::Value> v;
  EXPECT_EQ(true, v.IsEmpty()) << "Default constructed Local should be empty";
}
