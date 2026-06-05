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

#include "Storage.h"
#include "Codec.h"

#include "../Bytes.h"
#include "../Log.h"
#include "../Utilities/OS.h"

#include <stdio.h>
#include <string.h>

namespace RNS { namespace Provisioning {

	void Storage::build_path(const Namespace& ns, const char* suffix,
		char* out, size_t out_size) const {
		// Walk the parent chain once to collect ancestor names (root-first).
		// Cap at 8 deep — far beyond any plausible namespace tree, but small
		// enough that the local array costs nothing.
		const char* parts[8];
		size_t depth = 0;
		parts[depth++] = ns.name();
		if (_registry) {
			nid_t hop = ns.parent_id();
			while (hop != 0 && depth < 8) {
				const Namespace* p = _registry->find(hop);
				if (!p) break;
				parts[depth++] = p->name();
				hop = p->parent_id();
			}
		}
		// Compose "<root>/<root_ns>.<child>.<...>.<leaf><suffix>" in one pass.
		size_t pos = 0;
		auto append = [&](const char* s) {
			while (*s && pos + 1 < out_size) out[pos++] = *s++;
		};
		append(_root.c_str());
		if (pos + 1 < out_size) out[pos++] = '/';
		for (size_t i = depth; i > 0; --i) {
			append(parts[i - 1]);
			if (i > 1 && pos + 1 < out_size) out[pos++] = '.';
		}
		append(suffix);
		out[pos < out_size ? pos : out_size - 1] = '\0';
	}

	bool Storage::ensure_directory() {
		if (Utilities::OS::directory_exists(_root.c_str())) return true;
		return Utilities::OS::create_directory(_root.c_str());
	}

	bool Storage::load_namespace(Namespace& ns) {
		char tmp[MAX_PATH_LEN], final[MAX_PATH_LEN];
		build_path(ns, ".msgpack.tmp", tmp, sizeof(tmp));
		build_path(ns, ".msgpack",     final, sizeof(final));

		// Discard any stale .tmp from an interrupted save. microStore is
		// nothrow; failures here are best-effort and we proceed regardless.
		if (Utilities::OS::file_exists(tmp)) {
			Utilities::OS::remove_file(tmp);
		}

		if (!Utilities::OS::file_exists(final)) return false;

		Bytes raw;
		size_t n = Utilities::OS::read_file(final, raw);
		if (n == 0) return false;

		MsgPack::Unpacker unpacker;
		unpacker.feed(raw.data(), raw.size());
		if (!Codec::unpack_namespace_working(unpacker, ns)) {
			WARNINGF("Storage::load_namespace: malformed file for namespace %s", ns.name());
			return false;
		}
		ns.mark_clean();
		return true;
	}

	size_t Storage::load_all(Registry& registry) {
		if (!ensure_directory()) {
			WARNINGF("Storage::load_all: directory %s unavailable", _root.c_str());
			return 0;
		}
		size_t loaded = 0;
		for (const auto& ns_ptr : registry.namespaces()) {
			if (load_namespace(*ns_ptr)) ++loaded;
		}
		return loaded;
	}

	bool Storage::save_namespace(Namespace& ns) {
		if (!ns.dirty_for_persist()) return true;
		return save_namespace_force(ns);
	}

	bool Storage::save_namespace_force(Namespace& ns) {
		if (!ensure_directory()) return false;

		MsgPack::Packer packer;
		if (!Codec::pack_namespace_working(packer, ns)) return false;
		Bytes encoded(packer.data(), packer.size());

		char tmp[MAX_PATH_LEN], final[MAX_PATH_LEN];
		build_path(ns, ".msgpack.tmp", tmp, sizeof(tmp));
		build_path(ns, ".msgpack",     final, sizeof(final));

		size_t wrote = Utilities::OS::write_file(tmp, encoded);
		if (wrote != encoded.size()) {
			ERRORF("Storage::save: short write for %s (%zu/%zu)",
				ns.name(), wrote, encoded.size());
			Utilities::OS::remove_file(tmp);
			return false;
		}
		// Rename overwrites on POSIX; on Arduino flash backends the adapter
		// handles delete-then-rename internally.
		if (!Utilities::OS::rename_file(tmp, final)) {
			// Fallback: remove existing final and retry rename, since some
			// Arduino FS adapters won't overwrite atomically.
			Utilities::OS::remove_file(final);
			if (!Utilities::OS::rename_file(tmp, final)) {
				ERRORF("Storage::save: rename failed for %s", ns.name());
				Utilities::OS::remove_file(tmp);
				return false;
			}
		}

		ns.mark_clean();
		return true;
	}

	bool Storage::remove_namespace(const Namespace& ns) {
		char tmp[MAX_PATH_LEN], final[MAX_PATH_LEN];
		build_path(ns, ".msgpack.tmp", tmp, sizeof(tmp));
		build_path(ns, ".msgpack",     final, sizeof(final));
		bool ok = true;
		if (Utilities::OS::file_exists(tmp)) {
			ok = Utilities::OS::remove_file(tmp) && ok;
		}
		if (Utilities::OS::file_exists(final)) {
			ok = Utilities::OS::remove_file(final) && ok;
		}
		return ok;
	}

	bool Storage::factory_reset(const Registry& registry) {
		bool ok = true;
		for (const auto& ns_ptr : registry.namespaces()) {
			ok = remove_namespace(*ns_ptr) && ok;
		}
		return ok;
	}

} }
