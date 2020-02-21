V8_HOME ?= /home/danielbevenius/work/google/v8_src/v8
v8_build_dir = $(V8_HOME)/out/x64.release
v8_buildtools_dir = $(V8_HOME)/buildtools/third_party

v8_include_dir = $(V8_HOME)/include
v8_src_dir = $(V8_HOME)/src
v8_gen_dir = $(v8_build_dir)/gen
GTEST_FILTER ?= "*"
clang = "$(V8_HOME)/third_party/llvm-build/Release+Asserts/bin/clang"
ld = "$(V8_HOME)/third_party/binutils/Linux_x64/Release/bin/ld"

clang_cmd=$(clang)++ -g $@.cc -o $@ -std=c++14 \
	  -nostdinc++ -stdlib=libc++ \
	  -B$(V8_HOME)/third_party/binutils/Linux_x64/Release/bin \
	  -fno-exceptions -fno-rtti \
	  -isystem$(V8_HOME)/buildtools/third_party/libc++/trunk/include \
	  -isystem$(V8_HOME)/buildtools/third_party/libc++abi/trunk/include \
	  --sysroot=$(V8_HOME)/build/linux/debian_sid_amd64-sysroot \
          -I$(v8_include_dir) \
          -L$(v8_build_dir)/obj \
          $(v8_dylibs) \
          -Wl,-L$(v8_build_dir) -Wl,-lpthread

v8_dylibs=-lv8_monolith

current_dir=$(shell pwd)

COMPILE_TEST = g++ -v -std=c++11 -O0 -g -I`pwd`/deps/googletest/googletest/include -I$(v8_include_dir) -I$(v8_gen_dir) -I$(V8_HOME) $(v8_dylibs) -L$(v8_build_dir) -pthread  lib/gtest/libgtest.a -rpath $(v8_build_dir)

hello-world: hello-world.cc
	@echo "Using v8_home = $(V8_HOME)"
	$(clang_cmd)

.PHONY: run-hello
run-hello:
	@LD_LIBRARY_PATH=$(V8_HOME)/out/x64.release/ ./hello-world

.PHONY: gdb-hello
gdb-hello:
	@env LD_LIBRARY_PATH=$(v8_build_dir) gdb --cd=$(v8_build_dir) --args $(current_dir)/hello-world

contexts: snapshot_blob.bin contexts.cc
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

ns: snapshot_blob.bin ns.cc
	@echo "Using v8_home = $(v8_include_dir)"
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

instances: snapshot_blob.bin instances.cc
	clang++ -O0 -g -fno-rtti -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

run-script: snapshot_blob.bin run-script.cc
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

exceptions: snapshot_blob.bin exceptions.cc
	clang++ -O0 -g -fno-rtti -I$(v8_include_dir) -I$(V8_HOME) $(v8_dylibs) -L$(v8_build_dir) $@.cc $(v8_src_dir)/objects-printer.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

snapshot_blob.bin:
	@cp $(v8_build_dir)/$@ .

check: test/local_test test/persistent-object_test test/maybe_test test/smi_test test/string_test test/context_test

test/local_test: test/local_test.cc
	$(COMPILE_TEST) test/main.cc $< -o $@

test/persistent-object_test: test/persistent-object_test.cc
	$(COMPILE_TEST) test/main.cc $< -o $@

test/maybe_test: test/maybe_test.cc
	$(COMPILE_TEST) test/main.cc $< -o $@

test/smi_test: test/smi_test.cc
	$(COMPILE_TEST) test/main.cc $< -o $@

test/string_test: test/string_test.cc
	$(COMPILE_TEST) test/main.cc $< -o $@

test/jsobject_test: test/jsobject_test.cc
	$(COMPILE_TEST) test/main.cc $< -o $@

test/ast_test: test/ast_test.cc
	$(COMPILE_TEST) -Wno-everything test/main.cc $< -o $@

test/context_test: test/context_test.cc
	$(COMPILE_TEST) test/main.cc $< -o $@

test/heap_test: test/heap_test.cc
	$(COMPILE_TEST) test/main.cc $< -o $@

test/map_test: test/map_test.cc
	$(COMPILE_TEST) test/main.cc $< -o $@

list-gtest:
	./test/smi_test --gtest_list_test

.PHONY: clean list-gtest

clean: 
	@rm -f hello-world
	@rm -f instances
	@rm -f run-script
	@rm -rf exceptions
	@rm -f natives_blob.bin
	@rm -f snapshot_blob.bin
	@rm -rf hello-world.dSYM
	@rm -rf test/local_test
	@rm -rf test/persistent-object_test
	@rm -rf test/maybe_test
	@rm -rf test/smi_test
	@rm -rf test/string_test
	@rm -rf test/jsobject_test
	@rm -rf test/ast_test
	@rm -rf test/context_test
	@rm -rf test/map_test
