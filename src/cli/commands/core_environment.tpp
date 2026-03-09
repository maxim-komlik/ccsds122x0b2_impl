#pragma once
#include <vector>
#include <tuple>
#include <utility>
#include <memory>
#include <algorithm>
#include <functional>
#include <cstddef>

#include "dwt/bitmap.tpp"
#include "dwt/utility.hpp"
#include "common/utility.hpp"

#include "io/io_contexts.hpp"
#include "io/session_context.hpp"
#include "io/tasking.hpp"
#include "core/samples/routines.tpp"

#include "core_environment.hpp"
#include "load_image.tpp"

namespace cli::command {

using namespace cli::parameters;

namespace {

	template <typename T>
	inline size_t compute_image_bdepth(const bitmap<T>& src) {
		size_t result = 0;
		for (ptrdiff_t i = 0; i < src.get_meta().height; ++i) {
			bitmap_row row = src[i];
			result = std::max(result, bdepthv<T, 16>(row.ptr(), row.width()));
		}
		return result;
	}

}

template <template<typename /*ibwT/obwT*/, typename /*imgT*/, typename /*dwtT*/> typename Implementation>
struct session_parameters_parser: protected session_parameters_parser_base<session_parameters_parser<Implementation>> {
private:
	// make CRTP-caused public part of the interface unusable externally by wrapping the tuples

	struct load_image_context_parameters {
		std::tuple<
			session_context,
			std::reference_wrapper<const parameters::compress::source>,
			size_t> values;

		const session_context& get_session() const {
			return std::get<session_context>(this->values);
		}

		session_context& get_session() {
			return std::get<session_context>(this->values);
		}

		const parameters::compress::source& get_source_description() const {
			return std::get<std::reference_wrapper<const parameters::compress::source>>(this->values);
		}

		size_t get_channel_num() const {
			return std::get<size_t>(this->values);
		}
	};

	struct restore_context_parameters {
		std::tuple<
			session_context,
			std::vector<std::reference_wrapper<const data_descriptor>>,
			size_t> values;

		const session_context& get_session() const {
			return std::get<session_context>(this->values);
		}

		session_context& get_session() {
			return std::get<session_context>(this->values);
		}

		const std::vector<std::reference_wrapper<const data_descriptor>>& get_data_handles() const {
			return std::get<std::vector<std::reference_wrapper<const data_descriptor>>>(this->values);
		}

		std::vector<std::reference_wrapper<const data_descriptor>>& get_data_handles() {
			return std::get<std::vector<std::reference_wrapper<const data_descriptor>>>(this->values);
		}

		size_t get_channel_num() const {
			return std::get<size_t>(this->values);
		}
	};

public:
	static void load_image(session_context&& cx, size_t channel_count, 
			const parameters::compress::source& src_spec) {
		using xbw_t = sufficient_integral<intptr_t>;
		constexpr size_t xbw_size = sizeof(xbw_t) << 3;
		if (cx.settings_session.codeword_size != xbw_size) {
			// TODO: log?
			cx.settings_session.codeword_size = xbw_size;
		}

		if (xbw_size > 64) { // TODO: magic?
			// TODO: that is not standard-conformant. log? 128-bit machines? 80-bit paltforms? throw?
		}

		// skip unnecessary output stream related instantiations for encoding chain, always assume 
		// machine word-size type
		session_parameters_parser::template parse_dwt_type<xbw_t>(
			load_image_context_parameters{ { std::move(cx), src_spec, channel_count } });
	}

	static void restore(session_context&& cx, size_t channel_count, 
			std::vector<std::reference_wrapper<const data_descriptor>>&& handles) {
		session_parameters_parser::parse_codeword_size(
			restore_context_parameters{ { std::move(cx), std::move(handles), channel_count } });
	}

private:
	template <typename dwtT>
	static std::shared_ptr<session_context> make_shared_session(session_context&& cx, size_t channel_num) {
		auto result = std::make_shared<session_context>(std::move(cx));
		result->id = generate_session_id();
		result->init_channel_contexts<dwtT>(channel_num);

		return result;
	}

public:
	// had to make members below public to allow CRTP

	template <typename xbwT, typename imgT, typename dwtT>
	static void invoke_by_argument_type(restore_context_parameters&& params) {
		Implementation<xbwT, imgT, dwtT>::decompress(
			make_shared_session<dwtT>(std::move(params.get_session()), params.get_channel_num()),
			std::move(params.get_data_handles()));
	}

	template <typename xbwT, typename imgT, typename dwtT>
	static void invoke_by_argument_type(load_image_context_parameters&& params) {
		Implementation<xbwT, imgT, dwtT>::load_image(
			make_shared_session<dwtT>(std::move(params.get_session()), params.get_channel_num()),
			params.get_source_description());
	}
};


template <typename xbwT, typename imgT, typename dwtT>
struct flow_impl {
	using xbw_t = xbwT;		// codeword type, make the last argument and assign default ptrdiff_t/size_t/intptr_t?
	using img_t = imgT;
	using dwt_t = dwtT;

