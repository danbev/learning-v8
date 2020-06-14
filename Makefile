V8_HOME ?= /home/danielbevenius/work/google/v8_src/v8

v8_build_dir := $(V8_HOME)/out/x64.release_gcc
### The following is a build using clang which seems to work better with lldb
#v8_build_dir = $(V8_HOME)/out/x64.debug

gtest_home := $(CURDIR)/deps/googletest/googletest

v8_include_dir := $(V8_HOME)/include
v8_src_dir := $(V8_HOME)/src
v8_gen_dir := $(v8_build_dir)/gen
v8_dylibs :=-lv8 -lv8_libplatform -lv8_libbase

objs := $(patsubst %.cc, %,  $(wildcard test/*.cc))

cxx_comp_cmd=${CXX} -Wall -g -O0 $@.cc -o $@ -std=c++14 -Wcast-function-type \
	  -fno-exceptions -fno-rtti \
	  -DV8_COMPRESS_POINTERS \
          -I$(v8_include_dir) \
          -I$(V8_HOME) \
          -I$(v8_build_dir)/gen \
          -L$(v8_build_dir) \
          $(v8_dylibs) \
          -Wl,-L$(v8_build_dir) -Wl,-rpath,$(v8_build_dir) -Wl,-lpthread

cxx_test_comp_cmd=${CXX} -Wall -g -O0 test/main.cc $@.cc -o $@  ./lib/gtest/libgtest.a -std=c++14 \
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
          -Wl,-L$(v8_build_dir) -Wl,-rpath,$(v8_build_dir) -Wl,-lstdc++ -Wl,-lpthread

cxx_gtest_comp_cmd=${CXX} --verbose -Wall -O0 -g -c $(gtest_home)/src/gtest-all.cc \
          -o $(gtest_home)/gtest-all.o	-std=c++14 \
	  -fno-exceptions -fno-rtti \
          -I$(gtest_home) \
          -I$(gtest_home)/include

define run_compile
${CXX} -Wall -g -O0 test/main.cc $(subst ", ,$1) $@.cc -o $@  ./lib/gtest/libgtest.a -std=c++14 \
	  -fno-exceptions -fno-rtti -Wcast-function-type -Wno-unused-variable \
	  -Wno-class-memaccess -Wno-comment -Wno-unused-but-set-variable \
	  -DV8_INTL_SUPPORT \
	  -DV8_COMPRESS_POINTERS \
          -I$(v8_include_dir) \
          -I$(V8_HOME) \
          -I$(V8_HOME)/third_party/icu/source/common/ \
          -I$(v8_build_dir)/gen \
          -L$(v8_build_dir) \
          -I./deps/googletest/googletest/include \
          $(v8_dylibs) \
          -Wl,-L$(v8_build_dir) -Wl,-L/usr/lib64 -Wl,-lstdc++ -Wl,-lpthread
endef


hello-world: hello-world.cc
	@echo "Using v8_home = $(V8_HOME)"
	$(cxx_comp_cmd)

persistent-obj: persistent-obj.cc
	$(cxx_comp_cmd)

.PHONY: gtest-compile
gtest-compile: 
	@echo "Building gtest library"
	$(cxx_gtest_comp_cmd)
	@mkdir $(CURDIR)/lib/gtest
	ar -rv $(CURDIR)/lib/gtest/libgtest.a $(gtest_home)/gtest-all.o


.PHONY: run-hello
run-hello:
	@LD_LIBRARY_PATH=$(v8_build_dir)/ ./hello-world

.PHONY: gdb-hello
gdb-hello:
	@LD_LIBRARY_PATH=$(v8_build_dir)/ gdb --cd=$(v8_build_dir) --args $(CURDIR)/hello-world
	

contexts: snapshot_blob.bin contexts.cc
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

instances: snapshot_blob.bin instances.cc
	clang++ -O0 -g -fno-rtti -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

run-script: run-script.cc
	$(cxx_comp_cmd) 

exceptions: snapshot_blob.bin exceptions.cc
	clang++ -O0 -g -fno-rtti -I$(v8_include_dir) -I$(V8_HOME) $(v8_dylibs) -L$(v8_build_dir) $@.cc $(v8_src_dir)/objects-printer.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

snapshot_blob.bin:
	@cp $(v8_build_dir)/$@ .

test/%: test/%.cc
	$(cxx_test_comp_cmd)

test/map_test:
	$(call run_compile, "${v8_build_dir}/obj/v8_base_without_compiler/map.o")

test/builtins_test:
	$(call run_compile, "${v8_build_dir}/obj/v8_base_without_compiler/builtins.o ${v8_build_dir}/obj/v8_base_without_compiler/code.o ")

V8_TORQUE_BUILTINS_FILES:=$(addprefix src/builtins/,$(notdir $(wildcard $(V8_HOME)/src/builtins/*.tq)))
V8_TORQUE_OBJECTS_FILES:=$(addprefix src/objects/,$(notdir $(wildcard $(V8_HOME)/src/objects/*.tq)))
V8_TORQUE_WASM_FILES:=$(addprefix src/wasm/,$(notdir $(wildcard $(V8_HOME)/src/wasm/*.tq)))
V8_TORQUE_TP_FILES:=$(addprefix src/third_party/,$(notdir $(wildcard $(V8_HOME)/src/third_party/*.tq)))
V8_TORQUE_TEST_FILES:=$(addprefix test/torque/,$(notdir $(wildcard $(V8_HOME)/test/torque/*.tq)))

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

.PHONY: clean

clean: 
	@${RM} $(objs) hello-world
