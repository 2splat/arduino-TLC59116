ino:=$(shell /bin/ls -1 *.ino | head -n 1)

# ignores default "." otherwise
PHONY : .$(ino).menu
.$(ino).menu : $(ino)
	perl -n -e '/case ('"'"'(.)'"'"'|([0-9]))\s*:\s*\/\/(.+)/i && do {print "Serial.println(F(\"$$2$$3 $$4\"));\n"}' $< > $@
