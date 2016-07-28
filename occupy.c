/**
 * Copyright (c) 2016 Alexander Merritt
 *
 * Cache latency test.
 *
 * Creates array of cache-line-sized items and constructs a randomized list out
 * of them, then traverse list and measure overall cycles.
 *
 * The disassembly should show a large block like this:
 *
 * mov    (%rax),%rax
 * mov    (%rax),%rax
 * mov    (%rax),%rax
 * mov    (%rax),%rax
 * mov    (%rax),%rax
 * mov    (%rax),%rax
 * mov    (%rax),%rax
 * mov    (%rax),%rax
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

typedef unsigned long long ull;

static inline uint64_t rdtscp(void)
{
    uint64_t rax = 0, rdx = 0;
    __asm__ volatile ("lfence; rdtsc;"
            : "=a" (rax), "=d" (rdx)
            : : );
    return ((rdx << 32) + rax);
}

// knuth
void shuffle(int *idx, int n) {
    for (int ii = 0; ii < (1<<4); ii++) {
        for (int i = 0; i < n-1; i++) {
            int v = idx[i];
            int ii = (i+1) + (lrand48() % (n-(i+1)));
            idx[i] = idx[ii];
            idx[ii] = v;
        }
    }
}

// one cache line
struct node
{
    struct node *next;
    char pad[ 64 - sizeof(struct node*) ];
};

#define _next(_item)    ((_item) = (_item)->next)
#define _next8(_item)   \
        _next(_item); _next(_item); \
        _next(_item); _next(_item); \
        _next(_item); _next(_item); \
        _next(_item); _next(_item)

#define _next64(_item) \
        _next8(_item); _next8(_item); \
        _next8(_item); _next8(_item); \
        _next8(_item); _next8(_item); \
        _next8(_item); _next8(_item)

#define _next512(_item) \
        _next64(_item); _next64(_item); \
        _next64(_item); _next64(_item); \
        _next64(_item); _next64(_item); \
        _next64(_item); _next64(_item)

#define _next1k(_item) \
        _next512(_item); _next512(_item)

#define ITERS   (1<<14)
#define unroll(_item)   _next512(_item)

int __test_rand_list(size_t l3, double *cycles)
{
    const size_t line = 64;
    const long nlines = (l3 / line);
    ull c1, c2;

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(getpid(), sizeof(mask), &mask))
        return 1;

    struct node *nodes = calloc( (l3 / line), sizeof(struct node));
    if (!nodes) return 1;

    // randomize node order
    int *idx = calloc(nlines, sizeof(struct node));
    if (!idx) return 1;
    for (int i = 0; i < nlines; i++)
        idx[i] = i;
    shuffle(idx, nlines);

    // form the list (use volatile to avoid compiler optimizing out)
    volatile struct node *head = &nodes[ idx[0] ];
    for (int i = 1; i < nlines; i++) {
        head->next = &nodes[ idx[i] ];
        head = head->next;
    }
    head->next = &nodes[ idx[0] ];

    // warmup
    head = &nodes[ idx[0] ];
    { unroll(head); }

    // factor out the loop cost
    c1 = rdtscp();
    for (long i = 0; i < ITERS; i++) {
        asm volatile ("nop" : : : );
    }
    size_t loop = rdtscp()-c1;
    assert(loop > 0);

    head = &nodes[ idx[0] ];
    c1 = rdtscp();
    for (long i = 0; i < ITERS; i++) {
        unroll(head);
    }
    c2 = rdtscp();
    assert((c2-c1)>loop);

    *cycles = (double)(c2-c1-loop)/(ITERS * 512);
    return 0;
}

double mean(double *v, int n)
{
    double sum = 0;
    for (int i = 0; i < n; i++)
        sum += v[i];
    return (sum / n);
}

int test_rand_list(size_t from, size_t to, size_t incr)
{
    const int niter = 3;
    double v[niter];
    for (size_t at = from; at <= to; at+=incr) {
        for (int i = 0; i < niter; i++) {
            if (__test_rand_list(at, &v[i]))
                return 1;
        }
        printf("bytes %ld cycles %.2lf\n", at, mean(v, niter));
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        printf("Usage: ./occupy from-bytes to-bytes incr-bytes\n");
        return 1;
    }
    size_t from, to, incr;
    from = strtoll(argv[1], NULL, 10);
    to   = strtoll(argv[2], NULL, 10);
    incr = strtoll(argv[3], NULL, 10);
    test_rand_list( from, to, incr );
    return 0;
}
