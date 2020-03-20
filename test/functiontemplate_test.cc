#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects.h"
#include "src/objects/objects-inl.h"
#include "src/api/api.h"

using namespace v8;

class FunctionTemplateTest : public V8TestFixture {
};

void function_callback(const FunctionCallbackInfo<Value>& info) {
  std::cout << "in function_callback..." << '\n';

  EXPECT_STREQ(*String::Utf8Value(info.GetIsolate(), info.Data()), "some info");
}

TEST_F(FunctionTemplateTest, FunctionTemplate) {
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  // This value, data, will be made available via the FunctionCallbackInfo:
  Local<Value> data = String::NewFromUtf8(isolate_, "some info", NewStringType::kNormal).ToLocalChecked();
  Local<FunctionTemplate> ft = FunctionTemplate::New(isolate_, function_callback, data);
  Local<Function> function = ft->GetFunction(context).ToLocalChecked();
  Local<Value> recv = Object::New(isolate_);
  int argc = 0;
  Local<Value> argv[] = {}; 
  MaybeLocal<Value> ret = function->Call(context, recv, 0, nullptr);
}
