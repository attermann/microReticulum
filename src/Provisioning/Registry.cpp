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

namespace RNS { namespace Provisioning {

	Namespace* Registry::add_namespace(uint16_t id, const char* name) {
		if (_id_index.count(id) != 0) return nullptr;
		std::string sname = name ? name : "";
		if (!sname.empty() && _name_index.count(sname) != 0) return nullptr;
		size_t idx = _namespaces.size();
		_namespaces.push_back(std::unique_ptr<Namespace>(new Namespace(id, name)));
		_id_index[id] = idx;
		if (!sname.empty()) _name_index[sname] = idx;
		return _namespaces.back().get();
	}

	Namespace* Registry::find(uint16_t id) {
		auto it = _id_index.find(id);
		if (it == _id_index.end()) return nullptr;
		return _namespaces[it->second].get();
	}

	Namespace* Registry::find(const char* name) {
		if (!name) return nullptr;
		auto it = _name_index.find(name);
		if (it == _name_index.end()) return nullptr;
		return _namespaces[it->second].get();
	}

	const Namespace* Registry::find(uint16_t id) const {
		auto it = _id_index.find(id);
		if (it == _id_index.end()) return nullptr;
		return _namespaces[it->second].get();
	}

	const Namespace* Registry::find(const char* name) const {
		if (!name) return nullptr;
		auto it = _name_index.find(name);
		if (it == _name_index.end()) return nullptr;
		return _namespaces[it->second].get();
	}

	void Registry::clear() {
		_namespaces.clear();
		_id_index.clear();
		_name_index.clear();
	}

} }
