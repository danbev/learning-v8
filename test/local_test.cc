#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"

TEST(Local, local) {
  v8::Local<v8::Value> v;
  EXPECT_EQ(true, v.IsEmpty()) << "Default constructed Local should be empty";

  // A Local<T> can be converted into a MaybeLocal<T>
  v8::MaybeLocal<v8::Value> maybe = v8::MaybeLocal<v8::Value>(v);
  EXPECT_TRUE(maybe.IsEmpty());

  // Both -> and * return the value of the local.
  EXPECT_EQ(*v, nullptr);
  EXPECT_EQ(v.operator->(), nullptr);

  // The following can be useful in if statement to add branch for
  // when the local is empty.
  v8::Local<v8::Value> out;
  bool has_value = maybe.ToLocal<v8::Value>(&out);
  EXPECT_FALSE(has_value);

  // Calling ToLocalChecked will crash the process if called on an empty
  // MaybeLocal<T>
  ASSERT_DEATH(maybe.ToLocalChecked(), "Fatal error");
}
