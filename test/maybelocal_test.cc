#include <iostream>
#include "gtest/gtest.h"
#include "v8_test_fixture.h"
#include "v8.h"

using namespace v8;

class MaybeLocalTest : public V8TestFixture {
};

TEST_F(MaybeLocalTest, Basic) {
  Isolate::Scope isolate_scope(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  MaybeLocal<Value> m;
  EXPECT_TRUE(m.IsEmpty());
  ASSERT_DEATH(m.ToLocalChecked(), "Fatal error");

  // the {} will use the types, MaybeLocal default constructor so this would
  // be the same as writing MaybeLocal<Value> something = MaybeLocal<Value>();
  MaybeLocal<Value> something = {};
  EXPECT_TRUE(something.IsEmpty());
  MaybeLocal<Value> something2 = MaybeLocal<Value>();
  EXPECT_TRUE(something2.IsEmpty());
}

TEST_F(MaybeLocalTest, ToLocal) {
  Isolate::Scope isolate_scope(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  Local<Number> nr = Number::New(isolate_, 18);
  MaybeLocal<Number> maybe_nr = MaybeLocal<Number>(nr);
  EXPECT_FALSE(maybe_nr.IsEmpty());

  Local<Number> nr2;
  // The following pattern can be nice to use with if statements
  // since ToLocal returns a bool if the MaybeLocal is empty.
  EXPECT_TRUE(maybe_nr.ToLocal<Number>(&nr2));
  EXPECT_TRUE(maybe_nr.ToLocal(&nr2));
  EXPECT_EQ(nr2->Value(), 18);
}

TEST_F(MaybeLocalTest, FromMaybe) {
  Isolate::Scope isolate_scope(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  Local<String> str = String::NewFromUtf8Literal(isolate_, "bajja");
  MaybeLocal<String> maybe_str = MaybeLocal<String>(str);
  Local<Value> from_local = maybe_str.FromMaybe<Value>(Local<Value>());
  EXPECT_FALSE(from_local.IsEmpty());
  String::Utf8Value value(isolate_, from_local);
  EXPECT_STREQ("bajja", *value);

  maybe_str = MaybeLocal<String>();
  from_local = maybe_str.FromMaybe<Value>(Local<Value>());
  EXPECT_TRUE(from_local.IsEmpty());
}

MaybeLocal<Value> something() {
  MaybeLocal<Object> empty; // call some function that returns
  Local<Object> obj;
  if (!empty.ToLocal(&obj)) {
    // do some error handling
  }
  return obj; // just return the value or empty.
}

TEST_F(MaybeLocalTest, ReturnEmpty) {
  Isolate::Scope isolate_scope(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  MaybeLocal<Value> maybe = something();
  EXPECT_TRUE(maybe.IsEmpty());
}
