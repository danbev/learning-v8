#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"

using namespace v8;

class StringTest : public V8TestFixture {
};

TEST_F(StringTest, create) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  Local<String> str = String::NewFromOneByte(isolate_, 
      reinterpret_cast<const uint8_t*>("bajja"),
      NewStringType::kNormal,
      6).ToLocalChecked();
  String::Utf8Value value(str);
  EXPECT_STREQ("bajja", *value);
  EXPECT_EQ(str->Length(), 6);
  EXPECT_EQ(str->Utf8Length(), 6);
  EXPECT_EQ(str->IsOneByte(), true);
  EXPECT_EQ(str->IsExternal(), false);
  EXPECT_EQ(str->IsExternalOneByte(), false);
}

TEST_F(StringTest, empty) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  Local<String> str = String::Empty(isolate_); 
  EXPECT_EQ(str->Length(), 0);
  EXPECT_EQ(str->Utf8Length(), 0);
  EXPECT_EQ(str->IsOneByte(), true);
  EXPECT_EQ(str->ContainsOnlyOneByte(), true);
}

TEST_F(StringTest, concat) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  Local<String> left = String::NewFromOneByte(isolate_, 
      reinterpret_cast<const uint8_t*>("hey"),
      NewStringType::kNormal,
      6).ToLocalChecked();
  Local<String> right = String::NewFromOneByte(isolate_, 
      reinterpret_cast<const uint8_t*>(" bajja"),
      NewStringType::kNormal,
      6).ToLocalChecked();
  Local<String> joined = String::Concat(left, right);
  EXPECT_EQ(joined->Length(), 12);
}

TEST_F(StringTest, compare) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  Local<String> first = String::NewFromOneByte(isolate_,
      reinterpret_cast<const uint8_t*>("hey"),
      NewStringType::kNormal,
      6).ToLocalChecked();
  Local<String> second = String::NewFromOneByte(isolate_,
      reinterpret_cast<const uint8_t*>("hey"),
      NewStringType::kNormal,
      6).ToLocalChecked();
  v8::String::Utf8Value first_utf8(isolate_, first);
  v8::String::Utf8Value second_utf8(isolate_, second);
  EXPECT_STREQ(*first_utf8, *second_utf8);
}
