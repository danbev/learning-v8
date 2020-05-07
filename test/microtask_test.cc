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

class MicrotaskTest : public V8TestFixture {};

void function_callback(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  std::cout << "function_callback args= " << info.Length() << '\n';
}

TEST_F(MicrotaskTest, EnqueueMicrotaskFunction) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);

  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  Local<Value> data = String::NewFromUtf8(isolate_, "some info", NewStringType::kNormal).ToLocalChecked();
  Local<FunctionTemplate> ft = FunctionTemplate::New(isolate_, function_callback, data);
  Local<Function> function = ft->GetFunction(context).ToLocalChecked();
  Local<String> func_name = String::NewFromUtf8(isolate_, "SomeFunc", NewStringType::kNormal).ToLocalChecked();
  function->SetName(func_name);
  isolate_->EnqueueMicrotask(function);
  isolate_->PerformMicrotaskCheckpoint();
}

void callback(void* data) {
  std::cout << "callback..." << '\n';
}

TEST_F(MicrotaskTest, EnqueueMicrotaskCallback) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);

  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  isolate_->EnqueueMicrotask(callback, nullptr);
  isolate_->PerformMicrotaskCheckpoint();
}
