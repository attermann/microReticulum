#include "Reticulum.h"

#include "Transport.h"
#include "Log.h"

//#include <TransistorNoiseSource.h>
#include <RNG.h>

#ifdef ARDUINO
#include <Arduino.h>
//#include <TransistorNoiseSource.h>
#endif

using namespace RNS;
using namespace RNS::Type::Reticulum;
using namespace RNS::Utilities;

/*static*/ //std::string Reticulum::_storagepath;
/*static*/ char Reticulum::_storagepath[FILEPATH_MAXSIZE];
/*static*/ //std::string Reticulum::_cachepath;
/*static*/ char Reticulum::_cachepath[FILEPATH_MAXSIZE];

/*static*/ const Reticulum& Reticulum::_instance = {Type::NONE};

/*static*/ bool Reticulum::__transport_enabled = false;
/*static*/ bool Reticulum::__link_mtu_discovery = RNS::Type::Reticulum::LINK_MTU_DISCOVERY;
/*static*/ bool Reticulum::__remote_management_enabled = false;
/*static*/ bool Reticulum::__use_implicit_proof = true;
/*static*/ bool Reticulum::__allow_probes = false;
/*static*/ bool Reticulum::panic_on_interface_error = false;

#ifdef ARDUINO
// Noise source to seed the random number generator.
//TransistorNoiseSource noise(A1);
#endif

/*
Initialises and starts a Reticulum instance. This must be
done before any other operations, and Reticulum will not
pass any traffic before being instantiated.

:param configdir: Full path to a Reticulum configuration directory.
*/

/*p TODO
@staticmethod
void Reticulum::exit_handler():
	# This exit handler is called whenever Reticulum is asked to
	# shut down, and will in turn call exit handlers in other
	# classes, saving necessary information to disk and carrying
	# out cleanup operations.

	Transport::exit_handler()
	RNS.Identity.exit_handler()

@staticmethod
void Reticulum::sigint_handler(signal, frame):
	Transport::detach_interfaces()
	RNS.exit()


@staticmethod
void Reticulum::sigterm_handler(signal, frame):
	Transport::detach_interfaces()
	RNS.exit()
*/

