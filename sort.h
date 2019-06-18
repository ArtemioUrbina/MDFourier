/* Copyright (c) 2010-2017 Christopher Swenson. */
/* Copyright (c) 2012 Vojtech Fried. */
/* Copyright (c) 2012 Google Inc. All Rights Reserved. */

/* MDFourier: removed stable_sort, grail_sort and sqrt_sort to silence warnigs */
/* downloaded from: https://github.com/swenson/sort/ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifndef SORT_NAME
#error "Must declare SORT_NAME"
#endif

#ifndef SORT_TYPE
#error "Must declare SORT_TYPE"
#endif

#ifndef SORT_CMP
#define SORT_CMP(x, y)  ((x) < (y) ? -1 : ((x) == (y) ? 0 : 1))
#endif

#ifndef TIM_SORT_STACK_SIZE
#define TIM_SORT_STACK_SIZE 128
#endif

#ifndef TIM_SORT_MIN_GALLOP
#define TIM_SORT_MIN_GALLOP 7
#endif

#ifndef SORT_SWAP
#define SORT_SWAP(x,y) {SORT_TYPE _sort_swap_temp = (x); (x) = (y); (y) = _sort_swap_temp;}
#endif


/* Common, type-agnostic functions and constants that we don't want to declare twice. */
#ifndef SORT_COMMON_H
#define SORT_COMMON_H

#ifndef MAX
#define MAX(x,y) (((x) > (y) ? (x) : (y)))
#endif

#ifndef MIN
#define MIN(x,y) (((x) < (y) ? (x) : (y)))
#endif

static int compute_minrun(const uint64_t);

/* From http://oeis.org/classic/A102549 */
static const uint64_t shell_gaps[48] = {1, 4, 10, 23, 57, 132, 301, 701, 1750, 4376, 10941, 27353, 68383, 170958, 427396, 1068491, 2671228, 6678071, 16695178, 41737946, 104344866, 260862166, 652155416, 1630388541, 4075971353LL, 10189928383LL, 25474820958LL, 63687052396LL, 159217630991LL, 398044077478LL, 995110193696LL, 2487775484241LL, 6219438710603LL, 15548596776508LL, 38871491941271LL, 97178729853178LL, 242946824632946LL, 607367061582366LL, 1518417653955916LL, 3796044134889791LL, 9490110337224478LL, 23725275843061196LL, 59313189607652991LL, 148282974019132478LL, 370707435047831196LL, 926768587619577991LL, 2316921469048944978LL, 5792303672622362446LL};

#ifndef CLZ
#if defined(__GNUC__) && ((__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__GNUC__ > 3))
#define CLZ __builtin_clzll
#else

static int clzll(uint64_t);

/* adapted from Hacker's Delight */
static int clzll(uint64_t x) {
  int n;

  if (x == 0) {
    return 64;
  }

  n = 0;

  if (x <= 0x00000000FFFFFFFFL) {
    n = n + 32;
    x = x << 32;
  }

  if (x <= 0x0000FFFFFFFFFFFFL) {
    n = n + 16;
    x = x << 16;
  }

  if (x <= 0x00FFFFFFFFFFFFFFL) {
    n = n + 8;
    x = x << 8;
  }

  if (x <= 0x0FFFFFFFFFFFFFFFL) {
    n = n + 4;
    x = x << 4;
  }

  if (x <= 0x3FFFFFFFFFFFFFFFL) {
    n = n + 2;
    x = x << 2;
  }

  if (x <= 0x7FFFFFFFFFFFFFFFL) {
    n = n + 1;
  }

  return n;
}

#define CLZ clzll
#endif
#endif

static __inline int compute_minrun(const uint64_t size) {
  const int top_bit = 64 - CLZ(size);
  const int shift = MAX(top_bit, 6) - 6;
  const int minrun = size >> shift;
  const uint64_t mask = (1ULL << shift) - 1;

  if (mask & size) {
    return minrun + 1;
  }

  return minrun;
}

static __inline size_t rbnd(size_t len) {
  int k;

  if (len < 16) {
    return 2;
  }

  k = 62 - CLZ(len);
  return 1ULL << ((2 * k) / 3);
}

#endif /* SORT_COMMON_H */

#define SORT_CONCAT(x, y) x ## _ ## y
#define SORT_MAKE_STR1(x, y) SORT_CONCAT(x,y)
#define SORT_MAKE_STR(x) SORT_MAKE_STR1(SORT_NAME,x)

