// mca-hw: runs litmus tests on real Arm cores and counts every final state.
//
//   mca-hw --info                         the CPUs, their part numbers and features, as JSON
//   mca-hw [-n N] [-r R] [-a CPUS] [-d D] [TEST...]
//
//   -n  instances per batch (default 1000)    -r  batches (default 1000): N*R runs in total
//   -a  CPUs to pin to, e.g. 4,5: thread k goes on the k-th (wrapping); default: not pinned
//   -d  up to D no-ops of random delay before each instance (default 8), to vary the timing
//   -s  time (default): every thread starts instance i at the same tick of the shared counter
//       barrier: a spin barrier before every instance (looser: its release travels core to core)
//   -t  time mode: nanoseconds between instances (default 250)
//
// One JSON object per test on stdout. Each final state is marked with whether the MCA ARMv8
// model allows it: a forbidden state seen here means a broken test harness first, and only
// after that a question about the hardware.
#define _GNU_SOURCE
#include "harness.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#ifndef HWCAP_ATOMICS
#define HWCAP_ATOMICS (1UL << 8)
#endif
#ifndef HWCAP_LRCPC
#define HWCAP_LRCPC (1UL << 15)
#endif

#if defined(__aarch64__)
#define CPU_RELAX() __asm__ volatile("yield")
#define NOP() __asm__ volatile("nop")
#else  // the host smoke test (hw/host_smoke.c) builds this file for x86 too
#define CPU_RELAX() __asm__ volatile("pause")
#define NOP() __asm__ volatile("nop")
#endif

// ---- a spinning barrier: no sleeping, so the threads arrive at each instance together -------
// After a bounded spin it yields, so that two threads that do end up on one core (unpinned, or
// a scheduler's choice) still make progress instead of spinning out a whole timeslice.

typedef struct {
    _Alignas(128) atomic_uint count;
    _Alignas(128) atomic_uint gen;
    unsigned n;
} sbar_t;

static void sbar_init(sbar_t* b, unsigned n) {
    atomic_init(&b->count, 0);
    atomic_init(&b->gen, 0);
    b->n = n;
}

static void sbar_wait(sbar_t* b) {
    unsigned g = atomic_load_explicit(&b->gen, memory_order_acquire);
    if (atomic_fetch_add_explicit(&b->count, 1, memory_order_acq_rel) == b->n - 1) {
        atomic_store_explicit(&b->count, 0, memory_order_relaxed);
        atomic_store_explicit(&b->gen, g + 1, memory_order_release);
    } else {
        unsigned spins = 0;
        while (atomic_load_explicit(&b->gen, memory_order_acquire) == g) {
            if (++spins < 4096) CPU_RELAX();
            else sched_yield();
        }
    }
}

// ---- a clock every core shares ------------------------------------------------------------------
// The Arm generic timer's virtual count, readable at EL0. Every core sees the same count, and a
// tick reaches all of them together, so "start instance i at tick T" lines threads up to within a
// few nanoseconds: far tighter than a barrier, whose release travels from core to core.

