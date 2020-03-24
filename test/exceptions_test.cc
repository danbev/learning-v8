#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects.h"
#include "src/objects/objects-inl.h"
#include "src/api/api-inl.h"
#include "src/execution/messages.h"

using namespace v8;

class ExceptionsTest : public V8TestFixture {
};

void function_callback(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<v8::internal::Isolate*>(isolate);
  std::cout << "function_callback args...\n";

  i::Object exception{};
  //i::Object o = i_isolate->Throw(exception);
  isolate->ThrowException(String::NewFromUtf8(isolate, "some error").ToLocalChecked());
  std::cout << "has_pending_exception: " << i_isolate->has_pending_exception() << '\n';
  std::cout << "has_scheduled_exception: " << i_isolate->has_scheduled_exception() << '\n';

  // Set a return value
  ReturnValue<Value> return_value = info.GetReturnValue();
  return_value.Set(8);
}

TEST_F(ExceptionsTest, Exception) {
  i::Isolate* i_isolate = V8TestFixture::asInternal(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> ft = FunctionTemplate::New(isolate_, function_callback);
  Local<Function> function = ft->GetFunction(context).ToLocalChecked();

  Local<Object> recv = Object::New(isolate_);
  TryCatch try_catch{isolate_};
  MaybeLocal<Value> ret = function->Call(context, recv, 0, nullptr);
  EXPECT_TRUE(ret.IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
}
