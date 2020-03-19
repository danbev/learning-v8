#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects.h"
#include "src/objects/objects-inl.h"
#include "src/api/api.h"

using namespace v8;

class ObjectTemplateTest : public V8TestFixture {
};

TEST_F(ObjectTemplateTest, Scopes) {
  const HandleScope handle_scope(isolate_);
  Local<FunctionTemplate> constructor = Local<FunctionTemplate>();
  Local<ObjectTemplate> ot = ObjectTemplate::New(isolate_, constructor);

  // Add a property that all instanced created from this object template will
  // have. (Set is member function of class Template):
  const char* prop_name = "prop_name";
  Local<Name> name = String::NewFromUtf8(isolate_, prop_name, NewStringType::kNormal).ToLocalChecked();
  Local<Data> value = String::NewFromUtf8(isolate_, "value", NewStringType::kNormal).ToLocalChecked();
  ot->Set(name, value, PropertyAttribute::None);

  Handle<Context> context = Context::New(isolate_, nullptr, ot);
  MaybeLocal<Object> maybe_instance = ot->NewInstance(context);
  Local<Object> obj = maybe_instance.ToLocalChecked();
  MaybeLocal<Array> maybe_names = obj->GetPropertyNames(context);
  Local<Array> names = maybe_names.ToLocalChecked();
  EXPECT_EQ(static_cast<int>(names->Length()), 1);

  Local<Value> val = obj->GetRealNamedProperty(context, name).ToLocalChecked();
  EXPECT_TRUE(val->IsName());
  String::Utf8Value utf8_value{isolate_, val};
  EXPECT_STREQ(*utf8_value, "value");
}
