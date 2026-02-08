#pragma once
#include <type_traits>

// TODO: consider passing aligmnent value as template parameter
template <typename T = unsigned int>
struct _dwtcore_i {
	template <typename iT>
	void fwd(iT const * src, T* hdst, T* ldst, size_t count) {
		// requires src to have allocated offset ranges to the left and to the rigth: [-2, 62]
		iT const * aligned_src = src - 2;
		constexpr size_t vector_step = 16;
		constexpr size_t move_prepipeline_boundary = 2 * /*2 **/ vector_step;
		constexpr size_t h_prepipeline_boundary = 1 * vector_step;
		constexpr size_t l_prepipeline_boundary = 0 * vector_step;			// TODO: check if needed
		constexpr size_t move_postpipeline_boundary = 0 * 2 * vector_step;	// TODO: check if needed
		constexpr size_t h_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed
		constexpr size_t l_postpipeline_boundary = 1 * vector_step;			// TODO: check if needed

		// load pipeline 
		for (ptrdiff_t i = 0; i < move_prepipeline_boundary; i += /*2 **/ vector_step) {
			for (ptrdiff_t j = 0; j < /*2 **/ vector_step; ++j /* += 2*/) {
				// move even to lowpass buffer, odd to highpass buffer
				// Intermediate results for highpass are added to highpass
				// buffer, but input coefficents for highpass intermediate
				// are even and therefore are fetched from lowpass buffer.
				ldst[i + j] = aligned_src[(i + j) << 1];
				hdst[i + j] = aligned_src[((i + j) << 1) + 3];		// index -1 is never used, start from +1
			}
		}
		for (ptrdiff_t i = 0; i < h_prepipeline_boundary; i += vector_step) {
			for (ptrdiff_t j = 0; j < vector_step; ++j) {
				T htemp = (-ldst[i + j] + ldst[i + j + 1] + ldst[i + j + 2] - ldst[i + j + 3]) >> 3;
				hdst[i + j] -= ((ldst[i + j + 1] + ldst[i + j + 2]) + htemp + 1) >> 1;
			}
		}

		iT const * p_move_src = aligned_src + move_prepipeline_boundary * 2;
		T* p_move_hdst = hdst + move_prepipeline_boundary/* / 2*/;
		T* p_move_ldst = ldst + move_prepipeline_boundary/* / 2*/;
		T const * p_hsrc = ldst + h_prepipeline_boundary;
		T* p_hdst = hdst + h_prepipeline_boundary;
		T const * p_lsrc = hdst;
		T* p_ldst = ldst;
		ptrdiff_t p_count = count - vector_step;	// TODO: ensure alignment, likely need relu for correctness

		// run pipeline
		ptrdiff_t i = 0;
		for (/*size_t i = 0*/; i < p_count; i += vector_step) {
			for (ptrdiff_t j = 0; j < vector_step; ++j) {
				p_move_ldst[i + j] = p_move_src[(i + j) << 1];
				p_move_hdst[i + j] = p_move_src[((i + j) << 1) + 3];	// still start from +1
				T htemp = (-p_hsrc[i + j] + p_hsrc[i + j + 1] + p_hsrc[i + j + 2] - p_hsrc[i + j + 3]) >> 3;
				p_hdst[i + j] -= ((p_hsrc[i + j + 1] + p_hsrc[i + j + 2]) + htemp + 1) >> 1;
				p_ldst[i + j] = p_ldst[i + j + 1] - ((-p_lsrc[i + j - 1] - p_lsrc[i + j] + 2) >> 2);
				// TODO: code { p_lsrc[i + j - 1] } in the line above is the reason why corrlfwd
				// is needed: p_lsrc[-1] == ldst[-1] is never initialized, and therefore computation 
				// result value is not defined. Why dont just write { hdst[-1] = hdst[0] } before 
				// the main loop? It's just one memory move, although non-vectorized.
				// 
				// well, first two 2 rows of hdst content is expected to be allocated on vector 
				// registers by the moment the main loop begins. Still it is not clear how -1 
				// index access is meant to be performed. Vector cross-register right circular shift 
				// by one item (then the last item is moved to index 0, or just dropped and 
				// never used if non-circular shift is used)? But then the shifted-out value has 
				// to be transferred across sequential vector register loads somehow, probably using 
				// gpr if register pressure allows that
				//
			}
		}

		// free pipeline
		for (ptrdiff_t j = 0; j < vector_step; ++j) {
			p_ldst[i + j] = p_ldst[i + j + 1] - ((-p_lsrc[i + j - 1] - p_lsrc[i + j] + 2) >> 2);
		}
		return;
	}

