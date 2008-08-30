/*
 * $Id: macros.h 334 2008-08-19 11:00:26Z baconevi $
 *
 * {License}
 */

#ifndef _ASE_MACROS_H_
#define _ASE_MACROS_H_

/**
 * @file macros.h
 * @brief Define common macros
 *
 * This file defines various macros for assertion, memory allocation, 
 * data type manipulation, etc.
 */

#include <ase/types.h>

#ifdef __cplusplus
	/*#define ASE_NULL ((ase_uint_t)0)*/
	#define ASE_NULL (0)
#else
	#define ASE_NULL ((void*)0)
#endif

#define ASE_CHAR_EOF  ((ase_cint_t)-1)

#define ASE_SIZEOF(n)  (sizeof(n))
#define ASE_COUNTOF(n) (sizeof(n)/sizeof(n[0]))
#define ASE_OFFSETOF(type,member) ((ase_size_t)&((type*)0)->member)

#define ASE_TYPE_IS_SIGNED(type) (((type)0) > ((type)-1))
#define ASE_TYPE_IS_UNSIGNED(type) (((type)0) < ((type)-1))

#define ASE_TYPE_SIGNED_MAX(type) \
	((type)~((type)1 << (ASE_SIZEOF(type) * 8 - 1)))
#define ASE_TYPE_UNSIGNED_MAX(type) ((type)(~(type)0))

#define ASE_TYPE_SIGNED_MIN(type) \
	((type)((type)1 << (ASE_SIZEOF(type) * 8 - 1)))
#define ASE_TYPE_UNSIGNED_MIN(type) ((type)0)

/** 
 * @define ASE_TYPE_MAX(type)
 * @brief Get the maximum value \a type can hold 
 */
#define ASE_TYPE_MAX(type) \
	((ASE_TYPE_IS_SIGNED(type)? ASE_TYPE_SIGNED_MAX(type): ASE_TYPE_UNSIGNED_MAX(type)))
#define ASE_TYPE_MIN(type) \
	((ASE_TYPE_IS_SIGNED(type)? ASE_TYPE_SIGNED_MIN(type): ASE_TYPE_UNSIGNED_MIN(type)))

#define ASE_IS_POWOF2(x) (((x) & ((x) - 1)) == 0)

#define ASE_SWAP(x,y,original_type,casting_type) \
	do { \
		x = (original_type)((casting_type)(x) ^ (casting_type)(y)); \
		y = (original_type)((casting_type)(y) ^ (casting_type)(x)); \
		x = (original_type)((casting_type)(x) ^ (casting_type)(y)); \
	} while (0)

#define ASE_ABS(x) ((x) < 0? -(x): (x))

#define ASE_LOOP_CONTINUE(id) goto __loop_ ## id ## _begin__;
#define ASE_LOOP_BREAK(id)    goto __loop_ ## id ## _end__;
#define ASE_LOOP_BEGIN(id)    __loop_ ## id ## _begin__: {
#define ASE_LOOP_END(id)      ASE_LOOP_CONTINUE(id) } __loop_ ## id ## _end__:;

#define ASE_REPEAT(n,blk) \
	do { \
		ase_size_t __ase_repeat_x1__ = (ase_size_t)(n); \
		ase_size_t __ase_repeat_x2__ = __ase_repeat_x1__ >> 4; \
		__ase_repeat_x1__ &= 15; \
		while (__ase_repeat_x1__-- > 0) { blk; } \
		while (__ase_repeat_x2__-- > 0) { \
			blk; blk; blk; blk; blk; blk; blk; blk; \
			blk; blk; blk; blk; blk; blk; blk; blk; \
		} \
	} while (0);

#define ASE_MQ_I(val) #val
#define ASE_MQ(val)   ASE_MQ_I(val)
#define ASE_MC(ch)    ((ase_mchar_t)ch)
#define ASE_MS(str)   ((const ase_mchar_t*)str)
#define ASE_MT(txt)   (txt)

#define ASE_WQ_I(val)  (L ## #val)
#define ASE_WQ(val)    ASE_WQ_I(val)
#define ASE_WC(ch)     ((ase_wchar_t)L ## ch)
#define ASE_WS(str)    ((const ase_wchar_t*)L ## str)
#define ASE_WT(txt)    (L ## txt)

#if defined(ASE_CHAR_IS_MCHAR)
	#define ASE_Q(val) ASE_MQ(val)
	#define ASE_C(ch)  ASE_MC(ch)
	#define ASE_S(str) ASE_MS(str)
	#define ASE_T(txt) ASE_MT(txt)
#else
	#define ASE_Q(val) ASE_WQ(val)
	#define ASE_C(ch)  ASE_WC(ch)
	#define ASE_S(str) ASE_WS(str)
	#define ASE_T(txt) ASE_WT(txt)
#endif

#if defined(__GNUC__)
	#define ASE_BEGIN_PACKED_STRUCT(x) struct x {
	#define ASE_END_PACKED_STRUCT() } __attribute__((packed));
#else
	#define ASE_BEGIN_PACKED_STRUCT(x) struct x {
	#define ASE_END_PACKED_STRUCT() };
#endif

#ifdef NDEBUG
	#define ASE_ASSERT(expr) ((void)0)
	#define ASE_ASSERTX(expr,desc) ((void)0)
#else
	#ifdef __cplusplus
	extern "C" {
	#endif
		void ase_assert_failed (
        		const ase_char_t* expr, const ase_char_t* desc,
        		const ase_char_t* file, ase_size_t line);
	#ifdef __cplusplus
	}
	#endif

	#define ASE_ASSERT(expr) (void)((expr) || \
		(ase_assert_failed (ASE_T(#expr), ASE_NULL, ASE_T(__FILE__), __LINE__), 0))
	#define ASE_ASSERTX(expr,desc) (void)((expr) || \
		(ase_assert_failed (ASE_T(#expr), ASE_T(desc), ASE_T(__FILE__), __LINE__), 0))
#endif

#ifdef __cplusplus
	#define ASE_BEGIN_NAMESPACE(x)    namespace x {
	#define ASE_END_NAMESPACE(x)      }
	#define ASE_BEGIN_NAMESPACE2(x,y) namespace x { namespace y {
	#define ASE_END_NAMESPACE2(y,x)   }}
#endif

#endif