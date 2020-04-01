#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "src/objects/visitors.h"
#include "src/roots/roots.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/execution/isolate.h"
#include "src/execution/isolate-inl.h"

using namespace v8;
namespace i = v8::internal;

class RootsTest : public V8TestFixture {};

class DummyVisitor : public i::RootVisitor {
 public:
  void VisitRootPointers(i::Root root, const char* description,
                         i::FullObjectSlot start, i::FullObjectSlot end) override {}
};

TEST_F(RootsTest, visitor_roots) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  i::Isolate* i_isolate = asInternal(isolate_);

  // from src/objects/visitors.h
  int nr_of_roots = static_cast<int>(i::Root::kNumberOfRoots);
  std::cout << "Number of roots: " << nr_of_roots << '\n';
  for (int i = 0; i < nr_of_roots; i++) {
    std::cout << DummyVisitor::RootName(static_cast<i::Root>(i)) << '\n';
  }
}

TEST_F(RootsTest, list_root_index) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  i::Isolate* i_isolate = asInternal(isolate_);

  std::cout << "Number of roots: " << i::RootsTable::kEntriesCount << '\n';
  for (size_t i = 0; i < i::RootsTable::kEntriesCount; i++) {
    std::cout << i << ": " <<  i::RootsTable::name(static_cast<i::RootIndex>(i)) << '\n';
  }
}

TEST_F(RootsTest, roots) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  i::Isolate* i_isolate = asInternal(isolate_);

  const i::RootsTable& roots_table = i_isolate->roots_table();
  EXPECT_STREQ(i::RootsTable::name(i::RootIndex::kError_string), "Error_string");

  // We can get the Address of an entry in the roots array:
  i::Address const& a = roots_table[i::RootIndex::kError_string];
  // In this case I know it is a String so we can cast it to a v8::internal::String
  i::Object obj = i::Object(a);
  i::String str = i::String::unchecked_cast(obj);
  std::unique_ptr<char[]> cstr = str.ToCString();
  EXPECT_STREQ(cstr.get(), "Error");
}
