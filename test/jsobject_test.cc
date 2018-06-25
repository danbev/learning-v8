#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects.h"
#include "src/objects-inl.h"

using namespace v8;
namespace i = v8::internal;

class JSObjectTest : public V8TestFixture {
};

TEST_F(JSObjectTest, FastProperties) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);
  i::Factory* factory = internal_isolate->factory();
  // Create the Map
  i::Handle<i::Map> map = factory->NewMap(i::JS_OBJECT_TYPE, 24);
  EXPECT_EQ(i::JS_OBJECT_TYPE, map->instance_type());
  EXPECT_EQ(24, map->instance_size());
  EXPECT_EQ(0, map->GetInObjectProperties());
  // Create the JSObject instance using the Map
  i::Handle<i::JSObject> js_object = factory->NewJSObjectFromMap(map);
  EXPECT_TRUE(js_object->HasFastProperties());
  EXPECT_EQ(0, js_object->property_array()->length());

  // Add a property
  i::Handle<i::String> prop_name = factory->InternalizeUtf8String("something");
  i::Handle<i::String> prop_value = factory->InternalizeUtf8String("value");
  i::JSObject::AddProperty(js_object, prop_name, prop_value, i::NONE); 
  EXPECT_TRUE(i::JSReceiver::HasProperty(js_object, prop_name).FromJust());
  i::Handle<i::Object> v = i::JSReceiver::GetProperty(js_object, prop_name).ToHandleChecked();

  //i::Handle<i::JSObject> gv(i::JSObject::cast(*v));
  i::PropertyArray* ar = js_object->property_array();
  EXPECT_EQ(*v, ar->get(0));
  EXPECT_EQ(3, ar->length()) << "Why 3 and not 1?";

  i::Handle<i::JSObject> receiver = factory->NewJSObjectFromMap(map);
  EXPECT_TRUE(!receiver->map()->is_dictionary_map());
  EXPECT_TRUE(receiver->HasFastProperties());
  EXPECT_EQ(0, receiver->map()->GetInObjectProperties());
}

TEST_F(JSObjectTest, DescriptorArray) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);
  i::Factory* factory = internal_isolate->factory();

  i::Handle<i::Map> map = factory->NewMap(i::JS_OBJECT_TYPE, 24);
  i::Handle<i::JSObject> js_object = factory->NewJSObjectFromMap(map);
  i::DescriptorArray* descriptors = map->instance_descriptors();
  EXPECT_EQ(0, descriptors->number_of_descriptors());
}