	using routine_set = compression_routines<dwt_t>;

	struct forward {
		// seems there's no choise but use template hint explicitly for every nested typedef 
		// all below here. 
		// cannot make this names non-dependnet. cannot make them current instantiation. cannot 
		// establish them as template names before containing type is instantiated.
		// 
		// =(
		//

		using transform_tree = task_pool::flow_graph::root::then<decltype([](dwt_context cx) {
			return routine_set::template preprocess_image<img_t>(std::move(cx));
		})>::template split<decltype([](dwt_context cx) {
			routine_set::template transform_fragment<img_t>(std::move(cx));
		})>;

		using compress_tree = task_pool::flow_graph::root::then<decltype([](dwt_context cx) -> 
				segmentation_context<typename routine_set::subband_type, typename routine_set::segment_type> {
			return routine_set::preprocess_fragments(std::move(cx));
		})>::template then<decltype([](segmentation_context<typename routine_set::subband_type, typename routine_set::segment_type> cx) ->
				std::vector<compression_context<typename routine_set::segment_type>> {
			return routine_set::assemble_segments(std::move(cx));
		})>::template split<decltype([](compression_context<typename routine_set::segment_type> cx) -> void {
			routine_set::compress_segment(std::move(cx));
		})>;

		using transform_task_t = task_pool::flow_graph::parse<transform_tree>;
		using compress_task_t = task_pool::flow_graph::parse<compress_tree>;
	};

	struct backward {
		using decompress_tree = task_pool::flow_graph::root::then<decltype([](
				compression_context<typename routine_set::segment_type> cx) -> void {
			routine_set::template decode_segment<xbw_t>(std::move(cx));
		})>;
		
		using transform_tree = task_pool::flow_graph::root::then<decltype([](
				segmentation_context<typename routine_set::subband_type, typename routine_set::segment_type> cx) {
			return routine_set::disassemble_segments(std::move(cx));
		})>::template split<decltype([](segmentation_context<typename routine_set::subband_type, typename routine_set::segment_type> cx) -> void {
			routine_set::template restore_image<img_t>(std::move(cx));
		})>;

		using postprocess_tree = task_pool::flow_graph::root::then<decltype([](
				segmentation_context<typename routine_set::subband_type, typename routine_set::segment_type> cx) -> void {
			routine_set::template postprocess_image<img_t>(std::move(cx));
		})>;

		using decompress_task_t = task_pool::flow_graph::parse<decompress_tree>;
		using transform_task_t = task_pool::flow_graph::parse<transform_tree>;
		using postprocess_task_t = task_pool::flow_graph::parse<postprocess_tree>;
	};

	static void compress(std::shared_ptr<session_context> cx, 
			std::vector<std::reference_wrapper<const data_descriptor>>&& handles) {
		// TODO: deadlock on handles size == 0?
		std::vector<dwt_context> compress_input_contexts;

		for (const auto& handle : handles) {
			size_t z = handle.get().get_exported_data().channel_id.value();

			img_pos input_frame{};	// actual values will be deduced by routines from image data, 
				// but for further fragment-aware processing some x and y values will be needed.
			input_frame.z = z;

			dwt_context transform_context{
				generate_dwt_id(),
				cx->channel_contexts[z],
				input_frame,
				handle
			};

			dwt_context compress_context{
				generate_dwt_id(),
				cx->channel_contexts[z],
				input_frame,
				handle
			};

			compress_input_contexts.push_back(compress_context);

			cx->channel_contexts[z].descriptors.register_operation(transform_context);
			env.pool.add_tasks(typename forward::transform_task_t(std::move(transform_context)));
		}
		env.pool.execute_flow();

		// such consequtive calls to execute_flow is emulation for execution sync. Once sync task type is 
		// added, there will be no need to feed the execution_pool queue explicitly several times.

		for (auto&& context : compress_input_contexts) {
			ptrdiff_t z = context.frame.z;
			cx->channel_contexts[z].descriptors.register_operation(context);
			// TODO: what about wrapper like {context.channel_cx.descriptors.register_operation} for context param?

			env.pool.add_tasks(typename forward::compress_task_t(std::move(context)));
		}
		env.pool.execute_flow();
	}

