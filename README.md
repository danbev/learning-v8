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

Now, lets take a look how we
