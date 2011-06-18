# (c) 2009-2011 Jörg Seebohn

# global Makefile building all subprojects

# projects with suffix _ have to builds modes: Debug and Release
PROJECTS= preprocess \
          resources  \
          genmake_   \
          testchildprocess_ \
          textrescompiler_  \
          textdb_           \
          test_

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
	@naturaldocs -r -hl all --charset UTF-8 -i C-kern/ -o HTML html -p projekte/naturaldocs/
	@link_dirs=`find html/files -type d` ; for i in $$link_dirs ; do \
	   count="$${i}/" ; target="" ; \
	   while [ "$${count#*/}" != "$${count}" ]; do target="../$${target}" ; count="$${count#*/}" ; done ; \
	   target="$${target}C-kern/" ; \
	   if [ ! -L "$$i/C-kern" ]; then ln -s "$$target" "$$i/C-kern" ; fi ; \
	done


makefiles: $(patsubst %_,$(MAKEFILES_PREFIX)%,$(filter %_,$(PROJECTS)))

$(MAKEFILES_PREFIX)%: projekte/%.prj projekte/binary.gcc projekte/sharedobject.gcc | genmake_Release
	@bin/genmake $< > "$(@)"

preprocess: textdb_Release

resources: textrescompiler_Release

thor thor_Release thor_Debug: resources preprocess testchildprocess_Release

$(subst _,,$(PROJECTS)) \
$(subst _,_Debug,$(filter %_,$(PROJECTS))) \
$(subst _,_Release,$(filter %_,$(PROJECTS))):
	@if ! make -qf $(MAKEFILES_PREFIX)$(subst _, ,$(@)) ; then make -f $(MAKEFILES_PREFIX)$(subst _, ,$(@)) ; fi

