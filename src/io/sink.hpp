#pragma once

#include "bpe/obitwrapper.tpp"
#include "bpe/bpe.tpp"

#include "ccsds_protocol.hpp"
#include "io_settings.hpp"
#include "constant.hpp"

template <typename T>
class sink {
	// ABC, derived owns obitwrapper. Provides the interface only.
public:
	virtual ~sink() = default;
	void setup_session();

	virtual void finish_session(bool fill = false) = 0;

	virtual obitwrapper<T>& get_bitwrapper() = 0;
	virtual void handle_early_termination() = 0;
	virtual bool if_terminated_early() const = 0;
	virtual bool if_started() const = 0;                     
	virtual bool if_completed() const = 0;

	virtual storage_type get_type() const = 0;
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

	// // interface requirements:
	// void enable_segment_truncation(size_t byte_limit, bool use_fill = false);
	// void disable_segment_truncation();
	// virtual void set_truncation_params(size_t byte_limit, bool use_fill) = 0;

	// virtual void apply_params(const segment_settings& seg_params, const compression_settings& compr_params) = 0;

private:
	virtual void init_resources() {};
};

template <typename T>
void sink<T>::setup_session() {
	this->init_resources();
	// this->apply_params(seg_params, compr_params);
}


template <typename T>
class source {
public:
	virtual ~source() = default;

	virtual ibitwrapper<T>& get_bitwrapper() = 0;
	virtual storage_type get_type() const = 0;
	virtual void restart() = 0;

	void setup_session() { this->init_resources(); }

protected:
	source(source&& other) noexcept = default;
	source& operator=(source&& other) noexcept = default;

	source(const source& other) noexcept = default;
	source& operator=(const source& other) noexcept = default;

	source() = default;

private:
	virtual void init_resources() {};
};
