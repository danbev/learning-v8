### Learning Google V8
The sole purpose of this project is to aid me in leaning Google's V8 JavaScript engine

## Prerequisites
You'll need to have checked out the Google V8 Sources to you local file system and build it by following 
the instructions found [here](https://developers.google.com/v8/build).

What I've been using the following target:

    make native

After this has been done you can set an environment variable named `V8_HOME` which points to the the checked
out v8 directory. For example, :

    export V8_HOME=~/work/google/javascript/v8

## Building

    make

## Running

    ./hello-world

## Cleaning

    make clean

## Debugging

    lldb hello-world
    (lldb) breatpoint set --file hello-world.cc --line 27

### Local<String>

```c++
Local<String> script_name = ...;
```
So what is script_name. Well it is an object reference that is managed by the v8 GC.
The GC needs to be able to move things (pointers around) and also track if things should be GC'd

```shell
(lldb) p script_name.IsEmpty()
(bool) $12 = false
````

A Local<T> has overloaded a number of operators, for example ->:
```shell
(lldb) p script_name->Length()
(int) $14 = 7
````
Where Length is a method on the v8 String class.

