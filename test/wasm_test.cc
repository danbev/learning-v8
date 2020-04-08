#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/wasm/wasm-engine.h"
#include "src/execution/isolate.h"
#include "src/execution/isolate-inl.h"

using namespace v8;
namespace i = v8::internal;

class WasmTest : public V8TestFixture {};

bool module_callback(const FunctionCallbackInfo<Value>&) {
  std::cout << "wasm_module_callback...\n";
  return false;
}

TEST_F(WasmTest, engine) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  i::Isolate* i_isolate = asInternal(isolate_);

  i::wasm::WasmEngine* engine = i_isolate->wasm_engine();
  std::shared_ptr<i::wasm::WasmEngine> engine2 = i::wasm::WasmEngine::GetWasmEngine();

  i_isolate->set_wasm_module_callback(module_callback);
  //TODO: Compile and call module function
}
