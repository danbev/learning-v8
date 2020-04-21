V8_HOME ?= /home/danielbevenius/work/google/v8_src/v8
v8_build_dir = $(V8_HOME)/out/x64.release_gcc
### The following is a build using clang which seems to work better with lldb
#v8_build_dir = $(V8_HOME)/out/x64.debug
v8_buildtools_dir = $(V8_HOME)/buildtools/third_party
gtest_home = $(PWD)/deps/googletest/googletest
current_dir=$(shell pwd)

v8_include_dir = $(V8_HOME)/include
v8_src_dir = $(V8_HOME)/src
v8_gen_dir = $(v8_build_dir)/gen
v8_dylibs=-lv8 -lv8_libplatform -lv8_libbase
GTEST_FILTER ?= "*"
clang = "$(V8_HOME)/third_party/llvm-build/Release+Asserts/bin/clang"

clang_cmd=g++ -Wall -g -O0 $@.cc -o $@ -std=c++14 -Wcast-function-type \
	  -fno-exceptions -fno-rtti \
          -I$(v8_include_dir) \
          -I$(V8_HOME) \
          -I$(v8_build_dir)/gen \
          -L$(v8_build_dir) \
          $(v8_dylibs) \
          -Wl,-L$(v8_build_dir) -Wl,-lpthread

clang_test_cmd=g++ -Wall -g -O0 test/main.cc $@.cc -o $@  ./lib/gtest/libgtest-linux.a -std=c++14 \
	  -fno-exceptions -fno-rtti -Wcast-function-type -Wno-unused-variable \
	  -Wno-class-memaccess -Wno-comment -Wno-unused-but-set-variable \
	  -DV8_COMPRESS_POINTERS \
	  -DV8_INTL_SUPPORT \
          -I$(v8_include_dir) \
          -I$(V8_HOME) \
          -I$(V8_HOME)/third_party/icu/source/common/ \
          -I$(v8_build_dir)/gen \
          -L$(v8_build_dir) \
          -I./deps/googletest/googletest/include \
          $(v8_dylibs) \
          -Wl,-L$(v8_build_dir) -Wl,-L/usr/lib64 -Wl,-lstdc++ -Wl,-lpthread

clang_gtest_cmd=g++ --verbose -Wall -O0 -g -c $(gtest_home)/src/gtest-all.cc \
          -o $(gtest_home)/gtest-all.o	-std=c++14 \
	  -fno-exceptions -fno-rtti \
          -I$(gtest_home) \
          -I$(gtest_home)/include

