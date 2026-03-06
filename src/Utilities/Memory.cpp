#include "Memory.h"

#include <new>

using namespace RNS;
using namespace RNS::Utilities;

#ifndef RNS_DEFAULT_ALLOCATOR
	#if defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
		// Use TLSF on NRF52 boards by default since they don't embed their own advanced memory manager like ESP32 does
		#define RNS_DEFAULT_ALLOCATOR RNS_HEAP_POOL_ALLOCATOR
	#else
		#define RNS_DEFAULT_ALLOCATOR RNS_HEAP_ALLOCATOR
	#endif
#endif
#ifndef RNS_CONTAINER_ALLOCATOR
	#if defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
		// Use TLSF on NRF52 boards by default since they don't embed their own advanced memory manager like ESP32 does
		#define RNS_CONTAINER_ALLOCATOR RNS_HEAP_POOL_ALLOCATOR
	#else
		#define RNS_CONTAINER_ALLOCATOR RNS_HEAP_ALLOCATOR
	#endif
#endif

#ifndef RNS_HEAP_POOL_BUFFER_SIZE
	#define RNS_HEAP_POOL_BUFFER_SIZE 1024 * 1000
#endif
#ifndef RNS_PSRAM_POOL_BUFFER_SIZE
	#define RNS_PSRAM_POOL_BUFFER_SIZE 0
#endif
#ifndef RNS_ALTHEAP_POOL_BUFFER_SIZE
	#define RNS_ALTHEAP_POOL_BUFFER_SIZE 0
#endif
#ifndef BUFFER_FRACTION
	#define BUFFER_FRACTION 0.8
#endif


//char _tlsf_msg[256] = "";

/*static*/ Memory::pool_info Memory::heap_pool_info(RNS_HEAP_POOL_ALLOCATOR, RNS_HEAP_POOL_BUFFER_SIZE);
/*static*/ Memory::pool_info Memory::psram_pool_info(RNS_PSRAM_POOL_ALLOCATOR, RNS_PSRAM_POOL_BUFFER_SIZE);;
/*static*/ Memory::pool_info Memory::altheap_pool_info(RNS_ALTHEAP_POOL_ALLOCATOR, RNS_ALTHEAP_POOL_BUFFER_SIZE);;
/*static*/ Memory::allocator_info Memory::default_allocator_info;
/*static*/ Memory::allocator_info Memory::container_allocator_info;


