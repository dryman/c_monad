/* deallocator.c ---
 *
 * Filename: deallocator.c
 * Description:
 * Author: Felix Chern
 * Maintainer:
 * Copyright: (c) 2017 Felix Chern
 * Created: Thu Mar 16 22:04:56 2017 (-0700)
 * Version:
 * Package-Requires: ()
 * Last-Updated:
 *           By:
 *     Update #: 0
 * URL:
 * Doc URL:
 * Keywords:
 * Compatibility:
 *
 */

/* Commentary:
 *
 *
 *
 */

/* Change Log:
 *
 *
 */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Code: */

#include "opic/common/op_atomic.h"
#include "opic/common/op_log.h"
#include "deallocator.h"
#include "lookup_helper.h"

OP_LOGGER_FACTORY(logger, "opic.malloc.deallocator");

void
OPHeapReleaseHSpan(HugeSpanPtr hspan)
{
  OPHeap* heap;
  uintptr_t heap_base, _addr, _addr_hpage, _addr_bmidx, _addr_bmbit, hpages;
  uint64_t mask;
  MagicPattern pattern;
  heap = ObtainOPHeap(hspan.hpage);
  heap_base = (uintptr_t)heap;

  if (&heap->hpage == hspan.hpage)
    _addr_bmidx = _addr_bmbit = 0;
  else
    {
      _addr = hspan.uintptr - heap_base;
      _addr_hpage = _addr / HPAGE_SIZE;
      _addr_bmidx = _addr_hpage / 64;
      _addr_bmbit = _addr_hpage % 64;
    }

  pattern = hspan.magic->generic.pattern;
  if (pattern == TYPED_HPAGE_PATTERN ||
      pattern == RAW_HPAGE_PATTERN ||
      hspan.magic->huge_blob.huge_pages == 1)
    {
      while (!atomic_check_in(&heap->pcard))
        ;
      atomic_fetch_and_explicit(&heap->header_bmap[_addr_bmidx],
                                ~(1UL << _addr_bmbit),
                                memory_order_relaxed);
      atomic_fetch_and_explicit(&heap->occupy_bmap[_addr_bmidx],
                                ~(1UL << _addr_bmbit),
                                memory_order_release);
      atomic_check_out(&heap->pcard);
      return;
    }
  hpages = hspan.magic->huge_blob.huge_pages;
  if (_addr_bmbit + hpages <= 64)
    {
      while (!atomic_check_in(&heap->pcard))
        ;
      mask = ~(((1UL << hpages) - 1) << _addr_bmbit);
      atomic_fetch_and_explicit(&heap->header_bmap[_addr_bmidx],
                                ~(1UL << _addr_bmbit),
                                memory_order_relaxed);
      atomic_fetch_and_explicit(&heap->occupy_bmap[_addr_bmidx],
                                mask,
                                memory_order_release);
      atomic_check_out(&heap->pcard);
      return;
    }

  while (!atomic_check_in_book(&heap->pcard))
    ;
  atomic_fetch_and_explicit(&heap->header_bmap[_addr_bmidx],
                            ~(1UL << _addr_bmbit),
                            memory_order_relaxed);
  atomic_fetch_and_explicit(&heap->occupy_bmap[_addr_bmidx],
                            (1UL << _addr_bmbit) - 1,
                            memory_order_relaxed);
  hpages -= (64 - _addr_bmbit);
  _addr_bmidx++;
  while (hpages >= 64)
    {
      atomic_store_explicit(&heap->occupy_bmap[_addr_bmidx++],
                            0UL, memory_order_relaxed);
      hpages -= 64;
    }
  if (hpages)
    {
      atomic_fetch_and_explicit(&heap->occupy_bmap[_addr_bmidx],
                                ~((1UL << hpages) - 1),
                                memory_order_relaxed);
    }
  atomic_exit_check_out(&heap->pcard);
  return;
}


/* deallocator.c ends here */