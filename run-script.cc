#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "libplatform/libplatform.h"
#include "v8.h"

using namespace v8;

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
  V8::InitializeICU();
  V8::InitializeExternalStartupData(argv[0]);

  Platform* platform = platform::CreateDefaultPlatform();
  V8::InitializePlatform(platform);
  V8::Initialize();

  ArrayBufferAllocator allocator;
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = &allocator;
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);

    Local<FunctionTemplate> function_template = FunctionTemplate::New(isolate, NewPerson);
    function_template->SetClassName(String::NewFromUtf8(isolate, "Person"));
    function_template->InstanceTemplate()->SetInternalFieldCount(1);
    function_template->InstanceTemplate()->SetAccessor(String::NewFromUtf8(isolate, "name"), GetName, nullptr);

    Local<ObjectTemplate> person_template = ObjectTemplate::New(isolate, function_template);
    person_template->SetInternalFieldCount(1);

    Local<Context> context = Context::New(isolate, NULL, person_template);
    Context::Scope context_scope(context);
    auto ignored = context->Global()->Set(context, String::NewFromUtf8(isolate, "Person", NewStringType::kNormal).ToLocalChecked(), function_template->GetFunction());
    Local<FunctionTemplate> print_template = FunctionTemplate::New(isolate, Print);
    ignored = context->Global()->Set(context, String::NewFromUtf8(isolate, "print", NewStringType::kNormal).ToLocalChecked(), print_template->GetFunction());

    Local<String> source = ReadFile(isolate, "script.js").ToLocalChecked();

    Local<Script> script = Script::Compile(context, source).ToLocalChecked();
    script->Run(context).ToLocalChecked();
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

