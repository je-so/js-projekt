/* title: Intop-Byteorder

   TODO: describe module interface

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/math/int/byteorder.h
    Header file <Intop-Byteorder>.

   file: C-kern/math/int/byteorder.c
    Implementation file <Intop-Byteorder impl>.
*/
#ifndef CKERN_MATH_INT_BYTEORDER_HEADER
#define CKERN_MATH_INT_BYTEORDER_HEADER

/* typedef: struct int_t
 * Export <int_t> into global namespace. */
typedef struct int_t         int_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_byteorder
 * Test <int_t> functionality. */
int unittest_math_int_byteorder(void) ;
#endif


// struct: int_t

// group: byte-operations

/* function: htobe_int
 * Converts host_val from host byte order to big-endian order.
 * The returned value is in big-endian order.
 * This operation is a no-op on big-endian platforms. */
unsigned htobe_int(unsigned host_val) ;

/* function: htole_int
 * Converts host_val from host byte order to little-endian order.
 * The returned value is in little-endian order.
 * This operation is a no-op on little-endian platforms. */
unsigned htole_int(unsigned host_val) ;

/* function: betoh_int
 * Converts big_endian_val from big-endian byte order to host byte order.
 * The returned value is in host byte order.
 * This operation is a no-op on big-endian platforms. */
unsigned betoh_int(unsigned big_endian_val) ;

/* function: letoh_int
 * Converts little_endian_val from little-endian byte order to host byte order.
 * The returned value is in host byte order.
 * This operation is a no-op on little-endian platforms. */
unsigned letoh_int(unsigned little_endian_val) ;



// section: inline implementation

/* define: htobe_int
 * Implements <int_t.htobe_int>. */
#define htobe_int(host_val)                                    \
         ( __extension__ ({                                    \
            typeof(host_val) _v  = (host_val) ;                \
            static_assert((typeof(_v))-1 > 0, "unsigned") ;    \
            static_assert(sizeof(_v) <= sizeof(uint64_t), ) ;  \
            if (sizeof(_v) == sizeof(uint16_t)) {              \
               _v = (typeof(_v))htobe16((uint16_t)_v) ;        \
            } else if (sizeof(_v) == sizeof(uint32_t)) {       \
               _v = (typeof(_v))htobe32((uint32_t)_v) ;        \
            } else if (sizeof(_v) == sizeof(uint64_t)) {       \
               _v = (typeof(_v))htobe64((uint64_t)_v) ;        \
            }                                                  \
            _v ;                                               \
         }))

/* define: htole_int
 * Implements <int_t.htole_int>. */
#define htole_int(host_val)                                    \
         ( __extension__ ({                                    \
            typeof(host_val) _v  = (host_val) ;                \
            static_assert((typeof(_v))-1 > 0, "unsigned") ;    \
            static_assert(sizeof(_v) <= sizeof(uint64_t), ) ;  \
            if (sizeof(_v) == sizeof(uint16_t)) {              \
               _v = (typeof(_v))htole16((uint16_t)_v) ;        \
            } else if (sizeof(_v) == sizeof(uint32_t)) {       \
               _v = (typeof(_v))htole32((uint32_t)_v) ;        \
            } else if (sizeof(_v) == sizeof(uint64_t)) {       \
               _v = (typeof(_v))htole64((uint64_t)_v) ;        \
            }                                                  \
            _v ;                                               \
         }))

/* define: betoh_int
 * Implements <int_t.betoh_int>. */
#define betoh_int(host_val)                                    \
         ( __extension__ ({                                    \
            typeof(host_val) _v  = (host_val) ;                \
            static_assert((typeof(_v))-1 > 0, "unsigned") ;    \
            static_assert(sizeof(_v) <= sizeof(uint64_t), ) ;  \
            if (sizeof(_v) == sizeof(uint16_t)) {              \
               _v = (typeof(_v))be16toh((uint16_t)_v) ;        \
            } else if (sizeof(_v) == sizeof(uint32_t)) {       \
               _v = (typeof(_v))be32toh((uint32_t)_v) ;        \
            } else if (sizeof(_v) == sizeof(uint64_t)) {       \
               _v = (typeof(_v))be64toh((uint64_t)_v) ;        \
            }                                                  \
            _v ;                                               \
         }))

/* define: letoh_int
 * Implements <int_t.letoh_int>. */
#define letoh_int(host_val)                                    \
         ( __extension__ ({                                    \
            typeof(host_val) _v  = (host_val) ;                \
            static_assert((typeof(_v))-1 > 0, "unsigned") ;    \
            static_assert(sizeof(_v) <= sizeof(uint64_t), ) ;  \
            if (sizeof(_v) == sizeof(uint16_t)) {              \
               _v = (typeof(_v))le16toh((uint16_t)_v) ;        \
            } else if (sizeof(_v) == sizeof(uint32_t)) {       \
               _v = (typeof(_v))le32toh((uint32_t)_v) ;        \
            } else if (sizeof(_v) == sizeof(uint64_t)) {       \
               _v = (typeof(_v))le64toh((uint64_t)_v) ;        \
            }                                                  \
            _v ;                                               \
         }))

#endif
