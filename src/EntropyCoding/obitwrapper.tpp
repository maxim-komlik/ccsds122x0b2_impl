#pragma once

#include <functional>
#include <type_traits>

#include "entropy_types.h"
#include "exception.h"

template <typename buffer_t> 
class obitwrapper {
	typedef typename std::make_unsigned<buffer_t>::type ubuffer_t;
	typedef typename std::make_signed<buffer_t>::type sbuffer_t;
	buffer_t buffer = 0;
	const size_t capacity = sizeof(buffer_t) << 3;
	size_t wcount = capacity;

	size_t byte_limit; // TODO: but should be capable of holding 2^27 - 1. See 4.2.3.2.1
	size_t byte_count = 0;

	typedef std::function<void(buffer_t)> callback_t;
	callback_t dest;

public:
	using value_type = ubuffer_t;
	obitwrapper(const callback_t &callback, size_t dst_byte_limit = ((1 << 27) - 1)) 
			: dest(callback), byte_limit(dst_byte_limit) {};
	// obitwrapper(const callback_t &&callback) = delete;
	~obitwrapper() = default;
	// The owning code has to call flush before the object
	// is distructed to commit the buffer content.
	// 
	// {
	// 	// TODO: MAJOR: dtor may throw!
	// 	if (this->wcount < this->capacity) {
	// 		this->dest(this->buffer);
	// 	}
	// };

	// Nor copyable nor movable
	obitwrapper(const obitwrapper &other) = delete;
	obitwrapper& operator=(const obitwrapper &other) = delete;

	obitwrapper& operator<<(vlw_t symbol) {
		// requires that std::min(symbol.length, this->wcount) != capacity, that is, 
		// when symbol.length equals bitlen(buffer_t) may cause shift amount param 
		// overflow (as per x86 and riscv specs). If happens, writes 0 to the output 
		// collection instead of symbol.value.
		// That implies requirement of symbol.length < this->capacity so that the body 
		// of the loop below is executed no more than 2 times per function call.
		// 
		// Mitigated by explicit check below. Need to estimate performance impact.
		//
		do {
			size_t shift = std::min(symbol.length, this->wcount);
			symbol.length -= shift;

			[[likely]]
			if (shift != this->capacity) {
				this->wcount -= shift;
				this->buffer ^= ((~(((sbuffer_t)(-1)) << shift)) & (symbol.value >> symbol.length)) << this->wcount;
				if (this->wcount == 0) {
					this->flush();
				}
			} else {
				[[likely]]
				if (!this->dirty()) {
					this->flush_word((buffer_t)(symbol.value >> symbol.length));
				} else {
					// infinite recursion otherwise
					throw "unreachable by design!";
				}
			}
		} while (symbol.length > 0);
		return *this;
	}

	void flush() {
		// this->dest(this->buffer);
		this->__store_item(this->buffer);
		this->wcount = this->capacity;
		this->buffer = 0; // see 4.2.3.2.5, paragraph c
	}

	template <typename word_t>
	void flush_word(word_t word);

	template <>
	void flush_word(buffer_t word) {
		[[unlikely]]
		if (this->dirty()) {
			// check above guarantees no overflow will happen due to length == bitlen(buffer_t)
			(*this) << vlw_t{ sizeof(word) << 3, word };
		} else {
			this->__store_item(word);
		}
	}

	size_t ocount() const {
		return this->wcount;
	}

	constexpr size_t bcapacity() {
		return (sizeof(buffer_t) << 3);
	}

	bool dirty() const {
		return (this->wcount < this->capacity);
	}

private:
	// TODO: but inline is implicit here because defined inside class definition
	inline void __store_item(buffer_t item) {
		this->dest(item);

		this->byte_count += sizeof(decltype(item));
		[[unlikely]]
		if (this->byte_count >= this->byte_limit) {
			throw ccsds::bpe::byte_limit_exception();
		}
	}
};


template <typename buffer_t>
template <typename word_t>
void obitwrapper<buffer_t>::flush_word(word_t word) {
	// TODO: when fed with reference type as a template parameter, sizeof operator 
	// returns a result that breaks the logic below
	if constexpr (sizeof(word_t) < sizeof(buffer_t)) {
		(*this) << vlw_t{ sizeof(word) << 3, word };
	} else if constexpr (sizeof(word_t) == sizeof(buffer_t)) {
		this->flush_word<buffer_t>(word);
	} else {
		constexpr size_t ratio = sizeof(word_t) / sizeof(buffer_t);
		for (ptrdiff_t i = (ptrdiff_t)(ratio - 1); i >= 0; --i) {
			this->flush_word<buffer_t>(word >> (i * sizeof(buffer_t) * 8));
		}
	}
}

// TODO: consider adding accessible typename of the underlying buffer tipe in bitwrapper
//

// obitwrapper is neither copyable neither movable to guarantee that it does not outlive the context in which
// it is constructed, therefore it's lifetime is not longer than the lifetime of a container. Container is 
// expected to be passed to other scopes/contexts.
//

// Client code: obitwrapper constructor called with prvalue parameter: const & is bound and reference copied, 
// bu that results in a dangling reference, because the parameter is a temporary.
// 
// obitwrapper lifetime depends on callback lifetime. Callback lifetime depends on output collection lifetime.
// 
// bitwrapper is useless without callback: every input/output operation can cause callback call, that results 
// in a call via dangling reference if lifetime of a callback is ended already.
// callback has no meaning and no sense out of output collection lifetime and scope. Generally, it has no 
// use without obitwrapper.
// Output collection is free of dependencies and is self-contained.
// 
// 1. bitwrapper, callback and output collection are all different types
// bitwapper's lifetime is limited by non-copyable + non-movable constrain. bitwrapper lifetime cannot exceed 
// the scope in which it is declared (omitting specific tricks to extend lifetime).
// 
// Affected by the issue with passing temporary as a constructor parameter. Possible workarounds:
// + prohibit to pass rvalues as a constructor parameter. Requires several constructors declaration
// + copy passed constructor parameter to a complete member. Weakens dependency between bitwrapper and callback.
// Callback ownership problem. Requires additional data member to store the callback by value.
// + 
// 
// 2. bitwrapper and callback are members of a joint type 1.
// Bitwrapper lifetime strong dependency on callback dependnecy is encapsulated, lifetimes are guaranteed to be 
// same. Callback implementation is dependent on output collection type. Bitwrapper can store reference or 
// function pointer. Similar to option 1 with dedicated value member for callback
// 
// 3. bitwrapper, callback and output collection are members of joint type 2.
// Lifetimes are equal, ok to store references or pointers. Will have to move data member to pass output 
// collection out of the scope where the joint type is declared.
// 
// 4. obitwrapper derives from output collection class
// Not clear how to pass output collection out of the scope where bitwrapper is necessary. No callback is necessary
// (optional for behavior customization/additional payload only).
//
