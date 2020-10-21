#include <iostream>
#include "gtest/gtest.h"
#include "v8_test_fixture.h"
#include "v8.h"
#include "src/handles/handles-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/api/api-inl.h"

using namespace v8;

class EscapableHandleScopeTest : public V8TestFixture { };

MaybeLocal<Array> create_array(Isolate* isolate) {
  v8::HandleScope handle_scope(isolate);
  EXPECT_EQ(handle_scope.NumberOfHandles(isolate), 3);
  Local<Array> zero_length = Array::New(isolate);
  EXPECT_EQ(handle_scope.NumberOfHandles(isolate), 6);
  return zero_length;
}

MaybeLocal<Object> something(Isolate* isolate, bool empty) {
  EscapableHandleScope scope(isolate);
  std::cout << "something NumberOfHandles: " << scope.NumberOfHandles(isolate) << '\n';
  if (empty) 
    return scope.Escape(Local<Object>());
    //return MaybeLocal<Object>();

  MaybeLocal<Array> array = create_array(isolate);
  return scope.Escape(array.FromMaybe(Local<Object>()));
}

TEST_F(EscapableHandleScopeTest, EscapeMaybe) {
  v8::HandleScope handle_scope(isolate_);
  EXPECT_EQ(handle_scope.NumberOfHandles(isolate_), 0);
  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);
  EXPECT_EQ(handle_scope.NumberOfHandles(isolate_), 1);

  MaybeLocal<Object> empty = something(isolate_, true);
  EXPECT_TRUE(empty.IsEmpty());
  EXPECT_EQ(handle_scope.NumberOfHandles(isolate_), 2);
  std::cout << "Number of handles: " << handle_scope.NumberOfHandles(isolate_) << '\n';

  MaybeLocal<Object> maybe_object = something(isolate_, false);
  EXPECT_FALSE(maybe_object.IsEmpty());

  EXPECT_FALSE((std::is_base_of<MaybeLocal<Array>, MaybeLocal<Object>>::value));
  std::cout << std::boolalpha;
  std::cout << "Is MaybeLocal<Array> derived from MaybeLocal<Object>?: "
  << std::is_base_of<MaybeLocal<Array>, MaybeLocal<Object>>::value << '\n';
}
