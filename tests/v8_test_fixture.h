#ifndef TEST_V8_TEST_FIXTURE_H_
#define TEST_V8_TEST_FIXTURE_H_

#include <stdlib.h>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"

class V8TestFixture : public ::testing::Test {
 protected:
  v8::Isolate* isolate_;

  ~V8TestFixture() {
    TearDown();
  }

  virtual void SetUp() {
    v8::V8::InitializeExternalStartupData("fixture");
    platform_ = v8::platform::CreateDefaultPlatform();
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = v8::Isolate::New(create_params);
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