/*static*/ void Memory::pool_init(Memory::pool_info& pool_info) {
	if (pool_info.pool_init) {
		return;
	}
	pool_info.pool_init = true;
	printf("Initializing TLSF pool...\n");

#if defined(ESP32)
	heap_pool_info.contiguous_size = ESP.getMaxAllocHeap();
	psram_pool_info.contiguous_size = ESP.getMaxAllocPsram();
	altheap_pool_info.contiguous_size = ESP.getMaxAllocHeap();
	printf("TLSF: Heap contiguous_size: %u\n", ESP.getMaxAllocHeap());
	printf("TLSF: PSRAM contiguous_size: %u\n", ESP.getMaxAllocPsram());
	printf("TLSF: contiguous_size: %u\n", pool_info.contiguous_size);
	if (pool_info.buffer_size == 0) {
		// CBA NOTE Using fp mathhere somehow causes tlsf_create_with_pool() to fail.
		//pool_info.buffer_size = (size_t)(pool_info.contiguous_size * BUFFER_FRACTION);
		// Compute 80% exactly using integers
		pool_info.buffer_size = (pool_info.contiguous_size * 4) / 5;
	}
	// Round DOWN to TLSF alignment
	size_t align = tlsf_align_size();
	pool_info.buffer_size &= ~(align - 1);
	void* raw_buffer = nullptr;
	// If pool type is PSRAM then allocate pool buffer from PSRAM
	if (pool_info.type == RNS_PSRAM_POOL_ALLOCATOR) {
		raw_buffer = (void*)ps_malloc(pool_info.buffer_size);
	}
	// Otherwise just allocate from heap
	else {
		raw_buffer = (void*)aligned_alloc(align, pool_info.buffer_size);
	}
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	heap_pool_info.contiguous_size = dbgHeapFree();
	altheap_pool_info.buffer_size = dbgHeapFree();
	TRACEF("TLSF: contiguous_size: %u", pool_info.contiguous_size);
	if (pool_info.buffer_size == 0) {
		pool_info.buffer_size = (size_t)(pool_info.contiguous_size * BUFFER_FRACTION);
	}
	// For NRF52 round to kB
	pool_info.buffer_size = (size_t)(pool_info.buffer_size / 1024) * 1024;
	TRACEF("TLSF: buffer_size: %u", pool_info.buffer_size);
	void* raw_buffer = malloc(pool_info.buffer_size);
#else
	pool_info.buffer_size = (size_t)RNS_HEAP_POOL_BUFFER_SIZE;
	TRACEF("TLSF: buffer_size: %u", pool_info.buffer_size);
	void* raw_buffer = malloc(pool_info.buffer_size);
#endif
	if (raw_buffer == nullptr) {
		ERROR("-- TLSF: buffer allocation FAILED");
		printf("TLSF buffer allocation FAILED!\n");
		//strcpy(_tlsf_msg, "-- TLSF: buffer allocation FAILED!!!");
	}
	else {
#if 1
		pool_info.tlsf = tlsf_create_with_pool(raw_buffer, pool_info.buffer_size);
		if (pool_info.tlsf == nullptr) {
			//sprintf(_tlsf_msg, "TLSF: initialization with align=%d, contiguous=%d, size=%d FAILED!!!", tlsf_align_size(), pool_info.contiguous_size, pool_info.buffer_size);
			printf("TLSF: initialization with align=%d, contiguous=%d, size=%d FAILED!!!\n", tlsf_align_size(), pool_info.contiguous_size, pool_info.buffer_size);
		}
		else {
			//sprintf(_tlsf_msg, "TLSF: initialization with align=%d, contiguous=%d, size=%d SUCCESSFUL!!!", tlsf_align_size(), pool_info.contiguous_size, pool_info.buffer_size);
			printf("TLSF: initialization with align=%d, contiguous=%d, size=%d SUCCESSFUL!!!\n", tlsf_align_size(), pool_info.contiguous_size, pool_info.buffer_size);
		}
#else
		Serial.print("TLSF: raw_buffer: ");
		Serial.println((long)raw_buffer);
		Serial.print("TLSF: align_size: ");
		Serial.println((long)tlsf_align_size());
		void* aligned_buffer = (void*)(((size_t)raw_buffer + (tlsf_align_size() - 1)) & ~(tlsf_align_size() - 1));
		Serial.print("TLSF: aligned_buffer: ");
		Serial.println((long)aligned_buffer);
		pool_info.tlsf = tlsf_create_with_pool(aligned_buffer, pool_info.buffer_size-(size_t)((uint32_t)aligned_buffer - (uint32_t)raw_buffer));
		//tlfs = tlsf_create_with_pool(aligned_buffer, pool_info.buffer_size--(size_t)((uint32_t)aligned_buffer - (uint32_t)raw_buffer));
#endif
		if (pool_info.tlsf == nullptr) {
			ERROR("-- TLSF: initialization of FAILED");
		}
	}

}

