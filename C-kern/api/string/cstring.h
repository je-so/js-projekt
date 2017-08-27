/* title: CString
   Encapsulates C string datatype and hides memory management.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/string/cstring.h
    Header file of <CString>.

   file: C-kern/string/cstring.c
    Implementation file <CString impl>.
*/
#ifndef CKERN_STRING_CSTRING_HEADER
#define CKERN_STRING_CSTRING_HEADER

// forward
struct string_t;

/* typedef: struct cstring_t
 * Export <cstring_t>. */
typedef struct cstring_t cstring_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_cstring
 * Tests <cstring_t>. */
int unittest_string_cstring(void);
#endif


/* struct: cstring_t
 * Dynamically growing C string with trailing '\0' byte. */
struct cstring_t {
   /* variable: length
    * Length of string in bytes (sizeof(char)==1). The number of characters could be smaller if the encoding is UTF-8 for example.
    *
    * Invariant:
    * (0 == <chars>) || <chars>[size] == 0 */
   size_t   size;
   /* variable: capacity
    * Size of allocated memory block <chars> points to.
    *
    * Invariant:
    * (0 == <chars>) || <size> + 1 <= capacity */
   size_t   capacity;
   /* variable: chars
    * Pointer to character array with '\0' at the end of the string.
    *
    * Invariant:
    * ((0 == <chars>) && (0 == capacity)) || ((0 != chars) && (0 != capacity)) */
   uint8_t* chars;
};

// group: lifetime

/* define: cstring_FREE
 * Static initializer. Makes calling <free_cstring> a no op. */
#define cstring_FREE { (size_t)0, (size_t)0, (void*)0 }

/* define: cstring_INIT
 * Static initializer. Same as calling <init_cstring> with capacity set to 0. */
#define cstring_INIT { (size_t)0, (size_t)0, (void*)0 }

/* function: init_cstring
 * Inits <cstring_t> and preallocates capacity bytes of memory.
 * If you set *capacity* to 0 no memory is preallocated. */
int init_cstring(/*out*/cstring_t* cstr, size_t capacity);

/* function: initcopy_cstring
 * Inits <cstring_t> and copies data from <string_t>.
 * If parameter *copiedfrom* is empty no data is preallocated. */
int initcopy_cstring(/*out*/cstring_t* cstr, const struct string_t* copiedfrom);

/* function: initmove_cstring
 * Inits dest with content of source and sets source to <cstring_FREE>. */
void initmove_cstring(/*out*/cstring_t* restrict dest, cstring_t* restrict source);

/* function: free_cstring
 * Frees any allocated memory associated with type <cstring_t>. */
int free_cstring(cstring_t* cstr);

// group: query

/* function: addr_cstring
 * Returns start memory address of '\0' terminated string.
 * The returned value can be NULL in case cstr->chars is NULL.
 * The returned value is valid as long as *cstr* is not changed.
 * Rturns same value as <str_cstring> except for different pointer type. */
uint8_t* addr_cstring(cstring_t* cstr);

/* function: str_cstring
 * Returns '\0' terminated string.
 * The returned value can be NULL in case cstr->chars is NULL.
 * The returned value is valid as long as *cstr* is not changed. */
char* str_cstring(cstring_t* cstr);

/* function: size_cstring
 * Returns the size of the string in bytes.
 * For multibyte encoded characters the length of a string in characters
 * is less than the size in bytes.
 *
 * Invariant:
 * size_cstring(cstr) < capacity_cstring(cstr) */
size_t size_cstring(const cstring_t* cstr);

/* function: capacity_cstring
 * Returns the allocated buffer size in bytes.
 * To access the start address of the buffer use <str_cstring>. */
size_t capacity_cstring(const cstring_t* cstr);

// group: change

/* function: allocate_cstring
 * Makes sure *cstr* has at least *capacity* preallocated bytes.
 * The string buffer is reallocated if necessary. If it is big enough
 * nothing is done. After successful return the buffer returned from
 * <str_cstring> points to at least *capacity* bytes of memory. */
int allocate_cstring(cstring_t* cstr, size_t capacity);

/* function: append_cstring
 * Appends *str* with len *str_len* to cstr.
 *
 * Attention:
 * It is not allowed to use parts of the buffer of cstr as arguments for this function.
 * Cause during a possible reallocation of the buffer these arguments become invalid. */
int append_cstring(cstring_t* cstr, size_t str_len, const char str[str_len]);

/* function: set_cstring
 * Sets content of cstr to be equal to str[0..str_len-1].
 *
 * Attention:
 * It is not allowed to use parts of the buffer of cstr as arguments for this function.
 * Cause during a possible reallocation of the buffer these arguments become invalid. */
 int set_cstring(cstring_t* cstr, size_t str_len, const char str[str_len]);

/* function: clear_cstring
 * Sets size of string to 0.
 * This function has the same result as calling <truncate_cstring> with parameter 0.
 * No memory is deallocated. */
void clear_cstring(cstring_t* cstr);

/* function: printfappend_cstring
 * Appends printf formatted string to cstr. Like sprintf but the buffer is reallocated if it is too small
 * and the new content is appended to the end of the string.
 *
 * Attention:
 * It is not allowed to use parts of the buffer of cstr as arguments for this function.
 * Cause during a possible reallocation of the buffer these arguments become invalid. */
int printfappend_cstring(cstring_t* cstr, const char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));

/* function: setsize_cstring
 * Sets size of cstr to *new_size*.
 * If the new size is bigger than the current size the buffer will contain "random" characters.
 * new_size must always be less than capacity else EINVAL is returned. */
int setsize_cstring(cstring_t* cstr, size_t new_size);

/* function: adaptsize_cstring
 * Setze size von cstr auf Offset des ersten \0 Bytes.
 * Ist capacity == 0, wird nichts getan.
 * Ansonsten wird das letzt Byte an Offset capacity-1 auf 0 gesetzt,
 * bevor size bestimmt wird, nur für den Fall, daß cstr kein \0 enhält. */
void adaptsize_cstring(cstring_t* cstr);


// section: inline implementation

/* define: addr_cstring
 * Implements <cstring_t.addr_cstring>. */
#define addr_cstring(cstr) \
         ((cstr)->chars)

/* define: initmove_cstring
 * Implements <cstring_t.initmove_cstring>. */
#define initmove_cstring(dest, source) \
         do {  *(dest) = *(source); *(source) = (cstring_t) cstring_FREE; } while(0)

/* define: capacity_cstring
 * Implements <cstring_t.capacity_cstring>. */
#define capacity_cstring(cstr) \
         ((cstr)->capacity)

/* define: size_cstring
 * Implements <cstring_t.size_cstring>. */
#define size_cstring(cstr) \
         ((cstr)->size)

/* define: str_cstring
 * Implements <cstring_t.str_cstring>. */
#define str_cstring(cstr) \
         ((char*)(cstr)->chars)

#endif
