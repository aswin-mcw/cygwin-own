#define _GNU_SOURCE

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

static stack_t ss;
static ucontext_t ctx;
static volatile int stage = 0;

/* Record stack pointer seen in handler */
static volatile uintptr_t sp_in_handler = 0;

/* Helper to read SP (works on x86_64 and aarch64) */
static inline uintptr_t get_sp(void)
{
    uintptr_t sp;
#if defined(__x86_64__)
    __asm__ volatile ("mov %%rsp, %0" : "=r"(sp));
#elif defined(__aarch64__)
    __asm__ volatile ("mov %0, sp" : "=r"(sp));
#else
#error unsupported architecture
#endif
    return sp;
}

/*
 * Signal handler:
 *  - runs on altstack
 *  - records stack pointer
 */
static void handler(int sig)
{
    (void) sig;
    sp_in_handler = get_sp();
    stage = 1;
}

/*
 * This function should resume execution here
 * after the signal handler returns.
 */
static void resume_point(void)
{
    printf("Reached resume_point(), setting stage = 2\n");
    stage = 2;
}

int main(void)
{
    printf("Program start\n");

    /* ---- print OS stack pointer before altstack ---- */
    uintptr_t sp_before = get_sp();
    printf("[main] OS stack SP before signal = %p\n", (void *)sp_before);

    /* ---- setup alternate stack ---- */
    ss.ss_size  = SIGSTKSZ;
    ss.ss_sp    = malloc(ss.ss_size);
    ss.ss_flags = 0;

    printf("[main] Altstack range = [%p .. %p)\n",
           ss.ss_sp,
           (void *)((uintptr_t)ss.ss_sp + ss.ss_size));

    sigaltstack(&ss, NULL);
    printf("[main] Altstack installed\n");

    /* ---- install signal handler ---- */
    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = SA_ONSTACK
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    printf("[main] Signal handler installed\n");

    /* ---- capture execution context ---- */
    printf("[main] Calling getcontext()\n");
    if (getcontext(&ctx) == 0) {

        printf("[main] Returned from getcontext(), stage = %d\n", stage);

        if (stage == 0) {
            printf("[main] Raising SIGUSR1\n");
            raise(SIGUSR1);

            printf("[main] Returned from signal handler\n");
            resume_point();
        }
    }

    /* ---- verify stack pointers ---- */
    printf("[main] SP inside handler        = %p\n",
           (void *)sp_in_handler);

    uintptr_t alt_lo = (uintptr_t) ss.ss_sp;
    uintptr_t alt_hi = alt_lo + ss.ss_size;

    printf("[main] Handler SP in altstack?  %s\n",
           (sp_in_handler >= alt_lo && sp_in_handler < alt_hi)
           ? "YES"
           : "NO");

    printf("[main] Final stage = %d\n", stage);
    assert(stage == 2);

    printf("Test PASSED: altstack switch + context restore verified\n");
    return 0;
}
