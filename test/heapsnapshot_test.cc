#include <iostream>
#include "gtest/gtest.h"
#include "v8-profiler.h"
#include "v8_test_fixture.h"

using namespace v8;

class HeapSnapshotTest : public V8TestFixture {
};


class TestActivityControl : public ActivityControl {
 public:
  ~TestActivityControl() {}
  ControlOption ReportProgressValue(int done, int total) override {
     std::cout << "ReportProgressValue, done= " << done <<
      ", total= " << total << '\n';
    return ControlOption::kContinue;
  }
};

class TestObjectNameResolver : public HeapProfiler::ObjectNameResolver {
 public:
   const char* GetName(Local<Object> object) override {
     std::cout << "GetName" << '\n';
     return "something";
   }
};

class FileOutputStream : public OutputStream {
 public:
  FileOutputStream(FILE* stream) : stream_(stream) {}

   WriteResult WriteAsciiChunk(char* data, int size) override {
     const size_t len = static_cast<size_t>(size);
     size_t off = 0;

     while (off < len && !feof(stream_) && !ferror(stream_))
       off += fwrite(data + off, 1, len - off, stream_);

     return off == len ? kContinue : kAbort;
   }

   void EndOfStream() override {}

 private:
   FILE* stream_;
};

TEST_F(HeapSnapshotTest, TakeHeapSnapshot) {
  const HandleScope handle_scope(isolate_);
  Handle<Context> context = Context::New(isolate_);
  HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  TestActivityControl activity_control;
  TestObjectNameResolver objectname_resolver;
  const HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot(&activity_control, 
      &objectname_resolver, true); 
  std::cout << "Nr of nodes: " << snapshot->GetNodesCount() << '\n';

  FILE* file; 
  file = fopen("heap_snapshot.json", "w");
  FileOutputStream json(file);
  snapshot->Serialize(&json);
  fclose(file);
}

