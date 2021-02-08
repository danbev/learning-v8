# ARM64 Yarn Node 14.15.4 issue
This document contains notes related to a crash that is see on aarch64 systems
when running Node.js 14.15.4 and Yarn.

## Build issue
When building node with debug enabled the following error is generated from
mksnapshot:
```console
$ ./configure --debug && make -j8
 ...
 ln -fs out/Release/node node; fi
  touch 768c82b5c8db30aa6ca639ca252071b77b55f399.intermediate
  LD_LIBRARY_PATH=/home/sxa/node/out/Debug/lib.host:/home/sxa/node/out/Debug/lib.target:$LD_LIBRARY_PATH; export LD_LIBRARY_PATH; cd ../tools/v8_gypfiles; mkdir -p /home/sxa/node/out/Debug/obj.target/v8_snapshot/geni; "/home/sxa/node/out/Debug/mksnapshot" --turbo_instruction_scheduling "--target_os=linux" "--target_arch=arm64" --startup_src "/home/sxa/node/out/Debug/obj.target/v8_snapshot/geni/snapshot.cc" --embedded_variant Default --embedded_src "/home/sxa/node/out/Debug/obj.target/v8_snapshot/geni/embedded.S" --no-native-code-counters


#
# Fatal error in ../deps/v8/src/heap/memory-chunk.cc, line 50
# Debug check failed: kMaxRegularHeapObjectSize <= memory (131072 vs. 65536).
#
#
#
#FailureMessage Object: 0x3ffc32ff240
==== C stack trace ===============================

    /home/sxa/node/out/Debug/mksnapshot(v8::base::debug::StackTrace::StackTrace()+0x18) [0x209e480]
/bin/sh: line 1: 29000 Trace/breakpoint trap   "/home/sxa/node/out/Debug/mksnapshot" --turbo_instruction_scheduling "--target_os=linux" "--target_arch=arm64" --startup_src "/home/sxa/node/out/Debug/obj.target/v8_snapshot/geni/snapshot.cc" --embedded_variant Default --embedded_src "/home/sxa/node/out/Debug/obj.target/v8_snapshot/geni/embedded.S" --no-native-code-counters
tools/v8_gypfiles/v8_snapshot.target.mk:16: recipe for target '768c82b5c8db30aa6ca639ca252071b77b55f399.intermediate' failed
make[1]: *** [768c82b5c8db30aa6ca639ca252071b77b55f399.intermediate] Error 133
rm 768c82b5c8db30aa6ca639ca252071b77b55f399.intermediate
Makefile:104: recipe for target 'node_g' failed
make: *** [node_g] Error 2
```
This is a debug check which will not happen when doing a release build.
The following [issue](https://bugs.chromium.org/p/v8/issues/detail?id=10808)
contains a report with this same error.

Notice that the pagesize on the system above is:
```console
$ getconf PAGESIZE
65536
```

## Reproducing
```console
$ uname -m
aarch64

$ curl https://nodejs.org/dist/v14.15.4/node-v14.15.4-linux-arm64.tar.gz | tar xpfz -
$ cd node-v14.15.4-linux-arm64/                                                 
$ export PATH=$PWD/bin:$PATH                                                    

$ node --version                                                                
v14.15.4                                                                        

$ npm --version                                                                 
6.14.10                                                                         

$ mkdir tmp && cd tmp                                                           
$ npm install -g yarn                                                           
$ npm init                                                                      
$ yarn add lodash                                                               
yarn add v1.22.10                                                               
info No lockfile found.                                                         
[1/4] Resolving packages...                                                     
[2/4] Fetching packages...                                                      
[-] 0/1Segmentation fault (core dumped)
```

### Troubleshooting
To be able to do a debug build we need to apply
[patch](https://github.com/v8/v8/commit/f4376ec801e1dedfde5309b2cb50ed70f052b7e4.patch)
so that the build will pass (currently failing on mdsnapshot as mentioned above).
Then we should be able to run yarn in debugger to look at the issue there
(if it is different which it might not be and could just be the Release build
manifestation of this issue).

The above patch does not apply cleanly and needs to be manually applied for
Node.js 14.15.4 using this [patch](aarch64-node-14.15.4-pagesize.patch).

After applying that patch and retrying the above yarn command:
```console
$ cd danbev/test
$ export PATH=/home/sxx/node:$PATH
$ /home/sxa/node/deps/npm/bin/npm-cli.js init
$ /home/sxa/node/deps/npm/bin/npm-cli.js install yarn
$ ./node_modules/yarn/bin/yarn add lodash 
yarn add v1.22.10
warning ../../package.json: No license field
[1/4] Resolving packages...
[2/4] Fetching packages...
[3/4] Linking dependencies...
[4/4] Building fresh packages...
success Saved 1 new dependency.
info Direct dependencies
└─ lodash@4.17.20
info All dependencies
└─ lodash@4.17.20
Done in 4.19s.
```

[Notes about page sizes in V8](./heap.md#pagesize-in-v8) might be helpful to
try to understand this issue.

The version of V8 used in 14.15.4 is:
```console
[sxa@147 node]$ ./node -p process.versions
{
  node: '14.15.4',
  v8: '8.4.371.19-node.17',
}
```

This [pull request](https://github.com/nodejs/node/pull/37225) was opened for
this issue.
