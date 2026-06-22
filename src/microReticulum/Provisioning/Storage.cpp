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

namespace RNS { namespace Provisioning {

	// Persistence file names are derived from the namespace id, not the
	// human-readable name, so length stays bounded ("ns65535.msgpack.tmp" =
	// 19 chars) and stays valid on the strictest embedded filesystems we
	// target. Namespace ids are already guaranteed unique by the registry,
	// so flattening the hierarchy in the filename is safe — parent
	// relationships are still recorded in the serialized payload.
	//
	// Note: this is a storage-format break vs the prior name-based scheme.
	// Existing on-disk files written under names like "RNode General Config
	// .msgpack" will not be loaded by the id-based code path; devices with
	// stale config files should erase storage or accept reverting to
	// defaults.
	fstring_t Storage::file_path(const Namespace& ns) const {
		return _root + "/ns" + std::to_string(ns.id()) + ".msgpack";
	}

	fstring_t Storage::tmp_path(const Namespace& ns) const {
		return _root + "/ns" + std::to_string(ns.id()) + ".msgpack.tmp";
	}

	bool Storage::ensure_directory() {
		try {
			if (Utilities::OS::directory_exists(_root.c_str())) return true;
			return Utilities::OS::create_directory(_root.c_str());
		}
		catch (const std::exception& e) {
			ERRORF("Storage::ensure_directory: %s", e.what());
			return false;
		}
	}

	bool Storage::load_namespace(Namespace& ns) {
		const fstring_t tmp = tmp_path(ns);
		const fstring_t final = file_path(ns);

		// Discard any stale .tmp from an interrupted save.
		try {
			if (Utilities::OS::file_exists(tmp.c_str())) {
				Utilities::OS::remove_file(tmp.c_str());
			}
		}
		catch (...) { /* best-effort */ }

		try {
			if (!Utilities::OS::file_exists(final.c_str())) return false;
		}
		catch (...) { return false; }

		Bytes raw;
		try {
			size_t n = Utilities::OS::read_file(final.c_str(), raw);
			if (n == 0) return false;
		}
		catch (const std::exception& e) {
			ERRORF("Storage::load_namespace: read failed: %s", e.what());
			return false;
		}

		MsgPack::Unpacker unpacker;
		unpacker.feed(raw.data(), raw.size());
		if (!Codec::unpack_namespace_working(unpacker, ns)) {
			WARNINGF("Storage::load_namespace: malformed file for namespace %s", ns.name().c_str());
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

		const fstring_t tmp = tmp_path(ns);
		const fstring_t final = file_path(ns);

		try {
			size_t wrote = Utilities::OS::write_file(tmp.c_str(), encoded);
			if (wrote != encoded.size()) {
				ERRORF("Storage::save: short write for %s (%zu/%zu)",
					ns.name().c_str(), wrote, encoded.size());
				Utilities::OS::remove_file(tmp.c_str());
				return false;
			}
			// Rename overwrites on POSIX; on Arduino flash backends the
			// adapter handles delete-then-rename internally.
			if (!Utilities::OS::rename_file(tmp.c_str(), final.c_str())) {
				// Fallback: remove existing final and retry rename, since
				// some Arduino FS adapters won't overwrite atomically.
				try { Utilities::OS::remove_file(final.c_str()); } catch (...) {}
				if (!Utilities::OS::rename_file(tmp.c_str(), final.c_str())) {
					ERRORF("Storage::save: rename failed for %s", ns.name().c_str());
					Utilities::OS::remove_file(tmp.c_str());
					return false;
				}
			}
		}
		catch (const std::exception& e) {
			ERRORF("Storage::save: %s", e.what());
			return false;
		}

		ns.mark_clean();
		return true;
	}

	bool Storage::remove_namespace(const Namespace& ns) {
		const fstring_t tmp = tmp_path(ns);
		const fstring_t final = file_path(ns);
		bool ok = true;
		try {
			if (Utilities::OS::file_exists(tmp.c_str())) {
				ok = Utilities::OS::remove_file(tmp.c_str()) && ok;
			}
			if (Utilities::OS::file_exists(final.c_str())) {
				ok = Utilities::OS::remove_file(final.c_str()) && ok;
			}
		}
		catch (const std::exception& e) {
			ERRORF("Storage::remove_namespace: %s", e.what());
			return false;
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
