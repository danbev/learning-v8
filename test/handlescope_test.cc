#include <iostream>
#include "gtest/gtest.h"
#include "v8_test_fixture.h"
#include "v8.h"
#include "src/handles/handles-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/api/api-inl.h"

namespace i = v8::internal;

class HandleScopeTest : public V8TestFixture { };

TEST_F(HandleScopeTest, HandleScopeData) {
  /*
  i::Isolate* isolate = asInternal(isolate_);
  i::HandleScope handle_scope(isolate);
  i::HandleScopeData data{};
  data.Initialize();
  EXPECT_EQ(data.next, nullptr);
  EXPECT_EQ(data.limit, nullptr);
  EXPECT_EQ(data.canonical_scope, nullptr);
  EXPECT_EQ(data.level, 0);
  EXPECT_EQ(data.sealed_level, 0);
  */
}

TEST_F(HandleScopeTest, Create) {
  i::Isolate* i_isolate = asInternal(isolate_);
  i::HandleScope handle_scope{i_isolate};
  i::Object obj{18};
  i::Handle<i::Object> handle(obj, i_isolate);
  EXPECT_FALSE(handle.is_null());
  EXPECT_EQ(*handle, obj);
}

TEST_F(HandleScopeTest, HandleScopeImplementer) {
  i::Isolate* i_isolate = asInternal(isolate_);
  i::HandleScopeImplementer implementer{i_isolate};
  // Context is just a HeapObject so we can construct one the default not
  // args constructor.
  i::Context context{};

  implementer.SaveContext(context);
  EXPECT_TRUE(implementer.HasSavedContexts());

  implementer.EnterContext(context);
  EXPECT_EQ(static_cast<int>(implementer.EnteredContextCount()), 1);
  implementer.LeaveContext();
  EXPECT_EQ(static_cast<int>(implementer.EnteredContextCount()), 0);

  i::DetachableVector<i::Address*>* blocks = implementer.blocks();
  EXPECT_TRUE(blocks->empty());
  i::Address* block = implementer.GetSpareOrNewBlock();
  blocks->push_back(block);
  EXPECT_FALSE(blocks->empty());
}
