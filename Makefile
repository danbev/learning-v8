V8_HOME ?= /Users/danielbevenius/work/google/javascript/v8
v8_build_dir = $(V8_HOME)/out.gn/learning
v8_include_dir = $(V8_HOME)/include
v8_src_dir = $(V8_HOME)/src
GTEST_FILTER ?= "*"

v8_dylibs = -lv8 -lv8_libbase -lv8_libplatform -licuuc -licui18n 

COMPILE_TEST = clang++ -std=c++11 -O0 -g -I`pwd`/deps/googletest/googletest/include -I$(v8_include_dir) -I$(V8_HOME) $(v8_dylibs) -L$(v8_build_dir) -pthread  lib/gtest/libgtest.a -rpath $(v8_build_dir) 

LD_LIBRARY_PATH=$(v8_build_dir)

hello-world: natives_blob.bin snapshot_blob.bin hello-world.cc
	@echo "Using v8_home = $(v8_include_dir)"
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

contexts: natives_blob.bin snapshot_blob.bin contexts.cc
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

ns: natives_blob.bin snapshot_blob.bin ns.cc
	@echo "Using v8_home = $(v8_include_dir)"
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

instances: natives_blob.bin snapshot_blob.bin instances.cc
	clang++ -O0 -g -fno-rtti -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

run-script: natives_blob.bin snapshot_blob.bin run-script.cc
	clang++ -O0 -g -I$(v8_include_dir) $(v8_dylibs) -L$(v8_build_dir) $@.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

exceptions: natives_blob.bin snapshot_blob.bin exceptions.cc
	clang++ -O0 -g -fno-rtti -I$(v8_include_dir) -I$(V8_HOME) $(v8_dylibs) -L$(v8_build_dir) $@.cc $(v8_src_dir)/objects-printer.cc -o $@ -pthread -std=c++0x -rpath $(v8_build_dir)

natives_blob.bin:
	@cp $(v8_build_dir)/$@ .

snapshot_blob.bin:
	@cp $(v8_build_dir)/$@ .

check: tests/local_test tests/persistent-object_test tests/maybe_test tests/smi_test tests/string_test tests/context_test

tests/local_test: tests/local_test.cc
	$(COMPILE_TEST) tests/main.cc $< -o $@

tests/persistent-object_test: tests/persistent-object_test.cc
	$(COMPILE_TEST) tests/main.cc $< -o $@

tests/maybe_test: tests/maybe_test.cc
	$(COMPILE_TEST) tests/main.cc $< -o $@

tests/smi_test: tests/smi_test.cc
	$(COMPILE_TEST) tests/main.cc $< -o $@

tests/string_test: tests/string_test.cc
	$(COMPILE_TEST) tests/main.cc $< -o $@

tests/jsobject_test: tests/jsobject_test.cc
	$(COMPILE_TEST) tests/main.cc $< -o $@

tests/ast_test: tests/ast_test.cc
	$(COMPILE_TEST) -Wno-everything tests/main.cc $< -o $@

tests/context_test: tests/context_test.cc
	$(COMPILE_TEST) tests/main.cc $< -o $@

list-gtests:
	./tests/smi_test --gtest_list_tests

.PHONY: clean list-gtests

clean: 
	rm -f hello-world
	rm -f instances
	rm -f run-script
	rm -rf exceptions
	rm -f natives_blob.bin
	rm -f snapshot_blob.bin
	rm -rf hello-world.dSYM
	rm -rf tests/local_test
	rm -rf tests/persistent-object_test
	rm -rf tests/maybe_test
	rm -rf tests/smi_test
	rm -rf tests/string_test
	rm -rf tests/jsobject_test
	rm -rf tests/ast_test
	rm -rf tests/context_test
