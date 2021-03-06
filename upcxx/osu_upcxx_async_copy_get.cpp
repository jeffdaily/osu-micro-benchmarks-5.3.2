#define BENCHMARK "OSU UPC++ Async Copy (Get) Test"
/*
 * Copyright (C) 2002-2015 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University.
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include <upcxx.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <osu_common.h>

using namespace upcxx;

#define VERIFY 0
#define MAX_MSG_SIZE         (1<<22)
#define SKIP_LARGE  10
#define LOOP_LARGE  100
#define LARGE_MESSAGE_SIZE  8192

int skip = 1000;
int loop = 10000;

int
main (int argc, char **argv)
{
    init(&argc, &argv);

    int iters=0;
    double t_start, t_end;
    int peerid = (myrank()+1)%ranks();
    int iamsender = 0;
    int i;

    if (ranks() == 1) {
        if (myrank() == 0) {
            fprintf(stderr, "This test requires at least two UPC threads\n");
        }
        return 0;
    }

    if (myrank() < ranks()/2) {
        iamsender = 1;
    }

    shared_array<global_ptr<char>, 1> data_ptrs (ranks());

    /*
     * allocate memory to each global pointer.
     */
    data_ptrs[myrank()] = allocate<char>(myrank(), sizeof(char)
            * MAX_MSG_SIZE);

    /*
     * put a barrier since allocate is non-blocking in upc++
     */
    barrier();

    /*
     * my peer's pointer from where I will memget.
     */
    global_ptr<char> remote = data_ptrs[peerid];

    /*
     * cast my global pointer to a local pointer.
     */
    global_ptr<char> local = (global_ptr<char>)data_ptrs[myrank()];

    barrier();

    if (!myrank()) {
        fprintf(stdout, HEADER);
        fprintf(stdout, "# [ pairs: %d ]\n", ranks()/2);
        fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
        fflush(stdout);
    }

    for (int size = 1; size <= MAX_MSG_SIZE; size*=2) {
        if (iamsender) {
            for (i = 0; i < size; i++) {
                char *lptr = (char *)local;
                lptr[i] = 'a';
            }
        } else {
            for (i = 0; i < size; i++) {
                char *lptr = (char *)local;
                lptr[i] = 'b';
            }
        }

        barrier();

        if (size > LARGE_MESSAGE_SIZE) {
            loop = LOOP_LARGE;
            skip = SKIP_LARGE;
        }

        if (iamsender) {
            for ( i = 0; i < loop + skip; i++) {
                if (i == skip) {
                    barrier();
                    t_start = getMicrosecondTimeStamp();
                }

                async_copy(remote, local, size);
            }

            async_wait();
            barrier();

            t_end = getMicrosecondTimeStamp();
            if (!myrank()) {
                double latency = (t_end - t_start)/(1.0 * loop);
                fprintf(stdout, "%-*d%*.*f\n", 10, size, FIELD_WIDTH,
                        FLOAT_PRECISION, latency);
                fflush(stdout);
            }
        } else {
            barrier();
            barrier();
        }
    }

    if (VERIFY) {
        if (iamsender) {
            /*
             * my local and my remote ptr should have same data
             */
            char *lptr = (char *)local;
            for (int i = 0; i < MIN(20, MAX_MSG_SIZE); i++) {
                printf("sender_rank():%d --- lptr[%d]=%c , rptr[%d]=%c \n",
                        myrank(), i, lptr[i], i, (char)remote[i]);

            }
        }
    }

    deallocate(local);
    barrier();
    finalize();

    return 0;
}
