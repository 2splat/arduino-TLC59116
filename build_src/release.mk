# use make ... dev_branch=vx.x to specify a branch, otherwise tries to figure out the last branch and uses that.

.PHONY : what_release
what_release :
	@ echo Tagged $(released_tag), will use dev-branch '$(dev_branch)'


.PHONY : release
release : dev_branch_real working_dir_clean merge_dev update_tag
	# figure out branch to merge: same as last or override with dev_branch=vx.x

.PHONY : dev_branch_real
dev_branch_real :
	if [ "`git branch --list '$(dev_branch)'`" = '' ]; then echo "No such branch: '$(dev_branch)'"; exit 1; fi
	# merge (as necessary)

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
		echo "Last tag " `git describe --long master | sed 's/-[^-]\+$$//; s/-\([0-9]\+\)/, \1 commit behind/'`
		# figure out and tag: either +1 or .0
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
	@git diff-index --quiet HEAD -- || (echo "Working dir not committed, not committing gh-pages branch" && false)	
	@if !(cd build/html && git diff-index --quiet HEAD --);then \
		(echo -n "branch: "; git branch | grep '*' | awk '{print $$2}'; git log -n 1) | (cd build/html && git commit -a -F -);\
		cd build/html; \
		git log -n 1 --oneline; \
	fi
	@ cd build/html;\
	git config --get-regexp '^remote\.github' >/dev/null || (git push github gh-pages || true)

