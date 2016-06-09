#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "libplatform/libplatform.h"
#include "v8.h"

using namespace v8;

int age = 41;

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
    public:
        virtual void* Allocate(size_t length) {
            void* data = AllocateUninitialized(length);
            return data == NULL ? data : memset(data, 0, length);
        }
        virtual void* AllocateUninitialized(size_t length) {
            return malloc(length);
        }
        virtual void Free(void* data, size_t) {
            free(data);
        }
};

void doit(const FunctionCallbackInfo<Value>& args) {
    String::Utf8Value str(args[0]);
    printf("doit argument = %s...\n", *str);
    args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), "done", NewStringType::kNormal).ToLocalChecked());
}

void ageGetter(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    info.GetReturnValue().Set(age);
}

void ageSetter(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    age = value->Int32Value();
}

void propertyListener(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    String::Utf8Value utf8_value(name);
    std::string key = std::string(*utf8_value);
    printf("ageListener called for nam %s.\n", key.c_str());
}

int main(int argc, char* argv[]) {
    // International Components for Unicode (ICU) deals with internationalization (i18n)
    // ICU provides support locale-sensitve string comparisons, date/time/number/currency formatting
    // etc. There is an optional API called ECMAScript 402 which V8 suppports and which is enabled by
    // default.
    // https://github.com/v8/v8/wiki/i18n-support says that even if your application does not use ICU
    // you still need to call InitializeICU.
    V8::InitializeICU();
    
    // Now this is where the files 'natives_blob.bin' and snapshot_blob.bin' come into play. But what
    // are these bin files?
    // JavaScript specifies a lot of built-in functionality which every V8 context must provide.
    // For example, you can run Math.PI and that will work in a JavaScript console/repl. The global object
    // and all the built-in functionality must be setup and initialized into the V8 heap. This can be time
    // consuming and affect runtime performance if this has to be done every time. The blobs above are prepared
    // snapshots that get directly deserialized into the heap to provide an initilized context.
    V8::InitializeExternalStartupData(argv[0]);

    // set up thread pool etc.
    Platform* platform = platform::CreateDefaultPlatform();
    // justs sets the platform created above.
    V8::InitializePlatform(platform);
    V8::Initialize();

    ArrayBufferAllocator allocator;
    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = &allocator;
    // An Isolate is an independant copy of the V8 runtime which includes its own heap.
    // Two different Isolates can run in parallel and can be seen as entierly different
    // sandboxed instances of a V8 runtime.
    Isolate* isolate = Isolate::New(create_params);
    {
        // Will set the scope using Isolate::Scope whose constructor will call
        // isolate->Enter() and its destructor isolate->Exit()
        // I think this pattern is called "Resource Acquisition Is Initialisation" (RAII),
        // R, A double I for the cool kids. The resouce allocation is done by the constructor,
        // and the release by the descructor when this instance goes out of scope.
        Isolate::Scope isolate_scope(isolate);
        // Create a stack-allocated handle scope.
        // A container for handles. Instead of having to manage individual handles (like deleting) them
        // you can simply delete the handle scope.
        HandleScope handle_scope(isolate);

        // Create a JavaScript template object allowing the object (in this case a function which is
        // also an object in JavaScript remember).
        Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
        // associate 'doit' with the doit function, allowing JavaScript to call it.
        global->Set(String::NewFromUtf8(isolate, "doit", NewStringType::kNormal).ToLocalChecked(),
                FunctionTemplate::New(isolate, doit));
        // make 'age' available to JavaScript
        global->SetAccessor(String::NewFromUtf8(isolate, "age", NewStringType::kNormal).ToLocalChecked(),
                ageGetter,
                ageSetter);
        // set a named property interceptor
        global->SetNamedPropertyHandler(propertyListener);

        // Inside an instance of V8 (an Isolate) you can have multiple unrelated JavaScript applications
        // running. JavaScript has global level stuff, and one application should not mess things up for
        // another running application. Context allow for each application not step on each others toes.
        Local<Context> context = Context::New(isolate, NULL, global);
        // a Local<SomeType> is held on the stack, and accociated with a handle scope. When the handle
        // scope is deleted the GC can deallocate the objects.

        // Enter the context for compiling and running the script.
        Context::Scope context_scope(context);

        // Create a string containing the JavaScript source code.
        const char *js = "age = 40; doit(age);";
        Local<String> source = String::NewFromUtf8(isolate, js, NewStringType::kNormal).ToLocalChecked();

        // Compile the source code.
        Local<Script> script = Script::Compile(context, source).ToLocalChecked();

        // Run the script to get the result.
        Local<Value> result = script->Run(context).ToLocalChecked();

        // Convert the result to an UTF8 string and print it.
        String::Utf8Value utf8(result);
        printf("%s\n", *utf8);
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete platform;
    return 0;
}
