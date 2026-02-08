#pragma once

#include <vector>
#include <deque>
#include <any>
#include <optional>
#include <mutex>
#include <filesystem>
#include <cstddef>

#include "dwt/bitmap.tpp"

#include "session_context.hpp"
#include "constant.hpp"

struct segment_descriptor_base {
	static constexpr size_t id_unknown = (size_t)(ptrdiff_t)(-1);
	const size_t channel_id;
	const size_t segment_id;

protected:
	~segment_descriptor_base() = default;

	segment_descriptor_base(segment_descriptor_base&& other) noexcept = default;
	segment_descriptor_base& operator=(segment_descriptor_base&& other) noexcept = default;
	segment_descriptor_base(const segment_descriptor_base& other) noexcept = default;
	segment_descriptor_base& operator=(const segment_descriptor_base& other) noexcept = default;

	segment_descriptor_base(size_t channel_id, size_t segment_id):
		segment_id(segment_id), channel_id(channel_id) {}
};


struct file_descriptor {
	std::filesystem::path path;

protected:
	~file_descriptor() = default;

	file_descriptor(file_descriptor&& other) noexcept = default;
	file_descriptor& operator=(file_descriptor&& other) noexcept = default;
	file_descriptor(const file_descriptor& other) noexcept = default;
	file_descriptor& operator=(const file_descriptor& other) noexcept = default;

	file_descriptor(std::filesystem::path&& path) : path(std::move(path)) {}

	file_descriptor(const std::filesystem::path& path) : path(path) {}
};

struct memory_descriptor {
	std::vector<std::byte> data;

protected:
	~memory_descriptor() = default;

	memory_descriptor(memory_descriptor&& other) noexcept = default;
	memory_descriptor& operator=(memory_descriptor&& other) noexcept = default;
	memory_descriptor(const memory_descriptor& other) noexcept = default;
	memory_descriptor& operator=(const memory_descriptor& other) noexcept = default;

	memory_descriptor(std::vector<std::byte>&& data) : data(std::move(data)) {}
};


struct segment_file_descriptor : public segment_descriptor_base, public file_descriptor {
	segment_file_descriptor(std::filesystem::path&& path, size_t channel_id, size_t segment_id) :
		segment_descriptor_base(channel_id, segment_id), file_descriptor(std::move(path)) {}

	segment_file_descriptor(const std::filesystem::path& path, size_t channel_id, size_t segment_id) :
		segment_descriptor_base(channel_id, segment_id), file_descriptor(path) {
	}

	segment_file_descriptor(size_t channel_id, size_t segment_id) :
		segment_descriptor_base(channel_id, segment_id), file_descriptor({}) {
	}
};

struct segment_memory_descriptor : public segment_descriptor_base, public memory_descriptor {
	segment_memory_descriptor(std::vector<std::byte>&& segment, size_t channel_id, size_t segment_id) :
		segment_descriptor_base(channel_id, segment_id), memory_descriptor(std::move(segment)) {}
	
	segment_memory_descriptor(size_t channel_id, size_t segment_id) :
		segment_descriptor_base(channel_id, segment_id), memory_descriptor({}) {}
};


struct image_descriptor_base {
	static constexpr size_t id_unknown = (size_t)(ptrdiff_t)(-1);
	const size_t channel_id;

protected:
	~image_descriptor_base() = default;

	image_descriptor_base(image_descriptor_base&& other) noexcept = default;
	image_descriptor_base& operator=(image_descriptor_base&& other) noexcept = default;
	image_descriptor_base(const image_descriptor_base& other) noexcept = default;
	image_descriptor_base& operator=(const image_descriptor_base& other) noexcept = default;

	image_descriptor_base(size_t channel_id) : channel_id(channel_id) {}
};

template <typename T>
struct image_memory_descriptor : public image_descriptor_base {
	bitmap<T> image;

	image_memory_descriptor(bitmap<T>&& image, size_t channel_id) :
		image_descriptor_base(channel_id), image(std::move(image)) {}
};


enum class data_category {
	input, 
	output
};

enum class data_content_type {
	segment, 
	image
};


template <storage_type type>
struct segment_descriptor_value_type;

template <>
struct segment_descriptor_value_type<storage_type::file> {
	using type = segment_file_descriptor;
};

template <>
struct segment_descriptor_value_type<storage_type::memory> {
	using type = segment_memory_descriptor;
};


template <storage_type type>
struct segment_selector {
	static constexpr storage_type storage_type = type;
	using descriptor_type = segment_descriptor_value_type<type>::type;
};

template <typename T>
struct image_selector {
	using bitmap_value_type = T;
	using descriptor_type = image_memory_descriptor<T>;
};


struct data_descriptor {
	// better be nested type, but forward declaration is needed
public:
	struct exported_descriptor_data {
		std::optional<size_t> channel_id;
	};

private:
	static constexpr size_t id_external = (size_t)(ptrdiff_t)(-2);

private:
	size_t session_id;
	data_category origin;
	data_content_type content_type;
	struct exported_descriptor_data exported;
	std::any data;

public:
	~data_descriptor() = default;
	data_descriptor(data_descriptor&& other) noexcept = default;
	data_descriptor& operator=(data_descriptor&& other) noexcept = default;

