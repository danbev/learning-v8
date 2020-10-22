V8_HOME ?= /home/danielbevenius/work/google/v8_src/v8

v8_out_dir := learning_v8
v8_build_dir := $(V8_HOME)/out/$(v8_out_dir)
v8_include_dir := $(V8_HOME)/include
v8_src_dir := $(V8_HOME)/src
v8_gen_dir := $(v8_build_dir)/gen
v8_dylibs := -lv8 -lv8_libplatform -lv8_libbase
objs := $(filter-out test/main,$(patsubst %.cc, %, $(wildcard test/*.cc)))
gtest_home := $(CURDIR)/deps/googletest/googletest

v8_gn_args = \
  v8_monolithic=false \
  v8_static_library=false \
  use_custom_libcxx=false \
  is_component_build=true \
  treat_warnings_as_errors=false \
  is_debug=true \
  is_clang=false \
  target_cpu="x64" \
  use_goma=false \
  use_gold=false \
  goma_dir="None" \
  v8_enable_backtrace=true \
  v8_enable_disassembler=true \
  v8_enable_object_print=true \
  v8_enable_verify_heap=true \
  v8_use_external_startup_data=false \
  v8_enable_i18n_support=true \
  v8_expose_symbols=true \
  v8_enable_gdbjit=true \
  v8_optimized_debug=false \
  v8_enable_debugging_features=true \
  v8_enable_fast_torque=false \
  v8_enable_fast_mksnapshot=false \
  is_asan = false

.PHONY: configure_v8
configure_v8:
	cd $(V8_HOME) && gn gen out/$(v8_out_dir) --args='$(v8_gn_args)'

.PHONY: compile_v8
compile_v8:
	cd $(V8_HOME) && ninja -C out/$(v8_out_dir)

CXXFLAGS = -Wall -g -O0 -std=c++14 -Wcast-function-type \
	    -fno-exceptions -fno-rtti \
	    -DV8_COMPRESS_POINTERS \
            -I$(v8_include_dir) \
            -I$(V8_HOME) \
            -I$(v8_build_dir)/gen \
            -L$(v8_build_dir) \
            $(v8_dylibs) \
            -Wl,-L$(v8_build_dir) -Wl,-rpath,$(v8_build_dir) -Wl,-lpthread

hello-world: hello-world.cc
	$(CXX) ${CXXFLAGS} $@.cc -o $@

.PHONY: gtest-compile
gtest-compile: CXXFLAGS = --verbose -Wall -O0 -g -c $(gtest_home)/src/gtest-all.cc \
          -o $(gtest_home)/gtest-all.o	-std=c++14 \
	  -fno-exceptions -fno-rtti \
          -I$(gtest_home) \
          -I$(gtest_home)/include
gtest-compile: 
	@echo "Building gtest library"
	$(CXX) ${CXXFLAGS} $@.cc -o $@
	@mkdir -p $(CURDIR)/lib/gtest
	${AR} -rv $(CURDIR)/lib/gtest/libgtest.a $(gtest_home)/gtest-all.o


.PHONY: gdb-hello
gdb-hello:
	@LD_LIBRARY_PATH=$(v8_build_dir)/ gdb --cd=$(v8_build_dir) --args $(CURDIR)/hello-world
	
instances: snapshot_blob.bin instances.cc
	$(CXX) ${CXXFLAGS} $@.cc -o $@
          
run-script: run-script.cc
	$(CXX) ${CXXFLAGS} $@.cc -o $@

exceptions: snapshot_blob.bin exceptions.cc
	$(CXX) ${CXXFLAGS} $@.cc -o $@

snapshot_blob.bin: $(v8_build_dir)/$@
	@cp $(v8_build_dir)/$@ .

test/backingstore_test: CXXFLAGS += "-fsanitize=address"
test/%: CXXFLAGS += test/main.cc $@.cc -o $@ ./lib/gtest/libgtest.a \
	  -Wcast-function-type -Wno-unused-variable \
	  -Wno-class-memaccess -Wno-comment -Wno-unused-but-set-variable \
	  -DV8_INTL_SUPPORT \
	  -DDEBUG \
          -I$(V8_HOME)/third_party/icu/source/common/ \
          -I./deps/googletest/googletest/include \
          -Wl,-lstdc++

test/%: test/%.cc test/v8_test_fixture.h
	$(CXX) ${CXXFLAGS}

backingstore-asn: test/backingstore_test

test/isolate_test: obj_files:="${v8_build_dir}/obj/v8_base_without_compiler/snapshot.o"
test/map_test: obj_files:="${v8_build_dir}/obj/v8_base_without_compiler/map.o"
test/builtins_test: obj_files:="${v8_build_dir}/obj/v8_base_without_compiler/builtins.o ${v8_build_dir}/obj/v8_base_without_compiler/code.o"
test/map_test test/builtins_test test/isolate_test: test/map_test.cc test/builtins_test.cc test/isolate_test.cc
	${CXX} -Wall -g -O0 test/main.cc $(subst ", ,${obj_files}) $@.cc -o $@ \
	./lib/gtest/libgtest.a -std=c++14 \
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
	-Wl,-L$(v8_build_dir) -Wl,-rpath,$(v8_build_dir) -Wl,-L/usr/lib64 -Wl,-lstdc++ -Wl,-lpthread


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
	@${RM} $(V8_HOME)/$<

.PHONY: all
all: snapshot_blob.bin $(objs)

.PHONY: check-all test-all test
check-all test-all test:
	@for test in $(objs) ; do \
		"$${test}" ; \
	done

src/backing-store-org: src/backing-store-original.cc
	g++ -g -fsanitize=address -o $@ $<

src/backing-store-new: src/backing-store-new.cc
	g++ -g -fsanitize=address -o $@ $<

.PHONY: clean

clean: 
	@${RM} $(objs) hello-world
