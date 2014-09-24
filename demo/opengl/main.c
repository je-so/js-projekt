/* title: Opengl-Demo-Starter

   Starts a single demo. The demo name must be given on the command line.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: tutorial/opengl/main.c
    Implementation file <Opengl-Demo-Starter>.
*/

#include "C-kern/konfig.h"

typedef struct demo_t demo_t;

struct demo_t {
   const char *         name;
   maincontext_thread_f run;
};

/////////////////
// demo functions

int setup_opengles_demo(maincontext_t * maincontext);
int pixel_framebuffer_demo(maincontext_t * maincontext);
int point_texture_demo(maincontext_t * maincontext);

static demo_t s_demos[] = {
   { "setup_opengles", &setup_opengles_demo },
   { "pixel_framebuffer", &pixel_framebuffer_demo },
   { "point_textures", &point_texture_demo }
};

// returns either a valid index into s_demos (return value >= 0) else -1
// input should be either "number" starting from 1 or "substring_of_demo_name".
// A trailing '\n' is removed from input.
static unsigned get_demo_index(const char * input)
{
   char name[20];
   int name_len;
   {
      size_t input_len = strlen(input);
      if (input_len == 0 || input_len >= sizeof(name)) return UINT_MAX;
      input_len -= ('\n' == input[input_len-1]);
      strncpy(name, input, input_len);
      name[input_len] = 0;
      name_len = (int)input_len;
   }

   int length = -1;
   unsigned idx = UINT_MAX;
   sscanf(name, "%u%n", &idx, &length);
   -- idx; // assume input is starting from number 1

   if (length != name_len) {
      idx = UINT_MAX;
      for (unsigned i = 0; i < sizeof(s_demos)/sizeof(s_demos[0]); ++i) {
         if (strstr(s_demos[i].name, name) != 0) {
            idx = i;
            break;
         }
      }
   }

   return idx >= sizeof(s_demos)/sizeof(s_demos[0]) ? UINT_MAX : idx;
}

int main(int argc, const char* argv[])
{
   int err;
   char input[20] = {0};
   unsigned demo_index = UINT_MAX;

   if (argc > 1) {
      demo_index = get_demo_index(argv[1]);
      if (demo_index != UINT_MAX) {
         // remove parameter argv[1] from list
         -- argc;
         memmove(&argv[1], &argv[2], sizeof(argv[0]) * (unsigned)(argc-1));
      }
   }

   if (demo_index == UINT_MAX) {
      printf("OpenGL Demos\n");
      for (unsigned i = 0; i < sizeof(s_demos)/sizeof(s_demos[0]); ++i) {
         printf("%u. %s\n", i+1, s_demos[i].name);
      }
      printf("\nSelect demo (number or substring of name): ");
      if (0 != fgets(input, sizeof(input), stdin))
         demo_index = get_demo_index(input);
   }

   if (demo_index == UINT_MAX) {
      printf("\nUnknown demo\n");
      goto ONABORT;
   }

   maincontext_startparam_t startparam = maincontext_startparam_INIT(
                                             maincontext_CONSOLE, argc, argv,
                                             s_demos[demo_index].run);
   err = initstart_maincontext(&startparam);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   return 1;
}
