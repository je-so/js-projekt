/* title: Typeadapt-Comparator

   Abstract interface (function table) to adapt
   a concrete user type to a container which needs
   comparing nodes and nodes and keys.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/typeadapt/comparator.h
    Header file <Typeadapt-Comparator>.

   file: C-kern/ds/typeadapt/comparator.c
    Implementation file <Typeadapt-Comparator impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_COMPARATOR_HEADER
#define CKERN_DS_TYPEADAPT_COMPARATOR_HEADER

// forward
struct typeadapt_t;
struct typeadapt_object_t;

/* typedef: struct typeadapt_comparator_it
 * Export <typeadapt_comparator_it> into global namespace. */
typedef struct typeadapt_comparator_it typeadapt_comparator_it;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt_comparator
 * Test <typeadapt_comparator_it> functionality. */
int unittest_ds_typeadapt_comparator(void);
#endif


/* struct: typeadapt_comparator_it
 * Declares interface for comparing two objects and key with object.
 * If you change this interface do not forget to adapt
 * <typeadapt_comparator_EMBED> and <cast_typeadaptcomparator>. */
struct typeadapt_comparator_it {
   /* variable: cmp_key_object
    * Compares key with an object. lkey is the left operand and robject the right one.
    *
    * Returns:
    * -1 - lkey <  robject
    * 0  - lkey == robject
    * +1 - lkey >  robject */
   int  (* cmp_key_object) (struct typeadapt_t * typeadp, const void * lkey, const struct typeadapt_object_t * robject);
   /* variable: cmp_object
    * Compares two objects. lobject is left operand and robject the right one.
    *
    * Returns:
    * -1 - lobject <  robject
    * 0  - lobject == robject
    * +1 - lobject >  robject */
   int  (* cmp_object)     (struct typeadapt_t * typeadp, const struct typeadapt_object_t * lobject, const struct typeadapt_object_t * robject);
};

// group: lifetime

/* define: typeadapt_comparator_FREE
 * Static initializer. Sets all functions pointers of <typeadapt_comparator_it> to 0. */
#define typeadapt_comparator_FREE { 0, 0 }

/* define: typeadapt_comparator_INIT
 * Static initializer. Sets all function pointers to the provided values.
 *
 * Parameters:
 * cmpkeyobj_f - Function pointer to function comparing key with object. See <typeadapt_comparator_it.cmp_key_object>.
 * cmpobj_f    - Function pointer to function comparing object with object. See <typeadapt_comparator_it.cmp_object>. */
#define typeadapt_comparator_INIT(cmpkeyobj_f, cmpobj_f)    { cmpkeyobj_f, cmpobj_f }

// group: query

bool isequal_typeadaptcomparator(const typeadapt_comparator_it * ladpcmp, const typeadapt_comparator_it * radpcmp) ;

// group: call-service

/* function: callcmpkeyobj_typeadaptcomparator
 * Calls function <typeadapt_comparator_it.cmp_key_object>.
 * The first parameter is of type <typeadapt_comparator_it> the others are the same as in <typeadapt_comparator_it.cmp_key_object>.
 * This function is implemented as macro and supports types derived from <typeadapt_comparator_it> - see use of <typeadapt_comparator_DECLARE>. */
int callcmpkeyobj_typeadaptcomparator(typeadapt_comparator_it * adpcmp, struct typeadapt_t * typeadp, const void * key, const struct typeadapt_object_t * robject) ;

/* function: callcmpobj_typeadaptcomparator
 * Calls function <typeadapt_comparator_it.cmp_object>.
 * The first parameter is of type <typeadapt_comparator_it> the others are the same as in <typeadapt_comparator_it.cmp_object>.
 * This function is implemented as macro and supports types derived from <typeadapt_comparator_it> - see use of <typeadapt_comparator_DECLARE>. */
int callcmpobj_typeadaptcomparator(typeadapt_comparator_it * adpcmp, struct typeadapt_t * typeadp, const struct typeadapt_object_t * lobject, const struct typeadapt_object_t * robject) ;

