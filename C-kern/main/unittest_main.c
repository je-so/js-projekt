/*
   C-System-Layer: C-kern/main/unittest_main.c
   Copyright (C) 2010 Jörg Seebohn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/unittest.h"

int main(int argc, char* argv[])
{
   (void) argc ;
   (void) argv ;
   return run_unittest() ;
}