#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "src/roots/roots.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/execution/isolate.h"
#include "src/execution/isolate-inl.h"

using namespace v8;
namespace i = v8::internal;

class RootsTest : public V8TestFixture {};

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
