#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include <bitset>
#include "src/objects/objects-inl.h"
#include "src/objects/heap-object-inl.h"

namespace i = v8::internal;

TEST(HeapObject, Create) {
  // HeapObject extends Object which has a no arguments constructor
  // that delegatess to TaggedImpl(kNullAddress):
  i::HeapObject obj{};
  EXPECT_EQ(obj.ptr(), i::kNullAddress);
  EXPECT_EQ(obj.is_null(), true);

  i::Address addr{2};
  i::HeapObject obj2 = i::HeapObject::FromAddress(addr);
  EXPECT_EQ(obj2.is_null(), false);
}
