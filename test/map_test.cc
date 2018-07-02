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

  std::cout << i::Map::MAP_BIT_FIELD_FIELDS_Ranges::kHasNonInstancePrototypeBitStart << '\n';
  std::cout << i::Map::MAP_BIT_FIELD_FIELDS_Ranges::kHasNonInstancePrototypeBitEnd << '\n';

  i::byte bit_field = map->bit_field();
  auto res = i::Map::HasNonInstancePrototypeBit::decode(bit_field);
  std::cout << res << '\n';
}

TEST_F(MapTest, fields) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  auto context = Context::New(isolate_);
  Context::Scope context_scope(context);

  i::Handle<i::Map> map = i::Map::Create(asInternal(isolate_), 1);
  std::cout << i::kPointerSizeLog2 << '\n';
  std::cout << i::HeapObject::kHeaderSize << '\n';
  std::cout << i::Map::kInstanceSizeInWordsOffset << '\n';
  std::cout << i::Map::kInObjectPropertiesStartOrConstructorFunctionIndexOffset << '\n';
  EXPECT_EQ(24, map->instance_size());
  EXPECT_EQ(1, map->GetInObjectProperties());
  EXPECT_EQ(3, map->instance_size_in_words());
  EXPECT_EQ(3, map->inobject_properties_start_or_constructor_function_index());
}

TEST_F(MapTest, map_creation) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  auto context = Context::New(isolate_);
  Context::Scope context_scope(context);
  i::Isolate* internal_isolate = asInternal(isolate_);
  i::Factory* factory = internal_isolate->factory();

  i::Handle<i::Map> map = factory->NewMap(i::JS_OBJECT_TYPE, 32);

  i::Handle<i::JSObject> js_object = factory->NewJSObjectFromMap(map);
  i::DescriptorArray* descriptors = map->instance_descriptors();
  EXPECT_EQ(0, descriptors->number_of_descriptors());
}

/*
 *   0 instance_size
 *   1
 *   2
 *   3
 */
