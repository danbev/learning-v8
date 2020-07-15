#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"

using namespace v8;

TEST(SnapshotTest, CreateSnapshot) {
  v8::V8::SetFlagsFromString("--random_seed=42");
  std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
  V8::Initialize();
  StartupData startup_data;
  size_t index;

  std::vector<intptr_t> external_references = { reinterpret_cast<intptr_t>(nullptr)};
  Isolate* isolate = Isolate::Allocate();
  {
    v8::SnapshotCreator snapshot_creator(isolate, external_references.data());
    {
      HandleScope scope(isolate);
      snapshot_creator.SetDefaultContext(Context::New(isolate));
      Local<Context> context = Context::New(isolate);
      {
        Context::Scope context_scope(context);
        TryCatch try_catch(isolate);

        // Add the following function to the context
        const char* js = R"(function test_snapshot() { 
          return 'from test_snapshot function';
        })";
        Local<v8::String> src = String::NewFromUtf8(isolate, js).ToLocalChecked();
        ScriptOrigin origin(String::NewFromUtf8Literal(isolate, "function"));
        ScriptCompiler::Source source(src, origin);                     
        Local<v8::Script> script;
        EXPECT_TRUE(ScriptCompiler::Compile(context, &source).ToLocal(&script));
        EXPECT_FALSE(script->Run(context).ToLocalChecked().IsEmpty());
        EXPECT_FALSE(try_catch.HasCaught());
      }

      index = snapshot_creator.AddContext(context);
      std::cout << "context index: " << index << '\n';
    }
    startup_data = snapshot_creator.CreateBlob(SnapshotCreator::FunctionCodeHandling::kKeep);
    std::cout << "size of blob: " << startup_data.raw_size << '\n';

    /* Example of printing the blob:
    size_t size = static_cast<size_t>(startup_data.raw_size);
    for (size_t i = 0; i < size; i++) {
      char endchar = i != size - 1 ? ',' : '\n';
      std::cout << std::to_string(startup_data.data[i]) << endchar;
    }
    */
  }
  v8::V8::ShutdownPlatform();

  V8::InitializePlatform(platform.get());
  V8::Initialize();
  Isolate::CreateParams create_params;
  // Specify the startup_data blob created earlier.
  create_params.snapshot_blob = &startup_data;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate = Isolate::New(create_params);
  {
    HandleScope scope(isolate);
    // Create the Context from the snapshot index.
    Local<Context> context = Context::FromSnapshot(isolate, index).ToLocalChecked();
    Context::Scope context_scope(context);
    TryCatch try_catch(isolate);
    // Add JavaScript code that calls the function we added previously.
    const char* js = "test_snapshot();";
    Local<v8::String> src = String::NewFromUtf8(isolate, js).ToLocalChecked();
    ScriptOrigin origin(String::NewFromUtf8Literal(isolate, "usage"));
    ScriptCompiler::Source source(src, origin);                     
    Local<v8::Script> script;
    EXPECT_TRUE(ScriptCompiler::Compile(context, &source).ToLocal(&script));
    MaybeLocal<Value> maybe_result = script->Run(context);
    EXPECT_FALSE(maybe_result.IsEmpty());

    Local<Value> result = maybe_result.ToLocalChecked();
    String::Utf8Value utf8(isolate, result);
    EXPECT_STREQ("from test_snapshot function", *utf8);
    EXPECT_FALSE(try_catch.HasCaught());
  }
  v8::V8::ShutdownPlatform();
}
