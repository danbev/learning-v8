#ifndef TEST_V8_TEST_FIXTURE_H_
#define TEST_V8_TEST_FIXTURE_H_

#include <stdlib.h>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    return AllocateUninitialized(length);
  }

  virtual void* AllocateUninitialized(size_t length) {
    return calloc(length, 1);
  }

  virtual void Free(void* data, size_t) {
    free(data);
  }
};

class V8TestFixture : public ::testing::Test {
 protected:
  v8::Isolate::CreateParams params_;
  ArrayBufferAllocator allocator_;
  v8::Isolate* isolate_;

  ~V8TestFixture() {
    TearDown();
  }

  virtual void SetUp() {
    v8::V8::InitializeExternalStartupData("fixture");
    platform_ = v8::platform::CreateDefaultPlatform();
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
    params_.array_buffer_allocator = &allocator_;
    isolate_ = v8::Isolate::New(params_);
  }

  virtual void TearDown() {
    if (platform_ == nullptr) return;
    v8::V8::ShutdownPlatform();
    delete platform_;
    platform_ = nullptr;
  }

 private:
  v8::Platform* platform_ = nullptr;
};

#endif  // TEST_V8_TEST_FIXTURE_H_
