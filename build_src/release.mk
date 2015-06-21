# use make ... dev_branch=vx.x to specify a branch, otherwise tries to figure out the last branch and uses that.

.PHONY : what_release
what_release :
	@ echo Tagged $(released_tag), will use dev-branch '$(dev_branch)'


.PHONY : release
release : dev_branch_real working_dir_clean merge_dev update_tag

.PHONY : dev_branch_real
dev_branch_real :
	if [ "`git branch --list '$(dev_branch)'`" = '' ]; then echo "No such branch: '$(dev_branch)'"; exit 1; fi

.PHONY : merge_dev
merge_dev :
	git merge $(dev_branch)
	@ ( git diff --quiet && git diff --cached --quiet) || (echo "Working dir un-clean"; exit 1)
	# do we need to release? (any commits since last tag?)

.PHONY : update_tag
update_tag :
	if git describe --long master | sed 's/-[^-]\+$$//' | egrep '\-0$$'; then \
		echo "Already tagged at head as `git describe master`"; \
	else \
		echo "Last tag " `git describe --long master | sed 's/-[^-]\+$$//; s/-\([0-9]\+\)/, \1 commit behind/'`; \
		true 'figure out and tag: either +1 or .0'; \
		set -x; \
		if [ "$(last_released_branch)" = "$(dev_branch)" ]; then \
			newtag="`perl -e '@parts=split(/\./,"$(released_tag)"); $$parts[-1]++; print join(".",@parts),"\n";'`"; \
			git tag -a -m "Release for $$newtag" "$$newtag"; \
		else \
			newtag="$(last_released_branch).0"; \
			git tag -a -m "Release for $$newtag" "$$newtag"; \
		fi; \
	fi


.PHONY : gh-pages
# for git push pre-push hook!
gh-pages : build build/html build/html/.git/config
	# copy doc/html to build/html and push to gh-pages
	@rsync -r --links --safe-links --hard-links --perms --whole-file --delete --prune-empty-dirs --exclude '*.md5' --filter 'protect .git/' doc/html/ build/html
	@cd build/html && git status --porcelain | awk '/^\?\?/ {print $$2}' | egrep '\.(html|js|cs|map|png|css|gitignore)' | xargs --no-run-if-empty git add
	# @( git diff --quiet && git diff --cached --quiet) || (echo "Working dir not committed, not committing gh-pages branch" && false)	
	@if !(cd build/html && ( git diff --quiet && git diff --cached --quiet));then \
		(echo "version: $(released_tag)"; \
		 echo "branch: $(branch)"; \
		 git log -n 1; \
		) | (cd build/html && git commit -a -F -);\
		cd build/html; \
		git log -n 1 --oneline; \
	fi
	git checkout gh-pages && git pull gh-pages-build gh-pages
	# @ cd build/html;\
	# git config --get-regexp '^remote\.github' >/dev/null || (git push github gh-pages || true)

build/html :
	mkdir -p build/html

build/html/.git/config : build/html/.git .git/config gh-page-update
	@# copy the remote.github
	@if git config --get-regexp '^remote\.github' >/dev/null; then \
	if ! (cd build/html; git config --get-regexp '^remote\.github' >/dev/null); then \
	perl -n -e '(/^\[remote "github"/.../\[/) && push(@x,$$_); END {pop @x; print @x}' .git/config >> $@; \
	fi;fi

.PHONY : gh-page-update
gh-page-update :
	cd build/html && git fetch ../.. gh-pages 

build/html/.git :
	git clone --branch gh-pages .git build/html
	git config remote.gh-pages-build.url build/html/.git
	git config remote.gh-pages-build.fetch '+refs/heads/*:refs/remotes/gh-pages-build/*'

