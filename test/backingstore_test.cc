#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects.h"
#include "src/objects/objects-inl.h"

using namespace v8;
namespace i = v8::internal;

class BackingStoreTest : public V8TestFixture {
};

void BackingStoreDeleter(void* data, size_t length, void* deleter_data) {
  std::cout << "BackingStoreDeleter"
            << "  data: " << static_cast<char*>(data) 
            << ", length: " << length
            << ", deleter_data: ";
  std::cout << deleter_data << '\n';
}

TEST_F(BackingStoreTest, GetBackingStoreWithDeleter) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);

  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  char data[] = "bajja";
  std::cout << "data: " << data << '\n';
  int data_len = sizeof(data);
  Local<ArrayBuffer> ab;
  {
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        data, data_len, BackingStoreDeleter, nullptr);
    ab = ArrayBuffer::New(isolate_, std::move(bs));
    std::cout << "ArrayBuffer create\n";
  }

  // A BackingStore is a wrapper around raw memory
  std::shared_ptr<BackingStore> backingstore = ab->GetBackingStore();
  EXPECT_EQ(static_cast<int>(backingstore->ByteLength()), 6);

  ab->Detach();
}
