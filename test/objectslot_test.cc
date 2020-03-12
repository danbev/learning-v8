#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include <bitset>
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"

namespace i = v8::internal;

TEST(ObjectSlot, Create) {
  i::Object obj{18};
  i::FullObjectSlot slot{&obj};
  EXPECT_NE(slot.address(), obj.ptr());
  EXPECT_EQ(*slot, obj);

  i::Object* p = &obj;
  i::Object** pp = &p;
  EXPECT_EQ(*slot, **pp);
}