	static void decompress(std::shared_ptr<session_context> cx, 
			std::vector<std::reference_wrapper<const data_descriptor>>&& handles) {
		for (ptrdiff_t id = 0; id < handles.size(); ++id) {
			size_t z = handles[id].get().get_exported_data().channel_id.value();
			compression_context<typename routine_set::segment_type> context{
				generate_compression_id(),
				cx->channel_contexts[z],
				std::make_unique<segment<typename routine_set::segment_type>>(),
				handles[id]
			};
			
			// TODO: MAJOR: that makes implementation incompatible with multichannel images. Having set of 
			// generalized segments (on unspecified media), it's not possible to guess where a subsequent 
			// channel starts, until segment headers are parsed. As a result, segment id's are assigned 
			// during segment header pre-parsing, and stored in type-erased descriptors. 
			// But segment id is needed to get corresponding segmentation settings from session/channel 
			// context during segment decoding. That code in routines.tpp does not obtain strong typed 
			// segment descriptor, but uses type-erased source interface.
			// 
			// As it is implemented now, for any subseqent channel wrong segmentation settings are obtained, 
			// because id assigned below is linear and does not wrap around the channel size in segments.
			// 
			context.segment_data->id = id; 

			cx->channel_contexts[z].descriptors.register_operation(context);
			env.pool.add_tasks(typename backward::decompress_task_t(std::move(context))); // TODO: this way to populate pool queue 
				// is expensive...
		}
		env.pool.execute_flow();

		// such consequtive calls to execute_flow is emulation for execution sync. Once sync task type is 
		// added, there will be no need to feed the execution_pool queue explicitly several times.

		for (ptrdiff_t i = 0; i < cx->channel_contexts.size(); ++i) {
			segmentation_context<typename routine_set::subband_type, typename routine_set::segment_type> context{
				generate_segmentation_id(), 
				cx->channel_contexts[i], 
				{}, 
				nullptr,
				{0}
			};
			cx->channel_contexts[i].descriptors.register_operation(context);
			env.pool.add_tasks(typename backward::transform_task_t(std::move(context)));
		}
		env.pool.execute_flow();

		// such consequtive calls to execute_flow is emulation for execution sync. Once sync task type is 
		// added, there will be no need to feed the execution_pool queue explicitly several times.

		for (ptrdiff_t i = 0; i < cx->channel_contexts.size(); ++i) {
			segmentation_context<typename routine_set::subband_type, typename routine_set::segment_type> context{
				generate_segmentation_id(),
				cx->channel_contexts[i],
				{},
				nullptr,
				{0}
			};

			cx->channel_contexts[i].descriptors.register_operation(context);
			env.pool.add_tasks(typename backward::postprocess_task_t(std::move(context)));
		}
		env.pool.execute_flow();
	}

	static void load_image(std::shared_ptr<session_context> cx, const parameters::compress::source& src_spec) {
		std::vector<img_meta> channel_stats;
		std::vector<size_t> channel_bdepths;
		std::vector<std::reference_wrapper<const data_descriptor>> descriptors;

		channel_stats.reserve(cx->channel_contexts.size());
		channel_bdepths.reserve(cx->channel_contexts.size());
		descriptors.reserve(cx->channel_contexts.size());

		bool valid = true;
		for (auto& channel_cx : cx->channel_contexts) {
			auto channel_data = compress::load_image_channel<img_t>(src_spec, channel_cx.channel_index);
			channel_stats.push_back(channel_data.get_meta()); 
			valid &= (channel_stats.back().depth == 1);
			// TODO: calculate dynamic bdepth?
			channel_bdepths.push_back(compute_image_bdepth(channel_data));
			// TODO: it appears bdepth should be property of channel, not session

			if (cx->settings_session.transpose) {
				channel_data = channel_data.transpose();
			}

			const data_descriptor& descriptor = cx->data_registry.put_input(
				image_memory_descriptor<img_t>(std::move(channel_data), channel_cx.channel_index));
			descriptors.push_back(descriptor);
		}

		auto it = std::adjacent_find(channel_stats.cbegin(), channel_stats.cend(),
			[](const img_meta& lhs, const img_meta& rhs) -> bool {
				return (lhs.width != rhs.width) | (lhs.height != rhs.height);
			});

		valid &= !channel_stats.empty();
		valid &= (it == channel_stats.cend());
		if (!valid) {
			// TODO: throw, invalid image
		}

		auto padded_dims = padded_image_dimensions(channel_stats.back().width, channel_stats.back().height);

		valid &= (cx->settings_session.img_width == channel_stats.back().width);
		valid &= (cx->settings_session.rows_pad_count == (padded_dims.second - channel_stats.back().height));
		for (auto item : channel_bdepths) {
			valid &= (cx->settings_session.pixel_bdepth == item);
		}

		if (!valid) {
			// so what? we can just override properly and let it go
			// log?
			cx->settings_session.pixel_bdepth = channel_bdepths.back();
		}

		compress(std::move(cx), std::move(descriptors));
	}
};

}
