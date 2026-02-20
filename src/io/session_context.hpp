#pragma once

#include <vector>
#include <variant>
#include <type_traits>

#include "core_types.hpp"
#include "compressor_types.hpp"
#include "dwt/bitmap.tpp"

#include "io_settings.hpp"
#include "io_contexts.hpp"
#include "constant.hpp"

#include "io_data_registry_fwd.hpp"


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

enum class operation_state {
	pending, 
	processing, 
	done
};

#include <unordered_map>
#include <shared_mutex>

template <typename Derived>
class lockable_operation_descriptor {
public:
	void lock() noexcept {
		operation_state expected_pending = operation_state::pending;
		static_cast<Derived*>(this)->state.compare_exchange_strong(expected_pending, operation_state::processing);
	}

	void unlock() noexcept {
		operation_state expected_processing = operation_state::processing;
		static_cast<Derived*>(this)->state.compare_exchange_strong(expected_processing, operation_state::done);
	}

protected:
	~lockable_operation_descriptor() = default;
	lockable_operation_descriptor(lockable_operation_descriptor&& other) noexcept = default;
	lockable_operation_descriptor& operator=(lockable_operation_descriptor&& other) noexcept = default;
	lockable_operation_descriptor(const lockable_operation_descriptor& other) noexcept = default;
	lockable_operation_descriptor& operator=(const lockable_operation_descriptor& other) noexcept = default;
	lockable_operation_descriptor() = default;
};

struct dwt_descriptor: public lockable_operation_descriptor<dwt_descriptor> {
	size_t context_id;
	std::atomic<operation_state> state;
	img_pos frame;

	dwt_descriptor(size_t context_id, operation_state state, img_pos frame) :
			context_id(context_id), state(state), frame(frame) {}
};

struct segmentation_descriptor: public lockable_operation_descriptor<segmentation_descriptor> {
	size_t context_id;
	std::atomic<operation_state> state;
	img_pos frame;

	segmentation_descriptor(size_t context_id, operation_state state, img_pos frame) :
		context_id(context_id), state(state), frame(frame) {}
};

struct compression_descriptor: public lockable_operation_descriptor<compression_descriptor> {
	size_t context_id;
	std::atomic<operation_state> state;
	size_t segment_id;
	
	compression_descriptor(size_t context_id, operation_state state, size_t segment_id) :
		context_id(context_id), state(state), segment_id(segment_id) {}
};

class descriptor_registry {
private:
	mutable std::shared_mutex decorrelation_descriptors_mx;
	std::unordered_map<size_t, dwt_descriptor> decorrelation_descriptors;	// context id -> descriptor

	mutable std::shared_mutex segmentation_descriptors_mx;
	std::unordered_map<size_t, segmentation_descriptor> segmentation_descriptors;	// context id -> descriptor

	mutable std::shared_mutex compression_descriptors_mx;
	std::deque<compression_descriptor> compression_descriptors;		// segment id as index, 0-based
	std::unordered_map<size_t, size_t> compression_segment_id_map;	// context id -> segment id [== compression_descriptors index]

public:
	std::unique_lock<dwt_descriptor> start_operation(const dwt_context& cx) {
		auto get_descriptor = [this](size_t context_id) -> dwt_descriptor& {
			std::shared_lock lock(this->decorrelation_descriptors_mx);
			return this->decorrelation_descriptors.at(context_id);
		};
		return std::unique_lock(get_descriptor(cx.id));
	}

	template <typename T, typename D>
	std::unique_lock<segmentation_descriptor> start_operation(const segmentation_context<T, D>& cx) {
		auto get_descriptor = [this](size_t context_id) -> segmentation_descriptor& {
			std::shared_lock lock(this->segmentation_descriptors_mx);
			return this->segmentation_descriptors.at(context_id);
		};
		return std::unique_lock(get_descriptor(cx.id));
	}