// group: generic

/* define: cast_typeadaptcomparator
 * Casts parameter adpcmp into pointer to <typeadapt_comparator_it>.
 * The parameter *adpcmp* has to be of type "pointer to declared_it" where declared_it
 * is the name used as first parameter in <typeadapt_comparator_DECLARE>.
 * The second and third parameter must be the same as in <typeadapt_comparator_DECLARE>. */
typeadapt_comparator_it * cast_typeadaptcomparator(void * adpcmp, TYPENAME typeadapter_t, TYPENAME object_t, TYPENAME key_t) ;

/* define: typeadapt_comparator_DECLARE
 * Declares a derived interface from generic <typeadapt_comparator_it>.
 * It is structural compatible with <typeadapt_comparator_it>.
 * See <typeadapt_comparator_it> for a list of contained functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the interface.
 *                 The name should have the suffix "_it".
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_comparator_it> supports.
 * key_t         - The key type that <typeadapt_comparator_it> supports. Must be of size sizeof(void*). */
#define typeadapt_comparator_DECLARE(declared_it, typeadapter_t, object_t, key_t)      \
   static inline void compiletimeassert##declared_it(void) {                           \
      static_assert(sizeof(key_t)==sizeof(void*), "compatible with cmp_key_object") ;  \
   }                                                                                   \
   typedef struct declared_it             declared_it ;                                \
   struct declared_it {                                                                \
      typeadapt_comparator_EMBED(typeadapter_t, object_t, key_t) ;                     \
   }

/* define: typeadapt_comparator_EMBED
 * Declares a derived interface from generic <typeadapt_comparator_it>.
 * It is structural compatible with <typeadapt_comparator_it>.
 * See <typeadapt_comparator_it> for a list of contained functions.
 *
 * Parameter:
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_comparator_it> supports.
 * key_t         - The key type that <typeadapt_comparator_it> supports. Must be of size sizeof(void*). */
#define typeadapt_comparator_EMBED(typeadapter_t, object_t, key_t) \
   int  (* cmp_key_object)  (typeadapter_t * typeadp, const key_t lkey, const object_t * robject);         \
   int  (* cmp_object)      (typeadapter_t * typeadp, const object_t * lobject, const object_t  * robject)


// section: inline implementation

/* define: cast_typeadaptcomparator
 * Implements <typeadapt_comparator_it.cast_typeadaptcomparator>. */
#define cast_typeadaptcomparator(adpcmp, typeadapter_t, object_t, key_t) \
         ( __extension__ ({                                                      \
            typeof(adpcmp) _cm;                                                  \
            _cm = (adpcmp);                                                      \
            static_assert(                                                       \
               &(_cm->cmp_key_object)                                            \
               == (int (**) (typeadapter_t*,const key_t,const object_t*))        \
                  &((typeadapt_comparator_it*) _cm)->cmp_key_object              \
               && &(_cm->cmp_object)                                             \
                  == (int (**) (typeadapter_t*,const object_t*,const object_t*)) \
                     &((typeadapt_comparator_it*) _cm)->cmp_object               \
               && sizeof(key_t) == sizeof(void*),                                \
               "ensure same structure") ;                                        \
            (typeadapt_comparator_it*) _cm;                                      \
         }))

/* function: callcmpkeyobj_typeadaptcomparator
 * Implements <typeadapt_comparator_it.callcmpkeyobj_typeadaptcomparator>. */
#define callcmpkeyobj_typeadaptcomparator(adpcmp, typeadp, key, robject) \
         ((adpcmp)->cmp_key_object((typeadp), (key), (robject)))

/* function: callcmpobj_typeadaptcomparator
 * Implements <typeadapt_comparator_it.callcmpobj_typeadaptcomparator>. */
#define callcmpobj_typeadaptcomparator(adpcmp, typeadp, lobject, robject) \
         ((adpcmp)->cmp_object((typeadp), (lobject), (robject)))

#endif
