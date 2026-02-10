#define _GNU_SOURCE
#include <ucontext.h>
#include <stdio.h>
#include <assert.h>

static ucontext_t ctx_main, ctx_child;
static int stage = 0;

void child(void)
{
    printf("[child] entered, stage=%d\n", stage);

    stage = 2;
    printf("[child] stage set to 2, returning from child()\n");

    /* returning → should go via uc_link / trampoline */
}

int main(void)
{
    char stack[64 * 1024];

    printf("[main] start\n");

    printf("[main] calling getcontext(&ctx_main)\n");
    getcontext(&ctx_main);
    printf("[main] returned from getcontext(&ctx_main)\n");

    printf("[main] calling getcontext(&ctx_child)\n");
    getcontext(&ctx_child);
    printf("[main] returned from getcontext(&ctx_child)\n");

    printf("[main] setting up child stack\n");
    ctx_child.uc_stack.ss_sp = stack;
    ctx_child.uc_stack.ss_size = sizeof(stack);
    ctx_child.uc_link = &ctx_main;

    printf("[main] calling makecontext(&ctx_child, child)\n");
    makecontext(&ctx_child, child, 0);
    printf("[main] returned from makecontext()\n");

    stage = 1;
    printf("[main] stage set to 1\n");

    printf("[main] calling swapcontext(&ctx_main, &ctx_child)\n");
    swapcontext(&ctx_main, &ctx_child);

    /* Execution resumes here after child() returns */
    printf("[main] returned from swapcontext(), stage=%d\n", stage);

    printf("[main] asserting stage == 2\n");
    assert(stage == 2);

    printf("[main] PASS: swapcontext + makecontext minimal\n");
    return 0;
}