/*static*/ void* Memory::pool_malloc(Memory::pool_info& pool_info, size_t size) {
	if (size == 0) return nullptr;
	//if (pool_info.tlsf == nullptr) {
	if (!pool_info.pool_init) {
		Memory::pool_init(pool_info);
	}
	void* p;
	if (pool_info.tlsf != nullptr) {
		//struct tlsf_stats stats;
		//memset(&stats, 0, sizeof(stats));
		//tlsf_walk_pool(tlsf_get_pool(pool_info.tlsf), tlsf_mem_walker, &stats);
		//if (tlsf_check(pool_info.tlsf) != 0) {
		//	printf("--- HEAP CORRUPTION DETECTED!!!\n");
		//}
		//TRACEF("--- allocating memory from tlsf (%u bytes)", size);
		//printf("--- allocating memory from tlsf (%u bytes) (%u free)\n", size, stats.free_size);
		p = tlsf_malloc(pool_info.tlsf, size);
		//TRACEF("--- allocated memory from tlsf (addr=%lx) (%u bytes)", p, size);
		//printf("--- allocated memory from tlsf (addr=%lx) (%u bytes)\n", p, size);
	}
	else {
		//TRACEF("--- allocating memory (%u bytes)", size);
		p = malloc(size);
		//TRACEF("--- allocated memory (addr=%lx) (%u bytes)", p, size);
		++pool_info.alloc_fault;
	}
	return p;
}

/*static*/ void Memory::pool_free(Memory::pool_info& pool_info, void* p, size_t size /*= 0*/) noexcept {
	if (p == nullptr) return;
	if (pool_info.tlsf != nullptr) {
		//TRACEF("--- freeing memory from tlsf (addr=%lx)", p);
		//printf("--- freeing memory from tlsf (addr=%lx)\n", p);
		tlsf_free(pool_info.tlsf, p);
	}
	else {
		//TRACEF("--- freeing memory (addr=%lx)", p);
		free(p);
		++pool_info.free_fault;
	}
	/*
	if (free_count == _alloc_count) {
		TRACE("TLFS deinitializing");
		dump_memory_stats();
		tlsf_destroy(pool_info.tlsf);
		pool_info.tlsf = nullptr;
	}
	*/
}


struct tlsf_stats {
	uint32_t used_count = 0;
	uint32_t used_size = 0;
	uint32_t free_count = 0;
	uint32_t free_size = 0;
	uint32_t free_max_size = 0;
};

void tlsf_mem_walker(void* ptr, size_t size, int used, void* user)
{
	if (!user) {
		return;
	}
	struct tlsf_stats* stats = (struct tlsf_stats*)user;
	if (used) {
		stats->used_count++;
		stats->used_size += size;
	}
	else {
		stats->free_count++;
		stats->free_size += size;
		if (size > stats->free_max_size) {
			stats->free_max_size = size;
		}
	}
}

/*static*/ void Memory::dump_pool_stats(Memory::pool_info& pool_info, const char* name /*= ""*/) {
	//TRACEF("TLSF Message: %s", _tlsf_msg);
	if (pool_info.tlsf == nullptr) {
		return;
	}
	struct tlsf_stats stats;
	memset(&stats, 0, sizeof(stats));
	tlsf_walk_pool(tlsf_get_pool(pool_info.tlsf), tlsf_mem_walker, &stats);
	HEADF(LOG_TRACE, "%sPool Stats", name);
	TRACEF("  Buffer Size:     %u", pool_info.buffer_size);
	TRACEF("  Contiguous Size: %u", pool_info.contiguous_size);
	TRACEF("  Used Count:      %u", stats.used_count);
	TRACEF("  Used Size:       %u (%u%% used)", stats.used_size, (unsigned)((double)stats.used_size / (double)pool_info.buffer_size * 100.0));
	TRACEF("  Free Count:      %u", stats.free_count);
	TRACEF("  Free Size:       %u (%u%% free)", stats.free_size, (unsigned)((double)stats.free_size / (double)pool_info.buffer_size * 100.0));
	TRACEF("  Max Free Size:   %u (%u%% fragmented)", stats.free_max_size, (unsigned)(100.0 - (double)stats.free_max_size / (double)stats.free_size * 100.0));
	//TRACEF("  alloc_count:     %u", pool_info.alloc_count);
	TRACEF("  alloc_fault:     %u", pool_info.alloc_fault);
	//TRACEF("  free_count:      %u", pool_info.free_count);
	TRACEF("  free_fault:      %u", pool_info.free_fault);
}

