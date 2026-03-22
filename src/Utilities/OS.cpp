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

#include "OS.h"

#include "../Type.h"
#include "../Log.h"

#include <new>

using namespace RNS;
using namespace RNS::Utilities;

/*static*/ microStore::FileSystem OS::_filesystem;
/*static*/ uint64_t OS::_time_offset = 0;
/*static*/ OS::LoopCallback OS::_on_loop = nullptr;
