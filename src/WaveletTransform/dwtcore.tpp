#pragma once
#include <type_traits>

// TODO: consider passing alligmnent value as template parameter
template <typename T = unsigned int>
struct _dwtcore_i {
	void fwd(T const * src, T* hdst, T* ldst, size_t count) {
		// requires src to have allocated offset ranges to the left and to the rigth: [-2, 62]
		T const * alligned_src = src - 2;
		constexpr size_t vector_step = 16;
		constexpr size_t move_prepipeline_boundary = 2 * /*2 **/ vector_step;
		constexpr size_t h_prepipeline_boundary = 1 * vector_step;
		constexpr size_t l_prepipeline_boundary = 0 * vector_step;			// TODO: check if needed
		constexpr size_t move_postpipeline_boundary = 0 * 2 * vector_step;	// TODO: check if needed
		constexpr size_t h_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed
		constexpr size_t l_postpipeline_boundary = 1 * vector_step;			// TODO: check if needed

		// load pipeline 
		for (size_t i = 0; i < move_prepipeline_boundary; i += /*2 **/ vector_step) {
			for (size_t j = 0; j < /*2 **/ vector_step; ++j /* += 2*/) {
				// move even to lowpass buffer, odd to highpass buffer
				// Intermediate results for highpass are added to highpass
				// buffer, but input coefficents for highpass intermediate
				// are even and therefore are fetched from lowpass buffer.
				ldst[i + j] = alligned_src[(i + j) << 1];
				hdst[i + j] = alligned_src[((i + j) << 1) + 3];		// index -1 is never used, start from +1
			}
		}
		for (size_t i = 0; i < h_prepipeline_boundary; i += vector_step) {
			for (size_t j = 0; j < vector_step; ++j) {
				T htemp = (-ldst[i + j] + ldst[i + j + 1] + ldst[i + j + 2] - ldst[i + j + 3]) >> 3;
				hdst[i + j] -= ((ldst[i + j + 1] + ldst[i + j + 2]) + htemp + 1) >> 1;
			}
		}

		T const * p_move_src = alligned_src + move_prepipeline_boundary * 2;
		T* p_move_hdst = hdst + move_prepipeline_boundary/* / 2*/;
		T* p_move_ldst = ldst + move_prepipeline_boundary/* / 2*/;
		T const * p_hsrc = ldst + h_prepipeline_boundary;
		T* p_hdst = hdst + h_prepipeline_boundary;
		T const * p_lsrc = hdst;
		T* p_ldst = ldst;
		size_t p_count = count - vector_step;	// TODO: ensure alignment, likely need relu for correctness

		// run pipeline
		for (size_t i = 0; i < p_count; i += vector_step) {
			for (size_t j = 0; j < vector_step; ++j) {
				p_move_ldst[i + j] = p_move_src[(i + j) << 1];
				p_move_hdst[i + j] = p_move_src[((i + j) << 1) + 3];	// still start from +1
				T htemp = (-p_hsrc[i + j] + p_hsrc[i + j + 1] + p_hsrc[i + j + 2] - p_hsrc[i + j + 3]) >> 3;
				p_hdst[i + j] -= ((p_hsrc[i + j + 1] + p_hsrc[i + j + 2]) + htemp + 1) >> 1;
				p_ldst[i + j] = p_ldst[i + j + 1] - ((-p_lsrc[i + j - 1] - p_lsrc[i + j] + 2) >> 2);
			}
		}

		// free pipeline
		for (size_t i = p_count; i < count; i += vector_step) {
			for (size_t j = 0; j < vector_step; ++j) {
				p_ldst[i + j] = p_ldst[i + j + 1] - ((-p_lsrc[i + j - 1] - p_lsrc[i + j] + 2) >> 2);
			}
		}
		return;
	}

