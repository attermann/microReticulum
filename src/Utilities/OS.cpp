#include "OS.h"

#include "../Log.h"

using namespace RNS;
using namespace RNS::Utilities;


#if defined(RNS_USE_ALLOCATOR)

#if defined(RNS_USE_TLSF)
#if defined(ESP32)
	#define BUFFER_SIZE 1024 * 80
	#define BUFFER_FRACTION 0.8
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	#define BUFFER_SIZE 1024 * 80
	#define BUFFER_FRACTION 0.8
#endif

boolean tlsf_init = false;
size_t _buffer_size = BUFFER_SIZE;
size_t _contiguous_size = 0;

/*static*/ //tlsf_t OS::_tlsf = tlsf_create_with_pool(malloc(1024 * 1024), 1024 * 1024);
/*static*/ tlsf_t OS::_tlsf = nullptr;
#endif

uint32_t _new_count = 0;
uint32_t _new_fault = 0;
uint64_t _new_size = 0;
uint32_t _delete_count = 0;
uint32_t _delete_fault = 0;
size_t _min_size = 0;
size_t _max_size = 0;

// CBA Added attribute weak to avoid collision with new override on nrf52
void* operator new(size_t size) {
//__attribute__((weak)) void* operator new(size_t size) {
#if defined(RNS_USE_TLSF)
	//if (OS::_tlsf == nullptr) {
	if (!tlsf_init) {
		tlsf_init = true;
#if defined(ESP32)
		_contiguous_size = ESP.getMaxAllocHeap();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
		_contiguous_size = dbgHeapFree();
#endif
		TRACEF("contiguous_size: %u", _contiguous_size);

		_buffer_size = (size_t)((_contiguous_size * BUFFER_FRACTION) / 1024) * 1024;
		TRACEF("buffer_size: %u", _buffer_size);

		//void* raw_buffer = malloc(BUFFER_SIZE);
		void* raw_buffer = malloc(_buffer_size);
		if (raw_buffer == nullptr) {
			ERROR("-- allocation for tlsf FAILED");
		}
		else {
#if 1
			//OS::_tlsf = tlsf_create_with_pool(raw_buffer, BUFFER_SIZE);
			OS::_tlsf = tlsf_create_with_pool(raw_buffer, _buffer_size);
#else
			Serial.print("raw_buffer: ");
			Serial.println((long)raw_buffer);
			Serial.print("align_size: ");
			Serial.println((long)tlsf_align_size());
			void* aligned_buffer = (void*)(((size_t)raw_buffer + (tlsf_align_size() - 1)) & ~(tlsf_align_size() - 1));
			Serial.print("aligned_buffer: ");
			Serial.println((long)aligned_buffer);
			OS::_tlsf = tlsf_create_with_pool(aligned_buffer, BUFFER_SIZE-(size_t)((uint32_t)aligned_buffer - (uint32_t)raw_buffer));
			//tlfs = tlsf_create_with_pool(aligned_buffer, buffer_size--(size_t)((uint32_t)aligned_buffer - (uint32_t)raw_buffer));
#endif
			if (OS::_tlsf == nullptr) {
				ERROR("-- initialization of tlsf FAILED");
			}
		}
	}
#endif
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
		//TRACEF("--- allocating memory from tlsf (%u bytes)", size);
    	p = tlsf_malloc(OS::_tlsf, size);
		//TRACEF("--- allocated memory from tlsf (%u bytes) (addr=%lx)", size, p);
	}
	else {
		//TRACEF("--- allocating memory (%u bytes)", size);
		p = malloc(size);
		//TRACEF("--- allocated memory (%u bytes) (addr=%lx)", size, p);
		++_new_fault;
	}
#else
	//TRACEF("--- allocating memory (%u bytes)", size);
	p = malloc(size);
	//TRACEF("--- allocated memory (%u bytes) (addr=%lx)", size, p);
#endif
	return p;
}
 
