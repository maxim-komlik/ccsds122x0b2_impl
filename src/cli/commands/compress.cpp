#include <vector>
#include <tuple>
#include <array>
#include <optional>		// see anonimous namespace
#include <utility>
#include <memory>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <cstddef>

#include "dwt/bitmap.tpp"
#include "utils.hpp"

#include "common/constant.hpp"
#include "dwt/constant.hpp"
#include "bpe/constant.hpp"
#include "dwt/utility.hpp"

#include "io/io_settings.hpp"
#include "io/io_contexts.hpp"
#include "io/session_context.hpp"
#include "io/tasking.hpp"
#include "core/samples/routines.tpp"

#include "compress.hpp"
#include "load_image.tpp"

namespace cli::command::compress {

namespace {

	storage_type parse_storage_type(params::dst_type value);
	dwt_type_t parse_dwt_type(params::dwt_type type);
	shifts_t parse_shifts(const std::optional<params::shifts>& value);
	std::pair<size_t, compression_settings> parse_compression_settings(const params::stream::parameters_set& value);
	std::pair<size_t, segment_settings> parse_segment_settings(const params::segment::parameters_set& value);

	// TODO: merge with validation implementation
	struct image_description {
		size_t width;
		size_t height;
		size_t channel_num;
		size_t static_bdepth;
		std::optional<bool> if_signed;
	};

	image_description get_image_description(const params::source& parameters) {
		switch (parameters.type) {
		case params::src_type::generate: {

			const params::generate::generator& gen_params =
				std::get<params::generate::generator>(parameters.parameters);

			return image_description{
				.width = gen_params.dims.width,
				.height = gen_params.dims.height,
				.channel_num = gen_params.dims.depth,
				.static_bdepth = gen_params.bdepth,
				.if_signed = gen_params.pixel_signed
			};

			break;
		}
		default: {

		}
		}

		// TODO: C++23 std::unreachable?
	}
}


// implementation section:


struct execution_environment {
	task_pool pool;
};

static execution_environment env;

template <template<typename /*ibwT/obwT*/, typename /*imgT*/, typename /*dwtT*/> typename Implementation>
struct session_parameters_parser: protected session_parameters_parser_base<session_parameters_parser<Implementation>> {
private:
	// make CRTP-caused public part of the interface unusable externally by wrapping the tuples

	struct load_image_context_parameters {
		std::tuple<
			session_context,
			std::reference_wrapper<const params::source>,
			size_t> values;

		const session_context& get_session() const {
			return std::get<session_context>(this->values);
		}

		session_context& get_session() {
			return std::get<session_context>(this->values);
		}

		const params::source& get_source_description() const {
			return std::get<std::reference_wrapper<const params::source>>(this->values);
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
			const params::source& src_spec) {
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
				segmentation_context<routine_set::subband_type, routine_set::segment_type> {
			return routine_set::preprocess_fragments(std::move(cx));
		})>::template then<decltype([](segmentation_context<routine_set::subband_type, routine_set::segment_type> cx) ->
				std::vector<compression_context<routine_set::segment_type>> {
			return routine_set::assemble_segments(std::move(cx));
		})>::template split<decltype([](compression_context<routine_set::segment_type> cx) -> void {
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
		})>::template split<decltype([](segmentation_context<routine_set::subband_type, routine_set::segment_type> cx) -> void {
			routine_set::template restore_image<img_t>(std::move(cx));
		})>;

		using postprocess_tree = task_pool::flow_graph::root::then<decltype([](
				segmentation_context<routine_set::subband_type, routine_set::segment_type> cx) -> void {
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
			env.pool.add_tasks(forward::transform_task_t(std::move(transform_context)));
		}
		env.pool.execute_flow();

		// such consequtive calls to execute_flow is emulation for execution sync. Once sync task type is 
		// added, there will be no need to feed the execution_pool queue explicitly several times.

		for (auto&& context : compress_input_contexts) {
			ptrdiff_t z = context.frame.z;
			cx->channel_contexts[z].descriptors.register_operation(context);
			// TODO: what about wrapper like {context.channel_cx.descriptors.register_operation} for context param?

			env.pool.add_tasks(forward::compress_task_t(std::move(context)));
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
				std::make_unique<segment<routine_set::segment_type>>(),
				handles[id]
			};
			context.segment_data->id = id;
			cx->channel_contexts[z].descriptors.register_operation(context);
			env.pool.add_tasks(backward::decompress_task_t(std::move(context))); // TODO: this way to populate pool queue 
				// is expensive...
		}
		env.pool.execute_flow();

		// such consequtive calls to execute_flow is emulation for execution sync. Once sync task type is 
		// added, there will be no need to feed the execution_pool queue explicitly several times.

		for (ptrdiff_t i = 0; i < cx->channel_contexts.size(); ++i) {
			segmentation_context<routine_set::subband_type, routine_set::segment_type> context{
				generate_segmentation_id(), 
				cx->channel_contexts[i], 
				{}, 
				nullptr,
				{0}
			};
			cx->channel_contexts[i].descriptors.register_operation(context);
			env.pool.add_tasks(backward::transform_task_t(std::move(context)));
		}
		env.pool.execute_flow();

		// such consequtive calls to execute_flow is emulation for execution sync. Once sync task type is 
		// added, there will be no need to feed the execution_pool queue explicitly several times.

