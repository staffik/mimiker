#include <sys/thread.h>
#include <sys/filedesc.h>
#include <sys/sched.h>
#include <sys/exception.h>
#include <sys/libkern.h>
#include <sys/vm_map.h>
#include <sys/proc.h>
#include <sys/sbrk.h>

int do_fork(pid_t *cldpidp) {
  thread_t *td = thread_self();
  proc_t *parent = td->td_proc;

  /* Cannot fork non-user threads. */
  assert(parent);

  thread_t *newtd = thread_create(td->td_name, NULL, NULL, td->td_base_prio);

  /* Clone the thread. Since we don't use fork-oriented thread_t layout, we copy
     all necessary fields one-by one for clarity. The new thread is already on
     the all_thread list, has name and tid set. Many fields don't require setup
     as they will be prepared by sched_add. */

  /* Copy user context.. */
  exc_frame_copy(newtd->td_uframe, td->td_uframe);
  exc_frame_set_retval(newtd->td_uframe, 0, 0);

  /* New thread does not need the exception frame just yet. */
  newtd->td_kframe = NULL;
  newtd->td_onfault = 0;

  /* The new thread already has a new kernel stack allocated. There is no need
     to copy its contents, it will be discarded anyway. We just prepare the
     thread's kernel context to a fresh one so that it will continue execution
     starting from user_exc_leave (which serves as fork_trampoline). */
  thread_entry_setup(newtd, (entry_fn_t)user_exc_leave, NULL);

  newtd->td_wchan = NULL;
  newtd->td_waitpt = NULL;

  newtd->td_prio = td->td_prio;

  /* Now, prepare a new process. */
  proc_t *child = proc_create(newtd, parent);
  pgrp_enter(child, parent->p_pgrp->pg_id);

  /* Clone the entire process memory space. */
  child->p_uspace = vm_map_clone(parent->p_uspace);

  /* Find copied brk segment. */
  WITH_VM_MAP_LOCK (child->p_uspace)
    child->p_sbrk = vm_map_find_segment(child->p_uspace, SBRK_START);

  /* Copy the parent descriptor table. */
  /* TODO: Optionally share the descriptor table between processes. */
  child->p_fdtable = fdtab_copy(parent->p_fdtable);

  /* Copy signal handler dispatch rules. */
  memcpy(child->p_sigactions, parent->p_sigactions,
         sizeof(child->p_sigactions));

  proc_add(child);

  sched_add(newtd);

  *cldpidp = child->p_pid;
  return 0;
}
