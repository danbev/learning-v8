#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects.h"
#include "src/objects/objects-inl.h"
#include "src/api/api.h"
#include "src/objects/elements-kind.h"

using namespace v8;
namespace i = v8::internal;

class ArraysTest : public V8TestFixture {
};

TEST_F(ArraysTest, Array) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);

  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  Local<Array> zero_length = Array::New(isolate_);
  EXPECT_EQ(static_cast<int>(zero_length->Length()), 0);

  int size = 4;
  Local<Array> array = Array::New(isolate_, size);
  EXPECT_EQ(static_cast<int>(array->Length()), size);
  for (uint32_t i=0; i < array->Length(); i++) {
    Maybe<bool> res = array->Set(context, i, Number::New(isolate_, i));
  }
  // Show usage using Value::ToNumber:
  for (uint32_t i=0; i < array->Length(); i++) {
    Local<Value> val = array->Get(context, i).ToLocalChecked();
    Local<Number> nr = val->ToNumber(context).ToLocalChecked();
    EXPECT_EQ(nr->Value(), i);
  }
  // Show usage of ToLocal:
  for (uint32_t i=0; i < array->Length(); i++) {
    Local<Value> val;
    bool res = array->Get(context, i).ToLocal(&val);
    Local<Number> nr = val->ToNumber(context).ToLocalChecked();
    EXPECT_EQ(nr->Value(), i);
  }
  // Show usage of As<Number>:
  for (uint32_t i=0; i < array->Length(); i++) {
    Local<Number> nr = array->Get(context, i).ToLocalChecked().As<Number>();
    EXPECT_EQ(nr->Value(), i);
  }
  // Show usage of Cast<Number>:
  for (uint32_t i=0; i < array->Length(); i++) {
    Local<Number> nr = Local<Number>::Cast(array->Get(context, i).ToLocalChecked());
    EXPECT_EQ(nr->Value(), i);
  }
}

TEST_F(ArraysTest, HoleyArray) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);

  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  Local<Array> zero_length = Array::New(isolate_);
  i::Handle<i::JSArray> js_array = Utils::OpenHandle(*zero_length);
  print_handle(js_array);
  EXPECT_TRUE(js_array->HasHoleyElements());
  EXPECT_EQ(js_array->GetElementsKind(), i::ElementsKind::HOLEY_ELEMENTS);
}