#define BINARY_INSERTION_FIND          SORT_MAKE_STR(binary_insertion_find)
#define BINARY_INSERTION_SORT_START    SORT_MAKE_STR(binary_insertion_sort_start)
#define BINARY_INSERTION_SORT          SORT_MAKE_STR(binary_insertion_sort)
#define REVERSE_ELEMENTS               SORT_MAKE_STR(reverse_elements)
#define COUNT_RUN                      SORT_MAKE_STR(count_run)
#define CHECK_INVARIANT                SORT_MAKE_STR(check_invariant)
#define TIM_SORT                       SORT_MAKE_STR(tim_sort)
#define TIM_SORT_GALLOP                SORT_MAKE_STR(tim_sort_gallop)
#define TIM_SORT_RESIZE                SORT_MAKE_STR(tim_sort_resize)
#define TIM_SORT_MERGE                 SORT_MAKE_STR(tim_sort_merge)
#define TIM_SORT_MERGE_LEFT            SORT_MAKE_STR(tim_sort_merge_left)
#define TIM_SORT_MERGE_RIGHT           SORT_MAKE_STR(tim_sort_merge_right)
#define TIM_SORT_COLLAPSE              SORT_MAKE_STR(tim_sort_collapse)
#define HEAP_SORT                      SORT_MAKE_STR(heap_sort)
#define MEDIAN                         SORT_MAKE_STR(median)
#define QUICK_SORT                     SORT_MAKE_STR(quick_sort)
#define MERGE_SORT                     SORT_MAKE_STR(merge_sort)
#define MERGE_SORT_IN_PLACE            SORT_MAKE_STR(merge_sort_in_place)
#define MERGE_SORT_IN_PLACE_RMERGE     SORT_MAKE_STR(merge_sort_in_place_rmerge)
#define MERGE_SORT_IN_PLACE_BACKMERGE  SORT_MAKE_STR(merge_sort_in_place_backmerge)
#define MERGE_SORT_IN_PLACE_FRONTMERGE SORT_MAKE_STR(merge_sort_in_place_frontmerge)
#define MERGE_SORT_IN_PLACE_ASWAP      SORT_MAKE_STR(merge_sort_in_place_aswap)
#define SELECTION_SORT                 SORT_MAKE_STR(selection_sort)
#define SHELL_SORT                     SORT_MAKE_STR(shell_sort)
#define QUICK_SORT_PARTITION           SORT_MAKE_STR(quick_sort_partition)
#define QUICK_SORT_RECURSIVE           SORT_MAKE_STR(quick_sort_recursive)
#define HEAP_SIFT_DOWN                 SORT_MAKE_STR(heap_sift_down)
#define HEAPIFY                        SORT_MAKE_STR(heapify)
#define TIM_SORT_RUN_T                 SORT_MAKE_STR(tim_sort_run_t)
#define TEMP_STORAGE_T                 SORT_MAKE_STR(temp_storage_t)
#define PUSH_NEXT                      SORT_MAKE_STR(push_next)
#define BUBBLE_SORT                    SORT_MAKE_STR(bubble_sort)

#ifndef MAX
#define MAX(x,y) (((x) > (y) ? (x) : (y)))
#endif
#ifndef MIN
#define MIN(x,y) (((x) < (y) ? (x) : (y)))
#endif

typedef struct {
  size_t start;
  size_t length;
} TIM_SORT_RUN_T;


void SHELL_SORT(SORT_TYPE *dst, const size_t size);
void BINARY_INSERTION_SORT(SORT_TYPE *dst, const size_t size);
void HEAP_SORT(SORT_TYPE *dst, const size_t size);
void QUICK_SORT(SORT_TYPE *dst, const size_t size);
void MERGE_SORT(SORT_TYPE *dst, const size_t size);
void MERGE_SORT_IN_PLACE(SORT_TYPE *dst, const size_t size);
void SELECTION_SORT(SORT_TYPE *dst, const size_t size);
void TIM_SORT(SORT_TYPE *dst, const size_t size);
void BUBBLE_SORT(SORT_TYPE *dst, const size_t size);


/* Shell sort implementation based on Wikipedia article
   http://en.wikipedia.org/wiki/Shell_sort
*/
void SHELL_SORT(SORT_TYPE *dst, const size_t size) {
  /* don't bother sorting an array of size 0 or 1 */
  /* TODO: binary search to find first gap? */
  int inci = 47;
  size_t inc = shell_gaps[inci];
  size_t i;

  if (size <= 1) {
    return;
  }

  while (inc > (size >> 1)) {
    inc = shell_gaps[--inci];
  }

  while (1) {
    for (i = inc; i < size; i++) {
      SORT_TYPE temp = dst[i];
      size_t j = i;

      while ((j >= inc) && (SORT_CMP(dst[j - inc], temp) > 0)) {
        dst[j] = dst[j - inc];
        j -= inc;
      }

      dst[j] = temp;
    }

    if (inc == 1) {
      break;
    }

    inc = shell_gaps[--inci];
  }
}

/* Function used to do a binary search for binary insertion sort */
static __inline size_t BINARY_INSERTION_FIND(SORT_TYPE *dst, const SORT_TYPE x,
    const size_t size) {
  size_t l, c, r;
  SORT_TYPE cx;
  l = 0;
  r = size - 1;
  c = r >> 1;

  /* check for out of bounds at the beginning. */
  if (SORT_CMP(x, dst[0]) < 0) {
    return 0;
  } else if (SORT_CMP(x, dst[r]) > 0) {
    return r;
  }

  cx = dst[c];

  while (1) {
    const int val = SORT_CMP(x, cx);

    if (val < 0) {
      if (c - l <= 1) {
        return c;
      }

      r = c;
    } else { /* allow = for stability. The binary search favors the right. */
      if (r - c <= 1) {
        return c + 1;
      }

      l = c;
    }

    c = l + ((r - l) >> 1);
    cx = dst[c];
  }
}

/* Binary insertion sort, but knowing that the first "start" entries are sorted.  Used in timsort. */
static void BINARY_INSERTION_SORT_START(SORT_TYPE *dst, const size_t start, const size_t size) {
  size_t i;

  for (i = start; i < size; i++) {
    size_t j;
    SORT_TYPE x;
    size_t location;

    /* If this entry is already correct, just move along */
    if (SORT_CMP(dst[i - 1], dst[i]) <= 0) {
      continue;
    }

    /* Else we need to find the right place, shift everything over, and squeeze in */
    x = dst[i];
    location = BINARY_INSERTION_FIND(dst, x, i);

    for (j = i - 1; j >= location; j--) {
      dst[j + 1] = dst[j];

      if (j == 0) { /* check edge case because j is unsigned */
        break;
      }
    }

    dst[location] = x;
  }
}

