#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "v8_test_fixture.h"
#include "src/objects/objects-inl.h"
#include "src/objects/arguments.h"
#include "src/api/api-arguments.h"
#include "src/execution/isolate-inl.h"

using namespace v8;
namespace i = v8::internal;

class FunctionCallbackArgsTest : public V8TestFixture {};

TEST_F(FunctionCallbackArgsTest, Arguments) {
  i::Object obj{};
  i::Address address = obj.ptr();
  i::Arguments<i::ArgumentsType::kRuntime> args{1, &address};
  EXPECT_EQ(args.length(), 1);

  i::FullObjectSlot slot = args.slot_at(0);
}

TEST_F(FunctionCallbackArgsTest, create) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  i::Isolate* i_isolate = asInternal(isolate_);

  i::Object data{};
  i::HeapObject callee{};
  i::Object holder{};
  i::HeapObject new_target{};
  i::Object argv{};
  i::Address address = argv.ptr();
  int argc = 1;
  i::FunctionCallbackArguments args(i_isolate,
                                    data,
                                    callee,
                                    holder,
                                    new_target,
                                    &address,
                                    argc);
}
