#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/factory.h"
#include "src/objects.h"
#include "src/objects-inl.h"

using namespace v8;
namespace i = v8::internal;

class ContextTest : public V8TestFixture {
};

TEST_F(ContextTest, Scopes) {
  const Local<v8::ObjectTemplate> obt = v8::Local<v8::ObjectTemplate>();
  const v8::HandleScope handle_scope(isolate_);
  {
    Handle<Context> context1 = Context::New(isolate_, nullptr, obt);
    Context::Scope contextScope1(context1);
    // entered_contexts_ [context1], saved_contexts_[isolateContext]
    EXPECT_EQ(context1, isolate_->GetEnteredContext());
    EXPECT_EQ(context1, isolate_->GetCurrentContext());
    {
      Handle<Context> context2 = Context::New(isolate_, nullptr, obt);
      Context::Scope contextScope2(context2);
      // entered_contexts_ [context1, context2], saved_contexts[isolateContext, context1]
      EXPECT_EQ(context2, isolate_->GetEnteredContext());
      EXPECT_EQ(context2, isolate_->GetCurrentContext());
    }
  }
}
