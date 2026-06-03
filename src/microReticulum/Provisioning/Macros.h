/*
 * Copyright (c) 2026 Chad Attermann
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

#pragma once

#include "Provisioning.h"

// Convenience wrappers for declaring a namespace + its fields in a single
// terse block. These produce nothing at file scope — they're meant to be
// used inside a function body (typically the project-wide built-in
// registration function, or an app's own registration entry point).
//
//   void register_my_app_namespaces() {
//       RNS_PROVISION_BEGIN(p, "radio", 100)
//           RNS_FIELD_FLOAT(p, "frequency", 1, FF_REBOOT_REQUIRED, 915.0e6, 100e6, 1e9, my_setter)
//           RNS_FIELD_INT  (p, "sf",        4, FF_REBOOT_REQUIRED, 8,       7,    12,  my_setter)
//       RNS_PROVISION_END(p);
//   }
//
// Apps are equally free to call the fluent NamespaceBuilder API directly;
// these macros only exist for visual symmetry with the per-namespace id
// headers under src/Provisioning/Ids/.

#define RNS_PROVISION_BEGIN(builder_name, ns_name, ns_id) \
	{ auto builder_name = RNS::Provisioning::Manager::instance() \
		.register_namespace((ns_name), (ns_id));

#define RNS_PROVISION_END(builder_name) \
	  builder_name.end(); }

#define RNS_FIELD_BOOL(builder, name, id, flags, default_value, ...) \
	(builder).field_bool((name), (id), (flags), (default_value), ##__VA_ARGS__);

#define RNS_FIELD_INT(builder, name, id, flags, default_value, min_val, max_val, ...) \
	(builder).field_int((name), (id), (flags), (default_value), (min_val), (max_val), ##__VA_ARGS__);

#define RNS_FIELD_FLOAT(builder, name, id, flags, default_value, min_val, max_val, ...) \
	(builder).field_float((name), (id), (flags), (default_value), (min_val), (max_val), ##__VA_ARGS__);

#define RNS_FIELD_STRING(builder, name, id, flags, default_value, max_len, ...) \
	(builder).field_string((name), (id), (flags), (default_value), (max_len), ##__VA_ARGS__);

#define RNS_FIELD_BYTES(builder, name, id, flags, default_value, max_len, ...) \
	(builder).field_bytes((name), (id), (flags), (default_value), (max_len), ##__VA_ARGS__);
