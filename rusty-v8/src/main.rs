use rusty_v8 as v8;

fn main() {
    println!("Example of using rusty-v8: {}", v8::V8::get_version());

    let platform = v8::new_default_platform().unwrap();
    v8::V8::initialize_platform(platform);
    v8::V8::initialize();
    
    // TODO: implement the same functionality as ../hello-world.cc

    unsafe { v8::V8::dispose(); }
    v8::V8::shutdown_platform();
}
