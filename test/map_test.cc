#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects-inl.h"

using namespace v8;
namespace i = v8::internal;

extern void _v8_internal_Print_Object(void* object);

class MapTest : public V8TestFixture {
};

TEST_F(MapTest, NewMap) {
  const v8::HandleScope handle_scope(isolate_);
  i::Isolate* i_isolate = asInternal(isolate_);
  v8::Handle<v8::Context> context = v8::Context::New(isolate_, nullptr);

  std::cout << i::JSObject::kHeaderSize << '\n';
  i::Handle<i::Map> map = i_isolate->factory()->NewMap(i::JS_OBJECT_TYPE,
      i::JSObject::kHeaderSize);
  EXPECT_EQ(map->GetInObjectProperties(), 0);
  i::Handle<i::JSObject> object = i_isolate->factory()->NewJSObjectFromMap(map);
  i::Map heap_map = object->map();

  EXPECT_EQ(24, map->instance_size());
  EXPECT_EQ(i::InstanceType::JS_OBJECT_TYPE, map->instance_type());

  i::byte bit_field = map->bit_field();
  auto res = i::Map::Bits1::HasNonInstancePrototypeBit::decode(bit_field);
  std::cout << res << '\n';
}

