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

TEST_F(ObjectTemplateTest, AddProperty) {
  const HandleScope handle_scope(isolate_);
  Local<FunctionTemplate> constructor = Local<FunctionTemplate>();
  Local<ObjectTemplate> ot = ObjectTemplate::New(isolate_, constructor);

  // Add a property that all instanced created from this object template will
  // have. (Set is member function of class Template):
  const char* prop_name = "prop_name";
  const char* prop_value = "prop_value";
  Local<Name> name = String::NewFromUtf8(isolate_, prop_name, NewStringType::kNormal).ToLocalChecked();
  Local<Data> value = String::NewFromUtf8(isolate_, prop_value, NewStringType::kNormal).ToLocalChecked();
  ot->Set(name, value, PropertyAttribute::None);

  Handle<Context> context = Context::New(isolate_, nullptr, ot);
  MaybeLocal<Object> maybe_instance = ot->NewInstance(context);
  Local<Object> obj = maybe_instance.ToLocalChecked();

  // Verify that the property we added exist in the instance we created:
  MaybeLocal<Array> maybe_names = obj->GetPropertyNames(context);
  Local<Array> names = maybe_names.ToLocalChecked();
  EXPECT_EQ(static_cast<int>(names->Length()), 1);
  // If found it iteresting that Array does not have any methods except Length()
  // and thress static methods (New, New, and Cast). Since Array extends Object
  // we can use Object::Get with the index:
  Local<Value> name_from_array = names->Get(context, 0).ToLocalChecked();
  String::Utf8Value utf8_name{isolate_, name_from_array};
  EXPECT_STREQ(*utf8_name, prop_name);

  // Verify the value is correct.
  Local<Value> val = obj->GetRealNamedProperty(context, name).ToLocalChecked();
  EXPECT_TRUE(val->IsName());
  String::Utf8Value utf8_value{isolate_, val};
  EXPECT_STREQ(*utf8_value, prop_value);
}
