#pragma once

#include <memory>
#include <cstdint>

#include "io/session_context.hpp"
#include "io/sink.hpp"

template <typename T>
std::unique_ptr<sink<T>> make_sink(const session_context& context, size_t segment_id, sink_type type, size_t channel_index = 0);

extern template std::unique_ptr<sink<int8_t>> make_sink<int8_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
extern template std::unique_ptr<sink<int16_t>> make_sink<int16_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
extern template std::unique_ptr<sink<int32_t>> make_sink<int32_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
extern template std::unique_ptr<sink<int64_t>> make_sink<int64_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);

extern template std::unique_ptr<sink<uint8_t>> make_sink<uint8_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
extern template std::unique_ptr<sink<uint16_t>> make_sink<uint16_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
extern template std::unique_ptr<sink<uint32_t>> make_sink<uint32_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
extern template std::unique_ptr<sink<uint64_t>> make_sink<uint64_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
