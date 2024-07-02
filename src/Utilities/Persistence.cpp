#include "Persistence.h"

#include "Bytes.h"

using namespace RNS;

/*static*/ DynamicJsonDocument _document(Type::Persistence::DOCUMENT_MAXSIZE);
/*static*/ Bytes _buffer(Type::Persistence::BUFFER_MAXSIZE);
