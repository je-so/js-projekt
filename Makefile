# (c) 2009-2016 Jörg Seebohn

# global Makefile building all subprojects

# projects with suffix _ support the two build modes: Debug and Release
PROJECTS= pp-generrtab \
          pp-textdb   \
          pp-textres2 \
          generrtab_  \
          genmemdb_   \
          genmake_    \
          genfile_    \
          testchildprocess_ \
          testmodule_ \
          textres2compiler_ \
          textdb_     \
          demo_       \
          perftest_   \
          unittest_

# list of targets
.PHONY: all clean distclean makefiles html test
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

makefiles: $(patsubst %,$(MAKEFILES_PREFIX)%,$(subst _,,$(PROJECTS)))

html:
	@if [ ! -d html ]; then mkdir html; fi
	@naturaldocs -r -hl all -i C-kern/ -o HTML html -p projekte/naturaldocs/
	@link_dirs=`find html/files -type d` ; for i in $$link_dirs ; do \
	   count="$${i}/" ; target="" ; \
	   while [ "$${count#*/}" != "$${count}" ]; do target="../$${target}" ; count="$${count#*/}" ; done ; \
	   target="$${target}C-kern/" ; \
	   if [ ! -L "$$i/C-kern" ]; then ln -s "$$target" "$$i/C-kern" ; fi ; \
	done

test:	unittest testmodule
	bin/unittest

$(MAKEFILES_PREFIX)%: projekte/%.prj projekte/binary.gcc projekte/shared.gcc | genmake_Release
	@if [ -f "$(@)" ]; then rm "$(@)"; fi
	@bin/genmake -v?KONFIG_GRAPHICS -o "$(@)" $<

pp-generrtab: generrtab_Release

pp-textdb: textdb_Release

pp-textres2: textres2compiler_Release

textdb textdb_Release textdb_Debug: pp-textres2

unittest unittest_Release unittest_Debug: pp-textres2 pp-textdb testchildprocess_Release

$(subst _,,$(PROJECTS)) \
$(patsubst %,%_clean,$(subst _,,$(PROJECTS))) \
$(subst _,_Debug,$(filter %_,$(PROJECTS))) \
$(subst _,_Release,$(filter %_,$(PROJECTS))):
	@if ! make -qf $(MAKEFILES_PREFIX)$(subst _, ,$(@)) ; then echo make $(@) ; make SHELL=$(SHELL) -f $(MAKEFILES_PREFIX)$(subst _, ,$(@)) ; fi

## Dependencies for projekte/subsys

projekte/subsys/graphic: projekte/external-lib/freetype2

## Dependencies for projekte/*.prj

$(MAKEFILES_PREFIX)demo:  projekte/subsys/Linux-mini projekte/subsys/context-mini projekte/subsys/graphic projekte/subsys/OpenGL-EGL projekte/subsys/X11

$(MAKEFILES_PREFIX)generrtab: projekte/subsys/Linux-mini projekte/subsys/context-mini

$(MAKEFILES_PREFIX)genfile: projekte/subsys/Linux-mini projekte/subsys/context-mini

$(MAKEFILES_PREFIX)genmake: projekte/subsys/Linux-mini projekte/subsys/context-mini

$(MAKEFILES_PREFIX)genmemdb: projekte/subsys/Linux-mini projekte/subsys/context-mini

$(MAKEFILES_PREFIX)testchildprocess: projekte/subsys/Linux-mini projekte/subsys/context-mini

$(MAKEFILES_PREFIX)textdb: projekte/subsys/Linux-mini projekte/subsys/context-mini

$(MAKEFILES_PREFIX)textres2compiler: projekte/subsys/Linux-mini projekte/subsys/context-mini

$(MAKEFILES_PREFIX)perftest: projekte/subsys/Linux projekte/subsys/context

$(MAKEFILES_PREFIX)unittest: projekte/subsys/Linux projekte/subsys/X11 projekte/subsys/OpenGL-EGL projekte/subsys/graphic
