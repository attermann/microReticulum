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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RNS { namespace Provisioning {

	class Registry {

	public:
		Registry() = default;

		// Create a new namespace. Returns nullptr if id or name is taken.
		Namespace* add_namespace(uint16_t id, const char* name);

		Namespace* find(uint16_t id);
		Namespace* find(const char* name);
		const Namespace* find(uint16_t id) const;
		const Namespace* find(const char* name) const;

		const std::vector<std::unique_ptr<Namespace>>& namespaces() const { return _namespaces; }

		void clear();

	private:
		std::vector<std::unique_ptr<Namespace>> _namespaces;
		std::unordered_map<uint16_t, size_t> _id_index;
		std::unordered_map<std::string, size_t> _name_index;
	};

} }
