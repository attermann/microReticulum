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

/*static*/ std::string Reticulum::_storagepath;
/*static*/ std::string Reticulum::_cachepath;

/*static*/ bool Reticulum::__transport_enabled = false;
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

/* TODO
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
	_storagepath = "";
	_cachepath = "/cache";
#else
	_storagepath = ".";
	_cachepath = "./cache";
#endif

#ifdef ARDUINO
	// load time offset from file if it exists
	try {
		std::string time_offset_path = Reticulum::_storagepath + "/time_offset";
		if (OS::file_exists(time_offset_path.c_str())) {
			Bytes buf;
			if (OS::read_file(time_offset_path.c_str(), buf) == 8) {
				uint64_t offset = *(uint64_t*)buf.data();
				DEBUG("Read time offset of " + std::to_string(offset) + " from file");
				OS::setTimeOffset(offset);
			}
		}
	}
	catch (std::exception& e) {
		error("Failed to load time offset, the contained exception was: " + std::string(e.what()));
	}
#endif

	// Initialize time-based variables *after* time offset update
	_object->_last_data_persist = OS::time();
	_object->_last_cache_clean = 0.0;
	_object->_jobs_last_run = OS::time();

/* TODO
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

/*
	self.rpc_addr = ("127.0.0.1", self.local_control_port)
	self.rpc_key  = RNS.Identity.full_hash(RNS.Transport.identity.get_private_key())

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

/*p TODO
    @staticmethod
    def exit_handler():
        # This exit handler is called whenever Reticulum is asked to
        # shut down, and will in turn call exit handlers in other
        # classes, saving necessary information to disk and carrying
        # out cleanup operations.

        RNS.Transport.exit_handler()
        RNS.Identity.exit_handler()

    @staticmethod
    def sigint_handler(signal, frame):
        RNS.Transport.detach_interfaces()
        RNS.exit()


    @staticmethod
    def sigterm_handler(signal, frame):
        RNS.Transport.detach_interfaces()
        RNS.exit()


    @staticmethod
    def get_instance():
        """
        Return the currently running Reticulum instance
        """
        return Reticulum.__instance
*/

void Reticulum::start() {
	info("Starting Transport...");
	Transport::start(*this);
}

void Reticulum::loop() {
	assert(_object);
	if (!_object->_is_connected_to_shared_instance) {

		if (OS::time() > (_object->_jobs_last_run + JOB_INTERVAL)) {
			jobs();
			_object->_jobs_last_run = OS::time();
		}

		RNS::Transport::loop();
	}
	// Perform random number gnerator housekeeping
	RNG.loop();
}

void Reticulum::jobs() {

	double now = OS::time();

	if (now > _object->_last_cache_clean + CLEAN_INTERVAL) {
		clean_caches();
		_object->_last_cache_clean = OS::time();
	}

	if (now > _object->_last_data_persist + PERSIST_INTERVAL) {
		persist_data();
	}
}

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
	// write time offset to file
	try {
		std::string time_offset_path = Reticulum::_storagepath + "/time_offset";
		uint64_t offset = OS::ltime();
		DEBUG("Writing time offset of " + std::to_string(offset) + " to file");
		Bytes buf((uint8_t*)&offset, sizeof(offset));
		OS::write_file(time_offset_path.c_str(), buf);
	}
	catch (std::exception& e) {
		error("Failed to write time offset, the contained exception was: " + std::string(e.what()));
	}
#endif

	_object->_last_data_persist = OS::time();
}

void Reticulum::clean_caches() {
	TRACE("Cleaning resource and packet caches...");
	double now = OS::time();

/*
	// Clean resource caches
	for (auto& filename : OS::list_directory(resourcepath) {
		try {
			if (filename.length() == (Type::Identity::HASHLENGTH//8)*2) {
				std::string filepath = _resourcepath + "/" + filename;
				//p mtime = os.path.getmtime(filepath)
				//p age = now - mtime
				//p if (age > Types::Reticulum.RESOURCE_CACHE) {
				//p 	OS::remove_file(filepath);
				//p }
			}
		}
		catch (std::exception& e) {
			error("Error while cleaning resources cache, the contained exception was: " + std::string(e.what()));
		}
	}

	// Clean packet caches
	for (auto& filename : OS::list_directory(_cachepath.c_str())) {
		try {
			if (filename.length() == (Type::Identity::HASHLENGTH/8)*2) {
				std::string filepath = _cachepath + "/" + filename;
				//p mtime = os.path.getmtime(filepath)
				//p age = now - mtime
				//p if (age > Types::Transport::DESTINATION_TIMEOUT) {
				//p 	OS::remove_file(filepath);
				//p }
			}
		}
		catch (std::exception& e) {
			error("Error while cleaning packet cache, the contained exception was: " + std::string(e.what()));
		}
	}
*/

	Transport::clean_caches();

}

void Reticulum::clear_caches() {
	TRACE("Clearing resource and packet caches...");

	try {
		std::string destination_table_path = _storagepath + "/destination_table";
		OS::remove_file(destination_table_path.c_str());

		std::string packet_cache_path = _cachepath;
		OS::remove_directory(packet_cache_path.c_str());

#ifdef ARDUINO
		std::string time_offset_path = _storagepath + "/time_offset";
		OS::remove_file(time_offset_path.c_str());
#endif
	}
	catch (std::exception& e) {
		error("Failed to clear cache file(s), the contained exception was: " + std::string(e.what()));
	}
}
