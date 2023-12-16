#include <algorithm>

template <typename T, size_t allignment = 16>
class alligned_vector {
	T* m_begin;
	T* m_rawdata;
	size_t m_length;
	size_t m_offset;
	size_t m_rawdata_length;

	static constexpr size_t pallignment = allignment * sizeof(T);
public:
	~alligned_vector();
	alligned_vector(alligned_vector&& other) noexcept;
	alligned_vector(const alligned_vector& other);

	alligned_vector& operator=(alligned_vector&& other) noexcept;
	alligned_vector& operator=(alligned_vector& other);

	alligned_vector();
	alligned_vector(size_t length, T* origin);
	alligned_vector(size_t length, size_t offset = 16);

	void swap(alligned_vector& other) noexcept;
	friend void swap(alligned_vector& first, alligned_vector& second);

	void fill(size_t length, T* origin);
	void resize(size_t length);
	// T operator[](size_t index) const;
	T& operator[](size_t index) const;
	T* operator*() const;
private:
	void __reallocate();
};

template <typename T, size_t allignment>
alligned_vector<T, allignment>::~alligned_vector() {
	delete[] this->m_rawdata;
}

template <typename T, size_t allignment>
alligned_vector<T, allignment>::alligned_vector(alligned_vector&& other) noexcept :
		m_begin(other.m_begin), m_rawdata(other.m_rawdata),
		m_length(other.m_length), m_offset(other.m_offset), m_rawdata_length(other.m_rawdata_length) {
	other.m_rawdata = nullptr;
	other.m_begin = nullptr;
	other.m_rawdata_length = 0;
}

template <typename T, size_t allignment>
alligned_vector<T, allignment>::alligned_vector(const alligned_vector& other) : 
		m_length(other.m_length), m_offset(other.m_offset) {
	this->m_rawdata_length = std::min(other.m_rawdata_length,
		((other.m_length + allignment - 1) & (~(allignment - 1))) + 2 * other.m_offset);

	this->m_rawdata = new T[this->m_rawdata_length + (allignment - 1)];	// adds multiple of sizeof(T)
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->pallignment - 1))
		& (~(this->pallignment - 1)))) + this->m_offset;

	for (size_t i = 0; i < this->m_length; i += allignment) {
		for (size_t j = 0; j < allignment; ++j) {
			this->m_begin[i + j] = other.m_begin[i + j];
		}
	}
}

template <typename T, size_t allignment>
alligned_vector<T, allignment>& alligned_vector<T, allignment>::operator= (alligned_vector<T, allignment>&& other) noexcept {
	T* temp = this->m_rawdata;
	this->m_rawdata = other.m_rawdata;
	other.m_rawdata = temp;

	this->m_begin = other.m_begin;
	this->m_length = other.m_length;
	this->m_offset = other.m_offset;
	this->m_rawdata_length = other.m_rawdata_length;
	return *this;
}

template <typename T, size_t allignment>
alligned_vector<T, allignment>& alligned_vector<T, allignment>::operator= (alligned_vector<T, allignment>& other) {
	if (this != &other) {
		this->m_length = other.m_length;
		this->m_offset = other.m_offset;

		if (this->m_rawdata_length < (other.m_length + other.m_offset)) {
			this->m_rawdata_length = std::min(other.m_rawdata_length,
				((other.length + allignment - 1) & (~(allignment - 1))) + 2 * other.m_offset);
			this->__reallocate();
		}

		for (size_t i = 0; i < this->m_length; i += allignment) {
			for (size_t j = 0; j < allignment; ++j) {
				this->m_begin[i + j] = other.m_begin[i + j];
			}
		}
	}
	return *this;
}

template <typename T, size_t allignment>
alligned_vector<T, allignment>::alligned_vector() : m_rawdata(nullptr), m_begin(nullptr), m_rawdata_length(0) {}

template <typename T, size_t allignment>
alligned_vector<T, allignment>::alligned_vector(size_t length, T* origin) {
	this->m_length = length;
	// that ensures every new row starts with alligned address
	length = (length + allignment - 1) & (~(allignment - 1));
	this->m_offset = allignment;
	this->m_rawdata_length = length + 2 * allignment;

	this->m_rawdata = new T[this->m_rawdata_length + (allignment - 1)];	// adds multiple of sizeof(T)
	// physical memory allignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->pallignment - 1))
		& (~(this->pallignment - 1)))) + this->m_offset;

	ptrdiff_t i;
	for (i = 0; i < ((ptrdiff_t)length) - allignment; i += allignment) {
		for (ptrdiff_t j = 0; j < allignment; ++j) {
			this->m_begin[i + j] = origin[i + j];
		}
	}
	for (; i < length; ++i) {
		this->m_begin[i] = origin[i];
	}
}

template <typename T, size_t allignment>
alligned_vector<T, allignment>::alligned_vector(size_t length, size_t offset) {
	this->m_length = length;
	// that ensures every new row starts with alligned address
	length = (length + allignment - 1) & (~(allignment - 1));
	offset = (offset + allignment) & (~(allignment - 1));
	this->m_offset = offset;
	this->m_rawdata_length = length + 2 * offset;

	this->m_rawdata = new T[this->m_rawdata_length + (allignment - 1)];	// adds multiple of sizeof(T)
	// physical memory allignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->pallignment - 1))
		& (~(this->pallignment - 1)))) + this->m_offset;
}

template <typename T, size_t allignment>
void alligned_vector<T, allignment>::__reallocate() {
	delete[] this->m_rawdata;
	this->m_rawdata = new T[this->m_rawdata_length + (allignment - 1)];	// adds multiple of sizeof(T)
	// physical memory allignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->pallignment - 1))
		& (~(this->pallignment - 1)))) + this->m_offset;
}

template <typename T, size_t allignment>
void alligned_vector<T, allignment>::swap(alligned_vector<T, allignment>& other) noexcept {
	swap(*this, other);
}

// friend swap function
template <typename T, size_t allignment>
void swap(alligned_vector<T, allignment>& first, alligned_vector<T, allignment>& second) noexcept {
	using std::swap;
	swap(first.m_begin, second.m_begin);
	swap(first.m_rawdata, second.m_rawdata);
	swap(first.m_length, second.m_length);
	swap(first.m_offset, second.m_offset);
	swap(first.m_rawdata_length, second.rawdata_length);
}

namespace std {
	template <typename T, size_t allignment>
	void swap(alligned_vector<T, allignment>& first, alligned_vector<T, allignment>& second) noexcept {
		swap(first, second);
	}
}

template <typename T, size_t allignment>
T& alligned_vector<T, allignment>::operator[](size_t index) const {
	// TODO: boundary checks
	return this->m_begin[index];
}

template <typename T, size_t allignment>
T* alligned_vector<T, allignment>::operator*() const {
	return this->m_begin;
}