	template <typename T>
	std::unique_lock<compression_descriptor> start_operation(const compression_context<T>& cx) {
		auto get_descriptor = [this](size_t segment_id) -> compression_descriptor& {
			std::shared_lock lock(this->compression_descriptors_mx);
			return this->compression_descriptors.at(segment_id);
		};
		return std::unique_lock(get_descriptor(cx.segment_data->id));
		// use of context_id -> segment_id map is also possible
	}

	void register_operation(const dwt_context& cx) {
		std::lock_guard lock(this->decorrelation_descriptors_mx);
		this->register_operation_impl(cx);
	}

	template <typename T, typename D>
	void register_operation(const segmentation_context<T, D>& cx) {
		std::lock_guard lock(this->segmentation_descriptors_mx);
		this->register_operation_impl(cx);
	}

	template <typename T>
	void register_operation(const compression_context<T>& cx) {
		std::lock_guard lock(this->compression_descriptors_mx);

		bool valid = (this->compression_descriptors.size() == cx.segment_data->id);
		[[unlikely]]
		if (!valid) {
			// TODO: error handling

			// segment id sequence is broken?
		}

		this->register_operation_impl(cx);
	}

	size_t compression_done_lower_bound(size_t segment_id_begin, size_t segment_id_end) {
		std::shared_lock lock(this->compression_descriptors_mx);
		while ((segment_id_begin != segment_id_end) &&
			(this->compression_descriptors.at(segment_id_begin).state.load() == operation_state::done)) {
			++segment_id_begin;
		}

		return segment_id_begin;
	}

private:
	template <typename Context>
	struct register_operations_impl;

	template <>
	struct register_operations_impl<dwt_context>;

	template <typename T, typename D>
	struct register_operations_impl<segmentation_context<T, D>>;

	template <typename T>
	struct register_operations_impl<compression_context<T>>;

public:
	template <typename Collection>
	void register_operations(const Collection& contexts) {
		register_operations_impl<typename Collection::value_type>::apply(*this, contexts);
	}

private:
	template <>
	struct register_operations_impl<dwt_context> {
		template <typename Collection>
		static void apply(descriptor_registry& target_cx, const Collection& contexts) {
			std::lock_guard lock(target_cx.decorrelation_descriptors_mx);
			// std::count_if could be used if register_operation_impl returned bool, that 
			// would weaken strong sequencing provided by std::for_each
			std::for_each(contexts.cbegin(), contexts.cend(),
				[&target_cx](const dwt_context& item) -> void {
					target_cx.register_operation_impl(item);
				});
		}
	};

	template <typename T, typename D>
	struct register_operations_impl<segmentation_context<T, D>> {
		template <typename Collection>
		static void apply(descriptor_registry& target_cx, const Collection& contexts) {
			std::lock_guard lock(target_cx.segmentation_descriptors_mx);
			// std::count_if could be used if register_operation_impl returned bool, that 
			// would weaken strong sequencing provided by std::for_each
			std::for_each(contexts.cbegin(), contexts.cend(),
				[&target_cx](const segmentation_context<T, D>& item) -> void {
					target_cx.register_operation_impl(item);
				});
		}
	};

	template <typename T>
	struct register_operations_impl<compression_context<T>> {
		template <typename Collection>
		static void apply(descriptor_registry& target_cx, const Collection& contexts) {
			if (!contexts.empty()) {
				auto it = contexts.cbegin();
				size_t prev_segment_id = it->segment_data->id;
				it++;

				bool valid = true;
				std::for_each(it, contexts.cend(),
					[&prev_segment_id, &valid](const compression_context<T>& item) -> void {
						++prev_segment_id;
						valid &= (item.segment_data->id == prev_segment_id);
					});
				if (!valid) {
					// TODO: error handling, segment id sequence is broken
				}

				std::lock_guard lock(target_cx.compression_descriptors_mx);

				valid = (target_cx.compression_descriptors.size() == contexts.front().segment_data->id);
				[[unlikely]]
				if (!valid) {
					// TODO: error handling

					// segment id sequence is broken?
				}

				std::for_each(contexts.cbegin(), contexts.cend(),
					[&target_cx](const compression_context<T>& item) -> void {
						target_cx.register_operation_impl(item);
					});
			}
		}
	};

