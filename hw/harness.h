// The on-device litmus runner: what a generated test looks like to the harness.
#pragma once

#include <stddef.h>
#include <stdint.h>

#define MAX_THREADS 4
#define MAX_LOCS 8
#define MAX_OBS 8

// One shared location of one test instance, alone on its own 128-byte block so that two
// locations never share a cache line (Arm cores use 64-byte lines; 128 also defeats
// adjacent-line prefetching).
typedef struct {
    _Alignas(128) uint64_t v;
    char pad[120];
} cell_t;

// A thread of a test, run on instance i. L[l] is location l's array of cells; out receives the
// thread's observed registers for this instance.
typedef void (*body_fn)(cell_t* const* L, size_t i, uint64_t* out);

typedef struct {
    const char* name;
    int nthreads, nlocs, nobs;
    const char* loc_name[MAX_LOCS];
    uint64_t loc_init[MAX_LOCS];
    const char* obs_name[MAX_OBS];
    int obs_thread[MAX_OBS];  // >= 0: a register of that thread ...
    int obs_slot[MAX_OBS];    // ... stored in that slot of the thread's output
    int obs_loc[MAX_OBS];     // >= 0: the final value of that location
    int nslots[MAX_THREADS];
    body_fn body[MAX_THREADS];
    int (*cond)(const uint64_t* v);
    const char* quantifier;
    const char* model;               // the model's observation: Never / Sometimes / Always
    const char* const* allowed;      // the model's allowed final states, NULL-terminated
    int needs_lse, needs_rcpc;       // Armv8.1 atomics, Armv8.3 LDAPR
} test_t;

extern const test_t TESTS[];
extern const int NTESTS;
