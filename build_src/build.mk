# Make steps for version/dev branches

.PHONY : all
all : clean doc arduino_$(Device).zip

.PHONY : clean
clean : build
	find build -path build/html -prune -o \( -print0 \) | xargs --null -s 2000 rm -rf 

.PHONY : release
release : working_dir_clean must_be_old_version
	# do the release thing for a (old) version branch
	# tag .++ @ us
	# push
	exit 1

.PHONY: must_be_old_version
must_be_old_version :
	@# this can misfire, this assumes $branch.xx as tags. imagine a screwed up tag/branch...
	x="$(released_tag)"; if [ "$${x%.*}" = "$(branch)" ]; then echo "Most recent tag ($(released_tag) -> $${x%.*}) on master is this branch ($(branch)), but this operations wants it not to be."; exit 1; fi; \
	echo $$x

build : build/html
	@mkdir -p build

build/html :
	@mkdir -p build/html

# we need .hpp for clang to recognize a file as c++ header
build/%.hpp : %.h
	ln -s ../$< $@

.PHONY : menu
menu :
	@cd examples && make insertmenu

# looks like for ino'ify?
$(test_example_srcs) :
	rm $@
	ln -s examples/test_features/$@

# documentation, section 1, is in .cpp
# the first /* ... */ as markdown
.PHONY : doc
doc : arduino_$(Device)_doc.zip README.html

.PHONY : htmldoc
# an alias, abstracts it and I don't have to repeat it everwhere
htmldoc : doc/html/index.html README.html

doc/html/index.html : README.md $(doc_input) Doxyfile DoxygenLayout.xml *.h *.cpp examples/allfeatures/allfeatures.ino examples/basic_usage_single/basic_usage_single.ino
ifeq ($(shell which doxygen),)
	@echo WARNING: Couldn\'t make $@ because couldn\'t find doxygen command
else
	sed -i '/^PROJECT_NUMBER/ s/= .\+/= $(branch)/' Doxyfile
	doxygen Doxyfile
	git status --porcelain doc/html | awk '/^\?\?/ {print $$2}' | egrep '\.(html|js|cs|map|png|css|gitignore)' | xargs --no-run-if-empty git add
	@ echo "--- Probably bad files in doc/html:"
	cd doc/html && find . -name .git -prune -o \( -type f -print \) | (egrep -v '(.html|.js|.cs|.map|.png|.md5|.css|.gitignore|.swp)$$' || true)
endif

arduino_$(Device)_doc.zip : doc/html/index.html
	# Want the files in the archive to be html/...
	rm arduino_$(Device)_doc.zip 2>/dev/null || true
	cd doc && (cd html && git ls-tree -r --name-only HEAD) | awk '{print "html/"$$0}' | egrep '\.(html|js|cs|map|png|css)' | xargs -s 2000 zip -u ../arduino_$(Device)_doc.zip

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

# Make the README.html because we want to say the version
README.html : README.md BRANCH
ifeq ($(shell which markdown),)
	@echo WARNING: Couldn\'t make $@ because couldn\'t find markdown command
else
	sed 's/#BRANCH#/$(branch)/' README.md | markdown > README.html
endif

# To treat the branch (i.e. minor-version) as a prerequisite
BRANCH : FORCE
	if [ "$(branch)" != "`cat BRANCH 2>/dev/null`" ]; then echo "$(branch)" > BRANCH; fi

# 1. To treat the actual git-commit as a prerequisite
# 2. To include the commit as a file for the .zip (it's in $lib_files)
VERSION : FORCE
	x="$(branch) `git log -n 1 --pretty='format:%H %ai%n'`"; echo $$x; \
	if [ "$$x" != "`cat VERSION 2>/dev/null`" ]; then echo "$$x" > VERSION; fi

.PHONY : zip
zip : all 

arduino_$(Device).zip : $(ide_lib_files)  build_tools/syslibify VERSION
	# Move relevant files to build/ & then make the zip, which is a "library" for the arduino ide
	# We want to do some fixup, and get the dir name right
	rm -rf build/$(Device) 2>/dev/null || true
	rm $@ || true
	mkdir -p build/$(Device)
	@# copy
	env -u TAPE -u ARCHIVE tar c $(ide_lib_files) | tar x -C build/$(Device)
	@# Repair #include "", to #include <>
	cd build && perl -p -i ../build_tools/syslibify `find $(Device) -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.ino'`
	@# safety
	rm $@ 2>/dev/null || true
	cd build && zip -r ../$@ $(Device)
	rm -rf build/$(Device) 2>/dev/null || true

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