/* Binary insertion sort */
void BINARY_INSERTION_SORT(SORT_TYPE *dst, const size_t size) {
  /* don't bother sorting an array of size <= 1 */
  if (size <= 1) {
    return;
  }

  BINARY_INSERTION_SORT_START(dst, 1, size);
}

/* Selection sort */
void SELECTION_SORT(SORT_TYPE *dst, const size_t size) {
  size_t i, j;

  /* don't bother sorting an array of size <= 1 */
  if (size <= 1) {
    return;
  }

  for (i = 0; i < size; i++) {
    for (j = i + 1; j < size; j++) {
      if (SORT_CMP(dst[j], dst[i]) < 0) {
        SORT_SWAP(dst[i], dst[j]);
      }
    }
  }
}

/* In-place mergesort */
void MERGE_SORT_IN_PLACE_ASWAP(SORT_TYPE * dst1, SORT_TYPE * dst2, size_t len) {
  do {
    SORT_SWAP(*dst1, *dst2);
    dst1++;
    dst2++;
  } while (--len);
}

void MERGE_SORT_IN_PLACE_FRONTMERGE(SORT_TYPE *dst1, size_t l1, SORT_TYPE *dst2, size_t l2) {
  SORT_TYPE *dst0 = dst2 - l1;

  if (SORT_CMP(dst1[l1 - 1], dst2[0]) <= 0) {
    MERGE_SORT_IN_PLACE_ASWAP(dst1, dst0, l1);
    return;
  }

  do {
    while (SORT_CMP(*dst2, *dst1) > 0) {
      SORT_SWAP(*dst1, *dst0);
      dst1++;
      dst0++;

      if (--l1 == 0) {
        return;
      }
    }

    SORT_SWAP(*dst2, *dst0);
    dst2++;
    dst0++;
  } while (--l2);

  do {
    SORT_SWAP(*dst1, *dst0);
    dst1++;
    dst0++;
  } while (--l1);
}

size_t MERGE_SORT_IN_PLACE_BACKMERGE(SORT_TYPE * dst1, size_t l1, SORT_TYPE * dst2, size_t l2) {
  size_t res;
  SORT_TYPE *dst0 = dst2 + l1;

  if (SORT_CMP(dst1[1 - l1], dst2[0]) >= 0) {
    MERGE_SORT_IN_PLACE_ASWAP(dst1 - l1 + 1, dst0 - l1 + 1, l1);
    return l1;
  }

  do {
    while (SORT_CMP(*dst2, *dst1) < 0) {
      SORT_SWAP(*dst1, *dst0);
      dst1--;
      dst0--;

      if (--l1 == 0) {
        return 0;
      }
    }

    SORT_SWAP(*dst2, *dst0);
    dst2--;
    dst0--;
  } while (--l2);

  res = l1;

  do {
    SORT_SWAP(*dst1, *dst0);
    dst1--;
    dst0--;
  } while (--l1);

  return res;
}

/* merge dst[p0..p1) by buffer dst[p1..p1+r) */
void MERGE_SORT_IN_PLACE_RMERGE(SORT_TYPE *dst, size_t len, size_t lp, size_t r) {
  size_t i, lq;
  int cv;

  if (SORT_CMP(dst[lp], dst[lp - 1]) >= 0) {
    return;
  }

  lq = lp;

  for (i = 0; i < len; i += r) {
    /* select smallest dst[p0+n*r] */
    size_t q = i, j;

    for (j = lp; j <= lq; j += r) {
      cv = SORT_CMP(dst[j], dst[q]);

      if (cv == 0) {
        cv = SORT_CMP(dst[j + r - 1], dst[q + r - 1]);
      }

      if (cv < 0) {
        q = j;
      }
    }

    if (q != i) {
      MERGE_SORT_IN_PLACE_ASWAP(dst + i, dst + q, r); /* swap it with current position */

      if (q == lq && q < (len - r)) {
        lq += r;
      }
    }

    if (i != 0 && SORT_CMP(dst[i], dst[i - 1]) < 0) {
      MERGE_SORT_IN_PLACE_ASWAP(dst + len, dst + i, r); /* swap current position with buffer */
      MERGE_SORT_IN_PLACE_BACKMERGE(dst + (len + r - 1), r, dst + (i - 1),
                                    r);  /* buffer :merge: dst[i-r..i) -> dst[i-r..i+r) */
    }

    if (lp == i) {
      lp += r;
    }
  }
}

