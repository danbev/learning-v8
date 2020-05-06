#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects.h"
#include "src/objects/objects-inl.h"
#include "src/api/api.h"
#include "src/objects/elements-kind.h"

using namespace v8;
namespace i = v8::internal;

class PlatformTest : public V8TestFixture {
};

class SomeTask : public Task {
 public:
   SomeTask(const char* task_name) : task_name_(task_name) {};
   void Run() {
     std::cout << "Running " << task_name_ << '\n';
   }

 private:
   const char* task_name_;

};

TEST_F(PlatformTest, Jobs) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);

  Handle<v8::Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);
  auto task = std::make_unique<SomeTask>("doit");
  platform_->CallOnWorkerThread(std::move(task));
}
