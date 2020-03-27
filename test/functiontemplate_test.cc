#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects.h"
#include "src/objects/objects-inl.h"
#include "src/api/api-inl.h"

using namespace v8;

class FunctionTemplateTest : public V8TestFixture {
};

void function_callback(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  std::cout << "function_callback args= " << info.Length() << '\n';

  // If the function was called using the new operator the property
  // new.target(NewTarget) will be set.
  Local<Value> new_target_value = info.NewTarget();
  if (new_target_value.IsEmpty()) {
    std::cout << "new_target_value is undefined: " << new_target_value->IsUndefined() << '\n';
  }
  // This is the receiver passed as the second argument to the Call function,
  // which is like the this.
  Local<Object> receiver = info.This();
  Local<Name> name = String::NewFromUtf8(isolate, "nr", NewStringType::kNormal).ToLocalChecked();
  Local<Value> nr_local = receiver->GetRealNamedProperty(isolate->GetCurrentContext(), name).ToLocalChecked();
  Local<Number> nr = nr_local->ToNumber(isolate->GetCurrentContext()).ToLocalChecked();

  Local<Object> holder = info.Holder();

  ReturnValue<Value> return_value = info.GetReturnValue();
  double nr2 = nr->Value() + 2;
  return_value.Set(nr2);

  EXPECT_STREQ(*String::Utf8Value(isolate, info.Data()), "some info");
}

TEST_F(FunctionTemplateTest, FunctionTemplate) {
  i::Isolate* i_isolate = V8TestFixture::asInternal(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  // This value, data, will be made available via the FunctionCallbackInfo:
  Local<Value> data = String::NewFromUtf8(isolate_, "some info", NewStringType::kNormal).ToLocalChecked();
  Local<FunctionTemplate> ft = FunctionTemplate::New(isolate_, function_callback, data);
  Local<Function> function = ft->GetFunction(context).ToLocalChecked();
  Local<String> func_name = String::NewFromUtf8(isolate_, "SomeFunc", NewStringType::kNormal).ToLocalChecked();
  function->SetName(func_name);
  Local<Value> prototype = function->GetPrototype();
  V8TestFixture::print_local(prototype);

  Local<Object> recv = Object::New(isolate_);
  Local<Name> name = String::NewFromUtf8(isolate_, "nr", NewStringType::kNormal).ToLocalChecked();
  Local<Number> value = Number::New(isolate_, 18);
  recv->Set(context, name, value).Check();

  int argc = 0;
  Local<Value> argv[] = {}; 
  MaybeLocal<Value> ret = function->Call(context, recv, argc, nullptr);
  if (!ret.IsEmpty()) {
    Local<Number> nr = ret.ToLocalChecked()->ToNumber(context).ToLocalChecked();
    EXPECT_EQ(nr->Value(), 20);
  }

  i::RootsTable roots_table = i_isolate->roots_table();
  i::Heap* heap = i_isolate->heap();

  //Local<Function> function2 = ft->GetFunction(context).ToLocalChecked();
  //MaybeLocal<Value> ret = function->Call(context, recv, 0, nullptr);
}

TEST_F(FunctionTemplateTest, FunctionTemplateInfo) {
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  // This value, data, will be made available via the FunctionCallbackInfo:
  Local<Value> data = String::NewFromUtf8(isolate_, "some info", NewStringType::kNormal).ToLocalChecked();
  Local<FunctionTemplate> ft = FunctionTemplate::New(isolate_, function_callback, data);
  i::Handle<i::FunctionTemplateInfo> ft_info = i::Handle<i::FunctionTemplateInfo>(
      reinterpret_cast<i::Address*>(const_cast<FunctionTemplate*>(*ft)));
  i::Isolate* i_isolate = V8TestFixture::asInternal(isolate_);
  i::Handle<i::SharedFunctionInfo> sfi = i::FunctionTemplateInfo::GetOrCreateSharedFunctionInfo(
      i_isolate, ft_info, i::MaybeHandle<i::Name>());
  //std::cout << sfi->Name() << '\n';
  //ft_info->GetCFunction(i_isolate);
}