/* In-place Merge Sort implementation. (c)2012, Andrey Astrelin, astrelin@tochka.ru */
void MERGE_SORT_IN_PLACE(SORT_TYPE *dst, const size_t len) {
  /* don't bother sorting an array of size <= 1 */
  size_t r = rbnd(len);
  size_t lr = (len / r - 1) * r;
  SORT_TYPE *dst1 = dst - 1;
  size_t p, m, q, q1, p0;

  if (len <= 1) {
    return;
  }

  if (len < 16) {
    BINARY_INSERTION_SORT(dst, len);
    return;
  }

  for (p = 2; p <= lr; p += 2) {
    dst1 += 2;

    if (SORT_CMP(dst1[0], dst1[-1]) < 0) {
      SORT_SWAP(dst1[0], dst1[-1]);
    }

    if (p & 2) {
      continue;
    }

    m = len - p;
    q = 2;

    while ((p & q) == 0) {
      if (SORT_CMP(dst1[1 - q], dst1[-q]) < 0) {
        break;
      }

      q *= 2;
    }

    if (p & q) {
      continue;
    }

    if (q < m) {
      p0 = len - q;
      MERGE_SORT_IN_PLACE_ASWAP(dst + p - q, dst + p0, q);

      for (;;) {
        q1 = 2 * q;

        if ((q1 > m) || (p & q1)) {
          break;
        }

        p0 = len - q1;
        MERGE_SORT_IN_PLACE_FRONTMERGE(dst + (p - q1), q, dst + p0 + q, q);
        q = q1;
      }

      MERGE_SORT_IN_PLACE_BACKMERGE(dst + (len - 1), q, dst1 - q, q);
      q *= 2;
    }

    q1 = q;

    while (q1 > m) {
      q1 /= 2;
    }

    while ((q & p) == 0) {
      q *= 2;
      MERGE_SORT_IN_PLACE_RMERGE(dst + (p - q), q, q / 2, q1);
    }
  }

  q1 = 0;

  for (q = r; q < lr; q *= 2) {
    if ((lr & q) != 0) {
      q1 += q;

      if (q1 != q) {
        MERGE_SORT_IN_PLACE_RMERGE(dst + (lr - q1), q1, q, r);
      }
    }
  }

  m = len - lr;
  MERGE_SORT_IN_PLACE(dst + lr, m);
  MERGE_SORT_IN_PLACE_ASWAP(dst, dst + lr, m);
  m += MERGE_SORT_IN_PLACE_BACKMERGE(dst + (m - 1), m, dst + (lr - 1), lr - m);
  MERGE_SORT_IN_PLACE(dst, m);
}

/* Standard merge sort */
void MERGE_SORT(SORT_TYPE *dst, const size_t size) {
  SORT_TYPE *newdst;
  const size_t middle = size / 2;
  size_t out = 0;
  size_t i = 0;
  size_t j = middle;

  /* don't bother sorting an array of size <= 1 */
  if (size <= 1) {
    return;
  }

  if (size < 16) {
    BINARY_INSERTION_SORT(dst, size);
    return;
  }

  MERGE_SORT(dst, middle);
  MERGE_SORT(&dst[middle], size - middle);
  newdst = (SORT_TYPE *) malloc(size * sizeof(SORT_TYPE));

  while (out != size) {
    if (i < middle) {
      if (j < size) {
        if (SORT_CMP(dst[i], dst[j]) <= 0) {
          newdst[out] = dst[i++];
        } else {
          newdst[out] = dst[j++];
        }
      } else {
        newdst[out] = dst[i++];
      }
    } else {
      newdst[out] = dst[j++];
    }

    out++;
  }

  memcpy(dst, newdst, size * sizeof(SORT_TYPE));
  free(newdst);
}


/* Quick sort: based on wikipedia */

static __inline size_t QUICK_SORT_PARTITION(SORT_TYPE *dst, const size_t left,
    const size_t right, const size_t pivot) {
  SORT_TYPE value = dst[pivot];
  size_t index = left;
  size_t i;
  int not_all_same = 0;
  /* move the pivot to the right */
  SORT_SWAP(dst[pivot], dst[right]);

  for (i = left; i < right; i++) {
    int cmp = SORT_CMP(dst[i], value);
    /* check if everything is all the same */
    not_all_same |= cmp;

    if (cmp < 0) {
      SORT_SWAP(dst[i], dst[index]);
      index++;
    }
  }

  SORT_SWAP(dst[right], dst[index]);

  /* avoid degenerate case */
  if (not_all_same == 0) {
    return SIZE_MAX;
  }

  return index;
}

/* Return the median index of the objects at the three indices. */
static __inline size_t MEDIAN(const SORT_TYPE *dst, const size_t a, const size_t b,
                              const size_t c) {
  const int AB = SORT_CMP(dst[a], dst[b]) < 0;

  if (AB) {
    /* a < b */
    const int BC = SORT_CMP(dst[b], dst[c]) < 0;

    if (BC) {
      /* a < b < c */
      return b;
    } else {
      /* a < b, c < b */
      const int AC = SORT_CMP(dst[a], dst[c]) < 0;

      if (AC) {
        /* a < c < b */
        return c;
      } else {
        /* c < a < b */
        return a;
      }
    }
  } else {
    /* b < a */
    const int AC = SORT_CMP(dst[a], dst[b]) < 0;

    if (AC) {
      /* b < a < c */
      return a;
    } else {
      /* b < a, c < a */
      const int BC = SORT_CMP(dst[b], dst[c]) < 0;

      if (BC) {
        /* b < c < a */
        return c;
      } else {
        /* c < b < a */
        return b;
      }
    }
  }
}

static void QUICK_SORT_RECURSIVE(SORT_TYPE *dst, const size_t left, const size_t right) {
  size_t pivot;
  size_t new_pivot;

  if (right <= left) {
    return;
  }

  if ((right - left + 1U) < 16U) {
    BINARY_INSERTION_SORT(&dst[left], right - left + 1U);
    return;
  }

  pivot = left + ((right - left) >> 1);
  /* this seems to perform worse by a small amount... ? */
  /* pivot = MEDIAN(dst, left, pivot, right); */
  new_pivot = QUICK_SORT_PARTITION(dst, left, right, pivot);

  /* check for partition all equal */
  if (new_pivot == SIZE_MAX) {
    return;
  }

  QUICK_SORT_RECURSIVE(dst, left, new_pivot - 1U);
  QUICK_SORT_RECURSIVE(dst, new_pivot + 1U, right);
}

