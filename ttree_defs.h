/*
 * Copyright (c) 2008, 2009 Dan Kruchinin <dkruchinin@acm.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __TTREE_DEFS_H__
#define __TTREE_DEFS_H__

#include <assert.h>

/**
 * Default number of keys per T*-tree node
 */
#define TTREE_DEFAULT_NUMKEYS 8

/**
 * Minimum allowed number of keys per T*-tree node
 */
#define TNODE_ITEMS_MIN 2

/**
 * Maximum allowed numebr of keys per T*-tree node
 */
#define TNODE_ITEMS_MAX 4096

#define TTREE_ASSERT(cond) assert(cond)

/**
 * @brief Compile time assertion
 */
#define TTREE_CT_ASSERT(cond) \
    ((void)sizeof(char[1 - 2 * !(cond)]))

#ifndef offsetof
#define offsetof(type, field) \
    ((size_t)&(((type *)0)->field) - (size_t)((type *)0))
#endif /* !offsetof */

#if (defined(__cplusplus) || defined(__GNUC__) || defined(__INTEL_COMPILER))
#define __inline inline
#else /* __cplusplus || __GNUC__ || __INTEL_COMPILER  */
#define __inline
#endif /* !__cplusplus && !__GNUC__ && !__INTEL_COMPILER */

#ifdef __GNUC__
#if (__GNUC__ >= 3)
#define LIKELY(cond)   __builtin_expect((cond), 1)
#define UNLIKELY(cond) __builtin_expect((cond), 0)
#else /* __GNUC__ >= 3 */
#define LIKELY(cond)   cond
#define UNLIKELY(cond) cond
#endif /* __GNUC__ < 3 */
#endif /*__GNUC__ */

#endif /* !__TTREE_DEFS_H__ */
