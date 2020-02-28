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

void WeakCallback(const v8::WeakCallbackInfo<Something>& data) {
  Something* obj = data.GetParameter();
  std::cout << "in make weak callback..." << '\n';
}

void WeakCallbackVoid(const v8::WeakCallbackInfo<void>& data) {
  Something* obj = reinterpret_cast<Something*>(data.GetParameter());
  //std::cout << "in make weak callback..." << '\n';
}

void Something::make_weak() {
  /*
  auto cb = [](const v8::WeakCallbackInfo<Something>& data) {
        Something* obj = data.GetParameter();
        std::cout << "in make weak callback..." << '\n';
  };
  */
  typedef typename v8::WeakCallbackInfo<Something>::Callback Something_Callback;
  Something_Callback something_callback = WeakCallback;

  typedef typename v8::WeakCallbackInfo<void>::Callback v8_Callback;
  //#if defined(__GNUC__) && !defined(__clang__)
   // #pragma GCC diagnostic push
    //#pragma GCC diagnostic ignored "-Wcast-function-type"
  //#endif
    v8_Callback cb = reinterpret_cast<v8_Callback>(WeakCallbackVoid);
    //persistent_handle_.SetWeak(this, WeakCallback, v8::WeakCallbackType::kParameter);
  //#if defined(__GNUC__) && !defined(__clang__)
    //#pragma GCC diagnostic pop
  //#endif

}

TEST_F(PersistentTest, object) {
  const v8::HandleScope handle_scope(V8TestFixture::isolate_);
  v8::Handle<v8::Context> context = v8::Context::New(isolate_,
                                         nullptr,
                                         v8::Local<v8::ObjectTemplate>());
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> object = v8::Object::New(isolate_);
  Something s(isolate_, object);
  s.make_weak();
  EXPECT_EQ(false, s.persistent().IsEmpty()) << "Default constructed Local should be empty";
}
