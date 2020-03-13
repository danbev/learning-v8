#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "src/handles/handles.h"
#include "src/objects/objects-inl.h"

namespace i = v8::internal;

TEST(Handle, DefaultConstructor) {
  i::Handle<int> handle{};
  EXPECT_TRUE(handle.is_null());
  EXPECT_EQ(handle.location(), nullptr);
}

TEST(Handle, AddressConstructor) {
  int* value = new int{5};
  i::Address addr = reinterpret_cast<i::Address>(value);
  i::Object obj{addr};

  i::Address ptr = obj.ptr();
  i::Address* location = &ptr;
  i::Handle<i::Object> handle(location);

  EXPECT_EQ(handle.location(), &ptr);
  EXPECT_EQ(*handle.location(), ptr);
  i::Object deref = *handle;
  i::Address deref_addr = deref.ptr();
  int* deref_value = reinterpret_cast<int*>(deref_addr);
  EXPECT_EQ(*deref_value, *value);
  delete value;
}
