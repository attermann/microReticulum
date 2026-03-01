#include "OS.h"

#include "../Type.h"
#include "../Log.h"

#include <new>

using namespace RNS;
using namespace RNS::Utilities;


#if defined(RNS_USE_ALLOCATOR)

#if defined(RNS_USE_TLSF)
#if defined(ESP32)
	#ifndef RNS_TLSF_BUFFER_SIZE
		//#define RNS_TLSF_BUFFER_SIZE 1024 * 80
		#define RNS_TLSF_BUFFER_SIZE 0
	#endif
	#ifndef BUFFER_FRACTION
		#define BUFFER_FRACTION 0.8
	#endif
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	#ifndef RNS_TLSF_BUFFER_SIZE
		//#define RNS_TLSF_BUFFER_SIZE 1024 * 80
		#define RNS_TLSF_BUFFER_SIZE 0
	#endif
	#ifndef BUFFER_FRACTION
		#define BUFFER_FRACTION 0.8
	#endif
#else
	#ifndef RNS_TLSF_BUFFER_SIZE
		#define RNS_TLSF_BUFFER_SIZE 1024 * 1000
	#endif
	#ifndef BUFFER_FRACTION
		#define BUFFER_FRACTION 0
	#endif
#endif

struct tlsf_stats {
	uint32_t used_count = 0;
	uint32_t used_size = 0;
	uint32_t free_count = 0;
	uint32_t free_size = 0;
	uint32_t free_max_size = 0;
};

bool _pool_init = false;
//char _tlsf_msg[256] = "";
size_t _buffer_size = RNS_TLSF_BUFFER_SIZE;
size_t _contiguous_size = 0;

/*static*/ //tlsf_t OS::_tlsf = tlsf_create_with_pool(malloc(1024 * 1024), 1024 * 1024);
/*static*/ tlsf_t OS::_tlsf = nullptr;
#endif // RNS_USE_TLSF

uint32_t _new_count = 0;
uint32_t _new_fault = 0;
uint64_t _new_size = 0;
uint32_t _delete_count = 0;
uint32_t _delete_fault = 0;
size_t _min_size = 0;
size_t _max_size = 0;

void tlsf_mem_walker(void* ptr, size_t size, int used, void* user);
void dump_tlsf_stats();

#if defined(RNS_USE_TLSF)
inline void pool_init() {

#if defined(ESP32)
	// CBA Still unknown why the call to tlsf_create_with_pool() is so flaky on ESP32 with calculated buffer size. Reuires more research and unit tests.
	_contiguous_size = ESP.getMaxAllocHeap();
	TRACEF("TLSF: contiguous_size: %u", _contiguous_size);
	if (_buffer_size == 0) {
		// CBA NOTE Using fp mathhere somehow causes tlsf_create_with_pool() to fail.
		//_buffer_size = (size_t)(_contiguous_size * BUFFER_FRACTION);
		// Compute 80% exactly using integers
		_buffer_size = (_contiguous_size * 4) / 5;
	}
	// Round DOWN to TLSF alignment
	size_t align = tlsf_align_size();
	_buffer_size &= ~(align - 1);
	void* raw_buffer = (void*)aligned_alloc(align, _buffer_size);
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	_contiguous_size = dbgHeapFree();
	TRACEF("TLSF: contiguous_size: %u", _contiguous_size);
	if (_buffer_size == 0) {
		_buffer_size = (size_t)(_contiguous_size * BUFFER_FRACTION);
	}
	// For NRF52 round to kB
	_buffer_size = (size_t)(_buffer_size / 1024) * 1024;
	TRACEF("TLSF: buffer_size: %u", _buffer_size);
	void* raw_buffer = malloc(_buffer_size);
#else
	_buffer_size = (size_t)RNS_TLSF_BUFFER_SIZE;
	TRACEF("TLSF: buffer_size: %u", _buffer_size);
	void* raw_buffer = malloc(_buffer_size);
#endif
	if (raw_buffer == nullptr) {
		ERROR("-- TLSF: buffer allocation FAILED");
		//strcpy(_tlsf_msg, "-- TLSF: buffer allocation FAILED!!!");
	}
	else {
#if 1
		OS::_tlsf = tlsf_create_with_pool(raw_buffer, _buffer_size);
		if (OS::_tlsf == nullptr) {
			//sprintf(_tlsf_msg, "TLSF: initialization with align=%d, contiguous=%d, size=%d FAILED!!!", tlsf_align_size(), _contiguous_size, _buffer_size);
			printf("TLSF: initialization with align=%d, contiguous=%d, size=%d FAILED!!!\n", tlsf_align_size(), _contiguous_size, _buffer_size);
		}
		else {
			//sprintf(_tlsf_msg, "TLSF: initialization with align=%d, contiguous=%d, size=%d SUCCESSFUL!!!", tlsf_align_size(), _contiguous_size, _buffer_size);
			printf("TLSF: initialization with align=%d, contiguous=%d, size=%d SUCCESSFUL!!!\n", tlsf_align_size(), _contiguous_size, _buffer_size);
		}
#else
		Serial.print("TLSF: raw_buffer: ");
		Serial.println((long)raw_buffer);
		Serial.print("TLSF: align_size: ");
		Serial.println((long)tlsf_align_size());
		void* aligned_buffer = (void*)(((size_t)raw_buffer + (tlsf_align_size() - 1)) & ~(tlsf_align_size() - 1));
		Serial.print("TLSF: aligned_buffer: ");
		Serial.println((long)aligned_buffer);
		OS::_tlsf = tlsf_create_with_pool(aligned_buffer, _buffer_size-(size_t)((uint32_t)aligned_buffer - (uint32_t)raw_buffer));
		//tlfs = tlsf_create_with_pool(aligned_buffer, buffer_size--(size_t)((uint32_t)aligned_buffer - (uint32_t)raw_buffer));
#endif
		if (OS::_tlsf == nullptr) {
			ERROR("-- TLSF: initialization of FAILED");
		}
	}

}
#endif // RNS_USE_TLSF