void QUICK_SORT(SORT_TYPE *dst, const size_t size) {
  /* don't bother sorting an array of size 1 */
  if (size <= 1) {
    return;
  }

  QUICK_SORT_RECURSIVE(dst, 0U, size - 1U);
}


/* timsort implementation, based on timsort.txt */

static __inline void REVERSE_ELEMENTS(SORT_TYPE *dst, size_t start, size_t end) {
  while (1) {
    if (start >= end) {
      return;
    }

    SORT_SWAP(dst[start], dst[end]);
    start++;
    end--;
  }
}

static size_t COUNT_RUN(SORT_TYPE *dst, const size_t start, const size_t size) {
  size_t curr;

  if (size - start == 1) {
    return 1;
  }

  if (start >= size - 2) {
    if (SORT_CMP(dst[size - 2], dst[size - 1]) > 0) {
      SORT_SWAP(dst[size - 2], dst[size - 1]);
    }

    return 2;
  }

  curr = start + 2;

  if (SORT_CMP(dst[start], dst[start + 1]) <= 0) {
    /* increasing run */
    while (1) {
      if (curr == size - 1) {
        break;
      }

      if (SORT_CMP(dst[curr - 1], dst[curr]) > 0) {
        break;
      }

      curr++;
    }

    return curr - start;
  } else {
    /* decreasing run */
    while (1) {
      if (curr == size - 1) {
        break;
      }

      if (SORT_CMP(dst[curr - 1], dst[curr]) <= 0) {
        break;
      }

      curr++;
    }

    /* reverse in-place */
    REVERSE_ELEMENTS(dst, start, curr - 1);
    return curr - start;
  }
}

static int CHECK_INVARIANT(TIM_SORT_RUN_T *stack, const int stack_curr) {
  size_t A, B, C;

  if (stack_curr < 2) {
    return 1;
  }

  if (stack_curr == 2) {
    const size_t A1 = stack[stack_curr - 2].length;
    const size_t B1 = stack[stack_curr - 1].length;

    if (A1 <= B1) {
      return 0;
    }

    return 1;
  }

  A = stack[stack_curr - 3].length;
  B = stack[stack_curr - 2].length;
  C = stack[stack_curr - 1].length;

  if ((A <= B + C) || (B <= C)) {
    return 0;
  }

  return 1;
}

typedef struct {
  size_t alloc;
  SORT_TYPE *storage;
} TEMP_STORAGE_T;

static void TIM_SORT_RESIZE(TEMP_STORAGE_T *store, const size_t new_size) {
  if ((store->storage == NULL) || (store->alloc < new_size)) {
    SORT_TYPE *tempstore = (SORT_TYPE *)realloc(store->storage, new_size * sizeof(SORT_TYPE));

    if (tempstore == NULL) {
      fprintf(stderr, "Error allocating temporary storage for tim sort: need %lu bytes",
              (unsigned long)(sizeof(SORT_TYPE) * new_size));
      exit(1);
    }

    store->storage = tempstore;
    store->alloc = new_size;
  }
}


static size_t TIM_SORT_GALLOP(SORT_TYPE *dst, const size_t size, const SORT_TYPE key, size_t anchor,
                              int right) {
  int last_ofs = 0;
  int ofs, max_ofs, ofs_sign, cmp;
  size_t  l, c, r;
  cmp = SORT_CMP(key, dst[anchor]);

  if (cmp < 0 || (!right && cmp == 0)) {
    /* short cut */
    if (anchor == 0) {
      return 0;
    }

    ofs = -1;
    ofs_sign = -1;
    max_ofs = -anchor; /* ensure anchor+max_ofs is valid idx */
  } else {
    if (anchor == size - 1) {
      return size;
    }

    ofs = 1;
    ofs_sign = 1;
    max_ofs = size - anchor - 1;
  }

  for (;;) {
    /* deal with overflow */
    if (max_ofs / ofs <= 1) {
      ofs = max_ofs;

      if (ofs < 0) {
        cmp = SORT_CMP(key, dst[0]);

        if ((right && cmp < 0) || (!right && cmp <= 0)) {
          return 0;
        }
      } else {
        cmp = SORT_CMP(dst[size - 1], key);

        if ((right && cmp <= 0) || (!right && cmp < 0)) {
          return size;
        }
      }

      break;
    }

    c = anchor + ofs;
    /* right, 0<ofs:  dst[anchor+last_ofs] <=  key  <  dst[anchor+ofs]      */
    /* left,  0<ofs:  dst[anchor+last_ofs] <   key  <= dst[anchor+ofs]      */
    /* right, ofs<0:  dst[anchor+ofs]      <=  key  <  dst[anchor+last_ofs] */
    /* left,  ofs<0:  dst[anchor+ofs]      <   key  <= dst[anchor+last_ofs] */
    cmp = SORT_CMP(key, dst[c]);

    if (0 < ofs) {
      if ((right && cmp < 0) || (!right && cmp <= 0)) {
        break;
      }
    } else {
      if ((right && 0 <= cmp) || (!right && 0 < cmp)) {
        break;
      }
    }

    last_ofs = ofs;
    ofs = (ofs << 1) + ofs_sign;
  }

  /* key in region (l, r) , both l and r have already been compared */
  if (ofs < 0) {
    l = anchor + ofs;
    r = anchor + last_ofs;
  } else {
    l = anchor + last_ofs;
    r = anchor + ofs;
  }

  while (1 < r - l) {
    c = l + ((r - l) >> 1);
    cmp = SORT_CMP(key, dst[c]);

    if ((right && cmp < 0) || (!right && cmp <= 0)) {
      r = c;
    } else {
      l = c;
    }
  }

  return r;
}