/*static*/ void Memory::dump_basic_pool_stats() {

	size_t heap_free = 0;
	uint8_t heap_pct = 0;
	if (heap_pool_info.tlsf != nullptr) {
		struct tlsf_stats stats;
		memset(&stats, 0, sizeof(stats));
		tlsf_walk_pool(tlsf_get_pool(heap_pool_info.tlsf), tlsf_mem_walker, &stats);
		heap_free = stats.free_size;
		heap_pct = (uint8_t)((double)heap_free / (double)heap_pool_info.buffer_size * 100.0);
	}
	if (heap_pool_info.last_free == 0) {
		heap_pool_info.last_free = heap_free;
	}

	size_t psram_free = 0;
	uint8_t psram_pct = 0;
	if (psram_pool_info.tlsf != nullptr) {
		struct tlsf_stats stats;
		memset(&stats, 0, sizeof(stats));
		tlsf_walk_pool(tlsf_get_pool(psram_pool_info.tlsf), tlsf_mem_walker, &stats);
		psram_free = stats.free_size;
		psram_pct = (uint8_t)((double)psram_free / (double)psram_pool_info.buffer_size * 100.0);
	}
	if (psram_pool_info.last_free == 0) {
		psram_pool_info.last_free = psram_free;
	}

	size_t altheap_free = 0;
	uint8_t altheap_pct = 0;
	if (altheap_pool_info.tlsf != nullptr) {
		struct tlsf_stats stats;
		memset(&stats, 0, sizeof(stats));
		tlsf_walk_pool(tlsf_get_pool(altheap_pool_info.tlsf), tlsf_mem_walker, &stats);
		altheap_free = stats.free_size;
		altheap_pct = (uint8_t)((double)altheap_free / (double)altheap_pool_info.buffer_size * 100.0);
	}
	if (altheap_pool_info.last_free == 0) {
		altheap_pool_info.last_free = altheap_free;
	}

	VERBOSEF("heap_pool: %u (%u%%) [%d] psram_pool: %u (%u%%) [%d] alt_pool: %u (%u%%) [%d]", heap_free, heap_pct, heap_free - heap_pool_info.last_free, psram_free, psram_pct, psram_free - psram_pool_info.last_free, altheap_free, altheap_pct, altheap_free - altheap_pool_info.last_free);

	heap_pool_info.last_free = heap_free;
	psram_pool_info.last_free = psram_free;
	altheap_pool_info.last_free = altheap_free;
}

/*static*/ void Memory::dump_allocator_stats(Memory::allocator_info& allocator_info, const char* name /*= ""*/) {
	HEADF(LOG_TRACE, "%sAllocator Stats", name);
	TRACEF("  Active Count: %u", (allocator_info.alloc_count - allocator_info.free_count));
	TRACEF("  Min Size:     %u", allocator_info.min_alloc_size);
	TRACEF("  Max Size:     %u", allocator_info.max_alloc_size);
	TRACEF("  Avg Size:     %u", (size_t)((allocator_info.alloc_count > 0) ? (allocator_info.alloc_size / allocator_info.alloc_count) : 0));
	TRACEF("  alloc_count:  %u", allocator_info.alloc_count);
	TRACEF("  alloc_fault:  %u", allocator_info.alloc_fault);
	TRACEF("  free_count:   %u", allocator_info.free_count);
	TRACEF("  free_fault:   %u", allocator_info.free_fault);
}