	void bwd(T const * hsrc, T const * lsrc, T* dst, size_t count) {
		constexpr size_t vector_step = 1 << 4;
		constexpr size_t move_prepipeline_boundary = 0 * vector_step;		// TODO: check if needed
		constexpr size_t h_prepipeline_boundary = 1 * vector_step;
		constexpr size_t l_prepipeline_boundary = 2 * vector_step;
		constexpr size_t move_postpipeline_boundary = 1 * vector_step;		// TODO: check if needed
		constexpr size_t h_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed
		constexpr size_t l_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed

		T hbuffer[vector_step];

		// load pipeline 
		for (size_t j = 0; j < vector_step; ++j) {
			// restores starting from 2j-2
			dst[j] = lsrc[j - 1] + ((-hsrc[j - 2] - hsrc[j - 1] + 2) >> 2);
		}
		for (size_t i = vector_step; i < l_prepipeline_boundary; i += 2 * vector_step) {	// TODO: verify dst and src indexing
			for (size_t j = 0; j < vector_step; ++j) {
				// restores starting from 2j-2
				dst[i + j] = lsrc[i + j - 1] + ((-hsrc[i + j - 2] - hsrc[i + j - 1] + 2) >> 2);
				dst[i + j + vector_step] = dst[i + j];
			}
		}
		for (size_t i = 0; i < h_prepipeline_boundary; i += vector_step) {
			for (size_t j = 0; j < vector_step; ++j) {
				// restores starting from 2j+1
				T htemp = (-dst[i + j] + dst[i + j + 1] + dst[i + j + 2] - dst[i + j + 3]) >> 3;
				hbuffer[j] = hsrc[i + j] + (((dst[i + j + 1] + dst[i + j + 2]) + htemp + 1) >> 1);
			}
		}

		T const * p_move_hsrc = dst + 1;
		T* p_move_dst = dst;
		T const * p_hsrc = dst + 2 * h_prepipeline_boundary;
		T const * p_hacc = hsrc + h_prepipeline_boundary;
		T const * p_lsrc = hsrc + l_prepipeline_boundary;
		T const * p_lacc = lsrc + l_prepipeline_boundary;
		T* p_ldst = dst + l_prepipeline_boundary + vector_step;
		size_t p_count = ((count - 1) & (~(vector_step - 1))) - vector_step;	// TODO: verify alignment

		// run pipeline
		size_t i = 0, k = 0;
		for (/*size_t i = 0, k = 0*/; i < p_count; i += vector_step, k += 2 * vector_step) {	// i is for sources, k is for dst
			for (size_t j = vector_step; j > 0; --j) {
				p_move_dst[k + (j << 1) - 2] = p_move_hsrc[k + j - 1];		// dst + 1 - 1 == dst
				p_move_dst[k + (j << 1) - 1] = hbuffer[j - 1];
			}
			for (size_t j = 0; j < vector_step; ++j) {
				p_ldst[k + j] = p_lacc[i + j - 1] + ((-p_lsrc[i + j - 2] - p_lsrc[i + j - 1] + 2) >> 2);
				p_ldst[k + j + vector_step] = p_ldst[k + j];
				T htemp = (-p_hsrc[k + j] + p_hsrc[k + j + 1] + p_hsrc[k + j + 2] - p_hsrc[k + j + 3]) >> 3;
				hbuffer[j] = p_hacc[i + j] + (((p_hsrc[k + j + 1] + p_hsrc[k + j + 2]) + htemp + 1) >> 1);
			}
		}

		// free pipeline
		for (size_t j = vector_step; j > 0; --j) {
			p_move_dst[k + (j << 1) - 2] = p_move_hsrc[k + j - 1];		// dst + 1 - 1 == dst
			p_move_dst[k + (j << 1) - 1] = hbuffer[j - 1];
		}

		for (size_t j = 0; j < vector_step; ++j) {
			p_ldst[k + j] = p_lacc[i + j - 1] + ((-p_lsrc[i + j - 2] - p_lsrc[i + j - 1] + 2) >> 2);
			T htemp = (-p_hsrc[k + j] + p_hsrc[k + j + 1] + p_hsrc[k + j + 2] - p_hsrc[k + j + 3]) >> 3;
			hbuffer[j] = p_hacc[i + j] + (((p_hsrc[k + j + 1] + p_hsrc[k + j + 2]) + htemp + 1) >> 1);
		}
		k += 2 * vector_step;
		for (size_t j = vector_step; j > 0; --j) {
			p_move_dst[k + (j << 1) - 2] = p_move_hsrc[k + j - 1];		// dst + 1 - 1 == dst
			p_move_dst[k + (j << 1) - 1] = hbuffer[j - 1];
		}
		return;
	}

	inline void extfwd(T* begining) {
		begining[-1] = begining[1];
		begining[-2] = begining[2];
		begining[-4] = begining[4];
	}

	inline void rextfwd(T* ending) {
		ending[0] = ending[-2];
		ending[2] = ending[-4];
	}

	inline void exthbwd(T* begining) {
		begining[-1] = begining[0];
		begining[-2] = begining[1];
	}

	inline void rexthbwd(T* ending) {
		ending[0] = ending[-2];
		ending[1] = ending[-3];
	}

	inline void extlbwd(T* begining) {
		begining[-1] = begining[1];
	}

	inline void rextlbwd(T* ending) {
		ending[0] = ending[-1];
		ending[1] = ending[-2];
	}

	// should be called before backward DWT to correct first element of ldst
	inline void corrlfwd(T const * src, T const * hdst, T* ldst) {
		ldst[0] = src[0] - ((-hdst[0] + 1) >> 1);
	}

	inline void corrhfwd(T const * src, T* hdst, T const * ldst) { }
};

template <typename T, bool is_int>
struct _dwtcore;

template <typename T>
struct _dwtcore<T, true> {
	using type = typename _dwtcore_i<T>;
};

template <typename T>
using dwtcore = typename _dwtcore<T, std::is_integral<T>::value>::type;

namespace target {
	template <typename T = unsigned int>
	struct _dwtcore_i {
		void fwd(T* src, T* hdst, T* ldst, size_t count) {
			for (size_t i = 0; i < count; ++i) {
				hdst[i] = src[2 * i + 1] - ((src[2 * i] + src[2 * i + 2] +
					((src[2 * i] + src[2 * i + 2] - src[2 * i - 2] - src[2 * i + 4]) >> 3) + 1) >> 1);
			}
			for (size_t i = 0; i < count; ++i) {
				ldst[i] = src[2 * i] - ((-hdst[i - 1] - hdst[i] + 2) >> 2);
			}
			return;
		}

		void bwd(T* hsrc, T* lsrc, T* dst, size_t count) {
			for (size_t i = 0; i < count; ++i) {
				dst[2 * i] = lsrc[i] + ((-hsrc[i - 1] - hsrc[i] + 2) >> 2);
			}
			for (size_t i = 0; i < count; ++i) {
				dst[2 * i + 1] = hsrc[i] + ((dst[2 * i] + dst[2 * i + 2] +
					((dst[2 * i] + dst[2 * i + 2] - dst[2 * i - 2] - dst[2 * i + 4]) >> 3) + 1) >> 1);
			}
			return;
		}
	};

	template <typename T, bool is_int>
	struct _dwtcore;

	template <typename T>
	struct _dwtcore<T, true> {
		using type = typename _dwtcore_i<T>;
	};

	template <typename T>
	using dwtcore = typename _dwtcore<T, std::is_integral<T>::value>::type;
}