.. title:: clang-tidy - linuxkernel-local-locks

linuxkernel-local-locks
=======================

Checks Linux kernel code to see if it uses context-less functions to
disable/enable preemption and IRQs. Instead, suggests the respective local
counterpart, located in <linux/local_lock.h>.
However, the warning only occurs if the caller has a struct pointer param
(potential context to store the lock).

The local locks (available since Linux 5.8) add context to the lock
functions, enabling better traceability, documentation and tooling
(lockdep).

Further information
-------------------

* https://lwn.net/Articles/828477/
* https://lwn.net/ml/linux-kernel/20200527201119.1692513-1-bigeasy@linutronix.de/

Examples
--------

.. code-block:: c

  void fn(struct context *ctx) 
  {
    unsigned long flags;

    /* warns to use 'local_lock' */
    preempt_disable();

    /* warns to use 'local_unlock' */
    preempt_enable();

    /* warns to use 'local_lock_irq' */
    local_irq_disable();

    /* warns to use 'local_unlock_irq' */
    local_irq_enable();

    /* warns to use 'local_lock_irqsave' */
    local_irq_save(flags);

    /* warns to use 'local_unlock_irqrestore' */
    local_irq_restore(flags);
  }
