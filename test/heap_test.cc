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

TEST_F(HeapTest, AllocateRaw) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);

  v8::internal::Heap* heap = internal_isolate->heap();
  i::CodeLargeObjectSpace lo_space(heap);
  i::AllocationResult result = lo_space.AllocateRaw(64);
  EXPECT_EQ(result.IsRetry(), false);
  i::HeapObject object = result.ToObject();
}

TEST_F(HeapTest, MemoryChunk) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);

  v8::internal::Heap* heap = internal_isolate->heap();
  i::CodeLargeObjectSpace lo_space(heap);
  i::AllocationResult result = lo_space.AllocateRaw(64);
  i::HeapObject object = result.ToObject();
  i::MemoryChunk* chunk = i::MemoryChunk::FromHeapObject(object);
  EXPECT_EQ(chunk->IsWritable(), true);
}

TEST_F(HeapTest, RegisterNewlyAllocatedCodeObject) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);

  v8::internal::Heap* heap = internal_isolate->heap();
  i::CodeLargeObjectSpace lo_space(heap);
  i::AllocationResult result = lo_space.AllocateRaw(64);
  i::HeapObject object = result.ToObject();
  i::MemoryChunk* chunk = i::MemoryChunk::FromHeapObject(object);
  i::CodeObjectRegistry* r = chunk->GetCodeObjectRegistry();

  ASSERT_DEATH(r->RegisterNewlyAllocatedCodeObject(object.address()), "");
}
