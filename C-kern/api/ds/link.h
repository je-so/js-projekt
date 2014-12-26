/* title: DualLink

   Ein Link besteht aus zwei verbundenen Zeigern – ein Zeiger in einer Struktur
   zeigt auf den Zeiger des zugehörigen Links in der anderen Struktur
   und umgekehrt.

   A link is formed from two connected pointers – one pointer in one structure
   points to the pointer in other structure corresponding to the same link and
   vice versa.

   >  ╭────────╮      ╭────────╮
   >  | link_t ├──────┤ link_t |
   >  ╰────────╯1    1╰────────╯

   Ein Dual-Link (doppelter/double Link):

   >                    ╭─────────╮
   >    ╭───────────────┤ linkd_t ├─────────╮
   >    |           next╰─────────╯prev     |
   >    |╭─────────╮            ╭─────────╮ |
   >    ╰┤ linkd_t ├────────────┤ linkd_t ├─╯
   > prev╰─────────╯next    prev╰─────────╯next

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/ds/link.h
    Header file <DualLink>.

   file: C-kern/ds/link.c
    Implementation file <DualLink impl>.
*/
#ifndef CKERN_DS_LINK_HEADER
#define CKERN_DS_LINK_HEADER

/* typedef: struct link_t
 * Export <link_t> into global namespace. */
typedef struct link_t link_t;

/* typedef: struct linkd_t
 * Export <linkd_t> into global namespace. */
typedef struct linkd_t linkd_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_link
 * Test <link_t> functionality. */
int unittest_ds_link(void);
#endif


/* struct: link_t
 * Verbindet zwei Strukturen miteinander.
 * Enthält die eine Seite des Links.
 * Eine Änderung des einen Link hat die Änderung
 * des gegenüberliegenden zur Folge.
 *
 * Invariant(link_t * l):
 * - (l != l->link)
 * - (l->link == 0 || l->link->link == l) */
struct link_t {
   link_t * link;
};

// group: lifetime

/* define: link_FREE
 * Static initializer. */
#define link_FREE \
         { 0 }

/* function: init_link
 * Initialisiert zwei Link-Seiten zu einem verbundenen Link. */
void init_link(/*out*/link_t* __restrict__ link, /*out*/link_t* __restrict__ other);

/* function: free_link
 * Trennt einen link. Der Pointer dieses und der gegenüberliegenden Seite
 * werden auf 0 gesetzt. */
void free_link(link_t* link);

// group: query

/* function: isvalid_link
 * Return (link->link != 0). */
int isvalid_link(const link_t* link);

// group: update

/* function: relink_link
 * Verbindet link->link->link mit link.
 * Stellt die Verbindung zu dem Nachbar wieder her, nachdem link im Speicher verschoben wurde.
 *
 * Ungeprüfte Precondition:
 * o isvalid_link(link) */
void relink_link(link_t* link);

/* function: unlink_link
 * Invalidiert link->link->link, setzt ihn auf 0.
 * link selbst wird nicht verändert.
 * Dies ist eine Optimierung gegenüber <free_link>.
 *
 * Unchecked Precondition:
 * o isvalid_link(link) */
void unlink_link(const link_t* link);


/* struct: linkd_t
 * Doppelter Link erlaubt das Verlinken in einer Kette.
 * Mittels <prev> zeigt er auf den vorherigen Knoten,
 * der mittels <next> auf diesen zurückzeigt.
 * Mittels <next> zeigt er auf den nächsten Knoten,
 * der mittels <prev> auf diesen zurückzeigt.
 *
 * Dieser duale Link ist verschränkt, next ist mit prev und prev
 * mit next verbunden:
 *  >     ╭─────╮          ╭─────╮          ╭─────╮
 *  >     |  x  ├──────────┤  y  ├──────────┤  z  |
 *  > prev╰─────╯next  prev╰─────╯next  prev╰─────╯next
 *
 * Verschieben im Speicher:
 *
 * Wird ein Link im Speicher verschoben, dann muss danach <relink_linkd>
 * auf diesem Link aufgerufen werden, damit die Nachbarn wieder korrekt
 * auf diesen Link zeigen.
 *
 * Nach dem Entfernen des letzten d-Link Nachbarn:
 *
 * >           ╭─────────╮
 * >           ┤ linkd_t ├
 * >  0 == prev╰─────────╯next == 0
 *
 * Unchecked Invariant:
 * o (<prev> == 0 && <next> == 0) || (<prev> != 0 && <next> != 0)
 * */
struct linkd_t {
   // group: private fields
   /* variable: prev
    * Zeigt auf vorherigen Link Nachbarn. */
   linkd_t* prev;
   /* variable: next
    * Zeigt auf nächsten Link Nachbarn. */
   linkd_t* next;
};

// group: lifetime

/* define: linkd_FREE
 * Static initializer. */
#define linkd_FREE \
         { 0, 0 }

/* function: init_linkd
 * Initialisiert zwei Link-Seiten zu einem verbundenen Link. */
