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
    std::cout << "Created new Person(" << p->name() << ")" << std::endl;
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

        const char *js = "var user = new Person('Fletch'); user.name;";
        Local<String> source = String::NewFromUtf8(isolate, js, NewStringType::kNormal).ToLocalChecked();

        Local<Script> script = Script::Compile(context, source).ToLocalChecked();
        Local<Value> result = script->Run(context).ToLocalChecked();
        String::Utf8Value utf8(result);
        printf("Script return value: %s\n", *utf8);
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete platform;
    return 0;
}
