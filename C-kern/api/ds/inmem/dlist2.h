/* title: DoubleLinkedList2

   Implementiert eine doppelt verkettete Liste
   mit Zeiger auf ersten und letzten Knoten.

   Die andere Implementierung <dlist_t> speichert nur
   nur einen last Zeiger.

   > dlist2_t:
   >    ---------
   >    | last  |-------------------------------╮
   >    | first |                               |
   >    -|-------                               |
   >     |                                      |
   >     |                                      |
   >     ↓        dlist_node_t:                 ↓
   >    --------     --------              --------
   > ╭->| next | --> | next | --> ... -->  | next |-╮
   > |╭-| prev | <-- | prev | <-- ... <--  | prev | |
   > || --------     --------              -------- |
   > |╰-----------------------------------------^   |
   > ╰----------------------------------------------╯

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/ds/inmem/dlist2.h
    Header file <DoubleLinkedList2>.

   file: C-kern/ds/inmem/dlist2.c
    Implementation file <DoubleLinkedList2 impl>.
*/
#ifndef CKERN_DS_INMEM_DLIST2_HEADER
#define CKERN_DS_INMEM_DLIST2_HEADER

#include "C-kern/api/ds/inmem/node/dlist_node.h"

// forward
struct typeadapt_t;

/* typedef: struct dlist2_t
 * Export <dlist2_t> into global namespace. */
typedef struct dlist2_t dlist2_t;

/* typedef: struct dlist2_iterator_t
 * Export <dlist2_iterator_t>. */
typedef struct dlist2_iterator_t dlist2_iterator_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_dlist2
 * Teste Funktionalität von <dlist2_t>. */
int unittest_ds_inmem_dlist2(void);
#endif


/* struct: dlist2_iterator_t
 * Iteriert über in <dlist2_t> gespeicherte Elemente.
 * Der Iterator unterstützt das Entfernen oder Löschen des aktuellen und der
 * davorliegend iterierten Elemente.
 *
 * Example:
 * > dlist2_t list;
 * > fill_list(&list);
 * >  foreach (_dlist2, node, &list) {
 * >     if (need_to_remove(node)) {
 * >        remove_dlist2(&list, node);
 * >     }
 * >  }
 */
struct dlist2_iterator_t {
   dlist_node_t * next;
   dlist2_t     * list;
};

// group: lifetime

/* define: dlist2_iterator_FREE
 * Statischer Initialisierer. */
#define dlist2_iterator_FREE \
         { 0, 0 }

/* function: initfirst_dlist2iterator
 * Initialisiert den Iterator iter mit dem ersten Element der Liste list.
 * Gibt ENODATA zurück, falls list kein Element enthält. */
int initfirst_dlist2iterator(/*out*/dlist2_iterator_t * iter, dlist2_t * list);

/* function: initlast_dlist2iterator
 * Initialisiert den Iterator iter mit dem letzten Element der Liste list.
 * Gibt ENODATA zurück, falls list kein Element enthält. */
int initlast_dlist2iterator(/*out*/dlist2_iterator_t * iter, dlist2_t * list);

/* function: free_dlist2iterator
 * Setzt iter->next auf 0. Gibt immer 0 (OK) zurück.
 * Muss nicht aufgerufen werden, da keine Ressourcen freigegeben werden.
 * Nach Aufruf dieser Funktion würden weitere Aufrufe von
 * <next_dlist2iterator> bzw. <prev_dlist2iterator> *false* zurückgegeben. */
int free_dlist2iterator(dlist2_iterator_t * iter);

// group: iterate

/* function: next_dlist2iterator
 * Gibt das aktuelle Element des Iterator zurück und setzt ihn auf das nächste.
 * Der erste Aufruf dieser Funktion nach erfolgreichem Aufruf von <initfirst_dlist2iterator>
 * gibt das erste Listenelement zurück.
 *
 * Returns:
 * true  - node zeigt auf das aktuelle Element und iter wurde auf das nächste Element bewegt, falls
 *         node nicht auf das letzte Element zeigt.
 * false - Es gibt kein weiteres Element mehr. Das letzte Element wurde schon beim letzten Aufruf
 *         zurückgegeben. Der Parameter node kann aber trotzdem überschrieben worden sein. */
