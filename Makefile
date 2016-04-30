V8_HOME ?= /Users/danielbevenius/work/google/javascript/v8
v8_build_dir = $(V8_HOME)/out/native
v8_include_dir = $(V8_HOME)/include

v8_libs = $(v8_build_dir)/libv8_base.a $(v8_build_dir)/libv8_libbase.a $(v8_build_dir)/libv8_external_snapshot.a $(v8_build_dir)/libv8_libplatform.a $(v8_build_dir)/libicudata.a $(v8_build_dir)/libicuuc.a $(v8_build_dir)/libicui18n.a

hello-world: natives_blob snapshot_blob
	@echo "Using v8_home = $(v8_include_dir)"
	clang++ -I$(v8_include_dir) $(v8_libs) hello-world.cc -o hello-world -pthread -std=c++0x

natives_blob:
	cp $(v8_build_dir)/natives_blob.bin .

snapshot_blob:
	cp $(v8_build_dir)/snapshot_blob.bin . 

.PHONY: clean

clean: 
	rm -f hello-world
	rm -f natives_blob.bin
	rm -f snapshot_blob.bin
