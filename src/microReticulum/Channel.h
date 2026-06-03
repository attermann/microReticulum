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

#pragma once

#include "Destination.h"
#include "Type.h"

#include <memory>
#include <cassert>

namespace RNS {

	class Channel {

	public:
		Channel(Type::NoneConstructor none) {
			MEM("Channel NONE object created");
		}
		Channel(const Channel& resource) : _object(resource._object) {
			MEM("Channel object copy created");
		}
		virtual ~Channel(){
			MEM("Channel object destroyed");
		}

		Channel& operator = (const Channel& resource) {
			_object = resource._object;
			return *this;
		}
		operator bool() const {
			return _object.get() != nullptr;
		}
		bool operator < (const Channel& resource) const {
			return _object.get() < resource._object.get();
			//return _object->_hash < resource._object->_hash;
		}

	public:
		void _shutdown() {}

	private:
		class Object {
		public:
			Object() { MEMF("Channel::Data object created, this: %p", (void*)this); }
			virtual ~Object() { MEMF("Channel::Data object destroyed, this: %p", (void*)this); }
		private:

		friend class Channel;
		};
		std::shared_ptr<Object> _object;

	};


	class ChannelAdvertisement {

	};

}
