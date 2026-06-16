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

#include "Registry.h"
#include "../Reticulum.h"

#include <string>

namespace RNS { namespace Provisioning {

	// Per-namespace MsgPack files written atomically via <name>.msgpack.tmp
	// followed by rename to <name>.msgpack. On load, stale .tmp files are
	// removed so a power-loss mid-save leaves the previous file intact.
	class Storage {

	public:
		// registry pointer is used solely to walk parent chains when
		// building dotted-path filenames for hierarchical namespaces.
		// May be nullptr — in that case filenames are flat (just ns.name()).
		Storage(const char* root = nullptr, const Registry* registry = nullptr)
			: _root(root ? root : "./config"), _registry(registry) {}

		const fstring_t& root() const { return _root; }

		// Ensure the storage directory exists. Returns true on success
		// (already-exists counts as success).
		bool ensure_directory();

		// Load every namespace's saved file (if present) into its working map.
		// Returns the count of files successfully loaded.
		size_t load_all(Registry& registry);

		// Save the namespace if its working state has been touched since
		// the last clean mark. No-op (returns true) if not dirty.
		bool save_namespace(Namespace& ns);

		// Force-save regardless of dirty bit.
		bool save_namespace_force(Namespace& ns);

		// Delete the namespace's file (and any stray .tmp).
		bool remove_namespace(const Namespace& ns);

		// Remove every namespace file under the configured root. Used by
		// FACTORY_RESET. Returns true if all known files were removed.
		bool factory_reset(const Registry& registry);

	private:
		fstring_t _root;
		const Registry* _registry;	// optional; used for dotted-path filenames

		fstring_t file_path(const Namespace& ns) const;
		fstring_t tmp_path(const Namespace& ns) const;

		// Builds "Parent.Child.GrandChild" by walking ns.parent_id() up the
		// Registry. Falls back to ns.name() alone if _registry is null or
		// a parent link is broken.
		fstring_t dotted_name(const Namespace& ns) const;

		bool load_namespace(Namespace& ns);
	};

} }
