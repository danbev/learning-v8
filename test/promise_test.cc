#include <iostream>
#include "gtest/gtest.h"
#include "v8_test_fixture.h"
#include "v8.h"
#include "src/objects/js-promise-inl.h"

using namespace v8;
namespace i = v8::internal;

class PromiseTest : public V8TestFixture {
};

TEST_F(PromiseTest, PromiseObject) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);
  i::Isolate* internal_isolate = asInternal(isolate_);
  i::Factory* factory = internal_isolate->factory();

  // Print out the states just to remember the function for doing so:
  std::cout << i::JSPromise::Status(Promise::PromiseState::kPending) << '\n';
  std::cout << i::JSPromise::Status(Promise::PromiseState::kFulfilled) << '\n';
  std::cout << i::JSPromise::Status(Promise::PromiseState::kRejected) << '\n';

  const i::Handle<i::JSPromise> promise = factory->NewJSPromise();

  print_handle(promise);

  EXPECT_EQ(promise->status(), Promise::PromiseState::kPending);
  EXPECT_FALSE(promise->has_handler());
}