/*static*/ void Memory::dump_basic_allocator_stats() {

	int dflt_diff = 0;
	// Increase in used memory, negative diff
	if (default_allocator_info.alloc_size > default_allocator_info.last_size) {
		dflt_diff = (int)(default_allocator_info.alloc_size - default_allocator_info.last_size) * -1;
	}
	// Decrease in used memory, positive diff
	else {
		dflt_diff = (int)(default_allocator_info.last_size - default_allocator_info.alloc_size);
	}
	//if (default_allocator_info.last_size == 0) {
	//	default_allocator_info.last_size = default_allocator_info.alloc_size;
	//}

	int cntr_diff = 0;
	// Increase in used memory, negative diff
	if (container_allocator_info.alloc_size > container_allocator_info.last_size) {
		cntr_diff = (int)(container_allocator_info.alloc_size - container_allocator_info.last_size) * -1;
	}
	// Decrease in used memory, positive diff
	else {
		cntr_diff = (int)(container_allocator_info.last_size - container_allocator_info.alloc_size);
	}
	//if (container_allocator_info.last_size == 0) {
	//	container_allocator_info.last_size = container_allocator_info.alloc_size;
	//}

	VERBOSEF("dflt_alloc: %llu [%d] cntr_alloc: %llu [%d]", default_allocator_info.alloc_size, dflt_diff, container_allocator_info.alloc_size, cntr_diff);

	default_allocator_info.last_size = default_allocator_info.alloc_size;
	container_allocator_info.last_size = container_allocator_info.alloc_size;
}


size_t maxContiguousAllocation() {
	// Brute-force determine maximum allocation size
	//const size_t block_size = 256;
	const size_t block_size = 32;
	size_t block_count;
	for (block_count = 1; ; block_count++) {
		void* ptr = malloc(block_count * block_size);
		if (ptr == nullptr) {
			break;
		}
		free(ptr);
	}
	return (block_count - 1) * block_size;
}

/*static*/ size_t Memory::heap_size() {
#if defined(ESP32)
	return ESP.getHeapSize();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	return dbgHeapTotal();
#else
	return 0;
#endif
}

/*static*/ size_t Memory::heap_available() {
#if defined(ESP32)
	return ESP.getFreeHeap();
	//return ESP.getMaxAllocHeap();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	return dbgHeapFree();
#else
	return 0;
#endif
}

/*static*/ void Memory::dump_heap_stats() {
#if defined(ESP32)
	HEAD("Heap Stats", LOG_TRACE);
	TRACEF("  Heap size:       %u", ESP.getHeapSize());
	TRACEF("  Heap free:       %u (%u%% free)", ESP.getFreeHeap(), (unsigned)((double)ESP.getFreeHeap() / (double)ESP.getHeapSize() * 100.0));
	//TRACEF("  Heap free:       %u (%u%% free)", xPortGetFreeHeapSize(), (unsigned)((double)xPortGetFreeHeapSize() / (double)xPort * 100.0));
	TRACEF("  Heap min free:   %u", ESP.getMinFreeHeap());
	//TRACEF("  Heap min free:   %u", xPortGetMinimumEverFreeHeapSize());
	TRACEF("  Heap max alloc:  %u (%u%% fragmented)", ESP.getMaxAllocHeap(), (unsigned)(100.0 - (double)ESP.getMaxAllocHeap() / (double)ESP.getFreeHeap() * 100.0));
	//TRACEF("  Heap max alloc:  %u (%u%% fragmented)", ESP.getMaxAllocHeap(), (unsigned)(100.0 - (double)ESP.getMaxAllocHeap() / (double)xPortGetFreeHeapSize() * 100.0));
	TRACEF("  PSRAM size:      %u", ESP.getPsramSize());
	TRACEF("  PSRAM free:      %u (%u%% free)", ESP.getFreePsram(), (ESP.getPsramSize() > 0) ? (unsigned)((double)ESP.getFreePsram() / (double)ESP.getPsramSize() * 100.0) : 0);
	TRACEF("  PSRAM min free:  %u", ESP.getMinFreePsram());
	TRACEF("  PSRAM max alloc: %u (%u%% fragmented)", ESP.getMaxAllocPsram(), (ESP.getFreePsram() > 0) ? (unsigned)(100.0 - (double)ESP.getMaxAllocPsram() / (double)ESP.getFreePsram() * 100.0) : 0);
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	HEAD("Heap Stats", LOG_TRACE);
	if (loglevel() == LOG_TRACE) {
		dbgMemInfo();
	}
#endif

	dump_allocator_stats(default_allocator_info, "Default ");
	dump_allocator_stats(container_allocator_info, "Container ");

	dump_pool_stats(heap_pool_info, "Heap ");
	dump_pool_stats(psram_pool_info, "PSRAM ");
	dump_pool_stats(altheap_pool_info, "AltHeap ");
	dump_basic_pool_stats();
	dump_basic_allocator_stats();

}


