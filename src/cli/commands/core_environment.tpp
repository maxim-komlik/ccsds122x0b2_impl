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
#include "io/image.hpp"

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

	struct compress_context_parameters {
		std::tuple<
			std::shared_ptr<session_context>,
			std::vector<std::reference_wrapper<const data_descriptor>>> values;

		const session_context& get_session() const {
			return *std::get<std::shared_ptr<session_context>>(this->values);
		}

		session_context& get_session() {
			return *std::get<std::shared_ptr<session_context>>(this->values);
		}

		std::shared_ptr<session_context>& get_session_ptr() {
			return std::get<std::shared_ptr<session_context>>(this->values);
		}

		const std::vector<std::reference_wrapper<const data_descriptor>>& get_data_handles() const {
			return std::get<std::vector<std::reference_wrapper<const data_descriptor>>>(this->values);
		}

		std::vector<std::reference_wrapper<const data_descriptor>>& get_data_handles() {
			return std::get<std::vector<std::reference_wrapper<const data_descriptor>>>(this->values);
		}
	};

	struct restore_context_parameters {
		std::tuple<
			std::shared_ptr<session_context>,
			std::vector<std::reference_wrapper<const data_descriptor>>> values;

		const session_context& get_session() const {
			return *std::get<std::shared_ptr<session_context>>(this->values);
		}

		session_context& get_session() {
			return *std::get<std::shared_ptr<session_context>>(this->values);
		}

		std::shared_ptr<session_context>& get_session_ptr() {
			return std::get<std::shared_ptr<session_context>>(this->values);
		}

		const std::vector<std::reference_wrapper<const data_descriptor>>& get_data_handles() const {
			return std::get<std::vector<std::reference_wrapper<const data_descriptor>>>(this->values);
		}

		std::vector<std::reference_wrapper<const data_descriptor>>& get_data_handles() {
			return std::get<std::vector<std::reference_wrapper<const data_descriptor>>>(this->values);
		}
	};

public:
	static void compress(std::shared_ptr<session_context> cx,
			std::vector<std::reference_wrapper<const data_descriptor>>&& handles) {
		using xbw_t = sufficient_integral<intptr_t>;
		constexpr size_t xbw_size = sizeof(xbw_t) << 3;
		if (cx->settings_session.codeword_size != xbw_size) {
			// TODO: log?
			cx->settings_session.codeword_size = xbw_size;
		}

		if (xbw_size > 64) { // TODO: magic?
			// TODO: that is not standard-conformant. log? 128-bit machines? 80-bit paltforms? throw?
		}

		// skip unnecessary output stream related instantiations for encoding chain, always assume 
		// machine word-size type
		session_parameters_parser::template parse_dwt_type<xbw_t>(
			compress_context_parameters{ { std::move(cx), std::move(handles) } });
	}

	static void restore(std::shared_ptr<session_context> cx,
			std::vector<std::reference_wrapper<const data_descriptor>>&& handles) {
		session_parameters_parser::parse_codeword_size(
			restore_context_parameters{ { std::move(cx), std::move(handles) } });
	}

private:
	template <typename dwtT>
	static void finish_session_initialization(session_context& cx) {
		cx.id = generate_session_id();

		for (auto& channel_cx : cx.channel_contexts) {
			channel_cx.init_compression_data<dwtT>();
		}
	}

public:
	// had to make members below public to allow CRTP

	template <typename xbwT, typename imgT, typename dwtT>
	static void invoke_by_argument_type(restore_context_parameters&& params) {
		finish_session_initialization<dwtT>(params.get_session());
		Implementation<xbwT, imgT, dwtT>::decompress(
			std::move(params.get_session_ptr()),
			std::move(params.get_data_handles()));
	}

	template <typename xbwT, typename imgT, typename dwtT>
	static void invoke_by_argument_type(compress_context_parameters&& params) {
		finish_session_initialization<dwtT>(params.get_session());
		Implementation<xbwT, imgT, dwtT>::compress(
			std::move(params.get_session_ptr()),
			std::move(params.get_data_handles()));
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
			size_t z = handle.get().get_exported_data().get_channel_id();

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
		for (const auto& handle : handles) {
			size_t channel_id = handle.get().get_exported_data().get_channel_id();
			compression_context<typename routine_set::segment_type> context{
				generate_compression_id(),
				cx->channel_contexts[channel_id],
				std::make_unique<segment<typename routine_set::segment_type>>(),
				handle
			};

			context.segment_data->id = handle.get().get_exported_data().get_object_id();

			cx->channel_contexts[channel_id].descriptors.register_operation(context);
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
};

}
