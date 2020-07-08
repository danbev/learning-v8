#include <iostream>
#include <cstdio>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/execution/isolate.h"
#include "src/execution/isolate-inl.h"
#include "src/api/api-inl.h"

using namespace v8;
namespace i = v8::internal;

class IsolateTest : public V8TestFixture {

};

TEST_F(IsolateTest, Members) {
  Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
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
  std::cout << "context: " << v8::internal::kContextAddress << '\n';

  i::Builtins* builtins = i_isolate->builtins();
  std::cout << "builtins count: " << builtins->builtin_count << '\n';
  i::Code code = builtins->builtin(3);
  std::cout << "Builtins::Name::kRecordWrite: " << i::Builtins::Name::kRecordWrite <<
  ", name: " << i::Builtins::name(i::Builtins::Name::kRecordWrite) << '\n';

  i::ThreadLocalTop* tlt = i_isolate->thread_local_top();
  std::cout << "thread_local_top bytes size: " << tlt->kSizeInBytes  << '\n';
  std::cout << "thread_local_top thread_id_: " << tlt->thread_id_.ToInteger()  << '\n';

  // Verify that the handler_ addresses are the same going via the Isolate
  // or the ThreadLocalTop pointer.
  i::Address* handler_address = i_isolate->handler_address();
  i::Address& handler_address2 = tlt->handler_;
  std::cout << "handler_address: " << handler_address << '\n';
  std::cout << "handler_address2: " << &handler_address2 << '\n';


  // ReadOnlyRoots
  i::ReadOnlyRoots ro_roots = i::ReadOnlyRoots(i_isolate);
  std::cout << "read only root entries: " << ro_roots.kEntriesCount << '\n';
  i::Map free_space_map = ro_roots.free_space_map();
  std::cout << "free_space_map instance size: " << free_space_map.instance_size() << '\n';
  i::Handle<i::Map> free_space_map_handle = ro_roots.free_space_map_handle();

  Local<ObjectTemplate> global = ObjectTemplate::New(isolate_);
  Local<Context> context = Context::New(isolate_, NULL, global);
  Context::Scope context_scope(context);

  // HandleScope
  i::HandleScopeData* h_data = i_isolate->handle_scope_data();

  i::Map m = ro_roots.object_template_info_map();
  i::InstanceType type = m.instance_type();
  std::cout << "m type " << type << '\n';

  // Map
  i::Address object_ptr = reinterpret_cast<i::Address>(*global);
  i::HeapObject heap_obj = i::HeapObject::FromAddress(object_ptr);
  //i::Map heap_map = heap_obj.map();
  //std::cout << "heap_map type " << heap_map.instance_type() << '\n';

  //i::ObjectSlot map_slot = heap_map.map_slot();
  //i::MapWord map_word = heap_map.map_word();

}