// Only override new/delete if RNS_DEFAULT_ALLOCATOR is not RNS_HEAP_ALLOCATOR
#if RNS_DEFAULT_ALLOCATOR != RNS_HEAP_ALLOCATOR

void* allocator_malloc(size_t size) {
	if (size == 0) return nullptr;
	++Memory::default_allocator_info.alloc_count;
	Memory::default_allocator_info.alloc_size += size;
	if (size < Memory::default_allocator_info.min_alloc_size || Memory::default_allocator_info.min_alloc_size == 0) {
		Memory::default_allocator_info.min_alloc_size = size;
	}
	if (size > Memory::default_allocator_info.max_alloc_size) {
		Memory::default_allocator_info.max_alloc_size = size;
	}
#if RNS_DEFAULT_ALLOCATOR == RNS_HEAP_POOL_ALLOCATOR
	return Memory::pool_malloc(Memory::heap_pool_info, size);
#elif RNS_DEFAULT_ALLOCATOR == RNS_PSRAM_ALLOCATOR
#if BOARD_HAS_PSRAM != 1
	#error "BOARD_HAS_PSRAM must be defined to use RNS_PSRAM_POOL_ALLOCATOR allocator."
#endif
	// CBA PSRAM may not be accessible early in startup, but since we use the same free() for both
	//  HEAP and PSRAM allocations, we canb simply fallback to HEAP allocation when PSRAM fails
	void* p = ps_malloc(size);
	if (p == nullptr) {
		++Memory::default_allocator_info.alloc_fault;
		p = malloc(size);
	}
	return p;
#elif RNS_DEFAULT_ALLOCATOR == RNS_PSRAM_POOL_ALLOCATOR
#error "RNS_DEFAULT_ALLOCATOR can not be set to RNS_PSRAM_POOL_ALLOCATOR because PSRAM isn't initialized early enough."
#if BOARD_HAS_PSRAM != 1
	#error "BOARD_HAS_PSRAM must be defined to use RNS_PSRAM_POOL_ALLOCATOR allocator."
#endif
	return Memory::pool_malloc(Memory::psram_pool_info, size);
#elif RNS_DEFAULT_ALLOCATOR == RNS_ALTHEAP_POOL_ALLOCATOR
	return Memory::pool_malloc(Memory::altheap_pool_info, size);
#elif RNS_DEFAULT_ALLOCATOR == RNS_HEAP_ALLOCATOR
	return ::operator new(n * sizeof(value_type));
#else
	#error "Unknown or invalid allocator."
#endif
}

void allocator_free(void* p, size_t size = 0) noexcept {
	if (p == nullptr) return;
	++Memory::default_allocator_info.free_count;
	Memory::default_allocator_info.alloc_size -= size;
#if RNS_DEFAULT_ALLOCATOR == RNS_HEAP_POOL_ALLOCATOR
	Memory::pool_free(Memory::heap_pool_info, p, size);
#elif RNS_DEFAULT_ALLOCATOR == RNS_PSRAM_ALLOCATOR
#if BOARD_HAS_PSRAM != 1
	#error "BOARD_HAS_PSRAM must be defined to use RNS_PSRAM_POOL_ALLOCATOR allocator."
#endif
	free(p);
#elif RNS_DEFAULT_ALLOCATOR == RNS_PSRAM_POOL_ALLOCATOR
#error "RNS_DEFAULT_ALLOCATOR can not be set to RNS_PSRAM_POOL_ALLOCATOR because PSRAM isn't initialized early enough."
#if BOARD_HAS_PSRAM != 1
	#error "BOARD_HAS_PSRAM must be defined to use RNS_PSRAM_POOL_ALLOCATOR allocator."
#endif
	Memory::pool_free(Memory::psram_pool_info, p, size);
#elif RNS_DEFAULT_ALLOCATOR == RNS_ALTHEAP_POOL_ALLOCATOR
	Memory::pool_free(Memory::altheap_pool_info, p, size);
#elif RNS_DEFAULT_ALLOCATOR == RNS_HEAP_ALLOCATOR
	::operator delete(p);
#else
	#error "Unknown or invalid allocator."
#endif
}

