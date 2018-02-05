#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "libplatform/libplatform.h"
#include "v8.h"

using namespace v8;

v8::MaybeLocal<v8::String> ReadFile(v8::Isolate* isolate, const char* name);
void Print(const v8::FunctionCallbackInfo<v8::Value>& args);

class Person {
  private:
    std::string name_;

  public:
    Person(std::string name) : name_(name) {
    }; 

    std::string name() const { 
      return name_;
    }

};

void NewPerson(const FunctionCallbackInfo<Value>& args) {
    String::Utf8Value str(args[0]);
    Person *p = new Person(*str);
    Local<Object> self = args.Holder();
    self->SetAlignedPointerInInternalField(0, p);
}

void GetName(Local<String> property, const PropertyCallbackInfo<Value>& info) {
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void* pointer = self->GetAlignedPointerFromInternalField(0);
  const std::string value = static_cast<Person*>(pointer)->name();
  info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), value.c_str(), NewStringType::kNormal).ToLocalChecked());
}

int main(int argc, char* argv[]) {
  V8::InitializeExternalStartupData(argv[0]);

  Platform* platform = platform::CreateDefaultPlatform();
  V8::InitializePlatform(platform);
  V8::Initialize();

  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);

    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);

    Local<FunctionTemplate> function_template = FunctionTemplate::New(isolate, NewPerson);
    function_template->SetClassName(String::NewFromUtf8(isolate, "Person"));
    function_template->InstanceTemplate()->SetInternalFieldCount(1);
    function_template->InstanceTemplate()->SetAccessor(String::NewFromUtf8(isolate, "name"), GetName, nullptr);

    Local<ObjectTemplate> person_template = ObjectTemplate::New(isolate, function_template);
    person_template->SetInternalFieldCount(1);

    global->Set(String::NewFromUtf8(isolate, "Person", NewStringType::kNormal).ToLocalChecked(), function_template);
    global->Set(String::NewFromUtf8(isolate, "print", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, Print));

    Local<Context> context = Context::New(isolate, NULL, global);
    Context::Scope context_scope(context);

    Local<String> source = ReadFile(isolate, "script.js").ToLocalChecked();
    MaybeLocal<Script> script = Script::Compile(context, source).ToLocalChecked();
    MaybeLocal<Value> result = script.ToLocalChecked()->Run(context);
  }

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  V8::Dispose();
  V8::ShutdownPlatform();
  delete platform;
  return 0;
}

// Reads a file into a v8 string.
v8::MaybeLocal<v8::String> ReadFile(v8::Isolate* isolate, const char* name) {
  FILE* file = fopen(name, "rb");
  if (file == NULL) {
    return v8::MaybeLocal<v8::String>();
  }

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (size_t i = 0; i < size;) {
    i += fread(&chars[i], 1, size - i, file);
    if (ferror(file)) {
      fclose(file);
      return v8::MaybeLocal<v8::String>();
    }
  }
  fclose(file);
  v8::MaybeLocal<v8::String> result = v8::String::NewFromUtf8(isolate, 
      chars, 
      v8::NewStringType::kNormal, 
      static_cast<int>(size));
  delete[] chars;
  return result;
}

void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(args.GetIsolate());
    if (first) {
      first = false;
    } else {
      printf(" ");
    }
    v8::String::Utf8Value str(args[i]);
    const char* cstr = *str;
    printf("%s", cstr);
  }
  printf("\n");
  fflush(stdout);
}

