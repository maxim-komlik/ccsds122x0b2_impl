#pragma once

#include <vector>

#include "core_types.hpp"
#include "dwt/bitmap.tpp"

#include "io_settings.hpp"
#include "io_contexts.hpp"
#include "constant.hpp"


// session also has info about:
// image source (for now expected raw bitmap loaded in memory)
// image source type (for now memory only)
// image sink type
// image dst resources (or prototypes, because concrete resources should be owned by segment processing routines)
// 
// assigned resources
// owned handlers for resources
// owned sync data structures
// 
// 

// session manager holds info about:
// available computational resources
// active sessions
// accounting data on assigned resources
// 
// processing modules instances
//

// Source can be in-memory image only, there's no point to hold uncompressed image 
// in any other source.
//


// network sink: shared output device with a stream interface, segments put into the stream
// in a strict order one by one.
//

// result for dwt transformation - bitmap set == subbands_t <templated by fp/integral type>
// result for segment assembly - vector of segments <templated by transformed integral type>
// result for file sink - filepath of the output file
// result for memory sink - pointer to a managed memory containing compressed data
// result for network sink - transmission finish timestamp and accumulative stream offset 
// 
// result for scheduling/management tasks - void
// 
// 
// 
//


// TODO: RECYCLE
// enum class task_state {
// 	pending, 
// 	schedule, 
// 	process, 
// 	done, 
// 	cancel
// };
// 
// struct task_token {
// 	size_t id;
// 	task_state state;
// 
// 	struct result_concept;
// 
// 	template <typename contentT>
// 	struct result_model : public result_concept {
// 		contentT content;
// 	};
// 
// 	std::unique_ptr<result_concept> result;
// 
// 	template <typename resultT>
// 	void put_result(std::unique_ptr<resultT> result_data) {
// 		if (!result_data) {
// 			// TODO: handle nullptr
// 		}
// 		this->result = std::make_unique<result_model<resultT>>(*result_data);
// 	}
// };


// Input image may have arbitrary pixel depth, i.e. single pixel can be representable by 
// types less than 64-bit per channel.
// For the purpose of DWT, we'd like to make sure that image data is casted to signed type
// (or first level handled with unsigned to signed casting internally, transformer buffers
// have to be signed anyway) and static range of chosen transformer type is at least twice 
// as big as image data dynamic range for positive or negative values, whichever is greater.
// 
// Potential benefits of using denser types in terms of memory performance should be tested.
// 
// So for unsigned input we at least must cast to wider signed alternative. And for signed 
// input we sometimes need to cast to wider alternative.
// 
// Therefore it seems to be not so bad idea to cast image data to int64_t anyway.
// 
struct session_context {
	size_t id;
	session_settings settings_session;

	std::vector<std::pair<size_t, compression_settings>> compr_settings; // max key is 256. At least 1 item must be present. Sorted
	std::vector<std::pair<size_t, segment_settings>> seg_settings; // max key is 256. At least 1 item must be present. Sorted

	// TODO: members below should have configurable underlying type
	std::vector<bitmap<int64_t>> image_channels; // TODO: int64_t can handle all types specified in the standard, but bitmap underlying type should be configurable
	std::vector<compression_context<int64_t>> compression_contexts; // max size is 256
	std::vector<segment<int64_t>> tail; // those segments that didn't fit into compression_contexts

	sink_type dst_type;
};

#include <algorithm>
#include <utility>
#include <type_traits>
#include <iterator>

std::pair<compression_settings, bool> get_compression_settings(const session_context& context, size_t id) {
	const auto& compr_settings = context.compr_settings;
	using value_t = std::remove_reference_t<decltype(compr_settings)>::value_type;
	auto result_iter = std::upper_bound(compr_settings.cbegin(), compr_settings.cend(), id, [](size_t target, const value_t& val) -> bool {
			return (val.first < target);
		});
	return {std::prev(result_iter)->second, (std::prev(result_iter)->first == id)};
}

std::pair<segment_settings, bool> get_segment_settings(const session_context& context, size_t id) {
	const auto& seg_settings = context.seg_settings;
	using value_t = std::remove_reference_t<decltype(seg_settings)>::value_type;
	auto result_iter = std::upper_bound(seg_settings.cbegin(), seg_settings.cend(), id, [](size_t target, const value_t& val) -> bool {
			return (val.first < target);
		});
	bool strict_match = (std::prev(result_iter)->first == id);
	return {std::prev(result_iter)->second, (std::prev(result_iter)->first == id)};
}

//
// manager thread:
// [session management code]
// create compression context. create sink via generator. move target segment. allocate 
// encoder resource. allocate thread resource or put the task to scheduler. Put different 
// workloads depending on session state/segment properties.
// 
// compression thread:
// [compression thread main code] for regular segment
// find encoding parameters and create local copy. set encoding parameters on sink. set 
// encoding parameters on encoder. initialize sink encoding session. execute encoding 
// steps. finalize encoding session. register outcome.
// [sink setup code]
// setup sink compression params. [? handle header content encoding. /moved to sink session init code/]
// [sink session init code]
// allocate system resources, allocate buffers. fill protocol data and flush to buffers
// [sink encoding code] 
// takes encoder and segment data as parameters. checks preconditions. perform encode 
// step.
// [sink session finilize code]
// handle fill params as needed
// 
// 
// {sink generator} --> spesific {sink} casted to polymorphic base. All sink specific initialization handled here
// {resource manager} --> {encoder} instance
// 
// [compression, segment params] -------> protocol setup, sink configure
// [compression, segment params] -------> encoder setup [early termination, heuristic/optional]
// 
// 
// first segment: known due to session state, just dedicated task with all necessary handling and 
// initialization.
// 
// last segment: if segment header overridable, encode all segments as regular. When end of input data 
// is signalled, override a header in the last segment written. If segment header is not overridable, 
// keep one segment unprocessed always until the end of input data is signaled, then process the 
// last remaining segment with proper handling of image ending.
//
