#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "v8_test_fixture.h"

class PersistentTest : public V8TestFixture {
};

class Something {
 public:
  Something(v8::Isolate* isolate, v8::Local<v8::Object> obj);
  v8::Persistent<v8::Object>& persistent();
  void make_weak();

 private:
  v8::Persistent<v8::Object> persistent_handle_;
};

Something::Something(v8::Isolate* isolate,
                     v8::Local<v8::Object> obj) : persistent_handle_(isolate, obj) {
}

v8::Persistent<v8::Object>& Something::persistent() {
  return persistent_handle_;
}

void Something::make_weak() {
  persistent_handle_.SetWeak(
      this,
      [](const v8::WeakCallbackInfo<Something>& data) {
        Something* obj = data.GetParameter();
        std::cout << "in make weak callback..." << '\n';
      }, v8::WeakCallbackType::kParameter);
}

TEST_F(PersistentTest, object) {
  const v8::HandleScope handle_scope(V8TestFixture::isolate_);
  v8::Local<v8::Object> object = v8::Object::New(isolate_);
  Something s(isolate_, object);
  s.make_weak();
  //EXPECT_EQ(true, v.IsEmpty()) << "Default constructed Local should be empty";
}