bool next_dlist2iterator(dlist2_iterator_t * iter, /*out*/dlist_node_t ** node);

/* function: prev_dlist2iterator
 * Gibt das aktuelle Element des Iterator zurück und setzt ihn auf das vorherige.
 * Der erste Aufruf dieser Funktion nach erfolgreichem Aufruf von <initlast_dlist2iterator>
 * gibt das letzte Listenelement zurück.
 *
 * Returns:
 * true  - node zeigt auf das aktuelle Element und iter wurde auf das vorherige Element bewegt, falls
 *         node nicht auf das erste Element zeigt.
 * false - Es gibt kein weiteres Element mehr. Das erste Element wurde schon beim letzten Aufruf
 *         zurückgegeben. Der Parameter node kann aber trotzdem überschrieben worden sein. */
bool prev_dlist2iterator(dlist2_iterator_t * iter, /*out*/dlist_node_t ** node);


/* struct: dlist2_t
 * Zirkuläre, doppelt verkettete Liste.
 * Das letzte zeigt mit next auf das erste Element der Liste und
 * das erste mit prev auf das letzte.
 * Falls die Lsite nur einen Knoten enthält, zeigen next und prev
 * des Knotens auf sich selbst.
 *
 * Achtung:
 * Sollte ein Knoten, der schon in einer Liste steht, in eine
 * weitere Liste eingefügt werden, wird kein Fehler zurückgegeben.
 * Aber die Liste, in der er voher stand, ist dann korrupt und mit
 * der neuen Liste verschränkt, was zu undefiniertem Verhalten führt.
 * */
struct dlist2_t {
   struct dlist_node_t * last;
   struct dlist_node_t * first;
};

// group: lifetime

/* define: dlist2_FREE
 * Statischer Initialisierer. */
#define dlist2_FREE \
         { 0, 0 }

/* define: dlist2_INIT
 * Statischer Initialisierer. Kann anstatt <init_dlist2> Verwendung finden. */
#define dlist2_INIT { 0, 0 }

/* define: dlist2_INIT_POINTER
 * Statischer Initialisierer. Kann verwendet werden, um eine Liste
 * mit vorher gesicherten Zeigern wieder neu aufzusetzen. */
#define dlist2_INIT_POINTER(last, first) \
         { (last), (first) }

/* function: init_dlist2
 * Initialisiert list als leere Liste. Implementiert als Zuweisung von <dlist2_INIT> an list. */
void init_dlist2(/*out*/dlist2_t* list);

/* function: free_dlist2
 * Entfernt alle Elemente aus der Liste und löscht diese.
 * Zu deisem zweck wird der Typeadapter Callback <typeadapt_lifetime_it.delete_object> aufgerufen.
 * Ist entweder Parameter typeadp == 0 oder <typeadapt_lifetime_it.delete_object> == 0 werden die
 * Knoten nur aus der Liste entfernt aber nicht freigegeben.
 * Für eine weitere Beschreibung siehe dazu auch <typeadapt_t>. */
int free_dlist2(dlist2_t* list, uint16_t nodeoffset, struct typeadapt_t* typeadp/*0=>no free called*/);

// group: query

/* function: isempty_dlist2
 * Gibt true zurück, falls list keine Elemente enthält (list->last == 0). */
int isempty_dlist2(const dlist2_t * list);

/* function: first_dlist2
 * Gibt den Zeiger auf das erste Element in der Liste zurück oder NULL. */
dlist_node_t * first_dlist2(const dlist2_t * list);

/* function: last_dlist2
 * Gibt den Zeiger auf das letzte Element in der Liste zurück oder NULL. */
dlist_node_t * last_dlist2(const dlist2_t * list);

/* function: next_dlist2
 * Alias für <next_dlistnode>. */
dlist_node_t * next_dlist2(const dlist_node_t * node);

/* function: prev_dlist2
 * Alias für <prev_dlistnode>. */
dlist_node_t * prev_dlist2(const dlist_node_t * node);

