Title: Readme

Author:
  (c) 2010-2011 Jörg Seebohn

Web location:
  Free software project hosted at sourceforge.net:  http://js-projekt.sourceforge.net/

Goal:
  To develop a database with its own transactional programming language.
  Implementation language is C99. Currently different sub-projects
  integrated as one C-kern(el) are used to build up the
  whole system step by step.

State:
  Architecture - design phase.
  Linux-only support during development.
  Some documentation is only in German during development.
 
Codenamed Subprojects:
  ------------
  : genXkode :
  ------------
  Goal: - Generate x86 code from generic Machinecode-Mnemonics
  Idea: - Compare interpreted generic Mnemonics vs. execution of
          generated code for unittests.
  This subproject allows a later stage to translate database-language
  (Idea: extended + simplified C-code) into executeable code.
  -----------
  : Genmake :
  -----------
  DONE: - Automate builds
  Uses simple project description and generates a makefile.
  -----------------
  : rtextcompiler :
  -----------------
  DONE: - Handle multilingual text resources
          Simple resource text compiler which generates ".h files"
          from text resources (C-kern/resource/errorlog.text).
  NEEDS IMPROVEMENT: - Simple text replacement is not enough for more complex langs.
                       Introduce algorithmic syntax (C-like syntax :-) => simple code generation)
  --------
  : Thor :
  --------
  Being planned: Handling of complex data structures.
  To develop a transactional hierarchical object database
  suitable to store context information gathered during parsing.
  --------
  : ???? :
  --------
  Being planned: Allow flexible syntax of language.
  To develop a parser generator language which allows for 
  "change of syntax at runtime".

License:
  Licensed under GNU General Public License v2 (see LICENSE),
  or (at your option) any later version.

Build:
  Run 'make' in the top level directory.
  This should build all subprojects.

  The main makefile ('Makefile') calls for every subproject the corresponding 
  makefile in 'projekte/makefiles/'.

Documentation:
  The documentation is extracted from the source files by naturaldoc.
  Type 'make html' to run it. Make sure you have installed v1.5 on your system.
  Type 'firefox html/index.html' to view it (replace firefox with your favourite browser).

Dependencies: 
  Libraries:
     * libpthread
     * librt
  Tools:
     * Git      (http://git-scm.com).
     * Codelite (http://codelite.org).
     * 'Natural Docs v1.5' (www.naturaldocs.org).

Project-structure:
  LICENSE     -  GPL v2 license
  Makefile    -  Top level makefile which calls project makefiles
  README      - This readme file
  C-kern/     - Contains all C99 source code and other resources, e.g. localized text
  C-kern/konfig.h -  Global configuration file (must be included by every source module)
  C-kern/resource/coding-conventions/ -  A collection of design decisions
  projekte/   - Contains project descriptions suitable for Genmake
  projekte/makefiles/ - Contains generated makefiles
