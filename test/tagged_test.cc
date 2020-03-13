#include <iostream>
#include <cstdio>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects.h"
#include "src/objects/tagged-impl-inl.h"
#include "src/common/globals.h"

using namespace v8;
namespace i = v8::internal;

class TaggedTest : public V8TestFixture {
};

TEST_F(TaggedTest, Create) {
  const v8::HandleScope handle_scope(isolate_);
  i::Isolate* i_isolate = asInternal(isolate_);

  i::TaggedImpl<i::HeapObjectReferenceType::STRONG, i::Address>  tagged{};
  EXPECT_EQ(tagged.ptr(), i::kNullAddress);
  EXPECT_EQ(tagged.IsObject(), true);
  EXPECT_EQ(tagged.IsSmi(), true);

  const i::Smi small_int = tagged.ToSmi();
}

