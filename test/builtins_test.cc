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
  EXPECT_STREQ(i::Code::Kind2String(code->kind()), "BUILTIN");

  using JSEntryFunction = i::GeneratedCode<i::Address(
      i::Address root_register_value, i::Address new_target, i::Address target,
      i::Address receiver, intptr_t argc, i::Address** argv)>;
  JSEntryFunction stub_entry = JSEntryFunction::FromAddress(i_isolate, code->InstructionStart());
}
