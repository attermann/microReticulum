#include "Reticulum.h"

#include "Transport.h"
#include "Log.h"
#include "Utilities/OS.h"

#include <RNG.h>

#ifdef ARDUINO
//#include <TransistorNoiseSource.h>
//#include <Ethernet.h>
#endif

using namespace RNS;
using namespace RNS::Type::Reticulum;

/*static*/ std::string Reticulum::storagepath;

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
	mem("Reticulum default object creating..., this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));

	// Initialkize random number generator
	RNG.begin("Reticulum");

	// CBA TEST Transport
	__transport_enabled = true;

	Utilities::OS::setup();

#ifdef ARDUINO
	// Stir in the Ethernet MAC address.
	//byte mac[6];
	//Ethernet.begin(mac);
	// WiFi.macAddress(mac);
	//RNG.stir(mac, sizeof(mac));
 
	// Add the noise source to the list of sources known to RNG.
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
// CBA MOCK
#ifdef ARDUINO
	storagepath = "";
#else
	storagepath = ".";
#endif

/* TODO
	Reticulum.__transport_enabled = False
	Reticulum.__use_implicit_proof = True
	Reticulum.__allow_probes = False

	Reticulum.panic_on_interface_error = False

	self.local_interface_port = 37428
	self.local_control_port   = 37429
	self.share_instance       = True
	self.rpc_listener         = None

	self.ifac_salt = Reticulum.IFAC_SALT

	self.requested_loglevel = loglevel
	self.requested_verbosity = verbosity
	if self.requested_loglevel != None:
		if self.requested_loglevel > RNS.LOG_EXTREME:
			self.requested_loglevel = RNS.LOG_EXTREME
		if self.requested_loglevel < RNS.LOG_CRITICAL:
			self.requested_loglevel = RNS.LOG_CRITICAL

		RNS.loglevel = self.requested_loglevel

	self.is_shared_instance = False
	self.is_connected_to_shared_instance = False
	self.is_standalone_instance = False
	self.jobs_thread = None
	self.last_data_persist = time.time()
	self.last_cache_clean = 0

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

	Transport::start(*this);

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

	mem("Reticulum default object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
}


void Reticulum::loop() {
	assert(_object);
	if (!_object->_is_connected_to_shared_instance) {
		RNS::Transport::loop();
	}
	// Perform random number gnerator housekeeping
	RNG.loop();
}
