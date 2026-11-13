// A host (x86) smoke test of the harness itself: barrier, reset, histogram, allowed-state marking
// and JSON. The bodies are plain C over volatile cells, so the answers are x86's, which are known:
//   SB          its relaxed state is seen on x86 (stores sit in a store buffer): a positive control
//   SB+mfences  never seen (mfence drains the store buffer): checks forbidden-state counting
//   MP          never seen on x86 (TSO keeps stores in order and loads in order)
// Build and run in WSL:  gcc -O2 -pthread -Ihw hw/harness.c hw/host_smoke.c -o /tmp/mca-host
#include "harness.h"

#define X(i) (&L[0][i].v)
#define Y(i) (&L[1][i].v)

static void sb_p0(cell_t* const* L, size_t i, uint64_t* out) { *(volatile uint64_t*)X(i) = 1; out[0] = *(volatile uint64_t*)Y(i); }
static void sb_p1(cell_t* const* L, size_t i, uint64_t* out) { *(volatile uint64_t*)Y(i) = 1; out[0] = *(volatile uint64_t*)X(i); }
static void sbf_p0(cell_t* const* L, size_t i, uint64_t* out) { *(volatile uint64_t*)X(i) = 1; __sync_synchronize(); out[0] = *(volatile uint64_t*)Y(i); }
static void sbf_p1(cell_t* const* L, size_t i, uint64_t* out) { *(volatile uint64_t*)Y(i) = 1; __sync_synchronize(); out[0] = *(volatile uint64_t*)X(i); }
static void mp_p0(cell_t* const* L, size_t i, uint64_t* out) { (void)out; *(volatile uint64_t*)X(i) = 1; *(volatile uint64_t*)Y(i) = 1; }
static void mp_p1(cell_t* const* L, size_t i, uint64_t* out) { out[0] = *(volatile uint64_t*)Y(i); out[1] = *(volatile uint64_t*)X(i); }

static int sb_cond(const uint64_t* v) { return v[0] == 0 && v[1] == 0; }
static int mp_cond(const uint64_t* v) { return v[0] == 1 && v[1] == 0; }

static const char* const sb_all[] = {"0:X2=0; 1:X2=0; ", "0:X2=0; 1:X2=1; ", "0:X2=1; 1:X2=0; ", "0:X2=1; 1:X2=1; ", NULL};
static const char* const sb_fenced[] = {"0:X2=0; 1:X2=1; ", "0:X2=1; 1:X2=0; ", "0:X2=1; 1:X2=1; ", NULL};
static const char* const mp_all[] = {"1:X0=0; 1:X2=0; ", "1:X0=0; 1:X2=1; ", "1:X0=1; 1:X2=0; ", "1:X0=1; 1:X2=1; ", NULL};

const test_t TESTS[] = {
    {.name = "SB", .nthreads = 2, .nlocs = 2, .nobs = 2, .loc_name = {"x", "y"}, .loc_init = {0, 0},
     .obs_name = {"0:X2", "1:X2"}, .obs_thread = {0, 1}, .obs_slot = {0, 0}, .obs_loc = {-1, -1},
     .nslots = {1, 1}, .body = {sb_p0, sb_p1}, .cond = sb_cond, .quantifier = "exists", .model = "Sometimes",
     .allowed = sb_all},
    {.name = "SB+mfences", .nthreads = 2, .nlocs = 2, .nobs = 2, .loc_name = {"x", "y"}, .loc_init = {0, 0},
     .obs_name = {"0:X2", "1:X2"}, .obs_thread = {0, 1}, .obs_slot = {0, 0}, .obs_loc = {-1, -1},
     .nslots = {1, 1}, .body = {sbf_p0, sbf_p1}, .cond = sb_cond, .quantifier = "exists", .model = "Never",
     .allowed = sb_fenced},
    {.name = "MP", .nthreads = 2, .nlocs = 2, .nobs = 2, .loc_name = {"x", "y"}, .loc_init = {0, 0},
     .obs_name = {"1:X0", "1:X2"}, .obs_thread = {1, 1}, .obs_slot = {0, 1}, .obs_loc = {-1, -1},
     .nslots = {0, 2}, .body = {mp_p0, mp_p1}, .cond = mp_cond, .quantifier = "exists", .model = "Sometimes",
     .allowed = mp_all},
};
const int NTESTS = 3;
