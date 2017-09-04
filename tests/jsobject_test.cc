#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/factory.h"
#include "src/objects.h"
#include "src/objects-inl.h"

using namespace v8;

class JSObjectTest : public V8TestFixture {
};

TEST_F(JSObjectTest, FastProperties) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  v8::internal::Isolate* internal_isolate =
    reinterpret_cast<v8::internal::Isolate*>(isolate_);
  v8::internal::Factory* factory = internal_isolate->factory();

  v8::internal::Handle<v8::internal::Map> map = factory->NewMap(v8::internal::JS_OBJECT_TYPE, 24);
  EXPECT_EQ(v8::internal::JS_OBJECT_TYPE, map->instance_type());
  EXPECT_EQ(24, map->instance_size());
  EXPECT_EQ(0, map->GetInObjectProperties());
  v8::internal::Handle<v8::internal::JSObject> js_object = factory->NewJSObjectFromMap(map);
  EXPECT_TRUE(js_object->HasFastProperties());
}