//--------------------------------------------------
// Standard allocation
//--------------------------------------------------

// CBA Added attribute weak to avoid collision with new override on nrf52
void* operator new(size_t size) {
//__attribute__((weak)) void* operator new(size_t size) {
	//printf("--- new allocating memory from tlsf (%u bytes)\n", size);
	void* p = allocator_malloc(size);
	if (p == nullptr) {
		throw std::bad_alloc();
	}
	return p;
}

void* operator new[](size_t size) {
	//printf("--- new[] allocating memory from tlsf (%u bytes)\n", size);
	void* p = allocator_malloc(size);
	if (p == nullptr) {
		throw std::bad_alloc();
	}
	return p;
}

//--------------------------------------------------
// Nothrow allocation
//--------------------------------------------------

void* operator new(size_t size, const std::nothrow_t&) noexcept {
	//printf("--- new no-throw allocating memory from tlsf (%u bytes)\n", size);
    return allocator_malloc(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
	//printf("--- new[] no-throw allocating memory from tlsf (%u bytes)\n", size);
    return allocator_malloc(size);
}

//--------------------------------------------------
// Standard delete
//--------------------------------------------------

// CBA Added attribute weak to avoid collision with new override on nrf52
void operator delete(void* p) noexcept {
//__attribute__((weak)) void operator delete(void* p) noexcept {
	//printf("--- delete freeing memory from tlsf (addr=%lx)\n", p);
	if (p != nullptr) allocator_free(p);
}

void operator delete[](void* p) noexcept {
	//printf("--- delete[] freeing memory from tlsf (addr=%lx)\n", p);
	if (p != nullptr) allocator_free(p);
}

//--------------------------------------------------
// Sized delete (C++14+)
//--------------------------------------------------

void operator delete(void* p, size_t size) noexcept {   // sized delete
	//printf("--- delete(size) freeing memory from tlsf (addr=%lx)\n", p);
	if (p != nullptr) allocator_free(p, size);
}

void operator delete[](void* p, size_t size) noexcept {
	//printf("--- delete[](size) freeing memory from tlsf (addr=%lx)\n", p);
	if (p != nullptr) allocator_free(p, size);
}

//--------------------------------------------------
// Nothrow delete
//--------------------------------------------------

void operator delete(void* p, const std::nothrow_t&) noexcept {
	//printf("--- delete no-throw freeing memory from tlsf (addr=%lx)\n", p);
	if (p != nullptr) allocator_free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
	//printf("--- delete[] no-throw freeing memory from tlsf (addr=%lx)\n", p);
	if (p != nullptr) allocator_free(p);
}

//--------------------------------------------------
// Aligned allocation/delete (C++17+)
//--------------------------------------------------

#if __cplusplus >= 201703L
void* operator new(size_t size, std::align_val_t) {
	//printf("--- new aligned allocating memory from tlsf (%u bytes)\n", size);
	void* p = allocator_malloc(size);
	if (p == nullptr) {
		throw std::bad_alloc();
	}
	return p;
}

void operator delete(void* p, std::align_val_t) {
	//printf("--- delete aligned freeing memory from tlsf (addr=%lx)\n", p);
	if (p != nullptr) allocator_free(p);
}
#endif

#endif