define run_compile
g++ -Wall -g -O0 test/main.cc $(subst ", ,$1) $@.cc -o $@  ./lib/gtest/libgtest-linux.a -std=c++14 \
	  -fno-exceptions -fno-rtti -Wcast-function-type -Wno-unused-variable \
	  -Wno-class-memaccess -Wno-comment -Wno-unused-but-set-variable \
	  -DV8_INTL_SUPPORT \
          -I$(v8_include_dir) \
          -I$(V8_HOME) \
          -I$(V8_HOME)/third_party/icu/source/common/ \
          -I$(v8_build_dir)/gen \
          -L$(v8_build_dir) \
          -I./deps/googletest/googletest/include \
          $(v8_dylibs) \
          -Wl,-L$(v8_build_dir) -Wl,-L/usr/lib64 -Wl,-lstdc++ -Wl,-lpthread
endef


COMPILE_TEST = g++ -v -std=c++11 -O0 -g -I$(V8_HOME)/third_party/googletest/src/googletest/include -I$(v8_include_dir) -I$(v8_gen_dir) -I$(V8_HOME) $(v8_dylibs) -L$(v8_build_dir) -pthread  lib/gtest/libgtest.a

hello-world: hello-world.cc
	@echo "Using v8_home = $(V8_HOME)"
	$(clang_cmd)

persistent-obj: persistent-obj.cc
	$(clang_cmd)

.PHONY: gtest-compile
gtest-compile: 
	@echo "Building gtest library"
	$(clang_gtest_cmd)
	ar -rv $(PWD)/lib/gtest/libgtest-linux.a $(gtest_home)/gtest-all.o


.PHONY: run-hello
run-hello:
	@LD_LIBRARY_PATH=$(v8_build_dir)/ ./hello-world

.PHONY: gdb-hello
gdb-hello:
	@LD_LIBRARY_PATH=$(v8_build_dir)/ gdb --cd=$(v8_build_dir) --args $(current_dir)/hello-world
	

contexts: snapshot_blob.bin contexts.cc
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

ns: snapshot_blob.bin ns.cc
	@echo "Using v8_home = $(v8_include_dir)"
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

instances: snapshot_blob.bin instances.cc
	clang++ -O0 -g -fno-rtti -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

run-script: run-script.cc
	$(clang_cmd) 

exceptions: snapshot_blob.bin exceptions.cc
	clang++ -O0 -g -fno-rtti -I$(v8_include_dir) -I$(V8_HOME) $(v8_dylibs) -L$(v8_build_dir) $@.cc $(v8_src_dir)/objects-printer.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

snapshot_blob.bin:
	@cp $(v8_build_dir)/$@ .

check: test/local_test test/persistent-object_test test/maybe_test test/smi_test test/string_test test/context_test

test/local_test: test/local_test.cc
	$(clang_test_cmd)

test/persistent-object_test: test/persistent-object_test.cc
	$(clang_test_cmd)

test/maybe_test: test/maybe_test.cc
	$(clang_test_cmd)

test/maybelocal_test: test/maybelocal_test.cc
	$(clang_test_cmd)

test/smi_test: test/smi_test.cc
	$(clang_test_cmd)

test/string_test: test/string_test.cc
	$(clang_test_cmd)

test/jsobject_test: test/jsobject_test.cc
	$(clang_test_cmd)

test/ast_test: test/ast_test.cc
	$(COMPILE_TEST) -Wno-everything test/main.cc $< -o $@

test/context_test: test/context_test.cc
	$(clang_test_cmd)

test/heap_test: test/heap_test.cc
	$(clang_test_cmd)

test/map_test: test/map_test.cc
	$(call run_compile, "${v8_build_dir}/obj/v8_base_without_compiler/map.o")

test/isolate_test: test/isolate_test.cc
	$(clang_test_cmd)

test/roots_test: test/roots_test.cc
	$(clang_test_cmd)

test/builtins_test: test/builtins_test.cc
	$(call run_compile, "${v8_build_dir}/obj/v8_base_without_compiler/builtins.o ${v8_build_dir}/obj/v8_base_without_compiler/code.o ")

test/tagged_test: test/tagged_test.cc
	$(clang_test_cmd)

test/object_test: test/object_test.cc
	$(clang_test_cmd)

test/heapobject_test: test/heapobject_test.cc
	$(clang_test_cmd)

test/objectslot_test: test/objectslot_test.cc
	$(clang_test_cmd)

test/handle_test: test/handle_test.cc
	$(clang_test_cmd)

test/handlescope_test: test/handlescope_test.cc
	$(clang_test_cmd)

test/csa_test: test/csa_test.cc
	$(clang_test_cmd)

test/objecttemplate_test: test/objecttemplate_test.cc
	$(clang_test_cmd)

test/functiontemplate_test: test/functiontemplate_test.cc
	$(clang_test_cmd)

test/exceptions_test: test/exceptions_test.cc
	$(clang_test_cmd)

test/arrays_test: test/arrays_test.cc
	$(clang_test_cmd)

test/wasm_test: test/wasm_test.cc
	$(clang_test_cmd)

test/functioncallbackargs_test: test/functioncallbackargs_test.cc
	$(clang_test_cmd)

V8_TORQUE_BUILTINS_FILES=$(addprefix src/builtins/,$(notdir $(wildcard $(V8_HOME)/src/builtins/*.tq)))
V8_TORQUE_OBJECTS_FILES=$(addprefix src/objects/,$(notdir $(wildcard $(V8_HOME)/src/objects/*.tq)))
V8_TORQUE_WASM_FILES=$(addprefix src/wasm/,$(notdir $(wildcard $(V8_HOME)/src/wasm/*.tq)))
V8_TORQUE_TP_FILES=$(addprefix src/third_party/,$(notdir $(wildcard $(V8_HOME)/src/third_party/*.tq)))
V8_TORQUE_TEST_FILES=$(addprefix test/torque/,$(notdir $(wildcard $(V8_HOME)/test/torque/*.tq)))

torque-example: torque-example.tq
	@mkdir -p gen/torque-generated
	$(info Generating Torque files in gen/torque-generated)
	@cp $< $(V8_HOME)
	@$(v8_build_dir)/torque -o gen/torque-generated -v8-root $(V8_HOME) \
		$(V8_TORQUE_BUILTINS_FILES) \
		$(V8_TORQUE_OBJECTS_FILES) \
		$(V8_TORQUE_WASM_FILES) \
		$(V8_TORQUE_TP_FILES) \
		$(V8_TORQUE_TEST_FILES) \
		$<
	@rm $(V8_HOME)/$<

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
