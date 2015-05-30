Device := TLC59116
lib_files := $(shell find . -type f -a -not -path './build/*' -a -not -path './_Inline/*' -a \( -name '*.cpp' -o -name '*.h' -o -name '*.ino' \) ) keywords.txt VERSION README.md library.properties
h_files = $(shell /bin/ls $(Device)*.h)
doc_input := $(shell /bin/ls *.h | xargs -n 1 -iX echo build/Xpp; /bin/ls *.cpp)
arduino_dir := $(shell which arduino | xargs --no-run-if-empty realpath | xargs --no-run-if-empty dirname)
arduino_inc_dirs := $(arduino_dir)/hardware/arduino/cores/arduino,$(arduino_dir)/hardware/tools/avr/lib/avr/include,$(arduino_dir)/hardware/arduino/variants/standard,$(arduino_dir)/libraries/*
ino_link := $(shell basename `/bin/pwd`).ino
test_example_srcs := $(shell find examples/test_features -name '*.h' -o -name '*.cpp' | xargs basename )
branch := $(shell git branch | grep '^\*' | awk '{print $$2}' )

# a=`which arduino` && b=`realpath $$a` && echo `dirname $$b`/hardware

.PHONY : all
all : clean doc arduino_$(Device).zip

.PHONY : clean
clean : build
	find build -path build/html -prune -o \( -print0 \) | xargs --null -s 2000 rm -rf 

.PHONY : lib_files
lib_files :
	@echo $(lib_files)

build : build/html
	@mkdir -p build

build/html :
	@mkdir -p build/html

build/%.hpp : %.h
	ln -s ../$< $@

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
.PHONY : doc
doc : doc/html/index.html arduino_$(Device)_doc.zip README.html

doc/html/index.html arduino_$(Device)_doc.zip : README.md $(doc_input) Doxyfile DoxygenLayout.xml *.h *.cpp examples/allfeatures/allfeatures.ino examples/basic_usage/basic_usage.ino
	sed -i '/^PROJECT_NUMBER/ s/= .\+/= $(branch)/' Doxyfile
	doxygen Doxyfile
	cd doc/html && git status --porcelain | awk '/^\?\?/ {print $$2}' | egrep '\.(html|js|cs|map|png|css|gitignore)' | xargs git add
	# Want the files in the archive to be html/...
	rm arduino_$(Device)_doc.zip 2>/dev/null || true
	cd doc && (cd html && git ls-tree -r --name-only HEAD) | awk '{print "html/"$$0}' | egrep '\.(html|js|cs|map|png|css)' | xargs -s 2000 zip -u ../arduino_$(Device)_doc.zip
	@ echo "--- Probably bad files in doc/html:"
	cd doc/html && find . -name .git -prune -o \( -type f -print \) | (egrep -v '(.html|.js|.cs|.map|.png|.md5|.css|.gitignore|.swp)$$' || true)

.PHONY : gh-pages
# for git push pre-push hook!
gh-pages : build build/html build/html/.git/config
	# copy doc/html to build/html and push to gh-pages
	@rsync -r --links --safe-links --hard-links --perms --whole-file --delete --prune-empty-dirs --exclude '*.md5' --filter 'protect .git/' doc/html/ build/html
	@cd build/html && git status --porcelain | awk '/^\?\?/ {print $$2}' | egrep '\.(html|js|cs|map|png|css|gitignore)' | xargs --no-run-if-empty git add
	@git diff-index --quiet HEAD -- || (echo "Working dir not committed, not committing gh-pages branch" && false)	
	@if !(cd build/html && git diff-index --quiet HEAD --);then \
		(echo -n "branch: "; git branch | grep '*' | awk '{print $$2}'; git log -n 1) | (cd build/html && git commit -a -F -);\
		cd build/html; \
		git log -n 1 --oneline; \
	fi
	@ cd build/html;\
	git config --get-regexp '^remote\.github' >/dev/null || (git push github gh-pages || true)

build/html/.git/config : build/html/.git .git/config
	@# copy the remote.github
	@if git config --get-regexp '^remote\.github' >/dev/null; then \
	if ! (cd build/html; git config --get-regexp '^remote\.github' >/dev/null); then \
	perl -n -e '(/^\[remote "github"/.../\[/) && push(@x,$$_); END {pop @x; print @x}' .git/config >> $@; \
	fi;fi

build/html/.git :
	git clone --branch gh-pages .git build/html

# Make the README.html because we need to fixup the link to the documentation:
README.html : README.md
ifeq ($(shell which markdown),)
	@echo WARNING: Couldn\'t make $@ because couldn\'t find markdown command
else
	sed 's/#BRANCH#/$(branch)/' README.md | markdown > README.html
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

library.properties : build_src/library.properties
	build_tools/cutnpaste_template.pm $< version=1.5 | sed '/^#/ d' > $@
