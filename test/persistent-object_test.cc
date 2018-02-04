#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"

TEST(Persistent, object) {
  v8::Persistent<v8::Object> o;
  //EXPECT_EQ(true, v.IsEmpty()) << "Default constructed Local should be empty";
}