void* pool_malloc(std::size_t size) {
#if defined(RNS_USE_TLSF)
	//if (OS::_tlsf == nullptr) {
	if (!_pool_init) {
		_pool_init = true;
		printf("Initializing TLSF pool...\n");
		pool_init();
	}
#endif // RNS_USE_TLSF
	++_new_count;
	_new_size += size;
	if (size < _min_size || _min_size == 0) {
		_min_size = size;
	}
	//if (size > _max_size) {
	if (size < 4192 && size > _max_size) {
		_max_size = size;
	}
	void* p;
#if defined(RNS_USE_TLSF)
	if (OS::_tlsf != nullptr) {
		//struct tlsf_stats stats;
		//memset(&stats, 0, sizeof(stats));
		//tlsf_walk_pool(tlsf_get_pool(OS::_tlsf), tlsf_mem_walker, &stats);
		//if (tlsf_check(OS::_tlsf) != 0) {
		//	printf("--- HEAP CORRUPTION DETECTED!!!\n");
		//}
		//TRACEF("--- allocating memory from tlsf (%u bytes)", size);
		//printf("--- allocating memory from tlsf (%u bytes) (%u free)\n", size, stats.free_size);
    	p = tlsf_malloc(OS::_tlsf, size);
		//TRACEF("--- allocated memory from tlsf (addr=%lx) (%u bytes)", p, size);
		//printf("--- allocated memory from tlsf (addr=%lx) (%u bytes)\n", p, size);
	}
	else {
		//TRACEF("--- allocating memory (%u bytes)", size);
		p = malloc(size);
		//TRACEF("--- allocated memory (addr=%lx) (%u bytes)", p, size);
		++_new_fault;
	}
#else
	//TRACEF("--- allocating memory (%u bytes)", size);
	p = malloc(size);
	//TRACEF("--- allocated memory (addr=%lx) (%u bytes)", p, size);
#endif // RNS_USE_TLSF
	return p;
}

void pool_free(void* p) noexcept {
#if defined(RNS_USE_TLSF)
	if (OS::_tlsf != nullptr) {
		//TRACEF("--- freeing memory from tlsf (addr=%lx)", p);
		//printf("--- freeing memory from tlsf (addr=%lx)\n", p);
		tlsf_free(OS::_tlsf, p);
	}
	else {
		//TRACEF("--- freeing memory (addr=%lx)", p);
		free(p);
		++_delete_fault;
	}
#else
	//TRACEF("--- freeing memory (addr=%lx)", p);
	//TRACE("--- freeing memory");
	free(p);
#endif // RNS_USE_TLSF
	++_delete_count;
#if defined(RNS_USE_TLSF)
	//if (_delete_count == _new_count) {
	//	TRACE("TLFS deinitializing");
	//	OS::dump_memory_stats();
	//	tlsf_destroy(OS::_tlsf);
	//	OS::_tlsf = nullptr;
	//}
#endif
}


// CBA Added attribute weak to avoid collision with new override on nrf52
void* operator new(std::size_t size) {
//__attribute__((weak)) void* operator new(std::size_t size) {
	//printf("--- new allocating memory from tlsf (%u bytes)\n", size);
	void* p = pool_malloc(size);
	if (p == nullptr) {
		throw std::bad_alloc();
	}
	return p;
}
 
// CBA Added attribute weak to avoid collision with new override on nrf52
void operator delete(void* p) noexcept {
//__attribute__((weak)) void operator delete(void* p) noexcept {
	//printf("--- delete freeing memory from tlsf (addr=%lx)\n", p);
	pool_free(p);
}


/**/
void* operator new[](std::size_t size) {
	//printf("--- new[] allocating memory from tlsf (%u bytes)\n", size);
	void* p = pool_malloc(size);
	if (p == nullptr) {
		throw std::bad_alloc();
	}
	return p;
}

void operator delete[](void* p) noexcept {
	//printf("--- delete[] freeing memory from tlsf (addr=%lx)\n", p);
	pool_free(p);
}


void operator delete(void* p, std::size_t size) noexcept {   // sized delete
	//printf("--- delete(size) freeing memory from tlsf (addr=%lx)\n", p);
	pool_free(p);
}

