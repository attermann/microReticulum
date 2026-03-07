#include "OS.h"

#include "../Type.h"
#include "../Log.h"

#include <new>

using namespace RNS;
using namespace RNS::Utilities;

/*static*/ FileSystem OS::_filesystem = {Type::NONE};
/*static*/ uint64_t OS::_time_offset = 0;
/*static*/ OS::LoopCallback OS::_on_loop = nullptr;
