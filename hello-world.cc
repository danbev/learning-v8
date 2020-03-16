#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "libplatform/libplatform.h"
#include "v8.h"

using namespace v8;

int age = 41;

void doit(const FunctionCallbackInfo<Value>& args) {
    String::Utf8Value str(args.GetIsolate(), args[0]);
    printf("doit argument = %s...\n", *str);
    args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), "doit...done", NewStringType::kNormal).ToLocalChecked());
}

void ageGetter(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    info.GetReturnValue().Set(age);
}

void ageSetter(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    age = value->Int32Value(info.GetIsolate()->GetCurrentContext()).FromJust();
}

void propertyListener(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    String::Utf8Value utf8_value(info.GetIsolate(), name);
    std::string key = std::string(*utf8_value);
    printf("ageListener called for nam %s.\n", key.c_str());
}

int main(int argc, char* argv[]) {
    // Now this is where the files snapshot_blob.bin' comes into play. But what
    // is this bin file?
    // JavaScript specifies a lot of built-in functionality which every V8 context must provide.
    // For example, you can run Math.PI and that will work in a JavaScript console/repl. The global object
    // and all the built-in functionality must be setup and initialized into the V8 heap. This can be time
    // consuming and affect runtime performance if this has to be done every time. The blob above is prepared
    // snapshot that get directly deserialized into the heap to provide an initilized context.
    V8::InitializeExternalStartupData(argv[0]);

    // Set up thread pool etc.
    std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
    // Just sets the platform created above.
    V8::InitializePlatform(platform.get());
    V8::Initialize();

    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    // An Isolate is an independant copy of the V8 runtime which includes its own heap.
    // Two different Isolates can run in parallel and can be seen as entierly different
    // sandboxed instances of a V8 runtime.
    Isolate* isolate = Isolate::New(create_params);
    {
        // Will set the scope using Isolate::Scope whose constructor will call
        // isolate->Enter() and its destructor isolate->Exit()
        // I think this pattern is called "Resource Acquisition Is Initialisation" (RAII),
        // The resouce allocation is done by the constructor,
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
        //global->SetNamedPropertyHandler(propertyListener);

        // Inside an instance of V8 (an Isolate) you can have multiple unrelated JavaScript applications
        // running. JavaScript has global level stuff, and one application should not mess things up for
        // another running application. Context allow for each application not step on each others toes.
        Local<Context> context = Context::New(isolate, NULL, global);
        // a Local<SomeType> is held on the stack, and accociated with a handle scope. When the handle
        // scope is deleted the GC can deallocate the objects.

        // Enter the context for compiling and running the script.
        Context::Scope context_scope(context);

        // Create a string containing the JavaScript source code.
        const char* js = "const age = 40; doit(age);";
        printf("js: %s\n", js);
        Local<String> source = String::NewFromUtf8(isolate, js, NewStringType::kNormal).ToLocalChecked();

        // Compile the source code.
        Local<Script> script = Script::Compile(context, source).ToLocalChecked();

        // Run the script to get the result.
        Local<Value> result = script->Run(context).ToLocalChecked();

        // Convert the result to an UTF8 string and print it.
        String::Utf8Value utf8(isolate, result);
        printf("%s\n", *utf8);
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    return 0;
}
