#include "gtest/gtest.h"
#include "v8.h"
#include "v8_test_fixture.h"

using namespace v8;

class ArrayBufferTest : public V8TestFixture {
};

TEST_F(ArrayBufferTest, ArrayBuffer) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);

  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate_, 8);
  EXPECT_EQ(static_cast<int>(ab->ByteLength()), 8);

  // A BackingStore is a wrapper around raw memory
  std::shared_ptr<BackingStore> backingstore = ab->GetBackingStore();
  EXPECT_EQ(static_cast<int>(backingstore->ByteLength()), 8);
}

TEST_F(ArrayBufferTest, Uint16Array) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);

  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate_, 32);
  Local<Uint16Array> array = Uint16Array::New(ab, 0, 2);
  EXPECT_EQ(static_cast<int>(array->Length()), 2);
  array->Set(context, 0, Number::New(isolate_, 220)).Check();
  array->Set(context, 1, Number::New(isolate_, 221)).Check();
}

TEST_F(ArrayBufferTest, Float64Array) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);

  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  int count = 32;
  int bytes_size = sizeof(double) * count;
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate_, bytes_size);
  Local<Float64Array> array = Float64Array::New(ab, 0, count);
  EXPECT_EQ(static_cast<int>(array->Length()), count);
  print_local(array);
}
