#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0))
#define smp_load_acquire(arg) arg
#define smp_store_release(p, v) *(p)=v
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))
#define READ_ONCE(p) ACCESS_ONCE(p)
#endif


#if (KERNEL_VERSION(3, 8, 0) <= LINUX_VERSION_CODE)
#define __devexit
#define __devinit
#define __devinitconst
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef fallthrough
#if __has_attribute(__fallthrough__)
# define fallthrough                    __attribute__((__fallthrough__))
#else
# define fallthrough                    do {} while (0)  /* fallthrough */
#endif
#endif


#ifdef KERNEL_DVB_CORE
#define DVB_DEVICE_CI 0
#define DVB_DEVICE_NS 6
#define DVB_DEVICE_NSD 7
#define DVB_DEVICE_MOD 8

#define APSK_128 21
#define APSK_256 22
#define APSK_256_L 23

#define SYS_ATSC3 (SYS_DVBC2+1)
#define SYS_ISDBS3 (SYS_ATSC3+1)

#define FEC_29_45 (FEC_7_15+1)
#define FEC_31_45 (FEC_29_45+1)
#endif

#if 0
#if (KERNEL_VERSION(6, 6, 3) >= LINUX_VERSION_CODE)
#include <linux/overflow.h>
/**
 * memdup_array_user - duplicate array from user space
 * @src: source address in user space
 * @n: number of array members to copy
 * @size: size of one array member
 *
 * Return: an ERR_PTR() on failure. Result is physically
 * contiguous, to be freed by kfree().
 */
static inline void *memdup_array_user(const void __user *src, size_t n, size_t size)
{
	size_t nbytes;

	if (check_mul_overflow(n, size, &nbytes))
		return ERR_PTR(-EOVERFLOW);

	return memdup_user(src, nbytes);
}
#endif
#endif

/* compat: RHEL kernel 6.12.0-211 backported the upstream removal of from_timer();
   alias it to the replacement timer_container_of() (identical signature). */
#ifndef from_timer
#define from_timer(var, callback_timer, timer_fieldname) \
	timer_container_of(var, callback_timer, timer_fieldname)
#endif

/**
 * compat: RHEL 9+ kernels backport 
 * - the 6.2 const struct device * class callbacks
 * - the 6.4 single-argument class_create()
 * into their 5.14 base.
 */
#if defined(RHEL_RELEASE_CODE) && defined(RHEL_RELEASE_VERSION)
#if (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9, 0))
#define DDDVB_HAS_CONST_DEVICE_CB 1
#define DDDVB_HAS_CLASS_CREATE_V2 1
#endif
#endif
#if !defined(DDDVB_HAS_CONST_DEVICE_CB) && (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0))
#define DDDVB_HAS_CONST_DEVICE_CB 1
#endif
#if !defined(DDDVB_HAS_CLASS_CREATE_V2) && (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0))
#define DDDVB_HAS_CLASS_CREATE_V2 1
#endif

/**
 * compat: RHEL 9 kernels ship intlog2/intlog10 in vmlinux (6.6 backport).
 * Do not export them again from dvb-core.
 */
#if defined(RHEL_RELEASE_CODE) && defined(RHEL_RELEASE_VERSION)
#if (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9, 0)) && (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(10, 0))
#define DDDVB_INTLOG_IN_VMLINUX 1
#endif
#endif
