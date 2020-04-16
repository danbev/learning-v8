#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "v8_test_fixture.h"

class LocalTest : public V8TestFixture {
};

TEST_F(LocalTest, local) {
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
  //ASSERT_DEATH(maybe.ToLocalChecked(), "Fatal error");

  const v8::HandleScope handle_scope(isolate_);
  // Example of using Local::Cast:
  v8::Local<v8::Number> nr = v8::Local<v8::Number>(v8::Number::New(isolate_, 12));
  v8::Local<v8::Value> val = v8::Local<v8::Value>::Cast(nr);
  // Example of using As:
  v8::Local<v8::Value> val2 = nr.As<v8::Value>();
  
}