void operator delete[](void* p, std::size_t size) noexcept {
	//printf("--- delete[](size) freeing memory from tlsf (addr=%lx)\n", p);
	pool_free(p);
}
/**/


#if defined(RNS_USE_TLSF)
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
void dump_tlsf_stats() {
	struct tlsf_stats stats;
	memset(&stats, 0, sizeof(stats));
	//TRACEF("TLSF Message: %s", _tlsf_msg);
	if (OS::_tlsf == nullptr) {
		return;
	}
	tlsf_walk_pool(tlsf_get_pool(OS::_tlsf), tlsf_mem_walker, &stats);
	HEAD("TLSF Stats", LOG_TRACE);
	TRACEF("Buffer Size:     %u", _buffer_size);
	TRACEF("Contiguous Size: %u", _contiguous_size);
	TRACEF("Used Count:      %u", stats.used_count);
	TRACEF("Used Size:       %u (%u%% used)", stats.used_size, (unsigned)((double)stats.used_size / (double)_buffer_size * 100.0));
	TRACEF("Free Count:      %u", stats.free_count);
	TRACEF("Free Size:       %u (%u%% free)", stats.free_size, (unsigned)((double)stats.free_size / (double)_buffer_size * 100.0));
	TRACEF("Max Free Size:   %u (%u%% fragmented)\n", stats.free_max_size, (unsigned)(100.0 - (double)stats.free_max_size / (double)stats.free_size * 100.0));
}
#endif // RNS_USE_TLSF

/*static*/ void OS::dump_allocator_stats() {
	HEAD("Allocator Stats", LOG_TRACE);
	TRACEF("New Count:    %u", _new_count);
	TRACEF("New Fault:    %u", _new_fault);
	TRACEF("Delete Count: %u", _delete_count);
	TRACEF("Delete Fault: %u", _delete_fault);
	TRACEF("Active Count: %u", (_new_count - _delete_count));
	TRACEF("Min Size: %u", _min_size);
	TRACEF("Max Size: %u", _max_size);
	TRACEF("Avg Size: %u\n", (size_t)(_new_size / _new_count));
#if defined(RNS_USE_TLSF)
	dump_tlsf_stats();
#endif
}

#endif	// RNS_USE_ALLOCATOR


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

/*static*/ FileSystem OS::_filesystem = {Type::NONE};
/*static*/ uint64_t OS::_time_offset = 0;

/*static*/ size_t OS::heap_size() {
#if defined(ESP32)
	return ESP.getHeapSize();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	return dbgHeapTotal();
#else
	return 0;
#endif
}

/*static*/ size_t OS::heap_available() {
#if defined(ESP32)
	return ESP.getFreeHeap();
	//return ESP.getMaxAllocHeap();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	return dbgHeapFree();
#else
	return 0;
#endif
}

/*static*/ void OS::dump_heap_stats() {
	HEAD("Heap Stats", LOG_TRACE);
#if defined(ESP32)
	TRACEF("Heap size:       %u", ESP.getHeapSize());
	TRACEF("Heap free:       %u (%u%% free)", ESP.getFreeHeap(), (unsigned)((double)ESP.getFreeHeap() / (double)ESP.getHeapSize() * 100.0));
	//TRACEF("Heap free:       %u (%u%% free)", xPortGetFreeHeapSize(), (unsigned)((double)xPortGetFreeHeapSize() / (double)xPort * 100.0));
	TRACEF("Heap min free:   %u", ESP.getMinFreeHeap());
	//TRACEF("Heap min free:   %u", xPortGetMinimumEverFreeHeapSize());
	TRACEF("Heap max alloc:  %u (%u%% fragmented)", ESP.getMaxAllocHeap(), (unsigned)(100.0 - (double)ESP.getMaxAllocHeap() / (double)ESP.getFreeHeap() * 100.0));
	//TRACEF("Heap max alloc:  %u (%u%% fragmented)", ESP.getMaxAllocHeap(), (unsigned)(100.0 - (double)ESP.getMaxAllocHeap() / (double)xPortGetFreeHeapSize() * 100.0));
	TRACEF("PSRAM size:      %u", ESP.getPsramSize());
	TRACEF("PSRAM free:      %u (%u%% free)", ESP.getFreePsram(), (ESP.getPsramSize() > 0) ? (unsigned)((double)ESP.getFreePsram() / (double)ESP.getPsramSize() * 100.0) : 0);
	TRACEF("PSRAM min free:  %u", ESP.getMinFreePsram());
	TRACEF("PSRAM max alloc: %u (%u%% fragmented)", ESP.getMaxAllocPsram(), (ESP.getFreePsram() > 0) ? (unsigned)(100.0 - (double)ESP.getMaxAllocPsram() / (double)ESP.getFreePsram() * 100.0) : 0);
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	if (loglevel() == LOG_TRACE) {
		dbgMemInfo();
	}
#endif
#if defined(RNS_USE_ALLOCATOR)
	OS::dump_allocator_stats();
#endif
}
