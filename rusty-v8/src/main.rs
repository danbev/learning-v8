use rusty_v8 as v8;

static mut AGE: u32 = 45;

fn doit(
  scope: &mut v8::HandleScope,
  args: v8::FunctionCallbackArguments,
  mut retval: v8::ReturnValue,
) {
  let str = args.get(0).to_string(scope).unwrap().to_rust_string_lossy(scope);
  println!("doit argument = {}", str);
  let ret_string = v8::String::new(scope, "doit...done").unwrap();
  retval.set(ret_string.into());
}

fn age_getter(scope: &mut v8::HandleScope,
              _property: v8::Local<v8::Name>,
              _args: v8::PropertyCallbackArguments,
              mut retval: v8::ReturnValue) {
    unsafe {
        println!("age_getter: current value: {}", AGE);
        retval.set(v8::Integer::new_from_unsigned(scope, AGE).into());
    }
}

fn age_setter(scope: &mut v8::HandleScope,
              _property: v8::Local<v8::Name>,
              value: v8::Local<v8::Value>,
              _info: v8::PropertyCallbackArguments) {
    let new_value = value.uint32_value(scope).unwrap();
    println!("age_setter: new_value {}", new_value);
    unsafe {
        AGE = new_value;
    }
}

fn main() {
    println!("Example of using rusty-v8: {}", v8::V8::get_version());

    let platform = v8::new_default_platform().unwrap();
    v8::V8::initialize_platform(platform);
    v8::V8::initialize();
    {
        let isolate = &mut v8::Isolate::new(v8::CreateParams::default());
        let handle_scope = &mut v8::HandleScope::new(isolate);

        let global = v8::ObjectTemplate::new(handle_scope);
        global.set(v8::String::new(handle_scope, "doit").unwrap().into(),
          v8::FunctionTemplate::new(handle_scope, doit).into(),
        );
        let age_key = v8::String::new(handle_scope, "age").unwrap();
        unsafe {
            let age_value = v8::Integer::new_from_unsigned(handle_scope, AGE);
            global.set(age_key.into(), age_value.into());
        }

        let context = v8::Context::new_from_template(handle_scope, global);
        let context_scope = &mut v8::ContextScope::new(handle_scope, context);

        context
            .global(context_scope)
            .set_accessor_with_setter(context_scope,
                                      age_key.into(),
                                      age_getter,
                                      age_setter);

        let source = v8::String::new(context_scope, "age = 25; doit(age);").unwrap();
        let script = v8::Script::compile(context_scope, source, None).unwrap();
        let result = script.run(context_scope).unwrap();
        let result = result.to_string(context_scope).unwrap();
        println!("{}", result.to_rust_string_lossy(context_scope));
    }

    unsafe {
        v8::V8::dispose();
    }
    v8::V8::shutdown_platform();
}
