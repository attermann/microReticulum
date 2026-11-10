// Synthetic active link: no physical interfaces or radio transmissions.
#include <sstream>
#include <functional>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <stdexcept>
#include <iostream>
#define private public
#define protected public
#include "microReticulum.h"
#include "microReticulum/LinkData.h"
#include <microStore/Adapters/UniversalFileSystem.h>
#undef protected
#undef private

static void require(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
class Sink final : public RNS::InterfaceImpl {
public:
    Sink() : InterfaceImpl("request-regression") {
        _IN = true; _OUT = true; _online = true; _bitrate = 1000000;
    }
    bool send_outgoing(const RNS::Bytes&) override { return true; }
};
int main() {
    microStore::FileSystem fs{microStore::Adapters::UniversalFileSystem()};
    fs.init(); RNS::Utilities::OS::register_filesystem(fs);
    RNS::Reticulum rns; rns.start();
    RNS::Interface iface(new Sink);
    RNS::Transport::register_interface(iface);
    RNS::Identity identity;
    RNS::Destination peer(identity, RNS::Type::Destination::OUT,
        RNS::Type::Destination::SINGLE, "regression", "requests");
    RNS::Link link{RNS::Type::NONE};
    link._object = std::make_shared<RNS::LinkData>(peer);
    link._object->_link_id = RNS::Cryptography::random(16);
    link._object->_hash = link._object->_link_id;
    link._object->_derived_key = RNS::Cryptography::Token::generate_key();
    link._object->_status = RNS::Type::Link::ACTIVE;
    link._object->_attached_interface = iface;
    const uint8_t nil = 0xc0;
    auto small = link.request(RNS::Bytes("/small"), RNS::Bytes(&nil, 1),
        nullptr, nullptr, nullptr, 1.25);
    require(bool(small), "small request returns a receipt");
    auto packet_receipt = small._object->_packet_receipt;
    require(packet_receipt._object->_timeout == 1.25, "fractional timeout preserved");
    require(!RNS::Transport::receipts().empty() &&
        RNS::Transport::receipts().back() == packet_receipt,
        "request and transport share the same receipt object");
    small.request_timed_out({RNS::Type::NONE});
    RNS::PacketReceipt large_timeout;
    large_timeout.set_timeout(40000.5);
    require(large_timeout._object->_timeout == 40000.5, "large timeout preserved");
    auto busy = RNS::Resource(RNS::Cryptography::random(1024), link).start();
    auto queued = link.request(RNS::Bytes("/queued"),
        RNS::Cryptography::random(8192), nullptr, nullptr, nullptr, 30);
    require(bool(queued) && queued._object->_resource.status() == RNS::Type::Resource::QUEUED,
        "request waits behind an active resource");
    busy.cancel(); link.tick_resources();
    require(queued._object->_resource.status() == RNS::Type::Resource::ADVERTISED,
        "maintenance resumes a queued request");
    queued._object->_resource.cancel();
    queued.request_timed_out({RNS::Type::NONE});
    RNS::Transport::deregister_interface(iface);
    std::cout << "PASS: shared packet receipt, fractional/large timeout, queued request progress\n";
}