	void register_operation_impl(const dwt_context& cx) {
		auto [it, inserted] = decorrelation_descriptors.try_emplace(cx.id,
			cx.id, operation_state::pending, cx.frame);
		if (!inserted) {
			// descriptor registry is broken?
		}
	}

	template <typename T, typename D>
	void register_operation_impl(const segmentation_context<T, D>& cx) {
		auto [it, inserted] = segmentation_descriptors.try_emplace(cx.id,
			cx.id, operation_state::pending, cx.frame);
		if (!inserted) {
			// descriptor registry is broken?
		}
	}

	template <typename T>
	void register_operation_impl(const compression_context<T>& cx) {
		auto [it, inserted] = compression_segment_id_map.insert({ cx.id, cx.segment_data->id });

		bool valid = inserted;

		[[unlikely]]
		if (!valid) {
			// TODO: error handling
			// descriptor registry is broken?
			// up to this moment strong exception safety is possible
		}

		compression_descriptors.emplace_back(cx.id, operation_state::pending, cx.segment_data->id);
	}
};

struct session_context;

template <typename T>
struct compression_data {
	using sT = compressor_type_params<T>::segment_type;
	using sbT = compressor_type_params<T>::subband_type;


	mutable std::mutex subband_fragments_mx;
	std::list<std::pair<subbands_t<sbT>, img_pos>> subband_fragments;
	std::list<std::unique_ptr<segment<sT>>> tail; // those segments that couldn't be dispatched
	mutable std::mutex tail_mx;
	std::unique_ptr<segment<sT>> incomplete_segment;	// TODO: integrate logic
	std::vector<std::pair<img_pos, std::reference_wrapper<const data_descriptor>>> fragments;
	mutable std::mutex fragments_mx;
};

struct channel_context {
	descriptor_registry descriptors;
	std::pair<size_t, size_t> allocated_segment_id = { 0, 0 }; // first is lower bound, second is upper

	std::variant<
			compression_data<int8_t>,
			compression_data<int16_t>,
			compression_data<int32_t>,
			compression_data<int64_t>, 
			compression_data<float>, 
			compression_data<double>
			// segment types for other dwt transformations are same as one of the above
		> data;

	session_context& session_cx;
	const size_t channel_index;

	template <typename T>
	channel_context(session_context& cx, size_t z, std::type_identity<T>) :
			session_cx(cx), channel_index(z), data(std::in_place_type<compression_data<T>>)		// msvc implementation couldn't deduce variant(T&& t) for some reason
	{
		auto& compr_data = std::get<compression_data<T>>(this->data);
		compr_data.incomplete_segment = std::make_unique<segment<typename compression_data<T>::sT>>();
	}

	void free_compressed_segment_data() {
		ptrdiff_t next_allocated_id = this->allocated_segment_id.first;
		size_t next_available_id = this->allocated_segment_id.second;

		this->allocated_segment_id.first = descriptors.compression_done_lower_bound(
			next_allocated_id, next_available_id);
	}
};

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
	// TODO: the following properties seem legitimate to vary across different channels of the same image:
	//		dwt shifts 
	//		segmentation settings (segment sizes)
	//			[overall, the limit on segment indexing of 256 segments processed simultaneously 
	//				is applicable to every channel independently]
	//		compression settings
	//

	size_t id; // TODO: uninitialized?
	session_settings settings_session;

	std::vector<std::pair<size_t, compression_settings>> compr_settings; // at least 1 item must be present. Sorted
	std::vector<std::pair<size_t, segment_settings>> seg_settings; // at least 1 item must be present. Sorted

	// std::vector would require value_type to be copy-constructible for emplace_back
	std::deque<channel_context> channel_contexts;

	io_data_registry& data_registry;
	// session_context is movable and copyable. Mind proper handling of reference member
	
	session_context(io_data_registry& registry) : data_registry(registry) {}


	template <typename dwtT>
	void init_channel_contexts(size_t channel_num = 1) {
		for (size_t i = 0; i < channel_num; ++i) {
			this->channel_contexts.emplace_back(*this, i, std::type_identity<dwtT>{});
		}

		// TODO: if the data decoded sequentially, compression and segment settings will be
		// populated on the fly, that would cause reallocations if std::vector is used.
		// The scenario should be examined on the subject of data races: no concurrent read
		// is allowed when settings data is populated; for seqential decoding it should be 
		// well-behaved due to lack of concurrent execution; for concurrent approaches, the 
		// data should be populated before the processing begins.
		//
	}

	size_t get_channel_num() const noexcept {
		return this->channel_contexts.size();
	}
};