#if defined(__aarch64__)
static inline uint64_t ticks(void) {
    uint64_t v;
    __asm__ volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(v));
    return v;
}
static uint64_t tick_hz(void) {
    uint64_t f;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
    return f;
}
#else
static inline uint64_t ticks(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
static uint64_t tick_hz(void) { return 1000000000ULL; }
#endif

// ---- one test, run ----------------------------------------------------------------------------

typedef enum { SYNC_BARRIER, SYNC_TIME } sync_t;

typedef struct {
    const test_t* t;
    size_t n;           // instances per batch
    long runs;          // batches
    int delay;          // max no-ops before an instance
    int cpu;            // -1: not pinned
    int k;              // which thread of the test
    sync_t sync;
    uint64_t stride;    // SYNC_TIME: ticks between instances
    const _Atomic uint64_t* start;  // SYNC_TIME: the tick instance 0 of this batch starts at
    cell_t** L;
    uint64_t* out;      // n * nslots[k]
    sbar_t* batch;      // all test threads and the main thread
    sbar_t* each;       // SYNC_BARRIER: the test threads, once per instance
    uint64_t cpus_seen; // bit c: ran on CPU c at some batch start
    long late;          // SYNC_TIME: instances this thread reached after their tick had passed
    int pin_error;
} worker_t;

static inline uint32_t xorshift(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

static void* worker(void* arg) {
    worker_t* w = arg;
    if (w->cpu >= 0) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(w->cpu, &set);
        if (sched_setaffinity(0, sizeof set, &set) != 0) w->pin_error = errno;
    }
    uint32_t seed = 0x9e3779b9u * (uint32_t)(w->k + 1);
    int nslots = w->t->nslots[w->k];
    for (long r = 0; r < w->runs; r++) {
        sbar_wait(w->batch);  // the main thread has reset every instance
        int c = sched_getcpu();
        if (c >= 0 && c < 64) w->cpus_seen |= 1ULL << c;
        uint64_t start = atomic_load_explicit(w->start, memory_order_acquire);
        for (size_t i = 0; i < w->n; i++) {
            if (w->sync == SYNC_BARRIER) {
                sbar_wait(w->each);
            } else {
                uint64_t target = start + (uint64_t)i * w->stride;
                if (ticks() >= target) w->late++;
                while (ticks() < target) {}
            }
            for (int d = (int)(xorshift(&seed) % (unsigned)(w->delay + 1)); d > 0; d--) NOP();
            w->t->body[w->k](w->L, i, w->out + i * (size_t)nslots);
        }
        sbar_wait(w->batch);  // done: the main thread reads the results
    }
    return NULL;
}

#define MAX_OUTCOMES 256

typedef struct {
    uint64_t v[MAX_OBS];
    long count;
} outcome_t;

static void key_text(const test_t* t, const uint64_t* v, char* buf, size_t len) {
    size_t used = 0;
    buf[0] = 0;
    for (int o = 0; o < t->nobs; o++)
        used += (size_t)snprintf(buf + used, len - used, "%s=%llu; ", t->obs_name[o], (unsigned long long)v[o]);
}

static int allowed_by_model(const test_t* t, const char* key) {
    for (const char* const* a = t->allowed; *a; a++)
        if (strcmp(*a, key) == 0) return 1;
    return 0;
}

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

typedef struct {
    size_t n;
    long runs;
    int delay;
    sync_t sync;
    uint64_t stride;    // SYNC_TIME: ticks between instances
    const int* cpus;
    int ncpus;
} options_t;

static void run_test(const test_t* t, const options_t* opt) {
    size_t n = opt->n;
    long runs = opt->runs;
    int delay = opt->delay;
    const int* cpus = opt->cpus;
    int ncpus = opt->ncpus;
    if (ncpus) {  // pinned: every thread needs a CPU of its own, or two spinners share one core
        int distinct = 0;
        for (int a = 0; a < ncpus; a++) {
            int seen = 0;
            for (int b = 0; b < a; b++) seen |= cpus[b] == cpus[a];
            distinct += !seen;
        }
        if (distinct < t->nthreads) {
            printf("{\"test\":\"%s\",\"skipped\":\"%d threads, %d distinct CPUs\"}\n", t->name, t->nthreads, distinct);
            fflush(stdout);
            return;
        }
    }
    unsigned long hw = getauxval(AT_HWCAP);
    if ((t->needs_lse && !(hw & HWCAP_ATOMICS)) || (t->needs_rcpc && !(hw & HWCAP_LRCPC))) {
        printf("{\"test\":\"%s\",\"skipped\":\"this CPU lacks %s\"}\n", t->name,
               t->needs_rcpc && !(hw & HWCAP_LRCPC) ? "LDAPR (FEAT_LRCPC)" : "LSE atomics");
        fflush(stdout);
        return;
    }

    cell_t* L[MAX_LOCS];
    for (int l = 0; l < t->nlocs; l++) {
        if (posix_memalign((void**)&L[l], 128, n * sizeof(cell_t)) != 0) { perror("alloc"); exit(1); }
    }
    sbar_t batch, each;
    sbar_init(&batch, (unsigned)t->nthreads + 1);
    sbar_init(&each, (unsigned)t->nthreads);
    static _Atomic uint64_t start;
    // Room for every thread to come out of the batch barrier before instance 0's tick: 50 us.
    const uint64_t margin = tick_hz() / 20000 + 1;
    worker_t w[MAX_THREADS];
    pthread_t th[MAX_THREADS];
    for (int k = 0; k < t->nthreads; k++) {
        w[k] = (worker_t){.t = t, .n = n, .runs = runs, .delay = delay, .k = k, .L = L,
                          .cpu = ncpus ? cpus[k % ncpus] : -1, .batch = &batch, .each = &each,
                          .sync = opt->sync, .stride = opt->stride, .start = &start};
        size_t slots = t->nslots[k] ? (size_t)t->nslots[k] : 1;
        w[k].out = calloc(n * slots, sizeof(uint64_t));
        pthread_create(&th[k], NULL, worker, &w[k]);
    }

    static outcome_t outcomes[MAX_OUTCOMES];
    int nout = 0;
    long cond_seen = 0;
    double t0 = now_s();
    for (long r = 0; r < runs; r++) {
        for (int l = 0; l < t->nlocs; l++)
            for (size_t i = 0; i < n; i++) L[l][i].v = t->loc_init[l];
        atomic_store_explicit(&start, ticks() + margin, memory_order_release);
        sbar_wait(&batch);  // go
        sbar_wait(&batch);  // all done
        for (size_t i = 0; i < n; i++) {
            uint64_t v[MAX_OBS];
            for (int o = 0; o < t->nobs; o++) {
                if (t->obs_loc[o] >= 0) v[o] = L[t->obs_loc[o]][i].v;
                else v[o] = w[t->obs_thread[o]].out[i * (size_t)t->nslots[t->obs_thread[o]] + (size_t)t->obs_slot[o]];
            }
            if (t->cond(v)) cond_seen++;
            int f = 0;
            while (f < nout && memcmp(outcomes[f].v, v, sizeof(uint64_t) * (size_t)t->nobs) != 0) f++;
            if (f == nout) {
                if (nout == MAX_OUTCOMES) continue;
                memcpy(outcomes[nout].v, v, sizeof v);
                outcomes[nout++].count = 0;
            }
            outcomes[f].count++;
        }
    }
    double elapsed = now_s() - t0;
    for (int k = 0; k < t->nthreads; k++) pthread_join(th[k], NULL);

    long forbidden = 0;
    printf("{\"test\":\"%s\",\"model\":\"%s\",\"quantifier\":\"%s\",\"runs\":%ld,\"instances\":%zu,"
           "\"iterations\":%ld,\"delay\":%d,\"sync\":\"%s\",\"stride_ticks\":%llu,\"tick_hz\":%llu,"
           "\"elapsed_s\":%.3f,\"condition_seen\":%ld,\"threads\":[",
           t->name, t->model, t->quantifier, runs, n, runs * (long)n, delay,
           opt->sync == SYNC_TIME ? "time" : "barrier", (unsigned long long)opt->stride,
           (unsigned long long)tick_hz(), elapsed, cond_seen);
    for (int k = 0; k < t->nthreads; k++) {
        printf("%s{\"cpu_requested\":%d,\"pin_error\":%d,\"late\":%ld,\"cpus_seen\":[", k ? "," : "", w[k].cpu,
               w[k].pin_error, w[k].late);
        int first = 1;
        for (int c = 0; c < 64; c++)
            if (w[k].cpus_seen >> c & 1) { printf("%s%d", first ? "" : ",", c); first = 0; }
        printf("]}");
    }
    printf("],\"outcomes\":[");
    for (int f = 0; f < nout; f++) {
        char key[512];
        key_text(t, outcomes[f].v, key, sizeof key);
        int ok = allowed_by_model(t, key);
        if (!ok) forbidden += outcomes[f].count;
        printf("%s{\"state\":\"%s\",\"count\":%ld,\"allowed\":%s,\"condition\":%s}", f ? "," : "", key,
               outcomes[f].count, ok ? "true" : "false", t->cond(outcomes[f].v) ? "true" : "false");
    }
    printf("],\"forbidden_seen\":%ld}\n", forbidden);
    fflush(stdout);
    fprintf(stderr, "%-34s %-9s condition seen %9ld / %ld%s\n", t->name, t->model, cond_seen, runs * (long)n,
            forbidden ? "   FORBIDDEN STATE SEEN" : "");

    for (int k = 0; k < t->nthreads; k++) free(w[k].out);
    for (int l = 0; l < t->nlocs; l++) free(L[l]);
}

// ---- what this phone is -----------------------------------------------------------------------

static void print_info(void) {
    struct utsname u;
    uname(&u);
    unsigned long hw = getauxval(AT_HWCAP);
    printf("{\"kernel\":\"%s %s\",\"cpus_online\":%ld,\"lse\":%s,\"lrcpc\":%s,\"cpus\":[", u.sysname, u.release,
           sysconf(_SC_NPROCESSORS_ONLN), hw & HWCAP_ATOMICS ? "true" : "false", hw & HWCAP_LRCPC ? "true" : "false");
    FILE* f = fopen("/proc/cpuinfo", "r");
    char line[256];
    int proc = -1, first = 1;
    unsigned impl = 0, part = 0, var = 0, rev = 0;
    while (f && fgets(line, sizeof line, f)) {
        unsigned x;
        if (sscanf(line, "processor : %d", &proc) == 1) continue;
        if (sscanf(line, "CPU implementer : %x", &x) == 1) impl = x;
        if (sscanf(line, "CPU variant : %x", &x) == 1) var = x;
        if (sscanf(line, "CPU part : %x", &x) == 1) part = x;
        if (sscanf(line, "CPU revision : %u", &x) == 1) {
            rev = x;
            char path[128];
            long khz = 0;
            snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", proc);
            FILE* q = fopen(path, "r");
            if (q) { if (fscanf(q, "%ld", &khz) != 1) khz = 0; fclose(q); }
            printf("%s{\"cpu\":%d,\"implementer\":\"0x%02x\",\"part\":\"0x%03x\",\"variant\":%u,\"revision\":%u,\"max_khz\":%ld}",
                   first ? "" : ",", proc, impl, part, var, rev, khz);
            first = 0;
        }
    }
    if (f) fclose(f);
    printf("]}\n");
}

int main(int argc, char** argv) {
    size_t n = 1000;
    long runs = 1000;
    int delay = 8, cpus[64], ncpus = 0;
    sync_t sync = SYNC_TIME;
    long stride_ns = 250;
    const char* only[256];
    int nonly = 0;
    for (int a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "--info")) { print_info(); return 0; }
        else if (!strcmp(argv[a], "-n") && a + 1 < argc) n = (size_t)atol(argv[++a]);
        else if (!strcmp(argv[a], "-r") && a + 1 < argc) runs = atol(argv[++a]);
        else if (!strcmp(argv[a], "-d") && a + 1 < argc) delay = atoi(argv[++a]);
        else if (!strcmp(argv[a], "-s") && a + 1 < argc) sync = !strcmp(argv[++a], "barrier") ? SYNC_BARRIER : SYNC_TIME;
        else if (!strcmp(argv[a], "-t") && a + 1 < argc) stride_ns = atol(argv[++a]);
        else if (!strcmp(argv[a], "-a") && a + 1 < argc) {
            char* save = NULL;
            for (char* p = strtok_r(argv[++a], ",", &save); p && ncpus < 64; p = strtok_r(NULL, ",", &save))
                cpus[ncpus++] = atoi(p);
        } else if (!strcmp(argv[a], "-l")) {
            for (int i = 0; i < NTESTS; i++) puts(TESTS[i].name);
            return 0;
        } else if (nonly < 256) only[nonly++] = argv[a];
    }
    uint64_t stride = (uint64_t)((double)stride_ns * 1e-9 * (double)tick_hz() + 0.5);
    if (stride == 0) stride = 1;
    options_t opt = {.n = n, .runs = runs, .delay = delay, .sync = sync, .stride = stride, .cpus = cpus, .ncpus = ncpus};
    for (int i = 0; i < NTESTS; i++) {
        int pick = nonly == 0;
        for (int j = 0; j < nonly; j++) if (!strcmp(only[j], TESTS[i].name)) pick = 1;
        if (pick) run_test(&TESTS[i], &opt);
    }
    return 0;
}
