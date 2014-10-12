Device := TLC59116
lib_files := $(shell find . -type f -a -not -path './build/*' -a -not -path './_Inline/*' -a \( -name '*.cpp' -o -name '*.h' -o -name '*.ino' \) ) keywords.txt README.md README.html
arduino_dir := $(shell which arduino | xargs --no-run-if-empty realpath | xargs --no-run-if-empty dirname)
arduino_inc_dir := $(arduino_dir)/hardware
ino_link := $(shell basename `/bin/pwd`).ino

# a=`which arduino` && b=`realpath $$a` && echo `dirname $$b`/hardware

.PHONY : all
all : build_dir README.md arduino_$(Device).zip

.PHONY : clean
clean : build_dir

.PHONY : lib_files
lib_files :
	@echo $(lib_files)

.PHONY : build_dir
build_dir :
	@mkdir -p build
	@rm -rf build/*

# makes this directory work as an .ino
.PHONY : inoify
inoify : $(ino_link)
$(ino_link) : $(shell /bin/ls -1 examples/*.ino)
	ln -s $< $@

# documentation, section 1, is in .cpp
# the first /* ... */ as markdown
README.md : $(Device).h
	awk 'FNR==2,/*\// {if ($$0 != "*/") {print}}' $< | sed 's/^    //' > $@
	echo >> $@
	awk '/\/* Use:/,/*\// {if ($$0 == "*/") {next}; sub(/^\/\*/, ""); print}' $< | sed 's/^ //' >> $@
ifneq ($(shell which markdown),)
	markdown README.md > README.html
endif

.PHONY : zip
zip : arduino_$(Device).zip

arduino_$(Device).zip : $(lib_files)
	rm -rf build/$(Device)
	mkdir -p build/$(Device)
	env -u TAPE -u ARCHIVE tar c $(lib_files) | tar x -C build/$(Device)
	rm $@ 2>/dev/null || true
	cd build && zip -r arduino_$(Device).zip $(Device)
	mv build/arduino_$(Device).zip $@

keywords.txt : $(Device).h
ifeq ($(shell which clang),)
	@echo WARNING: Couldn\'t rebuild $@ because couldn\'t find clang command
else
	ln -s -f `pwd`/$< build/$(Device).hpp
	clang -cc1 -ast-dump build/$(Device).hpp | perl -n build_tools/keyword_make.pm | sort > $@
	# Ignore .h not found errors
endif
