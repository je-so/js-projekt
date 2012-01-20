# (c) 2009-2012 Jörg Seebohn

# global Makefile building all subprojects

# projects with suffix _ have two builds modes: Debug and Release
PROJECTS= pp-textdb  \
          pp-textres \
          genmake_   \
          testchildprocess_ \
          textrescompiler_  \
          textdb_           \
          testunit_

# list of targets
.PHONY: all clean distclean makefiles html
# build modes
.PHONY: Debug Release
# projects
.PHONY: $(subst _,,$(PROJECTS))
# release versions of some projects
.PHONY: $(subst _,_Release,$(filter %_,$(PROJECTS)))
# debug versions of some projects
.PHONY: $(subst _,_Debug,$(filter %_,$(PROJECTS)))

# options

# submake flags
MAKEFLAGS=--no-print-directory
# location of project makefiles
MAKEFILES_PREFIX=projekte/makefiles/Makefile.
# Shell used for executing commands
SHELL=/bin/bash

#
#
#

all: Debug Release

Debug:   makefiles $(subst _,_Debug,$(PROJECTS))

Release: makefiles $(subst _,_Release,$(PROJECTS))

clean:
	if [ -d bin ]; then rm -r bin; fi

distclean:
	@echo DISTCLEAN
	@if [ -d html ]; then rm -r html; fi
	@if [ -d bin ]; then rm -r bin; fi

html:
	@if [ ! -d html ]; then mkdir html; fi
	@naturaldocs -r -hl all -i C-kern/ -o HTML html -p projekte/naturaldocs/
	@link_dirs=`find html/files -type d` ; for i in $$link_dirs ; do \
	   count="$${i}/" ; target="" ; \
	   while [ "$${count#*/}" != "$${count}" ]; do target="../$${target}" ; count="$${count#*/}" ; done ; \
	   target="$${target}C-kern/" ; \
	   if [ ! -L "$$i/C-kern" ]; then ln -s "$$target" "$$i/C-kern" ; fi ; \
	done


makefiles: $(patsubst %,$(MAKEFILES_PREFIX)%,$(subst _,,$(PROJECTS)))

$(MAKEFILES_PREFIX)%: projekte/%.prj projekte/binary.gcc projekte/sharedobject.gcc | genmake_Release
	@bin/genmake $< > "$(@)"

pp-textdb: textdb_Release

pp-textres: textrescompiler_Release

textdb textdb_Release textdb_Debug: pp-textres

testunit testunit_Release testunit_Debug: pp-textres pp-textdb testchildprocess_Release

$(subst _,,$(PROJECTS)) \
$(patsubst %,%_clean,$(subst _,,$(PROJECTS))) \
$(subst _,_Debug,$(filter %_,$(PROJECTS))) \
$(subst _,_Release,$(filter %_,$(PROJECTS))):
	@if ! make -qf $(MAKEFILES_PREFIX)$(subst _, ,$(@)) ; then echo make $(@) ; make SHELL=$(SHELL) -f $(MAKEFILES_PREFIX)$(subst _, ,$(@)) ; fi

