#pragma once

#include <memory>
#include <cstdint>

#include "io/session_context.hpp"
#include "io/io_data_registry.hpp"
#include "io/sink.hpp"

// template <typename T>
// std::unique_ptr<sink<T>> make_sink(const session_context& context, storage_type type, size_t segment_id, size_t channel_index = 0);

template <typename T>
std::unique_ptr<sink<T>> make_sink(const session_context& context, const data_descriptor& descriptor);

template <typename T> 
std::unique_ptr<source<T>> make_source(const session_context& context, const data_descriptor& handle);

// extern template std::unique_ptr<sink<int8_t>> make_sink<int8_t>(const session_context& constext, storage_type type, size_t segment_id, size_t channel_index);
// extern template std::unique_ptr<sink<int16_t>> make_sink<int16_t>(const session_context& constext, storage_type type, size_t segment_id, size_t channel_index);
// extern template std::unique_ptr<sink<int32_t>> make_sink<int32_t>(const session_context& constext, storage_type type, size_t segment_id, size_t channel_index);
// extern template std::unique_ptr<sink<int64_t>> make_sink<int64_t>(const session_context& constext, storage_type type, size_t segment_id, size_t channel_index);
// 
// extern template std::unique_ptr<sink<uint8_t>> make_sink<uint8_t>(const session_context& constext, storage_type type, size_t segment_id, size_t channel_index);
// extern template std::unique_ptr<sink<uint16_t>> make_sink<uint16_t>(const session_context& constext, storage_type type, size_t segment_id, size_t channel_index);
// extern template std::unique_ptr<sink<uint32_t>> make_sink<uint32_t>(const session_context& constext, storage_type type, size_t segment_id, size_t channel_index);
// extern template std::unique_ptr<sink<uint64_t>> make_sink<uint64_t>(const session_context& constext, storage_type type, size_t segment_id, size_t channel_index);

extern template std::unique_ptr<sink<int8_t>> make_sink<int8_t>(const session_context& constext, const data_descriptor& descriptor);
extern template std::unique_ptr<sink<int16_t>> make_sink<int16_t>(const session_context& constext, const data_descriptor& descriptor);
extern template std::unique_ptr<sink<int32_t>> make_sink<int32_t>(const session_context& constext, const data_descriptor& descriptor);
extern template std::unique_ptr<sink<int64_t>> make_sink<int64_t>(const session_context& constext, const data_descriptor& descriptor);

extern template std::unique_ptr<sink<uint8_t>> make_sink<uint8_t>(const session_context& constext, const data_descriptor& descriptor);
extern template std::unique_ptr<sink<uint16_t>> make_sink<uint16_t>(const session_context& constext, const data_descriptor& descriptor);
extern template std::unique_ptr<sink<uint32_t>> make_sink<uint32_t>(const session_context& constext, const data_descriptor& descriptor);
extern template std::unique_ptr<sink<uint64_t>> make_sink<uint64_t>(const session_context& constext, const data_descriptor& descriptor);

extern template std::unique_ptr<source<int8_t>> make_source<int8_t>(const session_context& context, const data_descriptor& descriptor);
extern template std::unique_ptr<source<int16_t>> make_source<int16_t>(const session_context& context, const data_descriptor& descriptor);
extern template std::unique_ptr<source<int32_t>> make_source<int32_t>(const session_context& context, const data_descriptor& descriptor);
extern template std::unique_ptr<source<int64_t>> make_source<int64_t>(const session_context& context, const data_descriptor& descriptor);

extern template std::unique_ptr<source<uint8_t>> make_source<uint8_t>(const session_context& context, const data_descriptor& descriptor);
extern template std::unique_ptr<source<uint16_t>> make_source<uint16_t>(const session_context& context, const data_descriptor& descriptor);
extern template std::unique_ptr<source<uint32_t>> make_source<uint32_t>(const session_context& context, const data_descriptor& descriptor);
extern template std::unique_ptr<source<uint64_t>> make_source<uint64_t>(const session_context& context, const data_descriptor& descriptor);
