// RUN: %check_clang_tidy %s linuxkernel-local-locks %t
/* clang-format off */
// CHECK-FIXES: #include <linux/local_lock.h>
#include <stdint.h>
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: missing header [linuxkernel-local-locks]

// Prototypes of the lock functions.
#define preempt_disable() do {} while (0)
#define preempt_enable() do {} while (0)
#define local_irq_disable() do {} while (0)
#define local_irq_enable() do {} while (0)
#define local_irq_save(flags) do {} while (0)
#define local_irq_restore(flags) do {} while (0)

void f1() {
	unsigned long flags;

	preempt_disable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_lock' could be used [linuxkernel-local-locks]
	preempt_enable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_unlock' could be used [linuxkernel-local-locks]
	local_irq_disable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_lock_irq' could be used [linuxkernel-local-locks]
	local_irq_enable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_unlock_irq' could be used [linuxkernel-local-locks]
	local_irq_save(flags);
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_lock_irqsave' could be used [linuxkernel-local-locks]
	local_irq_restore(flags);
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_unlock_irqrestore' could be used [linuxkernel-local-locks]
}


// Prototype of a context struct.
// CHECK-MESSAGES: :[[@LINE+1]]:1: warning: missing field [linuxkernel-local-locks]
struct context {
	unsigned int placeholder;
/* TODO: couldn't get it running, think because of the -1 offset.
 * {{^[[:space:]]*}}local_lock_t local_lock;
 */
}; 

void f2(struct context *ctx)
{
	unsigned long flags;

	preempt_disable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_lock' can be used [linuxkernel-local-locks]
	// CHECK-FIXES: local_lock(ctx->local_lock);
	preempt_enable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_unlock' can be used [linuxkernel-local-locks]
	// CHECK-FIXES: local_unlock(ctx->local_lock);
	local_irq_disable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_lock_irq' can be used [linuxkernel-local-locks]
	// CHECK-FIXES: local_lock_irq(ctx->local_lock);
	local_irq_enable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_unlock_irq' can be used [linuxkernel-local-locks]
	// CHECK-FIXES: local_unlock_irq(ctx->local_lock);
	local_irq_save(flags);
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_lock_irqsave' can be used [linuxkernel-local-locks]
	// CHECK-FIXES: local_lock_irqsave(ctx->local_lock, flags);
	local_irq_restore(flags);
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_unlock_irqrestore' can be used [linuxkernel-local-locks]
	// CHECK-FIXES: local_unlock_irqrestore(ctx->local_lock, flags);
}

void f3()
{
	unsigned long flags;

	preempt_disable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_lock' could be used [linuxkernel-local-locks]
	preempt_enable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_unlock' could be used [linuxkernel-local-locks]
	local_irq_disable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_lock_irq' could be used [linuxkernel-local-locks]
	local_irq_enable();
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_unlock_irq' could be used [linuxkernel-local-locks]
	local_irq_save(flags);
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_lock_irqsave' could be used [linuxkernel-local-locks]
	local_irq_restore(flags);
	// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: 'local_unlock_irqrestore' could be used [linuxkernel-local-locks]
}
