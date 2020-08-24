#include <iostream>
#include "gtest/gtest.h"
#include "v8_test_fixture.h"
#include "src/execution/simulator.h"
//#include "src/execution/isolate-inl.h"
//#include "src/api/api-inl.h"
//#include "src/builtins/builtins.h"

using namespace v8;
namespace i = v8::internal;

class BuiltinsTest : public V8TestFixture {};

TEST_F(BuiltinsTest, Members) {
  i::Isolate* i_isolate = reinterpret_cast<v8::internal::Isolate*>(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);
  i::Builtins* builtins = i_isolate->builtins();
  std::cout << "builtins count: " << builtins->builtin_count << '\n';
  std::cout << "Builtins::Name::kRecordWrite: " << i::Builtins::Name::kRecordWrite <<
  ", name: " << i::Builtins::name(i::Builtins::Name::kRecordWrite) << '\n';
}

TEST_F(BuiltinsTest, Code) {
  i::Isolate* i_isolate = reinterpret_cast<v8::internal::Isolate*>(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  i::Handle<i::Code> code = i_isolate->builtins()->builtin_handle(i::Builtins::kJSEntry);
  EXPECT_STREQ(i::CodeKindToString(code->kind()), "BUILTIN");

  using JSEntryFunction = i::GeneratedCode<i::Address(
      i::Address root_register_value, i::Address new_target, i::Address target,
      i::Address receiver, intptr_t argc, i::Address** argv)>;
  JSEntryFunction stub_entry = JSEntryFunction::FromAddress(i_isolate, code->InstructionStart());
}

/*
TEST_F(BuiltinsTest, MathIs42) {
  i::Isolate* i_isolate = reinterpret_cast<v8::internal::Isolate*>(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  i::Handle<i::JSObject> base =
        i_isolate->factory()->NewJSObject(i_isolate->object_function(), i::AllocationType::kOld);
  const char* name = "is42";
  i::Handle<i::String> inter_name = i_isolate->factory()->InternalizeUtf8String(name);
  i::Handle<i::String> name_handle = i::String::Flatten(i_isolate, inter_name, i::AllocationType::kOld);
  i::Builtins::Name call = i::Builtins::kMathIs42;
  i::NewFunctionArgs args = i::NewFunctionArgs::ForBuiltinWithoutPrototype(
      name_handle, call, i::LanguageMode::kStrict);
  i::Handle<i::JSFunction> fun = i_isolate->factory()->NewFunction(args);
  i::JSObject::MakePrototypesFast(fun, i::kStartAtReceiver, i_isolate);
  fun->shared().set_native(true);
  fun->shared().set_internal_formal_parameter_count(1);
  fun->shared().set_length(1);

  i::JSObject::AddProperty(i_isolate, base, inter_name, fun, i::PropertyAttributes::DONT_ENUM);
}
*/