/* function: isinlist_dlist2
 * Alias für <isinlist_dlistnode>. */
bool isinlist_dlist2(const dlist_node_t * node);

// group: foreach-support

/* typedef: iteratortype_dlist2
 * Deklaration assoziiert Iteratortyp <dlist2_iterator_t> mit <dlist2_t>. */
typedef dlist2_iterator_t     iteratortype_dlist2;

/* typedef: iteratedtype_dlist2
 * Deklaration assoziiert Rückgabetyp (iterierten Typ) <dlist_node_t> mit dem Iterator von <dlist2_t>. */
typedef struct dlist_node_t * iteratedtype_dlist2;

// group: update

/* function: insertfirst_dlist2
 * Fügt new_node als erstes Element in die Liste list ein.
 * Der Besitz am Objekt wechselt vom Aufrufer an die Liste.
 *
 * Ungeprüfte Precondition:
 * o new_node steht in keiner Liste. */
void insertfirst_dlist2(dlist2_t* list, dlist_node_t * new_node);

/* function: insertlast_dlist2
 * Fügt new_node als letztes Element in die Liste list ein.
 * Der Besitz am Objekt wechselt vom Aufrufer an die Liste.
 *
 * Ungeprüfte Precondition:
 * o new_node steht in keiner Liste. */
void insertlast_dlist2(dlist2_t * list, dlist_node_t * new_node);

/* function: insertafter_dlist2
 * Fügt new_node nach prev_node in die Liste list ein.
 * Der Besitz am Objekt wechselt vom Aufrufer an die Liste.
 * Falls (prev_node == <last_dlist2>(list)), wird das Objekt new_node
 * automatisch das neue letzte Element der Liste.
 *
 * Ungeprüfte Precondition:
 * o new_node steht in keiner Liste. */
void insertafter_dlist2(dlist2_t * list, dlist_node_t * prev_node, dlist_node_t * new_node);

/* function: insertbefore_dlist2
 * Fügt new_node vor next_node in die Liste list ein.
 * Der Besitz am Objekt wechselt vom Aufrufer an die Liste.
 * Falls (next_node == <first_dlist2>(list)), wird das Objekt new_node
 * automatisch das neue erste Element der Liste.
 *
 * Ungeprüfte Precondition:
 * o new_node steht in keiner Liste. */
void insertbefore_dlist2(dlist2_t * list, dlist_node_t * next_node, dlist_node_t * new_node);

/* function: removefirst_dlist2
 * Entfernt das zuerst in der Liste stehende Element.
 * Der Besitz am Objekt wechselt an den Aufrufer.
 * Inhalt von Parameter removed_node zeigt auf das entfernte, ehemals erste Element der Liste.
 *
 * Ungeprüfte Precondition:
 * o ! isempty_dlist2(list) */
void removefirst_dlist2(dlist2_t * list, struct dlist_node_t ** removed_node);

/* function: removelast_dlist2
 * Entfernt das zuletzt in der Liste stehende Element.
 * Der Besitz am Objekt wechselt an den Aufrufer.
 * Inhalt von Parameter removed_node zeigt auf das entfernte, ehemals letzte Element der Liste.
 *
 * Ungeprüfte Precondition:
 * o ! isempty_dlist2(list) */
void removelast_dlist2(dlist2_t * list, struct dlist_node_t ** removed_node);

/* function: remove_dlist2
 * Entfernt Element node aus der Liste.
 * Der Besitz am Objekt wechselt an den Aufrufer.
 *
 * Ungeprüfte Precondition:
 * o node ist Teil von list && ! isempty_dlist2(list) */
void remove_dlist2(dlist2_t * list, struct dlist_node_t * node);

/* function: replacenode_dlist2
 * Entfernt Element oldnode aus der Liste und fügt an dessen Stelle newnode ein.
 * Der Besitz am Objekt oldnode wechselt an den Aufrufer.
 * Der Besitz am Objekt newnode wechselt vom Aufrufer an die Liste.
 * Die Zeiger oldnode->next und oldnode->prev werden auf NULL gesetzt.
 *
 * Ungeprüfte Precondition:
 * o newnode steht in keiner Liste.
 * o oldnode ist Teil von list */