	void bwd(T const * hsrc, T const * lsrc, T* dst, size_t count) {
		// writes to dst [0, +48], reades from hsrc [min(31), 
		constexpr size_t vector_step = 1 << 4;
		constexpr size_t move_prepipeline_boundary = 0 * vector_step;		// TODO: check if needed
		constexpr size_t h_prepipeline_boundary = 1 * vector_step;
		constexpr size_t l_prepipeline_boundary = 2 * vector_step;
		constexpr size_t move_postpipeline_boundary = 1 * vector_step;		// TODO: check if needed
		constexpr size_t h_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed
		constexpr size_t l_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed

		T hbuffer[vector_step];

		// load pipeline 
		for (ptrdiff_t j = 0; j < vector_step; ++j) {
			// restores starting from 2j-2
			dst[j] = lsrc[j - 1] + ((-hsrc[j - 2] - hsrc[j - 1] + 2) >> 2);
		}
		for (ptrdiff_t i = vector_step; i < l_prepipeline_boundary; i += 2 * vector_step) {	// TODO: verify dst and src indexing
			for (ptrdiff_t j = 0; j < vector_step; ++j) {
				// restores starting from 2j-2
				dst[i + j] = lsrc[i + j - 1] + ((-hsrc[i + j - 2] - hsrc[i + j - 1] + 2) >> 2);
				dst[i + j + vector_step] = dst[i + j];
			}
		}
		for (ptrdiff_t i = 0; i < h_prepipeline_boundary; i += vector_step) {
			for (ptrdiff_t j = 0; j < vector_step; ++j) {
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
		ptrdiff_t p_count = ((ptrdiff_t)((count - 1) & (~(vector_step - 1)))) - vector_step;	// TODO: verify alignment

		// run pipeline
		ptrdiff_t i = 0, k = 0;
		for (/*ptrdiff_t i = 0, k = 0*/; i < p_count; i += vector_step, k += 2 * vector_step) {	// i is for sources, k is for dst
			for (ptrdiff_t j = vector_step; j > 0; --j) {
				p_move_dst[k + (j << 1) - 2] = p_move_hsrc[k + j - 1];		// dst + 1 - 1 == dst
				p_move_dst[k + (j << 1) - 1] = hbuffer[j - 1];
			}
			for (ptrdiff_t j = 0; j < vector_step; ++j) {
				p_ldst[k + j] = p_lacc[i + j - 1] + ((-p_lsrc[i + j - 2] - p_lsrc[i + j - 1] + 2) >> 2);
				p_ldst[k + j + vector_step] = p_ldst[k + j];
				T htemp = (-p_hsrc[k + j] + p_hsrc[k + j + 1] + p_hsrc[k + j + 2] - p_hsrc[k + j + 3]) >> 3;
				hbuffer[j] = p_hacc[i + j] + (((p_hsrc[k + j + 1] + p_hsrc[k + j + 2]) + htemp + 1) >> 1);
			}
		}

		// free pipeline
		for (ptrdiff_t j = vector_step; j > 0; --j) {
			p_move_dst[k + (j << 1) - 2] = p_move_hsrc[k + j - 1];		// dst + 1 - 1 == dst
			p_move_dst[k + (j << 1) - 1] = hbuffer[j - 1];
		}

		for (ptrdiff_t j = 0; j < vector_step; ++j) {
			p_ldst[k + j] = p_lacc[i + j - 1] + ((-p_lsrc[i + j - 2] - p_lsrc[i + j - 1] + 2) >> 2);
			T htemp = (-p_hsrc[k + j] + p_hsrc[k + j + 1] + p_hsrc[k + j + 2] - p_hsrc[k + j + 3]) >> 3;
			hbuffer[j] = p_hacc[i + j] + (((p_hsrc[k + j + 1] + p_hsrc[k + j + 2]) + htemp + 1) >> 1);
		}
		k += 2 * vector_step;
		for (ptrdiff_t j = vector_step; j > 0; --j) {
			p_move_dst[k + (j << 1) - 2] = p_move_hsrc[k + j - 1];		// dst + 1 - 1 == dst
			p_move_dst[k + (j << 1) - 1] = hbuffer[j - 1];
		}
		return;
	}

	template <typename iT>
	inline void extfwd(iT* begining) {
		begining[-1] = begining[1]; // TODO: source index -1 is never used? no need to copy?
		begining[-2] = begining[2];
		begining[-4] = begining[4]; // TODO: source index -4 is never used? no need to copy?
	}

	template <typename iT>
	inline void rextfwd(iT* ending) {
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
	template <typename iT>
	inline void corrlfwd(iT const * src, T const * hdst, T* ldst) {
		ldst[0] = src[0] - ((-hdst[0] + 1) >> 1);
	}

	template <typename iT>
	inline void corrhfwd(iT const * src, T* hdst, T const * ldst) { }
};

template <typename T = float>
struct _dwtcore_f {
	// TODO: not implemented
	template <typename iT>
	void fwd(iT const* src, T* hdst, T* ldst, size_t count) {}

	void bwd(T const* hsrc, T const* lsrc, T* dst, size_t count) {}

	template <typename iT>
	inline void extfwd(iT* begining) {}
	template <typename iT>
	inline void rextfwd(iT* ending) {}

	inline void exthbwd(T* begining) {}
	inline void rexthbwd(T* ending) {}
	inline void extlbwd(T* begining) {}
	inline void rextlbwd(T* ending) {}

	template <typename iT>
	inline void corrlfwd(iT const* src, T const* hdst, T* ldst) {}
	template <typename iT>
	inline void corrhfwd(iT const* src, T* hdst, T const* ldst) {}
};

template <typename T, bool is_int>
struct _dwtcore;

template <typename T>
struct _dwtcore<T, true> {
	using type = typename _dwtcore_i<T>;
};

template <typename T>
struct _dwtcore<T, false> {
	using type = typename _dwtcore_f<T>;
};

template <typename T>
using dwtcore = typename _dwtcore<T, std::is_integral<T>::value>::type;

namespace forward {
	template <typename T = double>
	constexpr T h[] = {
		0.852698679009,
		0.377402855613,
		-0.110624404418,
		-0.023849465020,
		0.037828455507
	};

	template <typename T = double>
	constexpr T g[] = {
		-0.788485616406,
		0.418092273222,
		0.040689417609,
		-0.064538882629
	};
}

namespace inverse {
	template <typename T = double>
	constexpr T q[] = {
		0.788485616406,
		0.418092273222,
		-0.040689417609,
		-0.064538882629
	};

	template <typename T = double>
	constexpr T p[] = {
		-0.852698679009,
		0.377402855613,
		0.110624404418,
		-0.023849465020,
		-0.037828455507
	};
}

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