#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"

using namespace v8;
namespace i = v8::internal;

class SymbolTest : public V8TestFixture {
};

TEST_F(SymbolTest, New) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  Local<String> name = String::NewFromOneByte(isolate_, 
      reinterpret_cast<const uint8_t*>("something"),
      NewStringType::kNormal).ToLocalChecked();
  Local<Symbol> symbol = Symbol::New(isolate_, name);
  Local<String> value = symbol->Description().As<String>();
  String::Utf8Value utf8_value(isolate_, value);
  EXPECT_STREQ(*utf8_value, "something");
}

TEST_F(SymbolTest, WellKnown) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  Local<Symbol> symbol = Symbol::GetToPrimitive(isolate_);
  Local<String> value = symbol->Description().As<String>();
  String::Utf8Value utf8_value(isolate_, value);
  std::cout << *utf8_value << '\n';
}

TEST_F(SymbolTest, InternalSymbol) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);

  i::Isolate* i_isolate = reinterpret_cast<v8::internal::Isolate*>(isolate_);

  i::Handle<i::Symbol> symbol = i_isolate->factory()->to_primitive_symbol();
  std::cout << std::boolalpha << symbol->is_private() << '\n';
  std::cout << std::boolalpha << symbol->is_well_known_symbol() << '\n';
  std::cout << std::boolalpha << symbol->is_interesting_symbol() << '\n';
}