		for (ptrdiff_t i = 0; i < cx->channel_contexts.size(); ++i) {
			segmentation_context<routine_set::subband_type, routine_set::segment_type> context{
				generate_segmentation_id(),
				cx->channel_contexts[i],
				{},
				nullptr,
				{0}
			};

			cx->channel_contexts[i].descriptors.register_operation(context);
			env.pool.add_tasks(backward::postprocess_task_t(std::move(context)));
		}
		env.pool.execute_flow();
	}

	static void load_image(std::shared_ptr<session_context> cx, const params::source& src_spec) {
		std::vector<img_meta> channel_stats;
		std::vector<std::reference_wrapper<const data_descriptor>> descriptors;

		channel_stats.reserve(cx->channel_contexts.size());
		descriptors.reserve(cx->channel_contexts.size());

		bool valid = true;
		for (auto& channel_cx : cx->channel_contexts) {
			auto channel_data = load_image_channel<img_t>(src_spec, channel_cx.channel_index);
			channel_stats.push_back(channel_data.get_meta()); 
			valid &= (channel_stats.back().depth == 1);
			// TODO: calculate dynamic bdepth?
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
		if (!valid) {
			// so what? we can just override properly and let it go
			// log?
		}

		compress(std::move(cx), std::move(descriptors));
	}
};

void compress_command_handler(const params::compress_command& parameters) {
	// image data is anyway somehow compressed when stored, there's no reason to try to load one bitmap 
	// type and then cast it to bitmap with some extended underlying type to satisfy dwt requirements
	// 
	// so we need to parse bdepth and dwt type according to compressor_types, and then use size_t as 
	// codeword type
	//

	// parse dst first to create registry?
	// then set all session parameters
	// then parse session parameters to get type mapping
	// then load image data
	// then validate image data and validate dependent session parameters
	// then call target command implementation
	//

	parameters.dst_params.protocol; // TODO: add support for protocol selection
	parameters.dst_params.parameters; // TODO:

	io_data_registry registry(parse_storage_type(parameters.dst_params.type));

	image_description img_desc = get_image_description(parameters.src_params);
	size_t height_padding = padded_image_dimensions(img_desc.width, img_desc.height).second - img_desc.height;

	parameters.dwt_params.frame;	// TODO:

	session_context cx(registry);
	cx.settings_session = session_settings {
		.dwt_type = parse_dwt_type(parameters.dwt_params.type),
		.img_width = img_desc.width,
		.pixel_bdepth = img_desc.static_bdepth, // TODO:
		.signed_pixel = parameters.img_signed,
		.transpose = parameters.img_transpose,
		.rows_pad_count = height_padding,
		.codeword_size = sizeof(uintptr_t) << 3,	// TODO: ?
		.custom_shifts = parameters.dwt_params.shifts.has_value(),
		.shifts = parse_shifts(parameters.dwt_params.shifts)
	};

	cx.compr_settings.push_back(parse_compression_settings(parameters.stream_params.first));
	for (const auto& item : parameters.stream_params.subsequent) {
		cx.compr_settings.push_back(parse_compression_settings(item));
	}

	cx.seg_settings.push_back(parse_segment_settings(parameters.segment_params.first));
	for (const auto& item : parameters.segment_params.subsequent) {
		cx.seg_settings.push_back(parse_segment_settings(item));
	}

	session_parameters_parser<flow_impl>::load_image(std::move(cx), img_desc.channel_num, parameters.src_params);

	// TODO: handle output data somehow?
}


namespace {

	storage_type parse_storage_type(params::dst_type value) {
		constexpr std::pair<params::dst_type, storage_type> map_content[]{
			{ params::dst_type::file, storage_type::file },
			{ params::dst_type::memory, storage_type::memory }
		};
		constexpr std::array storage_mapping = std::to_array(map_content);

		auto storage_it = std::find_if(storage_mapping.cbegin(), storage_mapping.cend(),
			[&](const auto& pair) -> bool {
				return pair.first == value;
			});

		if (storage_it == storage_mapping.cend()) {
			// TODO: throw: not supported
		}

		return storage_it->second;
	}

	dwt_type_t parse_dwt_type(params::dwt_type type) {
		constexpr std::pair<params::dwt_type, dwt_type_t> map_content[] = {
			{params::dwt_type::integer, dwt_type_t::idwt},
			{params::dwt_type::fp, dwt_type_t::fdwt}
		};
		constexpr std::array storage_mapping = std::to_array(map_content);

		auto storage_it = std::find_if(storage_mapping.cbegin(), storage_mapping.cend(),
			[&](const auto& pair) -> bool {
				return pair.first == type;
			});

		if (storage_it == storage_mapping.cend()) {
			// TODO: throw: not supported
		}

		return storage_it->second;
	}

	shifts_t parse_shifts(const std::optional<params::shifts>& value) {
		if (value) {
			return value->values;
		}
		return { 0 };
	}

	std::pair<size_t, compression_settings> parse_compression_settings(const params::stream::parameters_set& value) {
		bool bpe_truncated = false;
		// TODO: refactor early stream termination flag. It happens to be used for byte limit by 
		// protocol, but shouldn't. Likely member function aggregator needed

		return {
			value.id,
			{
				.seg_byte_limit = value.byte_limit,
				.use_fill = value.fill,
				.early_termination = bpe_truncated,
				.DC_stop = value.DC_stop,
				.bplane_stop = value.bplane_stop,
				.stage_stop = value.stage_stop
			}
		};
	}

	std::pair<size_t, segment_settings> parse_segment_settings(const params::segment::parameters_set& value) {
		return {
			value.id,
			{
				.size = value.size,
				.heuristic_quant_DC = value.heuristic_DC,
				.heuristic_bdepth_AC = value.heuristic_AC_bdepth
			}
		};
	}
}

}
