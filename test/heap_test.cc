#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/objects/objects.h"
#include "src/objects/objects-inl.h"

using namespace v8;
namespace i = v8::internal;

class HeapTest : public V8TestFixture {
};

TEST_F(HeapTest, FastProperties) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);

  v8::internal::Heap* heap = internal_isolate->heap();
}
