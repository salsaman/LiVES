/*
Copyright � 1998. The Regents of the University of California (Regents).
All Rights Reserved.

Written by Matt Wright, The Center for New Music and Audio Technologies,
University of California, Berkeley.

Permission to use, copy, modify, distribute, and distribute modified versions
of this software and its documentation without fee and without a signed
licensing agreement, is hereby granted, provided that the above copyright
notice, this paragraph and the following two paragraphs appear in all copies,
modifications, and distributions.

IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

The OpenSound Control WWW page is
    http://www.cnmat.berkeley.edu/OpenSoundControl
*/


/* OSC-priority-queue.c
   Priority queue used by OSC time tag scheduler

   This is the most trivial implementation, an unsorted array of queued
   objects, mostly for debug purposes.

   Matt Wright, 9/17/98
*/

#include <libOSC/OSC-common.h>
#include <libOSC/OSC-timetag.h>
#include <libOSC/OSC-priority-queue.h>

#include <inttypes.h>

#define PRINT_PRIORITY_QUEUE

#ifdef DEBUG_OSC_PRIORITY_QUEUE
#define PRINT_PRIORITY_QUEUE
#endif

// salsaman
#define SUPPRESS_WARNING


#if defined(PRINT_PRIORITY_QUEUE) || defined(DEBUG_OSC_PRIORITY_QUEUE)

#include <stdio.h>
void OSCQueuePrint(OSCQueue q);
#endif

#define CAPACITY 1000


struct OSCQueueStruct {
  OSCSchedulableObject list[CAPACITY];
  int n;
  int scanIndex;
};


OSCQueue OSCNewQueue(int maxItems, void *(*InitTimeMalloc)(int numBytes)) {
  OSCQueue result;

  if (maxItems > CAPACITY) fatal_error("Increase CAPACITY in OSC-priority-queue.c");

  result = (*InitTimeMalloc)(sizeof(*result));
  if (result == 0) return 0;

  result->n = 0;

#ifdef DEBUG_OSC_PRIORITY_QUEUE
  OSCQueuePrint(result);
#endif
  return result;
}

Boolean OSCQueueInsert(OSCQueue q, OSCSchedulableObject new) {
  if (q->n == CAPACITY) return FALSE;

  q->list[q->n] = new;
  ++(q->n);
#ifdef DEBUG_OSC_PRIORITY_QUEUE
  printf("OSCQueueInsert: just inserted %p\n", new);
  OSCQueuePrint(q);
#endif
  return TRUE;
}


OSCTimeTag OSCQueueEarliestTimeTag(OSCQueue q) {
  int i;
  OSCTimeTag smallest = OSCTT_BiggestPossibleTimeTag();

  for (i = 0; i < q->n; ++i) {
    if (OSCTT_Compare(smallest, q->list[i]->timetag) > 0) {
      smallest = q->list[i]->timetag;
    }
  }

#ifdef DEBUG_OSC_PRIORITY_QUEUE
  printf("OSCQueueEarliestTimeTag: about to return %llx\n", smallest);
  OSCQueuePrint(q);
#endif
  return smallest;
}


static void RemoveElement(int goner, OSCQueue q) {
  int i;
  --(q->n);

  for (i = goner; i < q->n; ++i) {
    q->list[i] = q->list[i + 1];
  }
}

OSCSchedulableObject OSCQueueRemoveEarliest(OSCQueue q) {
  OSCSchedulableObject result;
  int i, smallestIndex;

  if (q->n == 0) {
#ifndef SUPPRESS_WARNING
    OSCWarning("OSCQueueRemoveEarliest: empty queue");
#endif
    return NULL;
  }

#ifdef DEBUG_OSC_PRIORITY_QUEUE
  printf("OSCQueueRemoveEarliest: begin\n");
  OSCQueuePrint(q);
#endif

  smallestIndex = 0;
  for (i = 1; i < q->n; ++i) {
    if (OSCTT_Compare(q->list[smallestIndex]->timetag, q->list[i]->timetag) > 0) {
      smallestIndex = i;
    }
  }

  result = q->list[smallestIndex];

  RemoveElement(smallestIndex, q);

#ifdef DEBUG_OSC_PRIORITY_QUEUE
  printf("OSCQueueRemoveEarliest: done\n");
  OSCQueuePrint(q);
#endif
  return result;
}

#ifdef PRINT_PRIORITY_QUEUE

void OSCQueuePrint(OSCQueue q) {
  int i;
  printf("OSC Priority queue at %p has %d elements:\n", q, q->n);

  for (i = 0; i < q->n; ++i) {
    printf("   list[%2d] is %p, timetag = %"PRIu64"\n", i, q->list[i], *(uint64_t *) & (q->list[i]->timetag));
  }
  printf("\n\n");
}

#endif


void OSCQueueScanStart(OSCQueue q) {
  q->scanIndex = 0;
}

OSCSchedulableObject OSCQueueScanNext(OSCQueue q) {
  if (q->scanIndex >= q->n) return 0;

  return (q->list[(q->scanIndex)++]);
}

void OSCQueueRemoveCurrentScanItem(OSCQueue q) {
  /* Remember that q->scanIndex is the index of the *next*
     item that will be returned, so the "current" item, i.e.,
     the one most recently returned by OSCQueueScanNext(),
     is q->scanIndex-1. */

  RemoveElement(q->scanIndex - 1, q);
  --(q->scanIndex);
}

void CheckWholeQueue(void) {
}
