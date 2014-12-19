/* title: DualLink impl

   Implements <DualLink>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/ds/link.h
    Header file <DualLink>.

   file: C-kern/ds/link.c
    Implementation file <DualLink impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/link.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: link_t

// group: lifetime


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   link_t link  = link_FREE;
   link_t link2 = link_FREE;
   link_t link3 = link_FREE;
   linkd_t linkd  = linkd_FREE;
   linkd_t linkd2 = linkd_FREE;
   linkd_t linkd3 = linkd_FREE;
   linkd_t linkd4 = linkd_FREE;

   // === link_t ===

   // TEST link_FREE
   TEST(0 == link.link);

   // TEST init_link: link is free
   init_link(&link, &link2);
   TEST(&link2 == link.link);
   TEST(&link  == link2.link);

   // TEST init_link: link is valid
   init_link(&link, &link3);
   TEST(&link3 == link.link);
   TEST(&link  == link3.link);
   TEST(&link  == link2.link); // not changed !

   // TEST free_link: link is valid
   free_link(&link);
   TEST(0 == link.link);
   TEST(0 == link3.link);

   // TEST free_link: link is free
   TEST(0 == link.link);
   free_link(&link);
   TEST(0 == link.link);

   // TEST free_link: free link2 - the other side
   init_link(&link, &link2);
   free_link(&link2);
   TEST(0 == link.link);
   TEST(0 == link2.link);

   // === linkd_t ===

   // TEST linkd_FREE
   TEST(0 == linkd.prev);
   TEST(0 == linkd.next);

   // TEST init_linkd
   init_linkd(&linkd, &linkd2);
   TEST(&linkd2 == linkd.prev);
   TEST(&linkd2 == linkd.next);
   TEST(&linkd  == linkd2.prev);
   TEST(&linkd  == linkd2.next);
   init_linkd(&linkd, &linkd3);
   TEST(&linkd3 == linkd.prev);
   TEST(&linkd3 == linkd.next);
   TEST(&linkd  == linkd3.prev);
   TEST(&linkd  == linkd3.next);

   // TEST initprev_linkd: in chain of 2
   init_linkd(&linkd, &linkd3);
   initprev_linkd(&linkd2, &linkd3);
   TEST(&linkd3 == linkd.prev);
   TEST(&linkd2 == linkd.next);
   TEST(&linkd  == linkd2.prev);
   TEST(&linkd3 == linkd2.next);
   TEST(&linkd2 == linkd3.prev);
   TEST(&linkd  == linkd3.next);

   // TEST initprev_linkd: in chain of 3
   initprev_linkd(&linkd4, &linkd);
   TEST(&linkd4 == linkd.prev);
   TEST(&linkd2 == linkd.next);
   TEST(&linkd  == linkd2.prev);
   TEST(&linkd3 == linkd2.next);
   TEST(&linkd2 == linkd3.prev);
   TEST(&linkd4 == linkd3.next);
   TEST(&linkd3 == linkd4.prev);
   TEST(&linkd  == linkd4.next);

   // TEST initnext_linkd: in chain of 2
   init_linkd(&linkd, &linkd3);
   initnext_linkd(&linkd2, &linkd);
   TEST(&linkd3 == linkd.prev);
   TEST(&linkd2 == linkd.next);
   TEST(&linkd  == linkd2.prev);
   TEST(&linkd3 == linkd2.next);
   TEST(&linkd2 == linkd3.prev);
   TEST(&linkd  == linkd3.next);

   // TEST initnext_linkd: in chain of 3
   initnext_linkd(&linkd4, &linkd3);
   TEST(&linkd4 == linkd.prev);
   TEST(&linkd2 == linkd.next);
   TEST(&linkd  == linkd2.prev);
   TEST(&linkd3 == linkd2.next);
   TEST(&linkd2 == linkd3.prev);
   TEST(&linkd4 == linkd3.next);
   TEST(&linkd3 == linkd4.prev);
   TEST(&linkd  == linkd4.next);

   // TEST initself_linkd
   initself_linkd(&linkd);
   TEST(&linkd == linkd.prev);
   TEST(&linkd == linkd.next);
   // initself_linkd can be used to add elements without checking for valid links
   initprev_linkd(&linkd2, &linkd);
   TEST(&linkd2 == linkd.prev);
   TEST(&linkd2 == linkd.next);
   TEST(&linkd  == linkd2.prev);
   TEST(&linkd  == linkd2.next);

   // TEST free_linkd: link is already free
   linkd = (linkd_t) linkd_FREE;
   free_linkd(&linkd);
   TEST(0 == linkd.prev);
   TEST(0 == linkd.next);

   // TEST free_linkd: 2 nodes connected
   init_linkd(&linkd, &linkd2);
   free_linkd(&linkd);
   TEST(0 == linkd.prev);
   TEST(0 == linkd.next);
   TEST(0 == linkd2.prev);
   TEST(0 == linkd2.next);

   // TEST free_linkd: 3 nodes connected
   init_linkd(&linkd, &linkd2);
   initnext_linkd(&linkd3, &linkd2);
   free_linkd(&linkd);
   TEST(0 == linkd.prev);
   TEST(0 == linkd.next);
   TEST(&linkd3 == linkd2.prev);
   TEST(&linkd3 == linkd2.next);
   TEST(&linkd2 == linkd3.prev);
   TEST(&linkd2 == linkd3.next);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   link_t  link   = link_FREE;
   linkd_t linkd  = linkd_FREE;
   linkd_t linkd2 = linkd_FREE;

   // === link_t ===

   // TEST isvalid_link: link_FREE
   TEST(0 == isvalid_link(&link));

   // TEST isvalid_link: != link_FREE
   link.link = &link;
   TEST(0 != isvalid_link(&link));

   // === linkd_t ===

   // TEST isvalid_linkd: linkd_FREE
   TEST(0 == isvalid_linkd(&linkd));

   // TEST isself_linkd: linkd_FREE
   TEST(0 == isself_linkd(&linkd));

   // TEST isvalid_linkd: != linkd_FREE
   init_linkd(&linkd, &linkd2);
   TEST(0 != isvalid_linkd(&linkd));
   TEST(0 != isvalid_linkd(&linkd2));

   // TEST isself_linkd: true
   initself_linkd(&linkd);
   TEST(1 == isself_linkd(&linkd));

   // TEST isself_linkd: false
   init_linkd(&linkd, &linkd2);
   TEST(0 == isself_linkd(&linkd));
   TEST(0 == isself_linkd(&linkd2));

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   link_t link;
   link_t link2;
   link_t link3;
   linkd_t linkd[6];

   // === link_t ===

   // TEST relink_link: other side 0
   link.link  = &link2;
   link2.link = 0;
   relink_link(&link);
   TEST(&link == link2.link);

   // TEST relink_link: simulate move in memory
   link3 = link; // moved in memory
   relink_link(&link3);
   TEST(&link3 == link2.link);
   TEST(&link2 == link.link); // not changed

   // TEST unlink_link: connected
   init_link(&link, &link2);
   unlink_link(&link);
   TEST(0 == link2.link);
   TEST(&link2 == link.link); // not changed

   // === linkd_t ===

   // TEST relink_linkd
   init_linkd(&linkd[0], &linkd[1]);
   initnext_linkd(&linkd[2], &linkd[1]);
   linkd[3] = linkd[0];        // moved in memory
   relink_linkd(&linkd[3]); // adapt neighbours
   TEST(&linkd[2] == linkd[0].prev); // not changed
   TEST(&linkd[1] == linkd[0].next); // not changed
   TEST(&linkd[3] == linkd[1].prev);
   TEST(&linkd[2] == linkd[1].next);
   TEST(&linkd[1] == linkd[2].prev);
   TEST(&linkd[3] == linkd[2].next);
   TEST(&linkd[2] == linkd[3].prev);
   TEST(&linkd[1] == linkd[3].next);

   // TEST unlink0_linkd: self connected node
   initself_linkd(&linkd[0]);
   unlink0_linkd(&linkd[0]);
   TEST(0 == linkd[0].prev);
   TEST(0 == linkd[0].next);

   // TEST unlink0_linkd: 2 nodes connected
   init_linkd(&linkd[0], &linkd[2]);
   unlink0_linkd(&linkd[0]);
   TEST(0 == linkd[2].prev);
   TEST(0 == linkd[2].next);
   TEST(&linkd[2] == linkd[0].prev); // not changed
   TEST(&linkd[2] == linkd[0].next); // not changed

   // TEST unlink0_linkd: 3 nodes connected
   init_linkd(&linkd[0], &linkd[1]);
   initprev_linkd(&linkd[2], &linkd[0]);
   unlink0_linkd(&linkd[0]);
   TEST(&linkd[2] == linkd[1].prev);
   TEST(&linkd[2] == linkd[1].next);
   TEST(&linkd[1] == linkd[2].prev);
   TEST(&linkd[1] == linkd[2].next);
   TEST(&linkd[2] == linkd[0].prev); // not changed
   TEST(&linkd[1] == linkd[0].next); // not changed

   // TEST unlinkself_linkd: self connected node
   initself_linkd(&linkd[0]);
   unlinkself_linkd(&linkd[0]);
   TEST(&linkd[0] == linkd[0].prev); // links to self !!
   TEST(&linkd[0] == linkd[0].next);

   // TEST unlinkself_linkd: 2 nodes connected
   init_linkd(&linkd[0], &linkd[2]);
   unlinkself_linkd(&linkd[0]);
   TEST(&linkd[2] == linkd[2].prev); // links to self !!
   TEST(&linkd[2] == linkd[2].next);
   TEST(&linkd[2] == linkd[0].prev); // not changed
   TEST(&linkd[2] == linkd[0].next); // not changed

   // TEST unlinkself_linkd: 3 nodes connected
   init_linkd(&linkd[0], &linkd[1]);
   initprev_linkd(&linkd[2], &linkd[0]);
   unlinkself_linkd(&linkd[0]);
   TEST(&linkd[2] == linkd[1].prev);
   TEST(&linkd[2] == linkd[1].next);
   TEST(&linkd[1] == linkd[2].prev);
   TEST(&linkd[1] == linkd[2].next);
   TEST(&linkd[2] == linkd[0].prev); // not changed
   TEST(&linkd[1] == linkd[0].next); // not changed

   // TEST splice_linkd: self connected nodes
   initself_linkd(&linkd[0]);
   initself_linkd(&linkd[1]);
   splice_linkd(&linkd[0], &linkd[1]);
   TEST(&linkd[1] == linkd[0].prev);
   TEST(&linkd[1] == linkd[0].next);
   TEST(&linkd[0] == linkd[1].prev);
   TEST(&linkd[0] == linkd[1].next);

   // TEST splice_linkd: self connected node + list and vice versa
   for (int isswitch = 0; isswitch <= 1; ++isswitch) {
      initself_linkd(&linkd[0]);
      init_linkd(&linkd[1], &linkd[2]);
      splice_linkd(&linkd[isswitch], &linkd[1-isswitch]);
      TEST(&linkd[2] == linkd[0].prev);
      TEST(&linkd[1] == linkd[0].next);
      TEST(&linkd[0] == linkd[1].prev);
      TEST(&linkd[2] == linkd[1].next);
      TEST(&linkd[1] == linkd[2].prev);
      TEST(&linkd[0] == linkd[2].next);
   }

   // TEST splice_linkd: each list has 3 nodes
   init_linkd(&linkd[0], &linkd[1]);
   initnext_linkd(&linkd[2], &linkd[1]);
   init_linkd(&linkd[3], &linkd[4]);
   initnext_linkd(&linkd[5], &linkd[4]);
   splice_linkd(&linkd[0], &linkd[3]);
   for (int i = 0; i < 6; ++i) {
      int n = (i+1) % 6;
      int p = (i+5) % 6;
      TEST(&linkd[p] == linkd[i].prev);
      TEST(&linkd[n] == linkd[i].next);
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_ds_link()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_update())         goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
