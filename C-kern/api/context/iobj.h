/* title: InterfaceableObject

   Declares <iobj_t> which defines the standard structure
   of an interfaceable object.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/context/iobj.h
    Header file <InterfaceableObject>.

   file: C-kern/context/iobj.c
    Implementation file <InterfaceableObject impl>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/
#ifndef CKERN_CONTEXT_IOBJ_HEADER
#define CKERN_CONTEXT_IOBJ_HEADER

/* typedef: struct iobj_t
 * Export <iobj_t> into global namespace. */
typedef struct iobj_t iobj_t;

/* typedef: struct iobj_it
 * Define generic type <iobj_it> which is only
 * used in the declaration of <iobj_t>. */
typedef struct iobj_it iobj_it;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_iobj
 * Test <iobj_t> functionality. */
int unittest_context_iobj(void);
#endif


/* struct: iobj_t
 * This type has only template character.
 * It defines the general structure of an interfaceable object.
 * Such an object consists of a pointer to the object data and another
 * pointer to the function table implementing the interface.
 * The object is accessed only through the interface.
 *
 * Usage:
 * Use <iobj_T> to declare your own interfaceable object type.
 * Use <cast_iobj> to a type into its standard type.
 *
 * Example:
 * Consider an interfaceable object example_t with its interface example_it.
 * > struct example_it {
 * >    int (*fct1) (struct example_t* obj, ...);
 * >    int (*fct2) (struct example_t* obj, ...);
 * > } ;
 * >
 * > struct example_t {
 * >    struct example_t*  object;
 * >    struct example_it* iimpl;
 * > };
 *
 * The interfaceable object example_t could also be declared as
 * > typedef iobj_T(example) example_t;
 *
 * An anonymous type which is compatible with example_t can
 * be declared with:
 * > iobj_T(example) anon_example;
 *
 * The anonymous type is useful to prevent header inclusion and makes compilation faster.
 * To cast such an anonymous type to the canonical type example_t use
 * > example_t* iexample = cast_iobj(&anon_example, example);
 *
 * The macro <cast_iobj> checks that anon_example is compatible with example_t.
 *
 * */
struct iobj_t {
   /* variable: object
    * A pointer to the object data.
    * The object data is accessed through interface <iobj_it>.
    * The pointer to type <iobj_t> is casted into a custom type in the
    * implementation of the interface <iobj_it>. */
   iobj_t*  object;
   /* variable: iimpl
    * A pointer to the implementation of the interface <iobj_it>.
    * This pointer to a function table is used to access the functionality
    * of the object. */
   iobj_it* iimpl;
};

// group: lifetime

/* define: iobj_FREE
 * Static initializer. Sets both pointer to NULL. */
#define iobj_FREE \
         { 0, 0 }

/* define: iobj_INIT
 * Static initializer.
 *
 * Parameter:
 * object - Pointer to object data.
 * iimpl  - Pointer to function table/interface implementation of object. */
#define iobj_INIT(object, iimpl) \
         { (object), (iimpl) }

/* function: init_iobj
 * Generic initialization. Same as assigning <iobj_INIT>.
 * Can be used for any declared interfaceable object (see <iobj_T>). */
void init_iobj(/*out*/iobj_t* iobj, void* object, void* iimpl);

/* function: initcopy_iobj
 * Generic initialization. Same as assigning <iobj_INIT>(srciobj->object, srciobj->iimpl).
 * Can be used for any declared interfaceable object (see <iobj_T>). */
void initcopy_iobj(/*out*/iobj_t* destiobj, const iobj_t* srciobj);

/* function: free_iobj
 * Generic free operation. Same as assigning <iobj_FREE>.
 * Can be used for any declared interfaceable object (see <iobj_T>). */
void free_iobj(iobj_t* iobj);

// group: generic

/* function: cast_iobj
 * Casts pointer to an interfaceable object to its standard type name.
 * The standard type name is the prefix "typenameprefix" with the suffix "_t".
 * The first parameter is the pointer to a compatible type and the second is the same
 * as used in <iobj_T>. */
void* cast_iobj(void* iobj, IDNAME typenameprefix);

/* function: iobj_T
 * Declares a generic interfaceable object compatible with <iobj_t>.
 * The object type and interface type are derived from parameter type name *objecttype*.
 * The object type is constructed as objecttype ## _t and the interface type
 * as objecttype ## _it. */
#define iobj_T(objecttype) \
         struct {                           \
            struct objecttype##_t*  object; \
            struct objecttype##_it* iimpl;  \
         }



// section: inline implementation

/* define: free_iobj
 * Implements <iobj_t.free_iobj>. */
#define free_iobj(iobj) \
         do {                           \
            typeof(iobj) _obj = (iobj); \
            _obj->object = 0;           \
            _obj->iimpl  = 0;           \
         } while (0)

/* define: cast_iobj
 * Implements <iobj_t.cast_iobj>. */
#define cast_iobj(iobj, typenameprefix) \
         ( __extension__ ({                              \
            typeof(iobj) _o;                             \
            _o = (iobj);                                 \
            static_assert(                               \
               &(_o->object)                             \
               == &((typenameprefix##_t*) _o)->object    \
               && &(_o->iimpl)                           \
                  == &((typenameprefix##_t*) _o)->iimpl, \
               "compatible structure");                  \
            (typenameprefix##_t*) _o;                    \
         }))

/* define: init_iobj
 * Implements <iobj_t.init_iobj>. */
#define init_iobj(iobj, _object, _iimpl) \
         do {                           \
            typeof(iobj) _obj = (iobj); \
            _obj->object = (_object);   \
            _obj->iimpl  = (_iimpl);    \
         } while (0)

/* define: initcopy_iobj
 * Implements <iobj_t.initcopy_iobj>. */
#define initcopy_iobj(destiobj, srciobj) \
         do {                                    \
            typeof(srciobj)  _src  = (srciobj);  \
            typeof(destiobj) _dest = (destiobj); \
            _dest->object = _src->object;        \
            _dest->iimpl  = _src->iimpl;         \
         } while (0)

#endif
