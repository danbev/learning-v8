#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"

using namespace v8;

extern void _v8_internal_Print_Object(void* object);

TEST(SnapshotTest, CreateSnapshot) {
  std::cout << "SnapshotTest::CreateSnapshot\n";
  v8::V8::SetFlagsFromString("--random_seed=42");
  std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
  V8::Initialize();
  StartupData startup_data;

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
        const char* js = R"(function test_snapshot() { 
          return 'from test_snaphot function';
        }
        test_snapshot();)";
        Local<v8::String> src = String::NewFromUtf8(isolate, js).ToLocalChecked();
        ScriptOrigin origin(String::NewFromUtf8Literal(isolate, "test_snapshot"));
        ScriptCompiler::Source source(src, origin);                     
        Local<v8::Script> script;
        EXPECT_TRUE(ScriptCompiler::Compile(context, &source).ToLocal(&script));
        Local<Value> result = script->Run(context).ToLocalChecked();
        //String::Utf8Value utf8(isolate, result);
        //std::cout << "result from executing script: " << *utf8 << '\n';

        EXPECT_FALSE(try_catch.HasCaught());
        //snapshot_creator.SetDefaultContext(context);
      }

      size_t index = snapshot_creator.AddContext(context);
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
  create_params.snapshot_blob = &startup_data;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate = Isolate::New(create_params);
  {
    HandleScope scope(isolate);
    Local<Context> context = Context::FromSnapshot(isolate, 0).ToLocalChecked();
    Context::Scope context_scope(context);
    TryCatch try_catch(isolate);
    const char* js = "test_snapshot();";
    Local<v8::String> src = String::NewFromUtf8(isolate, js).ToLocalChecked();
    ScriptOrigin origin(String::NewFromUtf8Literal(isolate, "test_snapshot"));
    ScriptCompiler::Source source(src, origin);                     
    Local<v8::Script> script;
    EXPECT_TRUE(ScriptCompiler::Compile(context, &source).ToLocal(&script));
    MaybeLocal<Value> res = script->Run(context);
    if (res.IsEmpty()) {
      std::cout << std::boolalpha << "was an exception thrown: " << try_catch.HasCaught() << '\n';
      Local<Object> obj = try_catch.Exception()->ToObject(context).ToLocalChecked();
      Local<v8::String> msg_key = String::NewFromUtf8(isolate, "message").ToLocalChecked();
      Local<Value> message = obj->Get(context, msg_key).ToLocalChecked();
      Local<String> str = message->ToString(context).ToLocalChecked();
      String::Utf8Value e(isolate, str);
      std::cout << "exception: " << *e << '\n';
    } else {
      Local<Value> result = res.ToLocalChecked();
      String::Utf8Value utf8(isolate, result);
      std::cout << "result from executing script: " << *utf8 << '\n';

      EXPECT_FALSE(try_catch.HasCaught());
    }
  }
  v8::V8::ShutdownPlatform();
}
