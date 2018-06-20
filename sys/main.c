#define KL_LOG KL_USER
#include <klog.h>
#include <stdc.h>
#include <exec.h>
#include <ktest.h>
#include <malloc.h>

/* Borrowed from mips/malta.c */
char *kenv_get(const char *key);

int main(void) {
  const char *init = kenv_get("init");
  const char *test = kenv_get("test");

  if (init) {
    exec_args_t init_args = {
      .prog_name = init, .argc = 1, .argv = (const char *[]){init}};

    run_program(&init_args);
  } else if (test) {
    ktest_main(test);
  } else {
    /* This is a message to the user, so I intentionally use kprintf instead of
     * log. */
    kprintf("============\n");
    kprintf("No init specified!\n");
    kprintf("Use init=PROGRAM to start a user-space init program or test=TEST "
            "to run a kernel test.\n");
    kprintf("============\n");
    return 1;
  }

  klog("done!");
  return 0;
}
