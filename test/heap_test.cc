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
  //i::CodeLargeObjectSpace* lo_space = new i::CodeLargeObjectSpace(heap);
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
  i::HeapObject heap_object;
  ASSERT_TRUE(result.To(&heap_object));
  i::Address base_address = i::BasicMemoryChunk::BaseAddress(heap_object.ptr());
  i::BasicMemoryChunk* b = reinterpret_cast<i::BasicMemoryChunk*>(base_address);
  i::MemoryChunk* c = static_cast<i::MemoryChunk*>(b);
  ASSERT_TRUE(c->GetCodeObjectRegistry() != nullptr);

  i::BasicMemoryChunk* basic = i::BasicMemoryChunk::FromHeapObject(heap_object);
  i::MemoryChunk* chunk = i::MemoryChunk::FromHeapObject(heap_object);
  i::CodeObjectRegistry* r = chunk->GetCodeObjectRegistry();

  ASSERT_DEATH(r->RegisterNewlyAllocatedCodeObject(heap_object.address()), "");
}

TEST_F(HeapTest, BasicMemoryChunk) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);
  i::Address base = i::kNullAddress;
  i::Address start = i::kNullAddress;
  i::Address end = i::kNullAddress;
  v8::internal::Heap* heap = internal_isolate->heap();
  i::CodeLargeObjectSpace lo_space(heap);
  i::FreeList* freelist = i::FreeList::CreateFreeList();
  /*
  i::PagedSpace owner_space(heap,
      i::AllocationSpace::NEW_SPACE, 
      i::Executability::NOT_EXECUTABLE,
      freelist);
  */
  i::BasicMemoryChunk* chunk = i::BasicMemoryChunk::Initialize(heap,
      base, 32, start, end, &lo_space, i::VirtualMemory()); 
}

TEST_F(HeapTest, Space) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);
  i::Address base = i::kNullAddress;
  i::Address start = i::kNullAddress;
  i::Address end = i::kNullAddress;
  v8::internal::Heap* heap = internal_isolate->heap();
  i::FreeList* freelist = i::FreeList::CreateFreeList();
  i::PagedSpace space(heap,
      i::AllocationSpace::NEW_SPACE, 
      i::Executability::NOT_EXECUTABLE,
      freelist);

}
