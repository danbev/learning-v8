#ifndef TEST_V8_TEST_FIXTURE_H_
#define TEST_V8_TEST_FIXTURE_H_

#include <stdlib.h>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "src/execution/isolate-inl.h"

namespace i = v8::internal;

class V8TestFixture : public ::testing::Test {
 protected:
  static std::unique_ptr<v8::Platform> platform_;
  static std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_;
  static v8::Isolate::CreateParams create_params_;
  v8::Isolate* isolate_;

  static void SetUpTestCase() {
    v8::V8::InitializeExternalStartupData("fixture");
    platform_ = v8::platform::NewDefaultPlatform();
    allocator_.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
    create_params_.array_buffer_allocator = allocator_.get();
    v8::V8::InitializePlatform(platform_.get());
    v8::V8::Initialize();
  }

  static void TearDownTestCase() {
    v8::V8::ShutdownPlatform();
  }

  virtual void SetUp() {
    isolate_ = v8::Isolate::New(create_params_);
    asInternal(isolate_)->handle_scope_data()->Initialize();
  }

  virtual void TearDown() {
    isolate_->Dispose();
  }

  v8::internal::Isolate* asInternal(v8::Isolate* isolate) {
    return reinterpret_cast<v8::internal::Isolate*>(isolate);
  }

 private:
};

std::unique_ptr<v8::Platform> V8TestFixture::platform_;
v8::Isolate::CreateParams V8TestFixture::create_params_;
std::unique_ptr<v8::ArrayBuffer::Allocator> V8TestFixture::allocator_;

#endif  // TEST_V8_TEST_FIXTURE_H_