static void TIM_SORT_MERGE_LEFT(SORT_TYPE *A_src, SORT_TYPE *B_src, const size_t A, const size_t B,
                                SORT_TYPE* storage, int *min_gallop_p) {
  size_t pdst, pa, pb, k;
  int a_count, b_count;
  int min_gallop = *min_gallop_p;
  SORT_TYPE *dst = A_src;
  memcpy(storage, dst, A * sizeof(SORT_TYPE));
  A_src = storage;
  pdst = pa = pb = 0;
  /* first element must in B, otherwise skipped in the caller  */
  dst[pdst++] = B_src[pb++];

  if (B == 1) {
    goto copyA;
  }

  for (;;) {
    a_count = b_count = 0;

    for (;;) {
      if (SORT_CMP(A_src[pa], B_src[pb]) <= 0) {
        dst[pdst++] = A_src[pa++];
        ++a_count;
        b_count = 0;

        /* No need to check if pa == A because the last element must be in A
         * so pb will reach to B first. You can check pa == A-1 and do
         * some optimization if you wish.*/
        if (min_gallop <= a_count) {
          break;
        }
      } else {
        dst[pdst++] = B_src[pb++];
        ++b_count;
        a_count = 0;

        if (pb == B) {
          goto copyA;
        }

        if (min_gallop <= b_count) {
          break;
        }
      }
    }

    ++min_gallop;

    for (;;) {
      if (min_gallop != 0) {
        min_gallop --;
      }

      k = TIM_SORT_GALLOP(&A_src[pa], A - pa, B_src[pb], 0, 1);
      memcpy(&dst[pdst], &A_src[pa], k * sizeof(SORT_TYPE));
      pdst += k;
      pa += k;
      /* now we know the next must be in B */
      dst[pdst++] = B_src[pb++];

      if (pb == B) {
        goto copyA;
      }

      if (a_count && k < TIM_SORT_MIN_GALLOP) {
        ++min_gallop;
        break;
      }

      k = TIM_SORT_GALLOP(&B_src[pb], B - pb, A_src[pa], 0, 0);
      memmove(&dst[pdst], &B_src[pb], k * sizeof(SORT_TYPE));
      pdst += k;
      pb += k;

      if (pb == B) {
        goto copyA;
      }

      dst[pdst++] = A_src[pa++];

      if (b_count && k < TIM_SORT_MIN_GALLOP) {
        ++min_gallop;
        break;
      }
    }
  }

copyA:
  memcpy(&dst[pdst], &A_src[pa], (A - pa) * sizeof(SORT_TYPE));
  *min_gallop_p = min_gallop;
  return;
}


static void TIM_SORT_MERGE_RIGHT(SORT_TYPE *A_src, SORT_TYPE *B_src, const size_t A, const size_t B,
                                 SORT_TYPE* storage, int *min_gallop_p) {
  size_t k;
  int pdst, pa, pb, a_count, b_count;
  int min_gallop = *min_gallop_p;
  SORT_TYPE *dst = A_src;
  pa = A - 1;
  pb = B - 1;
  pdst = A + B - 1;
  memcpy(storage, B_src, B * sizeof(SORT_TYPE));
  B_src = storage;
  /* last element must in A, otherwise skipped in the caller  */
  dst[pdst--] = A_src[pa--];

  if (A == 1) {
    goto copyB;
  }

  for (;;) {
    a_count = b_count = 0;

    for (;;) {
      if (SORT_CMP(A_src[pa], B_src[pb]) <= 0) {
        dst[pdst--] = B_src[pb--];
        ++b_count;
        a_count = 0;

        if (min_gallop <= b_count) {
          break;
        }

        /* No need to check if pb == -1 because the first element must be in B
         * so pa will reach to -1 first. You can check pb == 0 and do
         * some optimization if you wish.*/
      } else {
        dst[pdst--] = A_src[pa--];
        ++a_count;
        b_count = 0;

        if (pa == -1) {
          goto copyB;
        }

        if (min_gallop <= a_count) {
          break;
        }
      }
    }

    ++min_gallop;

    for (;;) {
      if (min_gallop != 0) {
        min_gallop --;
      }

      k = TIM_SORT_GALLOP(A_src, pa + 1, B_src[pb], pa, 1);
      /* Understand the margin by considering k==0 */
      memmove(&dst[pb + k + 1], &A_src[k], (pa + 1 - k) * sizeof(SORT_TYPE));
      pdst = pb + k;
      pa = k - 1;

      if (pa == -1) {
        goto copyB;
      }

      /* now we know the next must be in B */
      dst[pdst--] = B_src[pb--];

      if (a_count && pa + 1 - k < TIM_SORT_MIN_GALLOP) {
        ++min_gallop;
        break;
      }

      k = TIM_SORT_GALLOP(B_src, pb + 1, A_src[pa], pb, 0);
      memcpy(&dst[pa + k + 1], &B_src[k], (pb + 1 - k) * sizeof(SORT_TYPE));
      pdst = pa + k;
      pb = k - 1;
      dst[pdst--] = A_src[pa--];

      if (pa == -1) {
        goto copyB;
      }

      if (b_count && pb + 1 - k < TIM_SORT_MIN_GALLOP) {
        ++min_gallop;
        break;
      }
    }
  }

copyB:
  memcpy(dst, B_src, (pb + 1) * sizeof(SORT_TYPE));
  *min_gallop_p = min_gallop;
  return;
}