	data_category get_category() const noexcept { return this->origin; }
	data_content_type get_content_type() const noexcept { return this->content_type; }
	size_t get_session_id() const noexcept { return this->session_id; }
	const exported_descriptor_data& get_exported_data() const noexcept { return this->exported; }

private:
	// make special members private so that accessible only by friends
	friend class io_data_registry;

	data_descriptor(const data_descriptor& other) = default;
	data_descriptor& operator=(const data_descriptor& other) = default;

	template <typename T>
	data_descriptor(size_t session_id, data_category category, data_content_type type, 
			exported_descriptor_data&& exported_data, T&& value) :
		session_id(session_id), origin(category), content_type(type), 
			exported(std::move(exported_data)), data(std::move(value)) {}
};

class io_data_registry {
private:
	std::deque<data_descriptor> handles;
	std::mutex handles_mx;

	const storage_type segment_storage_type;

public:
	io_data_registry(storage_type type) : segment_storage_type(type) {}

	template <typename T>
	static std::add_lvalue_reference_t<typename T::descriptor_type> get_data(const data_descriptor& data);

	void free_descriptor(const data_descriptor& descriptor) noexcept;

	const data_descriptor& put_output(const session_context& cx, segment_memory_descriptor&& value);
	const data_descriptor& put_output(const session_context& cx, segment_file_descriptor&& value);

	template <typename T>
	const data_descriptor& put_output(const session_context& cx, image_memory_descriptor<T>&& value);

	template <typename T>
	const data_descriptor& put_input(image_memory_descriptor<T>&& value);
	const data_descriptor& put_input(segment_memory_descriptor&& value);
	const data_descriptor& put_input(segment_file_descriptor&& value);

	template <typename T>
	const data_descriptor& put_input(const session_context& cx, image_memory_descriptor<T>&& value);
	const data_descriptor& put_input(const session_context& cx, segment_memory_descriptor&& value);
	const data_descriptor& put_input(const session_context& cx, segment_file_descriptor&& value);

	std::vector<data_descriptor> export_data()&&;

	storage_type get_segment_storage_type() const noexcept { return this->segment_storage_type; }

private:
	template <typename T>
	const data_descriptor& put_impl(size_t session_id, data_category category, data_content_type type, 
		data_descriptor::exported_descriptor_data&& exported_data, T&& value);
};


// io_data_registry implementation section:
//

template <typename T>
std::add_lvalue_reference_t<typename T::descriptor_type> 
io_data_registry::get_data(const data_descriptor& data) {
	using retT = std::add_lvalue_reference_t<typename T::descriptor_type>;
	return std::any_cast<retT>(const_cast<data_descriptor&>(data).data);
}

template <typename T>
const data_descriptor& io_data_registry::put_impl(size_t session_id, data_category category, 
		data_content_type type, data_descriptor::exported_descriptor_data&& exported_data, T&& value) {
	std::lock_guard lock(this->handles_mx);
	this->handles.push_back({ session_id, category, type, std::move(exported_data), std::move(value) });
	return this->handles.back();
}


template <typename T>
const data_descriptor& io_data_registry::put_output(const session_context& cx, 
		image_memory_descriptor<T>&& value) {
	size_t channel_id = value.channel_id;
	return this->put_impl(cx.id, data_category::output, data_content_type::image, 
		{ channel_id }, std::move(value));
}

template <typename T>
const data_descriptor& io_data_registry::put_input(image_memory_descriptor<T>&& value) {
	size_t channel_id = value.channel_id;
	return this->put_impl(data_descriptor::id_external, data_category::input, data_content_type::image, 
		{ channel_id }, std::move(value));
}

template <typename T>
const data_descriptor& io_data_registry::put_input(const session_context& cx, 
		image_memory_descriptor<T>&& value) {
	size_t channel_id = value.channel_id;
	return this->put_impl(cx.id, data_category::input, data_content_type::image, 
		{ channel_id }, std::move(value));
}

// output types:
//  file: 		std::filesystem::path
//  memory: 	std::vector
//  network: 	timepoint and statistics
//  image: 		bitmap in memory (client code to handle further processing, e.g. writing 
//		to file, converting to other formats etc.)
// 
// input types:
//	memory:		std::vector (+ segment id)
//  file:		std::filesystem::path (+ segment id)
//  image:		bitmap in memory
// 
// 
// image = handle.get_data();
// 
// handle:
//		content type
//		origin: input/output
//		storage reference
// 
// handle = storage.put(data);
// 
// 
// handle ----------+
//					|
//					|
//					A
//				   / \
//				  /	  \
// 	segment_hanlde		image_handle
//		  +					 +
//		  |					 |
//		  A					 A
//		 / \				/ \
//		/   \			   /   \
//   file	 +----+ +-----+
//				  |
//	memory	------+
//  network ------+
// 
// need shared ownership for image and memory segment
