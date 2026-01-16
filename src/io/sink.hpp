#pragma once

#include "bpe/obitwrapper.tpp"
#include "bpe/bpe.tpp"

#include "ccsds_protocol.hpp"
#include "io_settings.hpp"

enum class sink_type {
	memory, 
	file, 
	network
};

template <typename T>
class sink {
	// ABC, derived owns obitwrapper. Provides the interface only.
public:
	virtual ~sink() = default;
	void setup_session(const segment_settings& seg_params, const compression_settings& compr_params, 
			bool override_seg_params = false, bool override_compr_params = false);

	virtual void finish_session() = 0;

	virtual obitwrapper<T>& get_bitwrapper() = 0;
	virtual void handle_early_termination() = 0;
	virtual bool if_terminated_early() const = 0;
	virtual bool if_started() const = 0;                     
	virtual bool if_completed() const = 0;

	virtual sink_type get_type() const = 0;
	virtual bool if_supports_header_override() const { return false; }
	virtual bool if_supports_delayed_truncation() const { return false; }

	virtual std::span<std::byte> get_overridable_header_area(size_t target_header_size) = 0;
	virtual void commit_overridable_header(std::span<std::byte>) = 0;

protected:
	sink(sink&& other) noexcept = default;
	sink& operator=(sink&& other) noexcept = default;
	 
	sink(const sink& other) noexcept = default;
	sink& operator=(const sink& other) noexcept = default;

	sink() = default;

	// interface requirements:
	void enable_segment_truncation(size_t byte_limit, bool use_fill = false);
	void disable_segment_truncation();
	virtual void set_truncation_params(size_t byte_limit, bool use_fill) = 0;

	virtual void apply_params(const segment_settings& seg_params, const compression_settings& compr_params) = 0;

private:
	virtual void init_resources() {};
};

template <typename T>
void sink<T>::disable_segment_truncation() {
	constexpr size_t max_segment_size_bytes = ~((-1) << 27);
	this->set_truncation_params(max_segment_size_bytes, false);
}

template <typename T>
void sink<T>::enable_segment_truncation(size_t byte_limit, bool use_fill) {
	this->set_truncation_params(byte_limit, use_fill);
}

template <typename T>
void sink<T>::setup_session(const segment_settings& seg_params, const compression_settings& compr_params,
		bool override_seg_params, bool override_compr_params) {
	// // should not override protocol parameters if this is initial segment.
	// // caller should set bool params to false in such case.
	// if (override_seg_params) {
	// 	this->get_protocol().set_segment_params(seg_params);
	// }
	// if (override_compr_params) {
	// 	this->get_protocol().set_compression_params(compr_params);
	// }

	this->init_resources();
	this->apply_params(seg_params, compr_params);
}
