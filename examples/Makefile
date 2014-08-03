ino:=$(shell /bin/ls -1 *.ino | head -n 1)
menustart := \/\/ menu made by:
menuend := \/\/ end menu

# ignores default "." otherwise
PHONY : .$(ino).menu
.$(ino).menu : $(ino)
	perl -n -e '/case ('"'"'(.)'"'"'|([0-9]))\s*:\s*\/\/(.+)/i && do {print "Serial.println(F(\"$$2$$3 $$4\"));\n"}' $< > $@

PHONY : insertmenu
insertmenu : .$(ino).menu
	@grep "$(menustart)" $(ino) >/dev/null || (echo 'In $(ino), expected: $(menustart)' ; false)
	@grep "$(menuend)" $(ino) >/dev/null || (echo 'In $(ino), expected: $(menuend)' ; false)
	@ cp $(ino) $(ino).bak
	@ awk '/$(menustart)/ {print; system("cat $<")}; /$(menuend)/ {print}; /$(menustart)/,/$(menuend)/ {next}; {print}' $(ino).bak > $(ino)
	@echo Edited $(ino)