void replacenode_dlist2(dlist2_t * list, dlist_node_t * oldnode, dlist_node_t * newnode);

// group: set-update

/* function: removeall_dlist2
 * Entfernt alle Elemente aus der Liste und löscht diese.
 * Diese Funktion wird durch <free_dlist2> implementiert. */
int removeall_dlist2(dlist2_t * list, uint16_t nodeoffset, struct typeadapt_t * typeadp/*0=>no free called*/);

/* function: insertlastPlist_dlist2
 * Füge alle Knoten von nodes an Ende von list an.
 * Der Besitz der Knoten wird von nodes nach list transferiert.
 * Nach der Operation ist nodes leer und list->last zeigt auf den
 * Knoten, auf den vorher nodes->last zeigte. */
void insertlastPlist_dlist2(dlist2_t * list, dlist2_t * nodes);

// group: generic

/* function: cast_dlist2
 * Statischer cast des generischen Typs list nach Typ <dlist2_t>.
 * Der Typ von list muss mindestens einen last und first Pointer besitzen. */
dlist2_t* cast_dlist2(void* list);

/* function: castconst_dlist2
 * Statischer cast des generischen Typs list nach Typ const <dlist2_t>.
 * Der Typ von list muss mindestens einen last und first Pointer besitzen. */
const dlist2_t* castconst_dlist2(const void* list);

/* define: dlist2_IMPLEMENT
 * Generiert azs dlist2_t abgeleitetes Interface und dessen inline Implementierung.
 * Der Untersachied zum dlist2-Interface ist, daß Objekte vom Typ object_t verwaltet werden
 * anstatt vom Typ <dlist_node_t>.
 *
 * Parameter:
 * _fsuffix   - Die Endsilbe der Namen der generierten Funktionen, z.B. "init ## _fsuffix".
 * object_t   - Der Typ, der durch die Liste verwaltet werden soll.
 *              Der Typ muss ein Feld vom Typ <dlist_node_t> enthalten.
 * nodeprefix - Der Zugriffpfad einschließlich des Punktes ".", der zum Feld des Typs <dlist_node_t> führt.
 *              Falls object_t selbst einen next und prev Zeiger enthält, dann ist nodeprefix leer zu lassen.
 * */
void dlist2_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, IDNAME nodeprefix);



// section: inline implementation

// group: dlist2_iterator_t

/* define: free_dlist2iterator
 * Implements <dlist2_iterator_t.free_dlist2iterator>. */
#define free_dlist2iterator(iter) \
         (((iter)->next = 0), 0)

/* define: initfirst_dlist2iterator
 * Implements <dlist2_iterator_t.initfirst_dlist2iterator>. */
#define initfirst_dlist2iterator(iter, LIST) \
         ( __extension__ ({                  \
            int _err;                        \
            typeof(iter) _iter = (iter);     \
            typeof(LIST) _list = (LIST);     \
            if (_list->first) {              \
               _iter->next = _list->first;   \
               _iter->list = _list;          \
               _err = 0;                     \
            } else {                         \
               _err = ENODATA;               \
            }                                \
            _err;                            \
         }))

/* define: initlast_dlist2iterator
 * Implements <dlist2_iterator_t.initlast_dlist2iterator>. */
#define initlast_dlist2iterator(iter, LIST) \
         ( __extension__ ({               \
            int _err;                     \
            typeof(iter) _iter = (iter);  \
            typeof(LIST) _list = (LIST);  \
            if (_list->last) {            \
               _iter->next = _list->last; \
               _iter->list = _list;       \
               _err = 0;                  \
            } else {                      \
               _err = ENODATA;            \
            }                             \
            _err;                         \
         }))

/* define: next_dlist2iterator
 * Implements <dlist2_iterator_t.next_dlist2iterator>. */
#define next_dlist2iterator(iter, node) \
         ( __extension__ ({                                 \
            typeof(iter) _iter = (iter);                    \
            bool _isNext = (0 != _iter->next);              \
            if (_isNext) {                                  \
               *(node) = _iter->next;                       \
               _iter->next =                                \
                        (_iter->list->last == _iter->next)  \
                        ? 0                                 \
                        : next_dlistnode(_iter->next);      \
            }                                               \
            _isNext;                                        \
         }))

