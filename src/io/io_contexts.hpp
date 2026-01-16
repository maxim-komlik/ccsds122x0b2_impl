#pragma once

#include <memory>

#include "core_types.hpp"

#include "sink.hpp"


// compression context is created as soon as segment data is constructed
// by segment assembler; it owns constructed segment data. Sink and 
// associated protocols are initialized before the segment data is passed 
// to bpe encoder, and constructed obitwrapper of a specific sink is used
// as a parameter of bpe calls.
// 
// all compression contexts are owned by session context and stored as a 
// collection. Compression contexts are strictly ordered (as well as 
// owned segments which have ordered segment indexes). Compression can be 
// performed on several compression contexts simultaneously. Number of 
// compression contexts that exist simultaneously cannot exceed 256 due 
// to SegmentCount field width limitation of 8 bits in segment header 
// part 1a.
// 
// compression contexts in session context's collection hold dependencies.
// Compression settings and segment settings are effective starting from 
// the context they first defined at until the next redefinition in one 
// of the consequent contexts or the very last context in the end of the
// session, whatever comes first.
// 
// when fine-tuning compression parameters transmitted via header part 3 
// after actual compression is done (for example, to hint necessary 
// amount of memory to hold the whole segment via SegByteLimit field), 
// care should be taken to avoid applying the settings to all the 
// consequent compression contexts in the collection transitively. E.g. 
// compression settings for the next compression context should be 
// restored by setting old values explicitly if possible, otherwise 
// such fine-tuning should be discarded.
// 

// struct session_context;
struct channel_context;

// struct dwt_context { // name alternative: decorrelation_context?
// 	size_t id;
// 	session_context& session_cx;
// 	img_pos frame;
// };
// 
// template <typename segT>
// struct segmentation_context {
// 	size_t id;
// 	session_context& session_cx;
// 	subbands_t<segT> subband_data;
// 	std::unique_ptr<segment<segT>> incomplete_segment_data;
// };
// 
// template <typename segT>
// struct compression_context {
// 	using sink_value_type = size_t;
// 
// 	size_t id;
// 	session_context& session_cx;
// 	std::unique_ptr<sink<sink_value_type>> dst; // TODO: template parameter? Would like sink to be plain type, not template
// 	std::unique_ptr<segment<segT>> segment_data;
// };

struct dwt_context { // name alternative: decorrelation_context?
	size_t id;
	channel_context& channel_cx;
	img_pos frame;
};

template <typename segT>
struct segmentation_context {
	size_t id;
	channel_context& channel_cx;
	subbands_t<segT> subband_data;
	std::unique_ptr<segment<segT>> incomplete_segment_data;
	img_pos frame;
};

template <typename segT>
struct compression_context {
	using sink_value_type = size_t;

	size_t id;
	channel_context& channel_cx;
	std::unique_ptr<sink<sink_value_type>> dst; // TODO: template parameter? Would like sink to be plain type, not template
	std::unique_ptr<segment<segT>> segment_data;
};

inline size_t generate_compression_id() {
	static std::atomic<size_t> current_id = 0;
	return current_id.fetch_add(1, std::memory_order_relaxed);
}

inline size_t generate_segmentation_id() {
	static std::atomic<size_t> current_id = 0;
	return current_id.fetch_add(1, std::memory_order_relaxed);
}

inline size_t generate_dwt_id() {
	static std::atomic<size_t> current_id = 0;
	return current_id.fetch_add(1, std::memory_order_relaxed);
}