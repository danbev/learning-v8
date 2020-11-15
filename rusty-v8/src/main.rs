use rusty_v8 as v8;

fn doit(
  scope: &mut v8::HandleScope,
  args: v8::FunctionCallbackArguments,
  mut _retval: v8::ReturnValue,
) {
  let str = args.get(0).to_string(scope).unwrap().to_rust_string_lossy(scope);
  println!("doit argument = {}", str);
}

fn main() {
    println!("Example of using rusty-v8: {}", v8::V8::get_version());

    let platform = v8::new_default_platform().unwrap();
    v8::V8::initialize_platform(platform);
    v8::V8::initialize();
    
    // TODO: implement the same functionality as ../hello-world.cc
    let isolate = &mut v8::Isolate::new(v8::CreateParams::default());
    let handle_scope = &mut v8::HandleScope::new(isolate);

    let global = v8::ObjectTemplate::new(handle_scope);

    global.set(
      v8::String::new(handle_scope, "doit").unwrap().into(),
      v8::FunctionTemplate::new(handle_scope, doit).into(),
    );

    let context = v8::Context::new_from_template(handle_scope, global);
    let context_scope = &mut v8::ContextScope::new(handle_scope, context);

    let source = v8::String::new(context_scope, "const age = 40; doit(age);").unwrap();
    let script = v8::Script::compile(context_scope, source, None).unwrap();
    let result = script.run(context_scope).unwrap();
    let result = result.to_string(context_scope).unwrap();
    println!("{}", result.to_rust_string_lossy(context_scope));

    unsafe { v8::V8::dispose(); }
}