//def __init__(self,configdir=None, loglevel=None, logdest=None, verbosity=None):
Reticulum::Reticulum() : _object(new Object()) {
	MEM("Reticulum default object creating..., this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));

	// Initialize random number generator
	TRACE("Initializing RNG...");
	RNG.begin("Reticulum");
	TRACE("RNG initial random value: " + std::to_string(Cryptography::randomnum()));

#ifdef ARDUINO
	// Add a noise source to the list of sources known to RNG.
	//RNG.addNoiseSource(noise);
 #endif

	//z RNS.vendor.platformutils.platform_checks()

/*p TODO
	if configdir != None:
		Reticulum.configdir = configdir
	else:
		if os.path.isdir("/etc/reticulum") and os.path.isfile("/etc/reticulum/config"):
			Reticulum.configdir = "/etc/reticulum"
		elif os.path.isdir(Reticulum.userdir+"/.config/reticulum") and os.path.isfile(Reticulum.userdir+"/.config/reticulum/config"):
			Reticulum.configdir = Reticulum.userdir+"/.config/reticulum"
		else:
			Reticulum.configdir = Reticulum.userdir+"/.reticulum"

	if logdest == RNS.LOG_FILE:
		RNS.logdest = RNS.LOG_FILE
		RNS.logfile = Reticulum.configdir+"/logfile"

	Reticulum.configpath    = Reticulum.configdir+"/config"
	Reticulum.storagepath   = Reticulum.configdir+"/storage"
	Reticulum.cachepath     = Reticulum.configdir+"/storage/cache"
	Reticulum.resourcepath  = Reticulum.configdir+"/storage/resources"
	Reticulum.identitypath  = Reticulum.configdir+"/storage/identities"
*/
// CBA TEST
#ifdef ARDUINO
	//_storagepath = "";
	strncpy(_storagepath, "", FILEPATH_MAXSIZE);
	//_cachepath = "/cache";
	strncpy(_cachepath, "/cache", FILEPATH_MAXSIZE);
#else
	//_storagepath = ".";
	strncpy(_storagepath, ".", FILEPATH_MAXSIZE);
	//_cachepath = "./cache";
	strncpy(_cachepath, "./cache", FILEPATH_MAXSIZE);
#endif

#ifdef ARDUINO
#if defined(RNS_USE_FS)
	// load time offset from file if it exists
	try {
		char time_offset_path[FILEPATH_MAXSIZE];
		snprintf(time_offset_path, FILEPATH_MAXSIZE, "%s/time_offset", _storagepath);
		if (OS::file_exists(time_offset_path)) {
			Bytes buf;
			if (OS::read_file(time_offset_path, buf) == 8) {
				uint64_t offset = *(uint64_t*)buf.data();
				DEBUG("Read time offset of " + std::to_string(offset) + " from file");
				OS::setTimeOffset(offset);
			}
		}
	}
	catch (std::exception& e) {
		ERRORF("Failed to load time offset, the contained exception was: %s", e.what());
	}
#endif
#endif

	// Initialize time-based variables *after* time offset update
	_object->_last_data_persist = OS::time();
	_object->_last_cache_clean = 0.0;
	_object->_jobs_last_run = OS::time();

/*p TODO
	if not os.path.isdir(Reticulum.storagepath):
		os.makedirs(Reticulum.storagepath)

	if not os.path.isdir(Reticulum.cachepath):
		os.makedirs(Reticulum.cachepath)

	if not os.path.isdir(Reticulum.resourcepath):
		os.makedirs(Reticulum.resourcepath)

	if not os.path.isdir(Reticulum.identitypath):
		os.makedirs(Reticulum.identitypath)

	if os.path.isfile(self.configpath):
		try:
			self.config = ConfigObj(self.configpath)
		except Exception as e:
			RNS.log("Could not parse the configuration at "+self.configpath, RNS.LOG_ERROR)
			RNS.log("Check your configuration file for errors!", RNS.LOG_ERROR)
			RNS.panic()
	else:
		RNS.log("Could not load config file, creating default configuration file...")
		self.__create_default_config()
		RNS.log("Default config file created. Make any necessary changes in "+Reticulum.configdir+"/config and restart Reticulum if needed.")
		time.sleep(1.5)

	self.__apply_config()
	RNS.log("Configuration loaded from "+self.configpath, RNS.LOG_VERBOSE)

	RNS.Identity.load_known_destinations()
*/

	// CBA Moved to start() so Transport is not started  until after interfaces are setup
	//Transport::start(*this);

/*p TODO
	self.rpc_addr = ("127.0.0.1", self.local_control_port)
	self.rpc_key  = RNS.Identity.full_hash(Transport::identity.get_private_key())

	if self.is_shared_instance:
		self.rpc_listener = multiprocessing.connection.Listener(self.rpc_addr, authkey=self.rpc_key)
		thread = threading.Thread(target=self.rpc_loop)
		thread.daemon = True
		thread.start()

	atexit.register(Reticulum.exit_handler)
	signal.signal(signal.SIGINT, Reticulum.sigint_handler)
	signal.signal(signal.SIGTERM, Reticulum.sigterm_handler)
*/

	MEM("Reticulum default object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
}

void Reticulum::start() {

	INFO("Total memory: " + std::to_string(OS::heap_size()));
	INFO("Total flash: " + std::to_string(OS::storage_size()));

	INFO("Starting Transport...");
	Transport::start(*this);
}

void Reticulum::loop() {
	assert(_object);
	if (!_object->_is_connected_to_shared_instance) {

		// Perform Reticulum housekeeping
		if (OS::time() > (_object->_jobs_last_run + JOB_INTERVAL)) {
			jobs();
			_object->_jobs_last_run = OS::time();
		}

		// Perform interface processing
		for (auto& [hash, interface] : Transport::get_interfaces()) {
			interface.loop();
		}

		// Perform Filesystem processing
		FileSystem& filesystem = OS::get_filesystem();
		if (filesystem) {
			filesystem.loop();
		}


		// Perform Transport processing
		RNS::Transport::loop();
	}
	// Perform random number gnerator housekeeping
	RNG.loop();
}

void Reticulum::jobs() {

	double now = OS::time();

#if 1
	// CBA Detect low-memory condition and reset
	if (OS::heap_size() > 0) {
		uint8_t remaining = (uint8_t)((double)OS::heap_available() / (double)OS::heap_size() * 100.0);
		if (remaining <= 2) {
			head("DETECTED LOW-MEMORY CONDITION (" + std::to_string(remaining) + "%), RESETTING!!!", LOG_CRITICAL);
			persist_data();
#if defined(ESP32)
			ESP.restart();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
			//dbgDumpMemory();
			NVIC_SystemReset();
#endif
		}
	}
#endif

	if (now > _object->_last_cache_clean + CLEAN_INTERVAL) {
		clean_caches();
		_object->_last_cache_clean = OS::time();
	}

	if (now > _object->_last_data_persist + PERSIST_INTERVAL) {
		persist_data();
	}
}

// CBA TODO
/*
void Reticulum::start_local_interface() {
}

void Reticulum::apply_config() {
}

void Reticulum::_add_interface(self,interface, mode = None, configured_bitrate=None, ifac_size=None, ifac_netname=None, ifac_netkey=None, announce_cap=None, announce_rate_target=None, announce_rate_grace=None, announce_rate_penalty=None):
*/

void Reticulum::should_persist_data() {
	if (OS::time() > _object->_last_data_persist + GRACIOUS_PERSIST_INTERVAL) {
		persist_data();
	}
}

void Reticulum::persist_data() {
	TRACE("Persisting transport and identity data...");
	Transport::persist_data();
	Identity::persist_data();

#ifdef ARDUINO
#if defined(RNS_USE_FS)
	// write time offset to file
	try {
		char time_offset_path[FILEPATH_MAXSIZE];
		snprintf(time_offset_path, FILEPATH_MAXSIZE, "%s/time_offset", _storagepath);
		uint64_t offset = OS::ltime();
		DEBUGF("Writing time offset of %llu to file %s", offset, time_offset_path);
		Bytes buf((uint8_t*)&offset, sizeof(offset));
		OS::write_file(time_offset_path, buf);
	}
	catch (std::exception& e) {
		ERRORF("Failed to write time offset, the contained exception was: %s", e.what());
	}
#endif
#endif

	_object->_last_data_persist = OS::time();
}

void Reticulum::clean_caches() {
	TRACE("Cleaning resource and packet caches...");
	double now = OS::time();

#if defined(RNS_USE_FS) && defined(RNS_PERSIST_PATHS)
/*
	// Clean resource caches
	for (auto& filename : OS::list_directory(resourcepath) {
		try {
			if (filename.length() == (Type::Identity::HASHLENGTH//8)*2) {
				char filepath[FILEPATH_MAXSIZE];
				snprintf(filepath, FILEPATH_MAXSIZE, "%s/%s", _resourcepath, filename.c_str());
				//p mtime = os.path.getmtime(filepath)
				//p age = now - mtime
				//p if (age > Types::Reticulum.RESOURCE_CACHE) {
				//p 	OS::remove_file(filepath);
				//p }
			}
		}
		catch (std::exception& e) {
			ERROR("Error while cleaning resources cache, the contained exception was: %s", e.what());
		}
	}

	// Clean packet caches
	for (auto& filename : OS::list_directory(_cachepath.c_str())) {
		try {
			if (filename.length() == (Type::Identity::HASHLENGTH/8)*2) {
				char filepath[FILEPATH_MAXSIZE];
				snprintf(filepath, FILEPATH_MAXSIZE, "%s/%s", _cachepath, filename.c_str());
				//p mtime = os.path.getmtime(filepath)
				//p age = now - mtime
				//p if (age > Types::Transport::DESTINATION_TIMEOUT) {
				//p 	OS::remove_file(filepath);
				//p }
			}
		}
		catch (std::exception& e) {
			ERROR("Error while cleaning packet cache, the contained exception was: %s", e.what());
		}
	}
*/

	Transport::clean_caches();

	// CBA
	Identity::cull_known_destinations();
#endif

}

void Reticulum::clear_caches() {
	TRACE("Clearing resource and packet caches...");

	try {
		char destination_table_path[FILEPATH_MAXSIZE];
		snprintf(destination_table_path, FILEPATH_MAXSIZE, "%s/destination_table", _storagepath);
		OS::remove_file(destination_table_path);

		OS::remove_directory(_cachepath);

#ifdef ARDUINO
		char time_offset_path[FILEPATH_MAXSIZE];
		snprintf(time_offset_path, FILEPATH_MAXSIZE, "%s/time_offset", _storagepath);
		OS::remove_file(time_offset_path);
#endif
	}
	catch (std::exception& e) {
		ERRORF("Failed to clear cache file(s), the contained exception was: %s", e.what());
	}
}

/*p TODO

void Reticulum::__create_default_config() {
	self.config = ConfigObj(__default_rns_config__)
	self.config.filename = Reticulum.configpath
	
	if not os.path.isdir(Reticulum.configdir):
		os.makedirs(Reticulum.configdir)
	self.config.write()
}

void Reticulum::rpc_loop() {
}

void Reticulum::get_interface_stats() const {
}
*/

const std::map<Bytes, Transport::DestinationEntry>& Reticulum::get_path_table() const {
/*
	path_table = []
	for dst_hash in Transport::destination_table:
		entry = {
			"hash": dst_hash,
			"timestamp": Transport::destination_table[dst_hash][0],
			"via": Transport::destination_table[dst_hash][1],
			"hops": Transport::destination_table[dst_hash][2],
			"expires": Transport::destination_table[dst_hash][3],
			"interface": str(Transport::destination_table[dst_hash][5]),
		}
		path_table.append(entry)

	return path_table
*/
	return Transport::get_destination_table();
}

const std::map<Bytes, Transport::RateEntry>& Reticulum::get_rate_table() const {
/*
	rate_table = []
	for dst_hash in Transport::announce_rate_table:
		entry = {
			"hash": dst_hash,
			"last": Transport::announce_rate_table[dst_hash]["last"],
			"rate_violations": Transport::announce_rate_table[dst_hash]["rate_violations"],
			"blocked_until": Transport::announce_rate_table[dst_hash]["blocked_until"],
			"timestamps": Transport::announce_rate_table[dst_hash]["timestamps"],
		}
		rate_table.append(entry)

	return rate_table
*/
	return Transport::get_announce_rate_table();
}

bool Reticulum::drop_path(const Bytes& destination) {
	return Transport::expire_path(destination);
}

uint16_t Reticulum::drop_all_via(const Bytes& transport_hash) {
	uint16_t dropped_count = 0;
	//for (auto& destination_hash : Transport::get_destination_table()) {
	for (const auto& [destination_hash, destination_entry] : Transport::get_destination_table()) {
		if (destination_entry._received_from == transport_hash) {
			Transport::expire_path(destination_hash);
			++dropped_count;
		}
	}
	return dropped_count;
}

void Reticulum::drop_announce_queues() {
	Transport::drop_announce_queues();
}

const std::string Reticulum::get_next_hop_if_name(const Bytes& destination) const {
	return Transport::next_hop_interface(destination).name();
}

double Reticulum::get_first_hop_timeout(const Bytes& destination) const {
	return Transport::first_hop_timeout(destination);
}

const Bytes Reticulum::get_next_hop(const Bytes& destination) const {
	return Transport::next_hop(destination);
}

size_t Reticulum::get_link_count() const {
	return Transport::get_link_table().size();
}

/*p
void Reticulum::get_packet_rssi(const Bytes& packet_hash) const {
	for entry in Transport::local_client_rssi_cache:
		if entry[0] == packet_hash:
			return entry[1]

	return None

void Reticulum::get_packet_snr(const Bytes& packet_hash) const {
	for entry in Transport::local_client_snr_cache:
		if entry[0] == packet_hash:
			return entry[1]

	return None

void Reticulum::get_packet_q(const Bytes& packet_hash) const {
	for entry in Transport::local_client_q_cache:
		if entry[0] == packet_hash:
			return entry[1]

	return None
*/
