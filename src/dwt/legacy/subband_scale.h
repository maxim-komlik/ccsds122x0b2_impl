#include <array>
#include <vector>

#include "utils.h"

// TODO: MAJOR: Oops, subband weights are constant for the whole image
// as specified in 3.9.4 and 4.2.5

// subband_scale
class scaling_set {
public:
	enum extension_policy {
		begin,
		end
	};

private:
	using shifts_type = std::array<size_t, 10>;

	std::vector<shifts_type> shifts;
	extension_policy policy;

	ptrdiff_t last_segment_index = 0;
	ptrdiff_t expansion_start_index = 0;
	ptrdiff_t expansion_finish_index = 0;

	ptrdiff_t current_segment_index = 0;

public:
	void set_segment_count(size_t segment_count) {
		this->last_segment_index = relu(((ptrdiff_t)(segment_count)) - 1);
		this->recalculate();
	}

	void set_current_index(ptrdiff_t index) {
		this->current_segment_index = index;
	}

	void set_policy(extension_policy policy) {
		this->policy = policy;
		this->recalculate();
	}

	void set_shifts_collection(std::vector<shifts_type>&& shifts) {
		// TODO: runtime handling of vector size less than 3
		this->shifts = std::move(shifts);
		this->current_segment_index = 0;
		this->recalculate();
	}

	void set_shifts_collection(const std::vector<shifts_type>& shifts) {
		std::vector<shifts_type> temp(shifts);
		this->set_shifts_collection(std::move(temp));
	}

	extension_policy get_policy() const {
		return this->policy;
	}

	shifts_type get_next() {
		bool first = this->current_segment_index <= 0;
		bool last = this->current_segment_index >= this->last_segment_index;
		bool past_extension_begin = this->current_segment_index >= expansion_start_index;
		bool past_extension_end = this->current_segment_index >= expansion_finish_index;

		ptrdiff_t base_mask = -first;
		ptrdiff_t mask1 = -past_extension_begin;
		ptrdiff_t mask2 = -past_extension_end;
		ptrdiff_t mask3 = -last;

		ptrdiff_t base = this->current_segment_index & base_mask;
		ptrdiff_t offset1 = (this->current_segment_index - this->expansion_start_index) & mask1;
		ptrdiff_t offset2 = (this->current_segment_index - this->expansion_finish_index) & mask2;
		ptrdiff_t offset3 = (this->current_segment_index - this->last_segment_index) & mask3;

		ptrdiff_t shifts_index = base - offset1 + offset2 - offset3;

		++(this->current_segment_index);
		return this->shifts[shifts_index];
	}

private:
	void recalculate() {
		ptrdiff_t last_shifts_item_index = this->shifts.size() - 1;
		ptrdiff_t backward_offset = 0;

		switch (this->policy) {
			case extension_policy::begin: {
				backward_offset = 1;
			}
			case extension_policy::end: {
				backward_offset = last_shifts_item_index - 1;
			}
			default:
				break;
		}

		this->expansion_start_index = last_shifts_item_index - backward_offset;
		this->expansion_finish_index = this->last_segment_index - backward_offset;
	}
};


// begin:
// {0} -> 0 / first
// {1} -> 1, 2, 3, ....
// {2} -> last
// 
// {0} -> 0 / first
// {1} -> 1
// {2} -> 2, 3, 4, ....
// {3} -> last
// 
// end:
// {0} -> 0 / first
// {1} -> 1, 2, 3, ....
// {2} -> last
// 
// {0} -> 0 / first
// {1} -> 1, 2, 3, ....
// {2} -> one but last
// {3} -> last
//
