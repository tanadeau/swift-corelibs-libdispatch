/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#ifdef __BLOCKS__

#if __cplusplus < 201103L
#error Must build with C++11 or later
#endif

#if __has_feature(cxx_exceptions)
#error Must build without C++ exceptions
#endif

extern "C" {
#include "internal.h"
}

#if DISPATCH_DEBUG && DISPATCH_BLOCK_PRIVATE_DATA_DEBUG
#define _dispatch_block_private_data_debug(msg, ...) \
		_dispatch_debug("block_private[%p]: " msg, (this), ##__VA_ARGS__)
#else
#define _dispatch_block_private_data_debug(msg, ...)
#endif

#pragma mark -
#pragma mark _dispatch_block_create

// rdar://20766742 C++ helpers to enable block capture of vouchers and groups

struct dispatch_block_private_data_s {
	DISPATCH_BLOCK_PRIVATE_DATA_HEADER();
	static void* operator new(size_t) = delete;
	static void* operator new [] (size_t) = delete;
	explicit inline DISPATCH_ALWAYS_INLINE dispatch_block_private_data_s(
			dispatch_block_flags_t flags, voucher_t voucher,
			pthread_priority_t priority, dispatch_block_t block) noexcept :
			dbpd_magic(), dbpd_flags(flags), dbpd_atomic_flags(),
			dbpd_performed(), dbpd_priority(priority), dbpd_voucher(voucher),
			dbpd_block(block), dbpd_group(), dbpd_queue(), dbpd_thread()
	{
		// stack structure constructor, no releases on destruction
		_dispatch_block_private_data_debug("create, block: %p", dbpd_block);
	}
	inline DISPATCH_ALWAYS_INLINE dispatch_block_private_data_s(
			dispatch_block_private_data_s const &o) noexcept :
			dbpd_magic(DISPATCH_BLOCK_PRIVATE_DATA_MAGIC),
			dbpd_flags(o.dbpd_flags), dbpd_atomic_flags(), dbpd_performed(),
			dbpd_priority(o.dbpd_priority), dbpd_voucher(o.dbpd_voucher),
			dbpd_block(), dbpd_group(), dbpd_queue(), dbpd_thread()
	{
		// copy constructor, create copy with retained references
		if (dbpd_voucher) voucher_retain(dbpd_voucher);
		if (o.dbpd_block) dbpd_block = _dispatch_Block_copy(o.dbpd_block);
		_dispatch_block_private_data_debug("copy from %p, block: %p from %p",
				&o, dbpd_block, o.dbpd_block);
		if (!o.dbpd_magic) return; // No group in initial copy of stack object
		dbpd_group = _dispatch_group_create_and_enter();
	}
	inline DISPATCH_ALWAYS_INLINE ~dispatch_block_private_data_s() noexcept
	{
		_dispatch_block_private_data_debug("destroy%s, block: %p",
				dbpd_magic ? "" : " (stack)", dbpd_block);
		if (dbpd_magic != DISPATCH_BLOCK_PRIVATE_DATA_MAGIC) return;
		if (dbpd_group) {
			if (!dbpd_performed) dispatch_group_leave(dbpd_group);
			((void (*)(dispatch_group_t))dispatch_release)(dbpd_group);
		}
		if (dbpd_block) Block_release(dbpd_block);
		if (dbpd_voucher) voucher_release(dbpd_voucher);
	}
};

dispatch_block_t
_dispatch_block_create(dispatch_block_flags_t flags, voucher_t voucher,
		pthread_priority_t pri, dispatch_block_t block)
{
	struct dispatch_block_private_data_s dbpds(flags, voucher, pri, block);
	return _dispatch_Block_copy(^{
		// Capture stack object: invokes copy constructor (17094902)
		(void)dbpds;
		_dispatch_block_invoke(&dbpds);
	});
}

extern "C" {
// The compiler hides the name of the function it generates, and changes it if
// we try to reference it directly, but the linker still sees it.
extern void DISPATCH_BLOCK_SPECIAL_INVOKE(void *)
#ifdef __linux__
		asm("___dispatch_block_create_block_invoke");
#else
		asm("____dispatch_block_create_block_invoke");
#endif
void (*_dispatch_block_special_invoke)(void*) = DISPATCH_BLOCK_SPECIAL_INVOKE;
}

#endif // __BLOCKS__
