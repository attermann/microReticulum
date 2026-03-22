/*
 * Copyright (c) 2023 Chad Attermann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#pragma once

#include "../Log.h"

#include "tlsf.h"

#include <memory>

#define RNS_HEAP_ALLOCATOR 0		 // Use HEAP for allocator
#define RNS_HEAP_POOL_ALLOCATOR 1	 // Use HEAP pool for allocator
#define RNS_PSRAM_ALLOCATOR 2		 // Use PSRAM for allocator
#define RNS_PSRAM_POOL_ALLOCATOR 3	 // Use PSRAM pool for allocator
#define RNS_ALTHEAP_POOL_ALLOCATOR 4 // Use alternate HEAP pool for allocator

namespace RNS { namespace Utilities {

	class Memory {

	private:

		struct pool_info {
			constexpr pool_info(uint8_t t, size_t s) : type(t), buffer_size(s) {}
			uint8_t type = 0;
			bool pool_init = false;
			size_t buffer_size = 0;
			size_t contiguous_size = 0;
			tlsf_t tlsf = nullptr;
			uint32_t alloc_fault = 0;
			uint32_t free_fault = 0;
			size_t last_free = 0;
		};

		struct allocator_info {
			uint32_t alloc_count = 0;
			uint32_t alloc_fault = 0;
			uint32_t free_count = 0;
			uint32_t free_fault = 0;
			uint64_t alloc_size = 0;
			size_t min_alloc_size = 0;
			size_t max_alloc_size = 0;
			uint64_t last_size = 0;
		};

	public:

		static void pool_init(pool_info& pool_info);
		static void* pool_malloc(pool_info& pool_info, size_t size);
		static void pool_free(pool_info& pool_info, void* p, size_t size = 0) noexcept;

		static void dump_pool_stats(pool_info& pool_info, const char* name = "");
		static void dump_basic_pool_stats();

		static void dump_allocator_stats(Memory::allocator_info& allocator_info, const char* name = "");
		static void dump_basic_allocator_stats();

		static size_t heap_size();
		static size_t heap_available();
		static void dump_heap_stats();

	public:

		static pool_info heap_pool_info;
		static pool_info psram_pool_info;
		static pool_info altheap_pool_info;

		static allocator_info default_allocator_info;
		static allocator_info container_allocator_info;

	public:

		template <typename T>
		struct ContainerAllocator {
			using value_type = T;

			ContainerAllocator() noexcept {}

			// Essential for std::map internal node allocation
			template <class U>
			ContainerAllocator(const ContainerAllocator<U>&) noexcept {}

			/*
			// Required for embedded toolchains (ESP32 Xtensa GCC) that don't synthesiz
			// rebind automatically via std::allocator_traits
			template<class U>
			struct rebind {
				using other = ContainerAllocator<U>;
			};
			*/

			T* allocate(std::size_t n) {
				if (n == 0) return nullptr;
				size_t size = n * sizeof(value_type);
				++container_allocator_info.alloc_count;
				container_allocator_info.alloc_size += size;
				if (size < container_allocator_info.min_alloc_size || container_allocator_info.min_alloc_size == 0) {
					container_allocator_info.min_alloc_size = size;
				}
				if (size > container_allocator_info.max_alloc_size) {
					container_allocator_info.max_alloc_size = size;
				}
#if RNS_CONTAINER_ALLOCATOR == RNS_HEAP_POOL_ALLOCATOR
				void* p = pool_malloc(heap_pool_info, size);
#elif RNS_CONTAINER_ALLOCATOR == RNS_PSRAM_ALLOCATOR
#if BOARD_HAS_PSRAM != 1
	#error "BOARD_HAS_PSRAM must be defined to use RNS_PSRAM_POOL_ALLOCATOR allocator."
#endif
				// CBA PSRAM may not be accessible early in startup, but since we use the same free() for both
				//  HEAP and PSRAM allocations, we canb simply fallback to HEAP allocation when PSRAM fails
				void* p = ps_malloc(size);
				if (p == nullptr) {
					++container_allocator_info.alloc_fault;
					p = malloc(size);
				}
#elif RNS_CONTAINER_ALLOCATOR == RNS_PSRAM_POOL_ALLOCATOR
#if BOARD_HAS_PSRAM != 1
	#error "BOARD_HAS_PSRAM must be defined to use RNS_PSRAM_POOL_ALLOCATOR allocator."
#endif
				void* p = pool_malloc(psram_pool_info, size);
#elif RNS_CONTAINER_ALLOCATOR == RNS_ALTHEAP_POOL_ALLOCATOR
				void* p = pool_malloc(altheap_pool_info, size);
#elif RNS_CONTAINER_ALLOCATOR == RNS_HEAP_ALLOCATOR
				void* p = ::operator new(size);
#else
	#error "Unknown or invalid allocator."
#endif
				if (p == nullptr) {
					ERRORF("--- ContainerAllocator failed to allocate memory (%u bytes)", size);
					throw std::bad_alloc();
				}
				//TRACEF("--- ContainerAllocator allocated memory (addr=%lx) (%u bytes)", p, size);
				return static_cast<T*>(p);
			}

			void deallocate(T* p, std::size_t n) noexcept {
				if (p == nullptr) return;
				size_t size = n * sizeof(value_type);
				++container_allocator_info.free_count;
				container_allocator_info.alloc_size -= size;
				//TRACEF("--- ContainerAllocator freeing memory (addr=%lx) (%u bytes)", p, size);
#if RNS_CONTAINER_ALLOCATOR == RNS_HEAP_POOL_ALLOCATOR
				pool_free(heap_pool_info, p, size);
#elif RNS_CONTAINER_ALLOCATOR == RNS_PSRAM_ALLOCATOR
#if BOARD_HAS_PSRAM != 1
	#error "BOARD_HAS_PSRAM must be defined to use RNS_PSRAM_POOL_ALLOCATOR allocator."
#endif
				free(p);
#elif RNS_CONTAINER_ALLOCATOR == RNS_PSRAM_POOL_ALLOCATOR
#if BOARD_HAS_PSRAM != 1
	#error "BOARD_HAS_PSRAM must be defined to use RNS_PSRAM_POOL_ALLOCATOR allocator."
#endif
				pool_free(psram_pool_info, p, size);
#elif RNS_CONTAINER_ALLOCATOR == RNS_ALTHEAP_POOL_ALLOCATOR
				pool_free(altheap_pool_info, p, size);
#elif RNS_CONTAINER_ALLOCATOR == RNS_HEAP_ALLOCATOR
				::operator delete(p);
#else
		#error "Unknown or invalid allocator."
#endif
			}

			// Equality operators are required
			bool operator==(const ContainerAllocator&) const noexcept { return true; }
			bool operator!=(const ContainerAllocator&) const noexcept { return false; }
			//template <class T, class U>
			//bool operator==(const MyAllocator<T>&, const MyAllocator<U>&) { return true; }
			//template <class T, class U>
			//bool operator!=(const MyAllocator<T>&, const MyAllocator<U>&) { return false; }
		};

	};

} }
