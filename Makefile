SHELL:=/bin/sh

Device := TLC59116
# For the arduino ide lib zip (I include both .md & .html for convenience)
ide_lib_files = $(shell \
	git ls-tree --name-only HEAD . |\
	egrep '(\.h|\.cpp)$$'; git ls-tree -r --name-only HEAD examples | egrep -v '((/\.gitignore)$$)|(\.menu$$)' \
	)
# keywords.txt VERSION README.md README.html library.properties
# $(shell /bin/ls *.h *.cpp ; find examples -type f \( -not -name '.*.menu' -a -not -name '.gitignore' \) ) 
h_files = $(shell /bin/ls $(Device)*.h)
doc_input := $(shell /bin/ls *.h | xargs -n 1 -iX echo build/Xpp; /bin/ls *.cpp)
arduino_dir := $(shell which arduino | xargs --no-run-if-empty realpath | xargs --no-run-if-empty dirname)
arduino_inc_dirs := $(arduino_dir)/hardware/arduino/cores/arduino,$(arduino_dir)/hardware/tools/avr/lib/avr/include,$(arduino_dir)/hardware/arduino/variants/standard,$(arduino_dir)/libraries/*
ino_link := $(shell basename `/bin/pwd`).ino
test_example_srcs := $(shell find examples/test_features -name '*.h' -o -name '*.cpp' | xargs basename )
branch := $(shell git symbolic-ref --short HEAD)
released_tag := $(shell git describe --abbrev=0 master)
# can fail, if not the assumed tag & dev-branch named vn.n.n
last_released_branch := $(shell git describe --abbrev=0 master | sed 's/\.[0-9]\+$$//')
dev_branch := $(shell git describe --abbrev=0 master | sed 's/\.[0-9]\+$$//')

# Policy: Release from "master" branch. Indicate this working dir is the release with: touch .master-is-release
# So, on "master", there are only make-targets for release activities,
# And on non-master, building-type acivities.
ifeq ($(shell test '$(branch)' = 'master' && test "$$MASTEROVERRIDE" = '' && test -e .master-is-release && echo 1),1)

# inhibit := $(shell echo "release-tasks only for master branch. if you know what you are doing, use: env MASTEROVERRIDE=1 ..." > /dev/stderr)
include build_src/release.mk

else

include build_src/build.mk

endif

# makes this directory work as an .ino for testing
.PHONY : inoify
inoify : $(ino_link) $(test_example_srcs)
$(ino_link) : $(shell /bin/ls -1 examples/test_features/*.ino)
	rm $@
	ln -s $< $@

# Launch ide
.PHONY : ide
ide : log lib_dir_link
	test -e lib_dir_link/tlc59116 && rm lib_dir_link/tlc59116 || true
	arduino `pwd`/*.ino > log/ide.log 2>&1 &

.PHONY : ide_lib_files
# Just to see the list of lib_files
ide_lib_files :
	@echo $(ide_lib_files)

log :
	mkdir -p log

# echo the preprocessed file (depends on the log file)
.PHONY : preproc
preproc :
	@perl -n -e '/avr-g\+\+/ && /-o (\/tmp\/build.+)\.o/ && do {print("$$1\n"); exit(0)};' log/ide.log

# The always-run-it flag
FORCE :

.PHONY : working_dir_clean
working_dir_clean :
	@ (git diff --quiet && git diff --cached --quiet) || (echo "Working dir un-clean"; exit 1)

