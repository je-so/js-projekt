# (c) 2009-2011 Jörg Seebohn

# global Makefile building all subprojects

# list of targets
.PHONY: all clean distclean makefiles html
# build modes
.PHONY: Debug Release
# sub projects
.PHONY: genmake preprocess resources testchildprocess textrescompiler textdb test
# release versions of some sub projects
.PHONY: genmake_Release testchildprocess_Release textrescompiler_Release textdb_Release test_Release
# debug versions of some sub projects
.PHONY: genmake_Debug testchildprocess_Debug textrescompiler_Debug textdb_Debug test_Debug


# options

# submake flags
MAKEFLAGS=--no-print-directory
# location of project makefiles
MAKEFILES_PREFIX=projekte/makefiles/Makefile.


#
#
#

all: Debug Release


clean:
	if [ -d bin ]; then rm -r bin; fi

distclean:
	@echo DISTCLEAN
	@if [ -d html ]; then rm -r html; fi
	@if [ -d bin ]; then rm -r bin; fi

html:
	@if [ ! -d html ]; then mkdir html; fi
	@naturaldocs -r -hl all --charset UTF-8 -i C-kern/ -o HTML html -p projekte/naturaldocs/
	@link_dirs=`find html/files -type d` ; for i in $$link_dirs ; do \
	   count="$${i}/" ; target="" ; \
	   while [ "$${count#*/}" != "$${count}" ]; do target="../$${target}" ; count="$${count#*/}" ; done ; \
	   target="$${target}C-kern/" ; \
	   if [ ! -L "$$i/C-kern" ]; then ln -s "$$target" "$$i/C-kern" ; fi ; \
	done


makefiles: \
           $(MAKEFILES_PREFIX)testchildprocess \
           $(MAKEFILES_PREFIX)genmake \
           $(MAKEFILES_PREFIX)textdb \
           $(MAKEFILES_PREFIX)textrescompiler \
           $(MAKEFILES_PREFIX)test

$(MAKEFILES_PREFIX)%: projekte/%.prj projekte/binary.gcc projekte/sharedobject.gcc | genmake_Release
	@bin/genmake $< > "$(@)"

preprocess: | textdb_Release

resources: | textrescompiler_Release

test test_Release test_Debug: | resources testchildprocess_Release

genmake preprocess resources textrescompiler testchildprocess textdb test:
	@if ! make -qf $(MAKEFILES_PREFIX)$(@) ; then make -f $(MAKEFILES_PREFIX)$(@) ; fi

genmake_Release textrescompiler_Release testchildprocess_Release textdb_Release test_Release\
genmake_Debug   textrescompiler_Debug   testchildprocess_Debug   textdb_Debug   test_Debug:
	@if ! make -qf $(MAKEFILES_PREFIX)$(subst _, ,$(@)) ; then make -f $(MAKEFILES_PREFIX)$(subst _, ,$(@)) ; fi

Release Debug: $(MAKEFILES_PREFIX)textrescompiler \
               $(MAKEFILES_PREFIX)resources \
               $(MAKEFILES_PREFIX)textdb \
               $(MAKEFILES_PREFIX)preprocess \
               $(MAKEFILES_PREFIX)testchildprocess \
               $(MAKEFILES_PREFIX)test \
               $(MAKEFILES_PREFIX)genmake
	@for mf in $^ ; do if ! make -qf $${mf} $(@) ; then make -f $${mf} $(@) ; fi ; done

