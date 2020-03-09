#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/execution/isolate.h"

using namespace v8;
namespace i = v8::internal;

class IsolateTest : public V8TestFixture {};

TEST_F(IsolateTest, Members) {
  const v8::HandleScope handle_scope(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  i::Isolate* i_isolate = asInternal(isolate_);

  i::RootsTable& roots_table = i_isolate->roots_table();
  std::cout << "roots_ entries: " << roots_table.kEntriesCount << '\n';
  //EXPECT_EQ(roots_table.kEntriesCount, (const unsigned long) 606);

  i::IsolateData* data = i_isolate->isolate_data();
  EXPECT_EQ(&roots_table, &data->roots());

  EXPECT_STREQ(roots_table.name(i::RootIndex::kFreeSpaceMap), "free_space_map");

  i::ExternalReferenceTable* ext_ref_table = i_isolate->external_reference_table();
  std::cout << "external refs count: " << ext_ref_table->kBuiltinsReferenceCount << '\n';
  for (int i = 0; i < ext_ref_table->kBuiltinsReferenceCount; i++) {
    std::cout << "external refs count[" << i << "]: " << ext_ref_table->name(i) << '\n';
  }

  const StartupData* startup_data = i_isolate->snapshot_blob();
  std::cout << "isolate addresses count: " << v8::internal::kIsolateAddressCount  << '\n';
  std::cout << "handler: " << v8::internal::kHandlerAddress << '\n';
  std::cout << "c_entry_fp: " << v8::internal::kCEntryFPAddress << '\n';
  std::cout << "c_function: " << v8::internal::kCFunctionAddress << '\n';
}
