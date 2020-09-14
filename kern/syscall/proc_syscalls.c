#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

#include "opt-A2.h"
#include <mips/trapframe.h>
#include <kern/fcntl.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>
  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
#if OPT_A2
  if(curproc->parent != NULL){
      lock_acquire(curproc->parent->lock_parent);
      for(unsigned int i = 0; i < array_num(curproc->parent->children); i++){
          struct proc *child = array_get(curproc->parent->children, i);
          if(child == curproc){
              array_remove(curproc->parent->children, i);
              break;
          }
      }
      lock_acquire(curproc->parent->zomlock);
      //lock_release(curproc->parent->lock_parent);
      struct zombie *zomchild = kmalloc(sizeof(struct zombie));
      zomchild->pid = curproc->pid;
      zomchild->exit_code = exitcode;
      //lock_acquire(curproc->parent->zomlock);
      array_add(curproc->parent->zomchildren, zomchild, NULL);
      lock_release(curproc->parent->zomlock);
      lock_release(curproc->parent->lock_parent);
      cv_signal(curproc->myparent, curproc->lock_parent);
  }
#else 
  (void)exitcode;
#endif

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
  *retval = curproc->pid;
#else
  *retval = 1;
#endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
#if OPT_A2
  lock_acquire(curproc->lock_parent);
  int existence = -1;
  for(unsigned int i = 0; i < array_num(curproc->children); i++){
      struct proc *child = array_get(curproc->children, i);
      if(pid == child->pid){
          existence = 1;
          cv_wait(child->myparent, curproc->lock_parent);
          break;
      }
  }
  //while i reach here, child is either in zombies or doesnot exit(error)
  lock_acquire(curproc->zomlock);
  int inzom = -1;
  for(unsigned int j = 0; j < array_num(curproc->zomchildren); j++){
      struct zombie *zomchild = array_get(curproc->zomchildren, j);
      if(pid == zomchild->pid){
          inzom = 1;
          exitstatus = _MKWAIT_EXIT(zomchild->exit_code);
          break;
      }
  }
  if(inzom == -1){
      lock_release(curproc->zomlock);
      lock_release(curproc->lock_parent);
      *retval = -1;
      return -1;
  }
  lock_release(curproc->zomlock);
  lock_release(curproc->lock_parent);
    
#else
  exitstatus = 0;
#endif
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}
#if OPT_A2
int
sys_fork(pid_t *retval, struct trapframe *tf)
{
    struct proc *child = proc_create_runprogram("a");
    KASSERT(child != NULL);
    array_add(curproc->children, child, NULL);
    
    int resultas = as_copy(curproc_getas(), &(child->p_addrspace));
    if(resultas != 0) return resultas;
    child->parent = curproc;
    *retval = child->pid;
    //kprintf("%d", child->pid);
    //kprintf("%d", curproc->pid);
    struct trapframe *temp_tf = kmalloc(sizeof(struct trapframe));
    memcpy(temp_tf, tf, sizeof(struct trapframe));

    int resultth = thread_fork(curthread->t_name, child, &enter_forked_process, temp_tf, 0);
    if(resultth != 0){
        return -1;
    }
    return 0;
}
#endif

#if OPT_A2
int sys_execv(userptr_t progname, userptr_t arguments){
    struct addrspace *as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    char * program = (char*) progname;
    char ** args = (char **) arguments;
    //copy the program name into kernel space
    size_t plen = (strlen(program) + 1) * sizeof(char);
    size_t got;
    char *pdest = kmalloc(plen);
    KASSERT(pdest != NULL);
    result = copyinstr(progname, (void *)pdest, plen, &got);
    if(result){
        return result;
    }

    //copy each char pointer value
    //asize is the number of argument alen is the size 
    int asize = 0;
    while(args[asize] != NULL){
        asize += 1;
    }
    size_t alen = (asize + 1) * sizeof(char*);
    //adest here doesn't end with a null terminator
    char **adest = kmalloc(alen);
    KASSERT(adest != NULL);

    for(int i = 0; args[i] != NULL; i++){
        size_t singlelen = strlen(args[i]) + 1;
        char* singledest = kmalloc(singlelen * sizeof(char));
        KASSERT(singledest != NULL);
        result = copyinstr((const_userptr_t) args[i], (void *)singledest, singlelen, &got);
        if(result){
            return result;
        }
        adest[i] = singledest;
    }
    adest[asize] = NULL;
    

    //open the program file
    result = vfs_open(program, O_RDONLY, 0, &v);
    if(result){
        return result;
    }

    //KASSERT(curproc_getas() == NULL);
    //create a new address space
    as = as_create();
    if(as == NULL){
        vfs_close(v);
        return ENOMEM;
    }

    struct addrspace *oldas = curproc_setas(as);
    as_activate();

    result = load_elf(v, &entrypoint);
    if(result){
        vfs_close(v);
        return result;
    }

    vfs_close(v);

    result = as_define_stack(as, &stackptr);
    if(result){
        return result;
    }


    //copy arguments from user space into new address space
    char** sargs = kmalloc((asize + 1) * sizeof(char*));
    KASSERT(sargs != NULL);
    
    //copy all the strings to the stack
    sargs[asize] = NULL;
    for(int i = (asize - 1); i >= 0; i--){
        size_t singlelen = ROUNDUP(strlen(adest[i]) + 1, 4);
        size_t singlesize = singlelen * sizeof(char);
        stackptr -= singlesize;
        result = copyoutstr((void*) adest[i], (userptr_t) stackptr, singlelen, &got);
        if(result){
            return result;
        }
        sargs[i] = (char*)stackptr;
    }

    //copy the string pointers to stack
    size_t psize = sizeof(char*);
    for(int i = asize; i >= 0; i--){
        stackptr -= psize;
        result = copyout((void*) &sargs[i], (userptr_t) stackptr, psize);
        if(result){
            return result;
        }
    }

    //stackptr = ROUNDUP(stackptr, 8);

    as_destroy(oldas);
    kfree(pdest);
    for(int i = 0; adest[i] != NULL; i++){
        kfree(adest[i]);
    }
    kfree(adest);

    enter_new_process(asize, (userptr_t) stackptr, stackptr, entrypoint);

    panic("enter_new_process return\n");
    return EINVAL;
}
#endif