static void TIM_SORT_MERGE(SORT_TYPE *dst, const TIM_SORT_RUN_T *stack, const int stack_curr,
                           TEMP_STORAGE_T *store, int* min_gallop_p) {
  size_t A = stack[stack_curr - 2].length;
  size_t B = stack[stack_curr - 1].length;
  size_t A_start = stack[stack_curr - 2].start;
  size_t B_start = stack[stack_curr - 1].start;
  SORT_TYPE *storage;
  size_t k;
  /* A[k-1] <= B[0] < A[k] */
  k = TIM_SORT_GALLOP(&dst[A_start], A, dst[B_start], 0, 1);
  A_start += k;
  A -= k;

  if (A == 0) {
    *min_gallop_p /= 2;
    return;
  }

  /* B[k-1] < A[A-1] <= B[k] */
  k = TIM_SORT_GALLOP(&dst[B_start], B, dst[B_start - 1], B - 1, 0);
  B = k;
  TIM_SORT_RESIZE(store, MIN(A, B));
  storage = store->storage;

  if (A < B) {
    TIM_SORT_MERGE_LEFT(&dst[A_start], &dst[B_start], A, B, storage, min_gallop_p);
  } else {
    TIM_SORT_MERGE_RIGHT(&dst[A_start], &dst[B_start], A, B, storage, min_gallop_p);
  }
}

static int TIM_SORT_COLLAPSE(SORT_TYPE *dst, TIM_SORT_RUN_T *stack, int stack_curr,
                             TEMP_STORAGE_T *store, const size_t size, int* min_gallop_p) {
  while (1) {
    size_t A, B, C, D;
    int ABC, BCD, CD;

    /* if the stack only has one thing on it, we are done with the collapse */
    if (stack_curr <= 1) {
      break;
    }

    /* if this is the last merge, just do it */
    if ((stack_curr == 2) && (stack[0].length + stack[1].length == size)) {
      TIM_SORT_MERGE(dst, stack, stack_curr, store, min_gallop_p);
      stack[0].length += stack[1].length;
      stack_curr--;
      break;
    }
    /* check if the invariant is off for a stack of 2 elements */
    else if ((stack_curr == 2) && (stack[0].length <= stack[1].length)) {
      TIM_SORT_MERGE(dst, stack, stack_curr, store, min_gallop_p);
      stack[0].length += stack[1].length;
      stack_curr--;
      break;
    } else if (stack_curr == 2) {
      break;
    }

    B = stack[stack_curr - 3].length;
    C = stack[stack_curr - 2].length;
    D = stack[stack_curr - 1].length;

    if (stack_curr >= 4) {
      A = stack[stack_curr - 4].length;
      ABC = (A <= B + C);
    } else {
      ABC = 0;
    }

    BCD = (B <= C + D) || ABC;
    CD = (C <= D);

    /* Both invariants are good */
    if (!BCD && !CD) {
      break;
    }

    /* left merge */
    if (BCD && !CD) {
      TIM_SORT_MERGE(dst, stack, stack_curr - 1, store, min_gallop_p);
      stack[stack_curr - 3].length += stack[stack_curr - 2].length;
      stack[stack_curr - 2] = stack[stack_curr - 1];
      stack_curr--;
    } else {
      /* right merge */
      TIM_SORT_MERGE(dst, stack, stack_curr, store, min_gallop_p);
      stack[stack_curr - 2].length += stack[stack_curr - 1].length;
      stack_curr--;
    }
  }

  return stack_curr;
}

static __inline int PUSH_NEXT(SORT_TYPE *dst,
                              const size_t size,
                              TEMP_STORAGE_T *store,
                              const size_t minrun,
                              TIM_SORT_RUN_T *run_stack,
                              size_t *stack_curr,
                              size_t *curr,
                              int *min_gallop_p) {
  size_t len = COUNT_RUN(dst, *curr, size);
  size_t run = minrun;

  if (run > size - *curr) {
    run = size - *curr;
  }

  if (run > len) {
    BINARY_INSERTION_SORT_START(&dst[*curr], len, run);
    len = run;
  }

  run_stack[*stack_curr].start = *curr;
  run_stack[*stack_curr].length = len;
  (*stack_curr)++;
  *curr += len;

  if (*curr == size) {
    /* finish up */
    while (*stack_curr > 1) {
      TIM_SORT_MERGE(dst, run_stack, *stack_curr, store, min_gallop_p);
      run_stack[*stack_curr - 2].length += run_stack[*stack_curr - 1].length;
      (*stack_curr)--;
    }

    if (store->storage != NULL) {
      free(store->storage);
      store->storage = NULL;
    }

    return 0;
  }

  return 1;
}

