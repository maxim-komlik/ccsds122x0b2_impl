#include "io_data_registry.hpp"

std::vector<io_data> io_data_registry::export_data()&& {
	std::vector<io_data> result;
	result.reserve(this->handles.size());

	std::move(this->handles.begin(), this->handles.end(), std::back_inserter(result));

	auto it = std::remove_if(result.begin(), result.end(),
		[](const io_data& item) noexcept -> bool {
			return !item.data.has_value();
		});
	result.erase(it, result.end());

	return result;
}

void io_data_registry::free_descriptor(const data_descriptor& descriptor) noexcept {
	const_cast<data_descriptor&>(descriptor).data.reset();
}

// one-liners below depend on template member function, would like to have them inline'd
const data_descriptor& io_data_registry::put_output(const session_context& cx,
		segment_memory_descriptor&& value) {
	size_t channel_id = value.channel_id;
	return this->put_impl(cx.id, data_category::output, data_content_type::segment, std::move(value));
}

const data_descriptor& io_data_registry::put_output(const session_context& cx,
		segment_file_descriptor&& value) {
	size_t channel_id = value.channel_id;
	return this->put_impl(cx.id, data_category::output, data_content_type::segment, std::move(value));
}

const data_descriptor& io_data_registry::put_input(segment_memory_descriptor&& value) {
	size_t channel_id = value.channel_id;
	return this->put_impl(data_descriptor::id_external, data_category::input, data_content_type::segment, 
		std::move(value));
}

const data_descriptor& io_data_registry::put_input(segment_file_descriptor&& value) {
	size_t channel_id = value.channel_id;
	return this->put_impl(data_descriptor::id_external, data_category::input, data_content_type::segment, 
		std::move(value));
}

const data_descriptor& io_data_registry::put_input(const session_context& cx,
		segment_memory_descriptor&& value) {
	size_t channel_id = value.channel_id;
	return this->put_impl(cx.id, data_category::input, data_content_type::segment, std::move(value));
}
const data_descriptor& io_data_registry::put_input(const session_context& cx,
		segment_file_descriptor&& value) {
	size_t channel_id = value.channel_id;
	return this->put_impl(cx.id, data_category::input, data_content_type::segment, std::move(value));
}