#include <algorithm>
#include <utility>
#include <type_traits>
#include <iterator>

inline std::pair<compression_settings, bool> get_compression_settings(const session_context& context, size_t id) {
	const auto& compr_settings = context.compr_settings;
	using value_t = std::remove_reference_t<decltype(compr_settings)>::value_type;
	auto result_iter = std::upper_bound(compr_settings.cbegin(), compr_settings.cend(), id, [](size_t target, const value_t& val) -> bool {
			return (target < val.first);
		});
	return {std::prev(result_iter)->second, (std::prev(result_iter)->first == id)};
}

inline std::pair<segment_settings, bool> get_segment_settings(const session_context& context, size_t id) {
	const auto& seg_settings = context.seg_settings;
	using value_t = std::remove_reference_t<decltype(seg_settings)>::value_type;
	auto result_iter = std::upper_bound(seg_settings.cbegin(), seg_settings.cend(), id, [](size_t target, const value_t& val) -> bool {
			return (target < val.first);
		});
	bool strict_match = (std::prev(result_iter)->first == id);
	return {std::prev(result_iter)->second, (std::prev(result_iter)->first == id)};
}

inline size_t generate_session_id() {
	static std::atomic<size_t> current_id = 0;
	return current_id.fetch_add(1, std::memory_order_relaxed);
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




// template <typename dwtT>
// session_context(std::vector<result_handle>&& handles, size_t channel_count = 1) :
// 	data_handles(std::move(handles)) {
// 	for (size_t i = 0; i < channel_count; ++i) {
// 		this->channel_contexts.emplace_back(*this, i, dwtT{});
// 	}
// }

// session_context(std::vector<std::unique_ptr<result_handle>>&& handles) :
// 	data_handles(std::move(handles)) {
// }
// 
// template <typename dwtT>
// void setup_channels(size_t channel_count = 1) {
// 	for (size_t i = 0; i < channel_count; ++i) {
// 		this->channel_contexts.emplace_back(*this, i, dwtT{});
// 	}
// }



// template <typename T, typename dwtT = std::make_signed_t<T>>
// session_context(std::vector<bitmap<T>>&& img, storage_type target_dst_type = storage_type::memory) :
// 		image_channels(std::move(img)), dst_type(target_dst_type) {
// 	auto& img_channels = std::get<std::vector<bitmap<T>>>(this->image_channels);
// 	for (size_t i = 0; i < img_channels.size(); ++i) {
// 		this->channel_contexts.emplace_back(*this, i, std::type_identity<dwtT>{});
// 	}
// }
// 
// template <typename dwtT>
// session_context(decompression_context_params&& params, std::type_identity<dwtT>) : 
// 		data_handles(std::move(params.data_handles)), settings_session(std::move(params.session_params)), 
// 		compr_settings(std::move(params.compression_params)), seg_settings(std::move(params.segment_params)) {
// 	for (size_t i = 0; i < params.channel_count; ++i) {
// 		this->channel_contexts.emplace_back(*this, i, std::type_identity<dwtT>{});
// 	}
// 
// 	// TODO: if the data decoded sequentially, compression and segment settings will be
// 	// populated on the fly, that would cause reallocations if std::vector is used.
// 	// The scenario should be examined on the subject of data races: no concurrent read
// 	// is allowed when settings data is populated; for seqential decoding it should be 
// 	// well-behaved due to lack of concurrent execution; for concurrent approaches, the 
// 	// data should be populated before the processing begins.
// 	//
// }
