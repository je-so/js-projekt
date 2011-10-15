/* title: Task-Callback
   The main execution function of a task which is executed by
   an <exothread_t>, <osthread_t> or <process_t>.

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/aspect/callback/task.h
    Header file of <Task-Callback>.
*/
#ifndef CKERN_ASPECT_CALLBACK_TASK_HEADER
#define CKERN_ASPECT_CALLBACK_TASK_HEADER

// forward
struct callback_param_t ;

/* typedef: task_callback_t typedef
 * Shortcut for <task_callback_t>. */
typedef struct task_callback_t         task_callback_t ;

/* typedef: task_callback_f
 * Function signature of executable main function / task. */
typedef int                         (* task_callback_f) (struct callback_param_t * start_arg) ;


/* struct: task_callback_t
 * Stores callback function and user param for execution of main task. */
struct task_callback_t {
   /* variable: fct
    * The pointer to the executed main function / task. */
   task_callback_f            fct ;
   /* variable: arg
    * The pointer to the first parameter of the called back function. */
   struct callback_param_t  * arg ;
} ;

// group: lifetime

/* define: task_callback_INIT_FREEABLE
 * Static initializer. */
#define task_callback_INIT_FREEABLE    { (task_callback_f)0, (struct callback_param_t*)0 }

#endif
