#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <inttypes.h>

#include "libplatform/libplatform.h"
#include "v8.h"

extern void _v8_internal_Print_Object(void* object);
using namespace v8;

Isolate* isolate;
int age = 41;

void doit(const FunctionCallbackInfo<Value>& args) {
    String::Utf8Value str(args.GetIsolate(), args[0]);
    printf("doit argument = %s...\n", *str);
    args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), "done", NewStringType::kNormal).ToLocalChecked());
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

static void OnMessage(Local<Message> message, Local<Value> error) {
  printf("OnMessage message: \n");
  printf("LineNumber: %d\n",
      message->GetLineNumber(isolate->GetCurrentContext()).FromJust());
  printf("StartPosistion: %d\n", message->GetStartPosition());
  printf("ErrorLevel: %d\n", message->ErrorLevel());
  message->PrintCurrentStackTrace(isolate, stdout);
  
  printf("\nOnMessage error: ");
  _v8_internal_Print_Object(*error);
  printf("Length: %s\n", *String::Utf8Value(isolate, error));
  printf("\n");
}

static void OnFatalError(const char* location, const char* message) {
  printf("OnFatalError...%s : %s\n", location, message);
  //exit(1);
}


int main(int argc, char* argv[]) {
  V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate = Isolate::New(create_params);
  {
    isolate->AddMessageListener(OnMessage);
    isolate->SetFatalErrorHandler(OnFatalError);

    Isolate::Scope isolate_scope(isolate);
    TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);
    HandleScope handle_scope(isolate);
    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
    Local<Context> context = Context::New(isolate, NULL, global);
    Context::Scope context_scope(context);
    const char *js = "age = ajj40";
    Local<String> source = String::NewFromUtf8(isolate, js, NewStringType::kNormal).ToLocalChecked();
    Local<Script> script = Script::Compile(context, source).ToLocalChecked();
    //Local<Value> result = script->Run(context).ToLocalChecked();
    MaybeLocal<Value> result = script->Run(context);

    if (try_catch.HasCaught()) {
      printf("Caught: %s\n", *String::Utf8Value(isolate, try_catch.Exception()));
      //Local<Value> stack = try_catch.StackTrace(context).ToLocalChecked();
      //printf("StackFrames: %d\n", *(stack)->GetFrameCount());
    }
    
    //String::Utf8Value utf8(result);
    //printf("%s\n", *utf8);
  }

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  V8::Dispose();
  V8::ShutdownPlatform();
  return 0;
}
