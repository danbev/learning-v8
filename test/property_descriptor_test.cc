#include <iostream>
#include "gtest/gtest.h"
#include "v8_test_fixture.h"
#include "v8.h"

using namespace v8;
namespace i = v8::internal;

class PropertyDescriptorTest : public V8TestFixture {
};

TEST_F(PropertyDescriptorTest, Create) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  Local<Integer> nr = Local<Integer>(Integer::New(isolate_, 2));
  PropertyDescriptor desc(nr);

  EXPECT_TRUE(desc.has_value());
  EXPECT_EQ(desc.value().As<Integer>()->Value(), 2);
  desc.set_enumerable(false);
  EXPECT_FALSE(desc.enumerable());
}