/* define: prev_dlist2iterator
 * Implements <dlist2_iterator_t.prev_dlist2iterator>. */
#define prev_dlist2iterator(iter, node) \
         ( __extension__ ({                                 \
            typeof(iter) _iter = (iter);                    \
            bool _isNext = (0 != _iter->next);              \
            if (_isNext) {                                  \
               *(node) = _iter->next;                       \
               _iter->next =                                \
                        (_iter->list->first == _iter->next) \
                        ? 0                                 \
                        : prev_dlistnode(_iter->next);      \
            }                                               \
            _isNext;                                        \
         }))

// group: dlist2_t

/* define: cast_dlist2
 * Implements <dlist2_t.cast_dlist2>. */
#define cast_dlist2(list) \
         ( __extension__ ({                  \
            typeof(list) _l2 = (list);       \
            static_assert(                   \
               &(_l2->last)                  \
               == &((dlist2_t*) (uintptr_t)  \
                     _l2)->last              \
               && &(_l2->first)              \
               == &((dlist2_t*) (uintptr_t)  \
                     _l2)->first,            \
                  "kompatible struct"        \
            );                               \
            (dlist2_t*) _l2;                 \
         }))

/* define: castconst_dlist2
 * Implements <dlist2_t.castconst_dlist2>. */
#define castconst_dlist2(list) \
         ( __extension__ ({                  \
            typeof(list) _l2 = (list);       \
            static_assert(                   \
               &(_l2->last)                  \
               == &((dlist2_t*) (uintptr_t)  \
                     _l2)->last              \
               && &(_l2->first)              \
               == &((dlist2_t*) (uintptr_t)  \
                     _l2)->first,            \
                  "kompatible struct"        \
            );                               \
            (const dlist2_t*) _l2;           \
         }))

/* define: init_dlist2
 * Implements <dlist2_t.init_dlist2>. */
#define init_dlist2(list) \
         ((void)(*(list) = (dlist2_t)dlist2_INIT))

/* define: isempty_dlist2
 * Implements <dlist2_t.isempty_dlist2>. */
#define isempty_dlist2(list) \
         (0 == (list)->last)

/* define: isinlist_dlist2
 * Implements <dlist2_t.isinlist_dlist2>. */
#define isinlist_dlist2(node) \
         isinlist_dlistnode(node)

/* define: first_dlist2
 * Implements <dlist2_t.first_dlist2>. */
#define first_dlist2(list) \
         ((list)->first)

/* define: last_dlist2
 * Implements <dlist2_t.last_dlist2>. */
#define last_dlist2(list) \
         ((list)->last)

/* define: next_dlist2
 * Implements <dlist2_t.next_dlist2>. */
#define next_dlist2(node) \
         next_dlistnode(node)

/* define: prev_dlist2
 * Implements <dlist2_t.prev_dlist2>. */
#define prev_dlist2(node) \
         prev_dlistnode(node)

/* define: removeall_dlist2
 * Implements <dlist2_t.removeall_dlist2>. */
#define removeall_dlist2(list, nodeoffset, typeadp) \
         (free_dlist2((list), (nodeoffset), (typeadp)))

/* define: dlist2_IMPLEMENT
 * Implements <dlist2_t.dlist2_IMPLEMENT>. */
