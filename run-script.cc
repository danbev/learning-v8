#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "src/objects/objects.h"
#include "libplatform/libplatform.h"
#include "v8.h"

using namespace v8;

extern void _v8_internal_Print_Object(void* object);

v8::MaybeLocal<v8::String> ReadFile(v8::Isolate* isolate, const char* name);
void Print(const v8::FunctionCallbackInfo<v8::Value>& args);

class Person {
  private:
    char* name_;

  public:
    Person(char* name) : name_(name) {
      std::cout << "Person::Person name: " << name << '\n';
    }; 

    char* name() const { 
      std::cout << "Person::Person name(): " << name_ << '\n';
      return name_;
    }

};

void NewPerson(const FunctionCallbackInfo<Value>& args) {
    String::Utf8Value s(args.GetIsolate(), args[0]);
    char* ss = *s;
    std::cout << "NewPerson name: " << ss << '\n';
    Person* p = new Person(ss);
    HandleScope handle_scope(args.GetIsolate());
    Local<Object> obj = args.Holder();
    _v8_internal_Print_Object(* ((v8::internal::Object**) *obj));
    obj->SetAlignedPointerInInternalField(0, p);
    _v8_internal_Print_Object(* ((v8::internal::Object**) *obj));
}

void GetName(Local<String> property, const PropertyCallbackInfo<Value>& info) {
  Local<Object> obj = info.Holder();
  printf("Get name field count.... %d\n", obj->InternalFieldCount());
  _v8_internal_Print_Object(* ((v8::internal::Object**) *obj));
  void* p = obj->GetAlignedPointerFromInternalField(0);

  printf("Get name.... %p\n", p);
  Person* person = static_cast<Person*>(p);
  printf("Get name.... %p\n", person);
  const std::string value = person->name();
  printf("Get name.... %s\n", value.c_str());
  info.GetReturnValue().Set(
      String::NewFromUtf8(info.GetIsolate(), 
                          value.c_str(),
                          NewStringType::kNormal).ToLocalChecked());
}

int main(int argc, char* argv[]) {
  V8::InitializeExternalStartupData(argv[0]);

  std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);

    /*
    ObjectTemplate* ot = *global;
    v8::internal::Object** gpp = ((v8::internal::Object**)(ot));
    v8::internal::Address* addr = (v8::internal::Address*) *gpp;
    _v8_internal_Print_Object(addr);
    _v8_internal_Print_Object(* ((v8::internal::Address**) *global));
    _v8_internal_Print_Object(* ((v8::internal::Object**) *global));
    */

    Local<FunctionTemplate> function_template = FunctionTemplate::New(isolate, NewPerson);
    function_template->SetClassName(String::NewFromUtf8(isolate, "Person").ToLocalChecked());
    function_template->InstanceTemplate()->SetInternalFieldCount(1);
    function_template->InstanceTemplate()->SetAccessor(String::NewFromUtf8(isolate, "name").ToLocalChecked(), GetName, nullptr);

    Local<ObjectTemplate> person_template = ObjectTemplate::New(isolate, function_template);
    person_template->SetInternalFieldCount(1);
    person_template->SetAccessor(String::NewFromUtf8(isolate, "name").ToLocalChecked(), GetName, nullptr);

    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
    global->Set(String::NewFromUtf8(isolate, "Person", NewStringType::kNormal).ToLocalChecked(),
        function_template);
    global->Set(String::NewFromUtf8(isolate, "print", NewStringType::kNormal).ToLocalChecked(),
        FunctionTemplate::New(isolate, Print));

    Local<Context> context = Context::New(isolate, NULL, global);
    Context::Scope context_scope(context);

    //_v8_internal_Print_Object(((void*)(*global)));


    Local<String> source = ReadFile(isolate, "/home/danielbevenius/work/google/learning-v8/script.js").ToLocalChecked();
    MaybeLocal<Script> script = Script::Compile(context, source).ToLocalChecked();
    MaybeLocal<Value> result = script.ToLocalChecked()->Run(context);
  }

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  V8::Dispose();
  V8::ShutdownPlatform();
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
    v8::String::Utf8Value str(args.GetIsolate(), args[i]);
    printf("%s", *str);
  }
  printf("\n");
  fflush(stdout);
}

