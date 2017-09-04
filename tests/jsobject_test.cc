#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects.h"
#include "src/objects-inl.h"

using namespace v8;

class JSObjectTest : public V8TestFixture {
};

TEST_F(JSObjectTest, create) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  Local<v8::internal::JSObject> obj;
  EXPECT_TRUE(obj->IsObject());
  //v8::internal::Factory* factory = isolate_->factory();
  //Handle<Map> map = factory->NewMap(JS_WEAK_SET_TYPE, JSWeakSet::kSize);
  //Handle<JSObject> weakset_obj = factory->NewJSObjectFromMap(map);
  //v8::internal::Map* m = obj->map();
  /*
  EXPECT_STREQ("bajja", *value);
  EXPECT_EQ(str->Length(), 6);
  EXPECT_EQ(str->Utf8Length(), 6);
  EXPECT_EQ(str->IsOneByte(), true);
  EXPECT_EQ(str->IsExternal(), false);
  EXPECT_EQ(str->IsExternalOneByte(), false);
  */
}