void TIM_SORT(SORT_TYPE *dst, const size_t size) {
  size_t minrun;
  TEMP_STORAGE_T _store, *store;
  TIM_SORT_RUN_T run_stack[TIM_SORT_STACK_SIZE];
  size_t stack_curr = 0;
  size_t curr = 0;
  int min_gallop = TIM_SORT_MIN_GALLOP;

  /* don't bother sorting an array of size 1 */
  if (size <= 1) {
    return;
  }

  if (size < 64) {
    BINARY_INSERTION_SORT(dst, size);
    return;
  }

  /* compute the minimum run length */
  minrun = compute_minrun(size);
  /* temporary storage for merges */
  store = &_store;
  store->alloc = 0;
  store->storage = NULL;

  if (!PUSH_NEXT(dst, size, store, minrun, run_stack, &stack_curr, &curr, &min_gallop)) {
    return;
  }

  if (!PUSH_NEXT(dst, size, store, minrun, run_stack, &stack_curr, &curr, &min_gallop)) {
    return;
  }

  if (!PUSH_NEXT(dst, size, store, minrun, run_stack, &stack_curr, &curr, &min_gallop)) {
    return;
  }

  while (1) {
    if (!CHECK_INVARIANT(run_stack, stack_curr)) {
      stack_curr = TIM_SORT_COLLAPSE(dst, run_stack, stack_curr, store, size, &min_gallop);
      continue;
    }

    if (!PUSH_NEXT(dst, size, store, minrun, run_stack, &stack_curr, &curr, &min_gallop)) {
      return;
    }
  }
}

/* heap sort: based on wikipedia */

static __inline void HEAP_SIFT_DOWN(SORT_TYPE *dst, const size_t start, const size_t end) {
  size_t root = start;

  while ((root << 1) <= end) {
    size_t child = root << 1;

    if ((child < end) && (SORT_CMP(dst[child], dst[child + 1]) < 0)) {
      child++;
    }

    if (SORT_CMP(dst[root], dst[child]) < 0) {
      SORT_SWAP(dst[root], dst[child]);
      root = child;
    } else {
      return;
    }
  }
}

static __inline void HEAPIFY(SORT_TYPE *dst, const size_t size) {
  size_t start = size >> 1;

  while (1) {
    HEAP_SIFT_DOWN(dst, start, size - 1);

    if (start == 0) {
      break;
    }

    start--;
  }
}

void HEAP_SORT(SORT_TYPE *dst, const size_t size) {
  size_t end = size - 1;

  /* don't bother sorting an array of size <= 1 */
  if (size <= 1) {
    return;
  }

  HEAPIFY(dst, size);

  while (end > 0) {
    SORT_SWAP(dst[end], dst[0]);
    HEAP_SIFT_DOWN(dst, 0, end - 1);
    end--;
  }
}

#undef QUICK_SORT
#undef MEDIAN
#undef SORT_CONCAT
#undef SORT_MAKE_STR1
#undef SORT_MAKE_STR
#undef SORT_NAME
#undef SORT_TYPE
#undef SORT_CMP
#undef TEMP_STORAGE_T
#undef TIM_SORT_RUN_T
#undef PUSH_NEXT
#undef SORT_SWAP
#undef SORT_CONCAT
#undef SORT_MAKE_STR1
#undef SORT_MAKE_STR
#undef BINARY_INSERTION_FIND
#undef BINARY_INSERTION_SORT_START
#undef BINARY_INSERTION_SORT
#undef REVERSE_ELEMENTS
#undef COUNT_RUN
#undef TIM_SORT
#undef TIM_SORT_RESIZE
#undef TIM_SORT_COLLAPSE
#undef TIM_SORT_RUN_T
#undef TEMP_STORAGE_T
#undef MERGE_SORT
#undef MERGE_SORT_IN_PLACE
#undef MERGE_SORT_IN_PLACE_RMERGE
#undef MERGE_SORT_IN_PLACE_BACKMERGE
#undef MERGE_SORT_IN_PLACE_FRONTMERGE
#undef MERGE_SORT_IN_PLACE_ASWAP
#undef GRAIL_SWAP1
#undef REC_STABLE_SORT
#undef GRAIL_REC_MERGE
#undef GRAIL_SORT_DYN_BUFFER
#undef GRAIL_SORT_FIXED_BUFFER
#undef GRAIL_COMMON_SORT
#undef GRAIL_SORT
#undef GRAIL_COMBINE_BLOCKS
#undef GRAIL_LAZY_STABLE_SORT
#undef GRAIL_MERGE_WITHOUT_BUFFER
#undef GRAIL_ROTATE
#undef GRAIL_BIN_SEARCH_LEFT
#undef GRAIL_BUILD_BLOCKS
#undef GRAIL_FIND_KEYS
#undef GRAIL_MERGE_BUFFERS_LEFT_WITH_X_BUF
#undef GRAIL_BIN_SEARCH_RIGHT
#undef GRAIL_MERGE_BUFFERS_LEFT
#undef GRAIL_SMART_MERGE_WITH_X_BUF
#undef GRAIL_MERGE_LEFT_WITH_X_BUF
#undef GRAIL_SMART_MERGE_WITHOUT_BUFFER
#undef GRAIL_SMART_MERGE_WITH_BUFFER
#undef GRAIL_MERGE_RIGHT
#undef GRAIL_MERGE_LEFT
#undef GRAIL_SWAP_N
#undef SQRT_SORT
#undef SQRT_SORT_BUILD_BLOCKS
#undef SQRT_SORT_MERGE_BUFFERS_LEFT_WITH_X_BUF
#undef SQRT_SORT_MERGE_DOWN
#undef SQRT_SORT_MERGE_LEFT_WITH_X_BUF
#undef SQRT_SORT_MERGE_RIGHT
#undef SQRT_SORT_SWAP_N
#undef SQRT_SORT_SWAP_1
#undef SQRT_SORT_SMART_MERGE_WITH_X_BUF
#undef SQRT_SORT_SORT_INS
#undef SQRT_SORT_COMBINE_BLOCKS
#undef SQRT_SORT_COMMON_SORT
#undef SORT_CMP_A
#undef BUBBLE_SORT