#define dlist2_IMPLEMENT(_fsuffix, object_t, nodeprefix) \
   typedef dlist2_iterator_t  iteratortype##_fsuffix;    \
   typedef object_t *         iteratedtype##_fsuffix;    \
   static inline int  initfirst##_fsuffix##iterator(dlist2_iterator_t * iter, dlist2_t * list) __attribute__ ((always_inline));   \
   static inline int  initlast##_fsuffix##iterator(dlist2_iterator_t * iter, dlist2_t * list) __attribute__ ((always_inline));    \
   static inline int  free##_fsuffix##iterator(dlist2_iterator_t * iter) __attribute__ ((always_inline)); \
   static inline bool next##_fsuffix##iterator(dlist2_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)); \
   static inline bool prev##_fsuffix##iterator(dlist2_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)); \
   static inline void init##_fsuffix(dlist2_t * list) __attribute__ ((always_inline)); \
   static inline int  free##_fsuffix(dlist2_t * list, struct typeadapt_t * typeadp) __attribute__ ((always_inline)); \
   static inline int  isempty##_fsuffix(const dlist2_t * list) __attribute__ ((always_inline)); \
   static inline object_t * first##_fsuffix(const dlist2_t * list) __attribute__ ((always_inline));  \
   static inline object_t * last##_fsuffix(const dlist2_t * list) __attribute__ ((always_inline));   \
   static inline object_t * next##_fsuffix(object_t * node) __attribute__ ((always_inline)); \
   static inline object_t * prev##_fsuffix(object_t * node) __attribute__ ((always_inline)); \
   static inline bool isinlist##_fsuffix(object_t * node) __attribute__ ((always_inline)); \
   static inline void insertfirst##_fsuffix(dlist2_t * list, object_t * new_node) __attribute__ ((always_inline));   \
   static inline void insertlast##_fsuffix(dlist2_t * list, object_t * new_node) __attribute__ ((always_inline));    \
   static inline void insertafter##_fsuffix(dlist2_t * list, object_t * prev_node, object_t * new_node) __attribute__ ((always_inline)); \
   static inline void insertbefore##_fsuffix(dlist2_t * list, object_t* next_node, object_t * new_node) __attribute__ ((always_inline)); \
   static inline void removefirst##_fsuffix(dlist2_t * list, object_t ** removed_node) __attribute__ ((always_inline));  \
   static inline void removelast##_fsuffix(dlist2_t * list, object_t ** removed_node) __attribute__ ((always_inline));   \
   static inline void remove##_fsuffix(dlist2_t * list, object_t * node) __attribute__ ((always_inline)); \
   static inline void replacenode##_fsuffix(dlist2_t * list, object_t * oldnode, object_t * newnode) __attribute__ ((always_inline)); \
   static inline int removeall##_fsuffix(dlist2_t * list, struct typeadapt_t * typeadp) __attribute__ ((always_inline)); \
   static inline void insertlastPlist##_fsuffix(dlist2_t * tolist, dlist2_t * fromlist) __attribute__ ((always_inline)); \
   static inline uint16_t nodeoffset##_fsuffix(void) __attribute__ ((always_inline)); \
   static inline uint16_t nodeoffset##_fsuffix(void) { \
      static_assert(UINT16_MAX > (uintptr_t) & (((object_t*)0)->nodeprefix next), "offset fits in uint16_t"); \
      return (uint16_t) (uintptr_t) & (((object_t*)0)->nodeprefix next); \
   }  \
   static inline dlist_node_t * cast2node##_fsuffix(object_t * object) { \
      static_assert(&(((object_t*)0)->nodeprefix next) == (dlist_node_t**)((uintptr_t)nodeoffset##_fsuffix()), "correct type"); \
      static_assert(&(((object_t*)0)->nodeprefix prev) == (dlist_node_t**)(nodeoffset##_fsuffix() + sizeof(dlist_node_t*)), "correct type and offset"); \
      return (dlist_node_t *) ((uintptr_t)object + nodeoffset##_fsuffix()); \
   } \
   static inline object_t * cast2object##_fsuffix(dlist_node_t * node) { \
      return (object_t *) ((uintptr_t)node - nodeoffset##_fsuffix()); \
   } \
   static inline object_t * castnull2object##_fsuffix(dlist_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - nodeoffset##_fsuffix()) : 0; \
   } \
   static inline void init##_fsuffix(dlist2_t * list) { \
      init_dlist2(list); \
   } \
   static inline int free##_fsuffix(dlist2_t * list, struct typeadapt_t * typeadp) { \
      return free_dlist2(list, nodeoffset##_fsuffix(), typeadp); \
   } \
   static inline int isempty##_fsuffix(const dlist2_t * list) { \
      return isempty_dlist2(list); \
   } \
   static inline object_t * first##_fsuffix(const dlist2_t * list) { \
      return castnull2object##_fsuffix(first_dlist2(list)); \
   } \
   static inline object_t * last##_fsuffix(const dlist2_t * list) { \
      return castnull2object##_fsuffix(last_dlist2(list)); \
   } \
   static inline object_t * next##_fsuffix(object_t * node) { \
      return cast2object##_fsuffix(next_dlistnode(cast2node##_fsuffix(node))); \
   } \
   static inline object_t * prev##_fsuffix(object_t * node) { \
      return cast2object##_fsuffix(prev_dlistnode(cast2node##_fsuffix(node))); \
   } \
   static inline bool isinlist##_fsuffix(object_t * node) { \
      return isinlist_dlistnode(cast2node##_fsuffix(node)); \
   } \
   static inline void insertfirst##_fsuffix(dlist2_t * list, object_t * new_node) { \
      insertfirst_dlist2(list, cast2node##_fsuffix(new_node)); \
   } \
   static inline void insertlast##_fsuffix(dlist2_t * list, object_t * new_node) { \
      insertlast_dlist2(list, cast2node##_fsuffix(new_node)); \
   } \
   static inline void insertafter##_fsuffix(dlist2_t * list, object_t * prev_node, object_t * new_node) { \
      insertafter_dlist2(list, cast2node##_fsuffix(prev_node), cast2node##_fsuffix(new_node)); \
   } \
   static inline void insertbefore##_fsuffix(dlist2_t * list, object_t * next_node, object_t * new_node) { \
      insertbefore_dlist2(list, cast2node##_fsuffix(next_node), cast2node##_fsuffix(new_node)); \
   } \
   static inline void removefirst##_fsuffix(dlist2_t * list, object_t ** removed_node) { \
      dlist_node_t* _n; \
      removefirst_dlist2(list, &_n); \
      *removed_node = cast2object##_fsuffix(_n); \
   } \
   static inline void removelast##_fsuffix(dlist2_t * list, object_t ** removed_node) { \
      dlist_node_t* _n; \
      removelast_dlist2(list, &_n); \
      *removed_node = cast2object##_fsuffix(_n); \
   } \
   static inline void remove##_fsuffix(dlist2_t * list, object_t * node) { \
      remove_dlist2(list, cast2node##_fsuffix(node)); \
   } \
   static inline void replacenode##_fsuffix(dlist2_t * list, object_t * oldnode, object_t * newnode) { \
      replacenode_dlist2(list, cast2node##_fsuffix(oldnode), cast2node##_fsuffix(newnode)); \
   } \
   static inline int removeall##_fsuffix(dlist2_t * list, struct typeadapt_t * typeadp) { \
      return removeall_dlist2(list, nodeoffset##_fsuffix(), typeadp); \
   } \
   static inline void insertlastPlist##_fsuffix(dlist2_t * list, dlist2_t * nodes) { \
      insertlastPlist_dlist2(list, nodes); \
   } \
   static inline int initfirst##_fsuffix##iterator(dlist2_iterator_t * iter, dlist2_t * list) { \
      return initfirst_dlist2iterator(iter, list); \
   } \
   static inline int initlast##_fsuffix##iterator(dlist2_iterator_t * iter, dlist2_t * list) { \
      return initlast_dlist2iterator(iter, list); \
   } \
   static inline int free##_fsuffix##iterator(dlist2_iterator_t * iter) { \
      return free_dlist2iterator(iter); \
   } \
   static inline bool next##_fsuffix##iterator(dlist2_iterator_t * iter, object_t ** node) { \
      dlist_node_t* _n = 0; \
      bool isNext = next_dlist2iterator(iter, &_n); \
      *node = castnull2object##_fsuffix(_n); \
      return isNext; \
   } \
   static inline bool prev##_fsuffix##iterator(dlist2_iterator_t * iter, object_t ** node) { \
      dlist_node_t* _n = 0; \
      bool isNext = prev_dlist2iterator(iter, &_n); \
      *node = castnull2object##_fsuffix(_n); \
      return isNext; \
   }

#endif