void init_linkd(/*out*/linkd_t * link, /*out*/linkd_t * other);

/* function: initprev_linkd
 * Fügt prev vor Knoten link ein. Der Link prev wird initialisiert und verbunden mit den Knoten
 * oldprev=link->prev und link. Nach Rückkehr zeigt oldprev->next auf prev, prev->prev auf oldprev
 * und prev->next auf link und link->prev auf prev. */
void initprev_linkd(/*out*/linkd_t * prev, linkd_t * link);

/* function: initnext_linkd
 * Fügt next nach Knoten link ein. Der Link next wird initialisiert und verbunden mit den Knoten
 * link und oldnext=link->next. Nach Rückkehr zeigt oldnext->prev auf next, next->next auf oldnext
 * und next->prev auf link und link->next auf next. */
void initnext_linkd(/*out*/linkd_t * next, linkd_t * link);

/* function: initself_linkd
 * Initialisiert link, so daß er auf sich selbst verlinkt.
 * Ein, Link der auf sich selbst zeigt, kann nicht im Speicher verschoben werden,
 * da <relink_linkd> nicht funktioniert. Erst der erneute Aufruf <initself_linkd>
 * nach dem Verschieben im Speicher macht ihn wieder gültig.
 * Nach der Initialisierung können mittels <initprev_linkd> bzw. <initnext_linkd> neue Knoten
 * verlinkt werden. */
void initself_linkd(/*out*/linkd_t * link);

/* function: free_linkd
 * Trennt link aus einer Link-Kette heraus.
 * link wird auf den Wert <linkd_FREE> gesetzt.
 * Falls link der vorletzte Eintrag in der Kette war (link->next==link->prev),
 * werden link->next->prev und link->next->next auch auf 0 gestzt. */
void free_linkd(linkd_t * link);

// group: query

/* function: isvalid_linkd
 * Return (link->prev != 0). */
int isvalid_linkd(const linkd_t * link);

/* function: isself_linkd
 * Return (link->prev == link). */
int isself_linkd(const linkd_t * link);

// group: update

/* function: relink_linkd
 * Stellt die Verbindung zu den Nachbarn wieder her, nachdem link im Speicher verschoben wurde.
 *
 * Unchecked Precondition:
 * o isvalid_linkd(link) */
void relink_linkd(linkd_t* link);

// TODO: remove unlink0
// TODO: rename unlinkself -> unlink (always refer to self)
// TODO: add parameter to relink_sync (oldaddr) to be able to detect self reference !!

/* function: unlink0_linkd
 * Trennt link aus einer Link-Kette heraus.
 * link selbst wird nicht verändert.
 * Dies ist eine Optimierung gegenüber <free_linkd>.
 * Falls link der vorletzte Knoten in der Kette ist,
 * dann gilt (! <isvalid_linkd>(link->next)) nach Return.
 *
 * Unchecked Precondition:
 * o isvalid_linkd(link) */
void unlink0_linkd(const link_t* link);

/* function: unlinkself_linkd
 * Trennt link aus einer Link-Kette heraus.
 * link selbst wird nicht verändert.
 * Dies ist eine Optimierung gegenüber <free_linkd>.
 * Falls link der vorletzte Knoten in der Kette ist,
 * dann gilt <isself_linkd>(link->next) nach Return.
 *
 * Unchecked Precondition:
 * o isvalid_linkd(link) */
void unlinkself_linkd(const link_t* link);

/* function: splice_linkd
 * Fügt Liste der Knoten, auf die prev zeigt, vor Knoten link ein.
 * Nach Return zeigt link->prev auf alten Wert von prev->prev und prev->prev zeigt
 * alten Wert von link->prev.
 *
 * Darstellung alte Listen:
 *
 *  > list1 + list2 representation:
 *  > ╭───────────────────────────────────╮
 *  > |╭───────╮ ╭────────╮     ╭────────╮|
 *  > ╰┤ list  ├─┤ list   ├─...─┤ list   ├╯
 *  >  | (head)| | ->next |     | ->prev |
 *  >  ╰───────╯ ╰────────╯     ╰────────╯
 *  > (first node)              (last node)
 *
 * Darstellung verbundene Liste:
 *
 *  > spliced representation:
 *  > ╭──────────────────────────────────────╮
 *  > |╭───────╮ ╭────────╮     ╭────────╮   |
 *  > ╰┤ list1 ├─┤ list1  ├─...─┤ list1  ├─╮ |
 *  >  | (head)| | ->next |     | ->prev | | |
 *  >  ╰───────╯ ╰────────╯     ╰────────╯ | |
 *  > ╭────────────────────────────────────╯ |
 *  > |╭───────╮ ╭────────╮     ╭────────╮   |
 *  > ╰┤ list2 ├─┤ list2  ├─...─┤ list2  ├───╯
 *  >  | (head)| | ->next |     | ->prev |
 *  >  ╰───────╯ ╰────────╯     ╰────────╯
 *
 * Unchecked Precondition:
 * o isvalid_linkd(link)
 * o isvalid_linkd(prev)
 * */
