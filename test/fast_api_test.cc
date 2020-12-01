#include <iostream>
#include "gtest/gtest.h"
#include "v8_test_fixture.h"
#include "v8-fast-api-calls.h"

using namespace v8;

class FastApiTest : public V8TestFixture {
};

void print_nr_fast(int i) {
  std::cout << "print_nr_fast: " << i << '\n';
}

void print_nr_slow(const FunctionCallbackInfo<Value>& args) {
  int i = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
  std::cout << "print_nr_slow: " << i << '\n';
}

TEST_F(FastApiTest, Basic) {
  Isolate::Scope isolate_scope(isolate_);
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  Context::Scope context_scope(context);

  const CFunction c_func = CFunction::Make(print_nr_fast);

  Local<Function> function = FunctionTemplate::New(isolate_,
      print_nr_slow,
      Local<Value>(),
      Local<Signature>(), 0, 
      ConstructorBehavior::kThrow,
      SideEffectType::kHasSideEffect, 
      &c_func)->GetFunction(context).ToLocalChecked();

  Local<Object> recv = Object::New(isolate_);
  for (int i = 0; i < 1000; i++) {
    Local<Value> args[1] = {Number::New(isolate_, i)};
    MaybeLocal<Value> ret = function->Call(context, recv, 1, args);
  }
}

