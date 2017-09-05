#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/factory.h"
#include "src/objects.h"
#include "src/objects-inl.h"

using namespace v8;
namespace v8_int = v8::internal;

class JSObjectTest : public V8TestFixture {
};

v8_int::Isolate* asInternal(Isolate* isolate) {
  return reinterpret_cast<v8_int::Isolate*>(isolate);
}

TEST_F(JSObjectTest, FastProperties) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  v8_int::Isolate* internal_isolate = asInternal(isolate_);
  v8_int::Factory* factory = internal_isolate->factory();
  // Create the Map
  v8_int::Handle<v8_int::Map> map = factory->NewMap(v8_int::JS_OBJECT_TYPE, 24);
  EXPECT_EQ(v8_int::JS_OBJECT_TYPE, map->instance_type());
  EXPECT_EQ(24, map->instance_size());
  EXPECT_EQ(0, map->GetInObjectProperties());
  // Create the JSObject instance using the Map
  v8_int::Handle<v8_int::JSObject> js_object = factory->NewJSObjectFromMap(map);
  EXPECT_TRUE(js_object->HasFastProperties());
  EXPECT_EQ(0, js_object->property_array()->length());

  // Add a property
  v8_int::Handle<v8_int::String> prop_name = factory->InternalizeUtf8String("something");
  v8_int::Handle<v8_int::String> prop_value = factory->InternalizeUtf8String("value");
  v8_int::JSObject::AddProperty(js_object, prop_name, prop_value, v8_int::NONE); 
  EXPECT_TRUE(v8_int::JSReceiver::HasProperty(js_object, prop_name).FromJust());
  v8_int::Handle<v8_int::Object> v = v8_int::JSReceiver::GetProperty(js_object, prop_name).ToHandleChecked();

  v8_int::PropertyArray* ar = js_object->property_array();
  EXPECT_EQ(*v, ar->get(0));
  EXPECT_EQ(3, ar->length()) << "Why 3 and not 1?";

  v8_int::Handle<v8_int::JSObject> receiver = factory->NewJSObjectFromMap(map);
  EXPECT_TRUE(!receiver->map()->is_dictionary_map());
  EXPECT_TRUE(receiver->HasFastProperties());
  EXPECT_EQ(0, receiver->map()->GetInObjectProperties());
}

TEST_F(JSObjectTest, DescriptorArray) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  v8_int::Isolate* internal_isolate = asInternal(isolate_);
  v8_int::Factory* factory = internal_isolate->factory();

  v8_int::Handle<v8_int::Map> map = factory->NewMap(v8_int::JS_OBJECT_TYPE, 24);
  v8_int::Handle<v8_int::JSObject> js_object = factory->NewJSObjectFromMap(map);
  v8_int::DescriptorArray* descriptors = map->instance_descriptors();
  EXPECT_EQ(0, descriptors->number_of_descriptors());
}

