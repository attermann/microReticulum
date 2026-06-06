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

#include <set>
#include <memory>
#include <utility>
#include <cstddef>
#include <iterator>
#include <functional>

namespace RNS { namespace Utilities {

	// A FIFO-with-uniqueness container that is a drop-in replacement for std::set<T>.
	//
	// Internally maintains two generations (_active, _previous). After an insertion
	// that brings _active to (max_size + 1) / 2 entries, _active is moved into
	// _previous (discarding the prior _previous) and a fresh _active is started.
	// Peak total size is approximately max_size; effective dedup window is roughly
	// max_size/2 to max_size entries.
	//
	// max_size(0) (the default) disables rotation, yielding unbounded growth.
	// Unlike std::set, insert() may invalidate the where-component of iterators
	// (the inner std::set iterator remains valid due to std::set's stability
	// guarantees across move-assignment, but the generation it lives in changes
	// at the rotation boundary). Callers should not hold iterators across inserts.
	template <typename T,
	          typename Compare = std::less<T>,
	          typename Allocator = std::allocator<T>>
	class GenerationalSet {
	private:
		using InnerSet = std::set<T, Compare, Allocator>;

	public:
		using value_type     = T;
		using key_type       = T;
		using key_compare    = Compare;
		using value_compare  = Compare;
		using allocator_type = Allocator;
		using size_type      = std::size_t;
		using difference_type = std::ptrdiff_t;
		using reference       = const T&;
		using const_reference = const T&;
		using pointer         = const T*;
		using const_pointer   = const T*;

		class const_iterator {
		public:
			using iterator_category = std::forward_iterator_tag;
			using value_type        = T;
			using difference_type   = std::ptrdiff_t;
			using reference         = const T&;
			using pointer           = const T*;

			const_iterator() = default;

			const T& operator*() const  { return *_inner; }
			const T* operator->() const { return &*_inner; }

			const_iterator& operator++() {
				++_inner;
				normalize();
				return *this;
			}
			const_iterator operator++(int) {
				const_iterator tmp = *this;
				++(*this);
				return tmp;
			}

			bool operator==(const const_iterator& other) const {
				if (_where == END_ && other._where == END_) return true;
				return _where == other._where && _inner == other._inner;
			}
			bool operator!=(const const_iterator& other) const { return !(*this == other); }

		private:
			friend class GenerationalSet;
			enum Where { PREVIOUS, ACTIVE, END_ };

			const GenerationalSet* _parent = nullptr;
			Where _where = END_;
			typename InnerSet::const_iterator _inner{};

			const_iterator(const GenerationalSet* parent, Where where,
			               typename InnerSet::const_iterator inner)
				: _parent(parent), _where(where), _inner(inner) {
				normalize();
			}

			// Advance past empty ranges so that operator* never dereferences end().
			void normalize() {
				if (_where == PREVIOUS && _inner == _parent->_previous.end()) {
					_where = ACTIVE;
					_inner = _parent->_active.begin();
				}
				if (_where == ACTIVE && _inner == _parent->_active.end()) {
					_where = END_;
				}
			}
		};
		using iterator = const_iterator;

		GenerationalSet() = default;
		explicit GenerationalSet(const Compare& comp, const Allocator& alloc = Allocator())
			: _active(comp, alloc), _previous(comp, alloc) {}
		explicit GenerationalSet(const Allocator& alloc)
			: _active(alloc), _previous(alloc) {}

		// --- Capacity / rotation control ---
		size_type size() const { return _active.size() + _previous.size(); }
		bool empty() const { return _active.empty() && _previous.empty(); }
		void clear() { _active.clear(); _previous.clear(); }

		GenerationalSet& max_size(size_type n) {
			_max_size = n;
			return *this;
		}
		size_type max_size() const { return _max_size; }

		// --- Modifiers ---
		std::pair<iterator, bool> insert(const T& value) {
			auto pit = _previous.find(value);
			if (pit != _previous.end()) {
				return std::make_pair(make_iter(const_iterator::PREVIOUS, pit), false);
			}
			auto result = _active.insert(value);
			if (!result.second) {
				return std::make_pair(make_iter(const_iterator::ACTIVE, result.first), false);
			}
			if (rotation_due()) {
				_previous = std::move(_active);
				_active.clear();
				// std::set iterators survive move-assignment; result.first now points
				// into _previous.
				return std::make_pair(make_iter(const_iterator::PREVIOUS, result.first), true);
			}
			return std::make_pair(make_iter(const_iterator::ACTIVE, result.first), true);
		}

		std::pair<iterator, bool> insert(T&& value) {
			auto pit = _previous.find(value);
			if (pit != _previous.end()) {
				return std::make_pair(make_iter(const_iterator::PREVIOUS, pit), false);
			}
			auto result = _active.insert(std::move(value));
			if (!result.second) {
				return std::make_pair(make_iter(const_iterator::ACTIVE, result.first), false);
			}
			if (rotation_due()) {
				_previous = std::move(_active);
				_active.clear();
				return std::make_pair(make_iter(const_iterator::PREVIOUS, result.first), true);
			}
			return std::make_pair(make_iter(const_iterator::ACTIVE, result.first), true);
		}

		size_type erase(const T& value) {
			size_type n = _active.erase(value);
			n += _previous.erase(value);
			return n;
		}

		iterator erase(iterator pos) {
			if (pos._where == const_iterator::ACTIVE) {
				auto next = _active.erase(pos._inner);
				return make_iter(const_iterator::ACTIVE, next);
			}
			if (pos._where == const_iterator::PREVIOUS) {
				auto next = _previous.erase(pos._inner);
				return make_iter(const_iterator::PREVIOUS, next);
			}
			return end();
		}

		// --- Lookup ---
		iterator find(const T& value) const {
			auto ait = _active.find(value);
			if (ait != _active.end()) {
				return make_iter(const_iterator::ACTIVE, ait);
			}
			auto pit = _previous.find(value);
			if (pit != _previous.end()) {
				return make_iter(const_iterator::PREVIOUS, pit);
			}
			return end();
		}

		size_type count(const T& value) const {
			return (_active.count(value) || _previous.count(value)) ? 1 : 0;
		}

		bool contains(const T& value) const {
			return _active.find(value) != _active.end()
			    || _previous.find(value) != _previous.end();
		}

		// --- Iteration (previous then active) ---
		iterator begin() const  { return make_iter(const_iterator::PREVIOUS, _previous.begin()); }
		iterator end() const    { return iterator(this, const_iterator::END_, _active.end()); }
		iterator cbegin() const { return begin(); }
		iterator cend() const   { return end(); }

	private:
		InnerSet _active;
		InnerSet _previous;
		size_type _max_size = 0;

		iterator make_iter(typename const_iterator::Where where,
		                   typename InnerSet::const_iterator inner) const {
			return iterator(this, where, inner);
		}

		bool rotation_due() const {
			return _max_size > 0 && _active.size() >= (_max_size + 1) / 2;
		}
	};

}}
