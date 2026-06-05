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

#include "Registry.h"

#include <string.h>

namespace RNS { namespace Provisioning {

	Namespace* Registry::add_namespace(nid_t id, const char* name, nid_t parent_id) {
		if (_id_index.count(id) != 0) return nullptr;
		// Names must be unique among root namespaces (parent_id == 0) to
		// prevent filename collisions in Storage. Linear scan with strcmp,
		// O(N) over the small set of root namespaces.
		if (name && name[0] && parent_id == 0) {
			for (const auto& up : _namespaces) {
				if (up->parent_id() == 0 && up->name() && strcmp(up->name(), name) == 0) {
					return nullptr;
				}
			}
		}

		// Cycle check: walk up parent chain — if we hit `id`, refuse.
		if (parent_id != 0) {
			nid_t hop = parent_id;
			while (hop != 0) {
				if (hop == id) return nullptr;	// would form a cycle
				auto it = _id_index.find(hop);
				if (it == _id_index.end()) return nullptr;	// parent doesn't exist
				hop = _namespaces[it->second]->parent_id();
			}
		}

		size_t idx = _namespaces.size();
		_namespaces.push_back(std::unique_ptr<Namespace>(new Namespace(id, name, parent_id)));
		_id_index[id] = idx;
		return _namespaces.back().get();
	}

	std::vector<const Namespace*> Registry::children_of(nid_t parent_id) const {
		std::vector<const Namespace*> out;
		for (const auto& up : _namespaces) {
			if (up->parent_id() == parent_id) out.push_back(up.get());
		}
		return out;
	}

	Namespace* Registry::find(nid_t id) {
		auto it = _id_index.find(id);
		if (it == _id_index.end()) return nullptr;
		return _namespaces[it->second].get();
	}

	const Namespace* Registry::find(nid_t id) const {
		auto it = _id_index.find(id);
		if (it == _id_index.end()) return nullptr;
		return _namespaces[it->second].get();
	}

#ifndef RNS_PROVISIONING_NO_BY_NAME
	Namespace* Registry::find(const char* name) {
		if (!name) return nullptr;
		for (const auto& up : _namespaces) {
			if (up->name() && strcmp(up->name(), name) == 0) return up.get();
		}
		return nullptr;
	}

	const Namespace* Registry::find(const char* name) const {
		if (!name) return nullptr;
		for (const auto& up : _namespaces) {
			if (up->name() && strcmp(up->name(), name) == 0) return up.get();
		}
		return nullptr;
	}
#endif

	void Registry::clear() {
		_namespaces.clear();
		_id_index.clear();
	}

} }
