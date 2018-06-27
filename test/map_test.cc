#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects.h"
#include "src/objects-inl.h"

using namespace v8;
namespace i = v8::internal;

class MapTest : public V8TestFixture {
};

TEST_F(MapTest, instance_type) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  auto context = Context::New(isolate_);
  Context::Scope context_scope(context);

  i::Handle<i::Map> map = i::Map::Create(asInternal(isolate_), 10);
  EXPECT_EQ(104, map->instance_size());
  EXPECT_EQ(i::InstanceType::JS_OBJECT_TYPE, map->instance_type());
  i::Address instance_type_address = reinterpret_cast<i::Address>(*map + map->kInstanceTypeOffset - i::kHeapObjectTag);
  const uint16_t* uint_type = reinterpret_cast<const uint16_t*>(instance_type_address);
  i::InstanceType type = static_cast<i::InstanceType>(*uint_type);
  EXPECT_EQ(i::InstanceType::JS_OBJECT_TYPE, type);
}