static inline void splice_linkd(linkd_t* list1, linkd_t* list2);


// section: inline implementation

// group: link_t

/* define: init_link
 * Implementiert <link_t.init_link>. */
#define init_link(_link, other) \
         do {                       \
            link_t * _l1 = (_link); \
            link_t * _l2 = (other); \
            _l1->link = _l2;        \
            _l2->link = _l1;        \
         } while (0)

/* define: free_link
 * Implementiert <link_t.free_link>. */
#define free_link(_link) \
         do {                       \
            link_t * _l = (_link);  \
            if (_l->link) {         \
               _l->link->link = 0;  \
            }                       \
            _l->link = 0;           \
         } while (0)

/* define: isvalid_link
 * Implementiert <link_t.isvalid_link>. */
#define isvalid_link(_link) \
         ((_link)->link != 0)

/* define: relink_link
 * Implementiert <link_t.relink_link>. */
#define relink_link(_link) \
         do {                       \
            link_t * _l = (_link);  \
            _l->link->link = _l;    \
         } while (0)

/* define: unlink_link
 * Implementiert <link_t.unlink_link>. */
#define unlink_link(_link) \
         do {                     \
            link_t* _l = (_link); \
            _l->link->link = 0;   \
         } while (0)

// group: linkd_t

/* define: free_linkd
 * Implementiert <linkd_t.free_linkd>. */
#define free_linkd(link) \
         do {                       \
            linkd_t * _l2 = (link); \
            if (_l2->prev) {        \
               unlink0_linkd(_l2);  \
            }                       \
            _l2->next = 0;          \
            _l2->prev = 0;          \
         } while (0)

/* define: init_linkd
 * Implementiert <linkd_t.init_linkd>. */
#define init_linkd(link, other) \
         do {                        \
            linkd_t * _l1 = (link);  \
            linkd_t * _l2 = (other); \
            _l1->next = _l2;         \
            _l1->prev = _l2;         \
            _l2->next = _l1;         \
            _l2->prev = _l1;         \
         } while (0)

/* define: initnext_linkd
 * Implementiert <linkd_t.initnext_linkd>. */
#define initnext_linkd(_next, link) \
         do {                          \
            linkd_t * _l = (link);     \
            linkd_t * _ne = (_next);   \
            _ne->next = _l->next;      \
            _ne->next->prev = _ne;     \
            _ne->prev = _l;            \
            _l->next = _ne;            \
         } while (0)

/* define: initprev_linkd
 * Implementiert <linkd_t.initprev_linkd>. */
#define initprev_linkd(_prev, link) \
         do {                          \
            linkd_t * _l = (link);     \
            linkd_t * _pr = (_prev);   \
            _pr->prev = _l->prev;      \
            _pr->prev->next = _pr;     \
            _pr->next = _l;            \
            _l->prev = _pr;            \
         } while (0)

/* define: initself_linkd
 * Implementiert <linkd_t.initself_linkd>. */
#define initself_linkd(link) \
         do {                       \
            linkd_t * _l = (link);  \
            _l->prev = _l;          \
            _l->next = _l;          \
         } while (0)

/* define: isvalid_linkd
 * Implementiert <linkd_t.isvalid_linkd>. */
#define isvalid_linkd(link) \
         ((link)->prev != 0)

/* define: isself_linkd
 * Implementiert <linkd_t.isself_linkd>. */
#define isself_linkd(link) \
         ( __extension__ ({             \
            const linkd_t* _l = (link); \
            _l->prev == _l;             \
         }))

/* define: relink_linkd
 * Implementiert <linkd_t.relink_linkd>. */
#define relink_linkd(link) \
         do {                       \
            linkd_t * _l = (link);  \
            _l->prev->next = _l;    \
            _l->next->prev = _l;    \
         } while (0)

/* define: splice_linkd
 * Implementiert <link_t.splice_linkd>. */
static inline void splice_linkd(linkd_t* list1, linkd_t* list2)
{
         linkd_t* l1_last = list1->prev;
         linkd_t* l2_last = list2->prev;
         list1->prev = l2_last;
         l2_last->next = list1;
         list2->prev = l1_last;
         l1_last->next = list2;
}

/* define: unlink0_linkd
 * Implementiert <link_t.unlink0_linkd>. */
#define unlink0_linkd(link) \
         do {                             \
            linkd_t* _l = (link);         \
            if (_l->prev == _l->next) {   \
               _l->next->prev = 0;        \
               _l->next->next = 0;        \
            } else {                      \
               _l->next->prev = _l->prev; \
               _l->prev->next = _l->next; \
            }                             \
         } while (0)

/* define: unlinkself_linkd
 * Implementiert <link_t.unlinkself_linkd>. */
#define unlinkself_linkd(link) \
         do {                          \
            linkd_t * _l = (link);     \
            _l->next->prev = _l->prev; \
            _l->prev->next = _l->next; \
         } while (0)

#endif
