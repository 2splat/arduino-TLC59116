Device := TLC59116
lib_files := $(shell find . -type f -a -not -path './build/*' -a -not -path './_Inline/*' -a \( -name '*.cpp' -o -name '*.h' -o -name '*.ino' \) ) keywords.txt VERSION README.md README.html library.properties
arduino_dir := $(shell which arduino | xargs --no-run-if-empty realpath | xargs --no-run-if-empty dirname)
arduino_inc_dir := $(arduino_dir)/hardware
ino_link := $(shell basename `/bin/pwd`).ino
test_example_srcs := $(shell find examples/test_features -name '*.h' -o -name '*.cpp' | xargs basename )

# a=`which arduino` && b=`realpath $$a` && echo `dirname $$b`/hardware

.PHONY : all
all : build_dir README.html arduino_$(Device).zip

.PHONY : clean
clean : build_dir

.PHONY : lib_files
lib_files :
	@echo $(lib_files)

.PHONY : build_dir
build_dir :
	@mkdir -p build
	@rm -rf build/*

# makes this directory work as an .ino for testing
.PHONY : inoify
inoify : $(ino_link) $(test_example_srcs)
$(ino_link) : $(shell /bin/ls -1 examples/test_features/*.ino)
	rm $@
	ln -s $< $@

.PHONY : menu
menu :
	@cd examples && make insertmenu

$(test_example_srcs) :
	rm $@
	ln -s examples/test_features/$@

# Launch ide
.PHONY : ide
ide : log lib_dir_link
	test -e lib_dir_link/tlc59116 && rm lib_dir_link/tlc59116 || true
	arduino `pwd`/*.ino > log/ide.log 2>&1 &

log :
	mkdir -p log

# echo the preprocessed file (depends on the log file)
.PHONY : preproc
preproc :
	@perl -n -e '/avr-g\+\+/ && /-o (\/tmp\/build.+)\.o/ && do {print("$$1\n"); exit(0)};' log/ide.log

# documentation, section 1, is in .cpp
# the first /* ... */ as markdown
README.md : $(Device).h
	awk 'FNR==2,/*\// {if ($$0 != "*/") {print}}' $< | sed 's/^    //' > $@
	echo >> $@
	awk '/\/* Use:/,/*\// {if ($$0 == "*/") {next}; sub(/^\/\*/, ""); print}' $< | sed 's/^ //' >> $@

README.html : README.md
ifeq ($(shell which markdown),)
	@echo WARNING: Couldn\'t make $@ because couldn\'t find markdown command
else
	markdown README.md > README.html
endif

# Not actually phony, but remake always
.PHONY : VERSION
VERSION :
	echo -n `git branch | grep '*' | awk '{print $$2}'` '' > $@
	git log -n 1 --pretty='format:%H %ai%n' >> $@

.PHONY : zip
zip : all 

arduino_$(Device).zip : $(lib_files)  build_tools/syslibify VERSION
	rm -rf build/$(Device) 2>/dev/null || true
	rm $@ || true
	mkdir -p build/$(Device)
	@# copy
	env -u TAPE -u ARCHIVE tar c $(lib_files) | tar x -C build/$(Device)
	@# Repair #include "", to #include <>
	cd build && perl -p -i ../build_tools/syslibify `find $(Device) -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.ino'`
	@# safety
	rm $@ 2>/dev/null || true
	cd build && zip -r ../$@ $(Device)
	rm -rf build/$(Device) 2>/dev/null || true
	rm VERSION

keywords.txt : $(Device).h
ifeq ($(shell which clang),)
	@echo WARNING: Couldn\'t rebuild $@ because couldn\'t find clang command
else
	ln -s -f `pwd`/$< build/$(Device).hpp
	# clang -cc1 -ast-dump build/$(Device).hpp | perl -n build_tools/keyword_make.pm | sort > $@
	env TERM='dumb' clang-check -ast-dump build/$(Device).hpp -- | perl -n build_tools/keyword_make.pm | sort > $@
	# Ignore .h not found errors
endif

library.properties : src/library.properties
	build_tools/cutnpaste_template.pm $< version=1.5 | sed '/^#/ d' > $@
