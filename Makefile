Device := TLC59116
lib_files := $(shell find -type f -not -path './build/*' -a \( -name '*.cpp' -o -name '*.h' -o -name 'keywords.txt' -o -name '*.ino' -o -name 'README*' \) )
arduino_dir := $(shell which arduino | xargs --no-run-if-empty realpath | xargs --no-run-if-empty dirname)
arduino_inc_dir := $(arduino_dir)/hardware

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

examples/.chain_test.ino.menu : examples/chain_test.ino
	perl -n -e '/case ('"'"'([a-z])'"'"'|([0-9]))\s*:\s*\/\/(.+)/i && do {print "Serial.println(F(\"$$2$$3 $$4\"));\n"}' $< > $@

# documentation, section 1, is in .cpp
# the first /* ... */ as markdown
README.md : $(Device).cpp
	awk 'FNR==2,/*\// {if ($$0 != "*/") {print}}' $< | sed 's/^    //' > $@
	echo >> $@
	awk '/\/* Use:/,/*\// {if ($$0 == "*/") {next}; sub(/^\/\*/, ""); print}' $< | sed 's/^ //' >> $@
ifneq ($(shell which markdown),)
	markdown README.md > README.html
endif

.PHONY : zip
zip : ../arduino_$(Device).zip

../arduino_$(Device).zip : $(lib_files)
	rm -rf build/$(Device)
	mkdir -p build/$(Device)
	env -u TAPE -u ARCHIVE tar c $(lib_files) | tar x -C build/$(Device)
	rm $@ 2>/dev/null || true
	cd build && zip -r arduino_$(Device).zip $(Device)
	mv build/arduino_$(Device).zip $@

keywords.txt : $(Device).h
ifndef arduino_dir
	@echo WARNING: Couldn\'t rebuild $@ because couldn\'t find arduino command
else ifeq ($(shell which gccxml),)
	@echo WARNING: Couldn\'t rebuild $@ because couldn\'t find gccxml command
else ifeq ($(shell which xsltproc),)
	@echo WARNING: Couldn\'t rebuild $@ because couldn\'t find xsltproc command
else
	echo "-D __COMPILING_AVR_LIBC__=1" > build/$(Device).inc
	find $(arduino_inc_dir) -name '*.h' | xargs -n 1 dirname |  sort -u | awk '{print "-I"$$0}' >> build/$(Device).inc
	find $(arduino_dir)/libraries -name '*.h' | xargs -n 1 dirname |  sort -u | awk '{print "-I"$$0}' >> build/$(Device).inc
	echo $(arduino_dir)/hardware/arduino/cores/arduino/USBAPI.h >> build/$(Device).inc
	ln -s -f `pwd`/$< build/$(Device).hpp
	# oh yea, baby, 0bnnn is supposedly good in gcc 4.6, but I guess somebody lied
	perl -i -p -e 's/(0b[\d+])/oct($$1)/eg' build/$(Device).hpp
	gccxml --gccxml-gcc-options build/$(Device).inc build/$(Device).hpp -fxml=build/$(Device).h.xml
	xsltproc --stringparam help_page http://2splat.com/dandelion/reference/arduino-$(Device).html --stringparam class_name $(Device) make_keywords.xsl build/$(Device).h.xml | sort -u > $@
endif
