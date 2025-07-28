#include <linux/version.h>

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
