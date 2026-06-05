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

#include "Namespace.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace RNS { namespace Provisioning {

	class Registry {

	public:
		Registry() = default;

		// Create a new namespace. Returns nullptr if id or name is taken,
		// or if parent_id is non-zero and would create a cycle. parent_id
		// of 0 means root (no parent).
		Namespace* add_namespace(nid_t id, const char* name, nid_t parent_id = 0);

		Namespace* find(nid_t id);
		const Namespace* find(nid_t id) const;
#ifndef RNS_PROVISIONING_NO_BY_NAME
		Namespace* find(const char* name);
		const Namespace* find(const char* name) const;
#endif

		const std::vector<std::unique_ptr<Namespace>>& namespaces() const { return _namespaces; }

		// Direct children of a parent_id. parent_id == 0 returns roots.
		// O(N) over the Registry — the count is small in practice.
		std::vector<const Namespace*> children_of(nid_t parent_id) const;

		void clear();

	private:
		std::vector<std::unique_ptr<Namespace>> _namespaces;
		std::map<nid_t, size_t> _id_index;
		// No _name_index: find(const char*) linear-scans _namespaces. Saves
		// a std::map<std::string,_> instantiation worth of code.

	};

} }