// CBA Added attribute weak to avoid collision with new override on nrf52
void operator delete(void* p) {
//__attribute__((weak)) void operator delete(void* p) {
#if defined(RNS_USE_TLSF)
	if (OS::_tlsf != nullptr) {
		//TRACEF("--- freeing memory from tlsf (addr=%lx)", p);
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
#endif
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

#if defined(RNS_USE_TLSF)
uint32_t _tlsf_used_count = 0;
uint32_t _tlsf_used_size = 0;
uint32_t _tlsf_free_count = 0;
uint32_t _tlsf_free_size = 0;
uint32_t _tlsf_free_max_size = 0;
void tlsf_mem_walker(void* ptr, size_t size, int used, void* user)
{
	if (used) {
		_tlsf_used_count++;
		_tlsf_used_size += size;
	}
	else {
		_tlsf_free_count++;
		_tlsf_free_size += size;
		if (size > _tlsf_free_max_size) {
			_tlsf_free_max_size = size;
		}
	}
}
void dump_tlsf_stats() {
	_tlsf_used_count = 0;
	_tlsf_used_size = 0;
	_tlsf_free_count = 0;
	_tlsf_free_size = 0;
	_tlsf_free_max_size = 0;
	tlsf_walk_pool(tlsf_get_pool(OS::_tlsf), tlsf_mem_walker, nullptr);
	HEAD("TLSF Stats", LOG_TRACE);
	TRACEF("Buffer Size:     %u", _buffer_size);
	TRACEF("Contiguous Size: %u", _contiguous_size);
	TRACEF("Used Count:      %u", _tlsf_used_count);
	TRACEF("Used Size:       %u (%u%% used)", _tlsf_used_size, (unsigned)((double)_tlsf_used_size / (double)_buffer_size * 100.0));
	TRACEF("Free Count:      %u", _tlsf_free_count);
	TRACEF("Free Size:       %u (%u%% free)", _tlsf_free_size, (unsigned)((double)_tlsf_free_size / (double)_buffer_size * 100.0));
	TRACEF("Max Free Size:   %u (%u%% fragmented)\n", _tlsf_free_max_size, (unsigned)(100.0 - (double)_tlsf_free_max_size / (double)_tlsf_free_size * 100.0));
}
#endif

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

/*static*/ Filesystem OS::_filesystem = {Type::NONE};
/*static*/ uint64_t OS::_time_offset = 0;

/*static*/ void OS::register_filesystem(Filesystem& filesystem) {
	TRACE("Registering filesystem...");
	OS::_filesystem = filesystem;
}

/*static*/ void OS::deregister_filesystem() {
	TRACE("Deregistering filesystem...");
	OS::_filesystem = {Type::NONE};
}

/*static*/ bool OS::file_exists(const char* file_path) {
	if (!_filesystem) {
		WARNING("file_exists: filesystem not registered");
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_file_exists(file_path);
}

/*static*/ size_t OS::read_file(const char* file_path, Bytes& data) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_read_file(file_path, data);
}

/*static*/ size_t OS::write_file(const char* file_path, const Bytes& data) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_write_file(file_path, data);
}

/*static*/ bool OS::remove_file(const char* file_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_remove_file(file_path);
}

/*static*/ bool OS::rename_file(const char* from_file_path, const char* to_file_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_rename_file(from_file_path, to_file_path);
}

/*static*/ bool OS::directory_exists(const char* directory_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_directory_exists(directory_path);
}

/*static*/ bool OS::create_directory(const char* directory_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_create_directory(directory_path);
}

/*static*/ bool OS::remove_directory(const char* directory_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_remove_directory(directory_path);
}

/*static*/ std::list<std::string> OS::list_directory(const char* directory_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_list_directory(directory_path);
}

/*static*/ size_t OS::storage_size() {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_storage_size();
}

/*static*/ size_t OS::storage_available() {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_storage_available();
}

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
