#include <type_traits>

void fnWaveletTransform()
{
}

// template <typename T = unsigned int>
// void v_bwd_dwt(T* hsrc, T* lsrc, T* dst, size_t count) {
// 	// size_t alligned_size = ((count + 2 + 4) + (16 - 1)) & ((-1) << 4));
// 	// size_t alligned_count = (count + (16 - 1)) & ((-1) << 4));
// 	T* alligned_lsrc = lsrc + 1;
// 	constexpr size_t vector_step = 16;
// 	constexpr size_t move_prepipeline_boundary = 0 * /*2 * */vector_step;
// 	constexpr size_t h_prepipeline_boundary = 1 * vector_step;
// 	constexpr size_t l_prepipeline_boundary = 2 * vector_step;
// 	constexpr size_t move_postpipeline_boundary = 1 * /*2 * */vector_step;	// TODO: check if needed
// 	constexpr size_t h_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed
// 	constexpr size_t l_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed
// 
// 	// load pipeline 
// 	for (size_t i = 0; i < l_prepipeline_boundary; i += vector_step) {
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			// restores starting from 2j-2
// 			lsrc[i + j] = lsrc[i + j/* + 1*/] - ((hsrc[i + j] + hsrc[i + j + 1] - 2) >> 2);
// 		}
// 	}
// 	for (size_t i = 0; i < h_prepipeline_boundary; i += vector_step) {
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			// restores starting from 2j+1
// 			T htemp = (-lsrc[i + j] + lsrc[i + j + 1] + lsrc[i + j + 2] - lsrc[i + j + 3]) >> 3;
// 			hsrc[i + j] += ((lsrc[i + j + 1] + lsrc[i + j + 2]) + htemp + 1) >> 1;
// 		}
// 	}
// 
// 	T* p_move_lsrc = alligned_lsrc + move_prepipeline_boundary /** 2*/;
// 	T* p_move_hsrc = hsrc + move_prepipeline_boundary /*/ 2*/;
// 	T* p_move_dst = dst + move_prepipeline_boundary * 2;
// 	T* p_hsrc = lsrc + h_prepipeline_boundary;
// 	T* p_hdst = hsrc + h_prepipeline_boundary;
// 	T* p_lsrc = hsrc + l_prepipeline_boundary;
// 	T* p_ldst = lsrc + l_prepipeline_boundary;
// 	size_t p_count = count - vector_step;
// 
// 	// run pipeline
// 	for (size_t i = 0; i < p_count; i += vector_step) {
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			p_ldst[i + j] = p_lsrc[i + j/* + 1*/] - ((p_hsrc[i + j] + p_hsrc[i + j + 1] - 2) >> 2);
// 			T htemp = (-p_hsrc[i + j] + p_hsrc[i + j + 1] + p_hsrc[i + j + 2] - p_hsrc[i + j + 3]) >> 3;
// 			p_hdst[i + j] += ((p_hsrc[i + j + 1] + p_hsrc[i + j + 2]) + htemp + 1) >> 1;
// 			p_move_dst[(i + j) << 1] = p_move_lsrc[i + j];
// 			p_move_dst[((i + j) << 1) + 1] = p_move_hsrc[i + j];
// 		}
// 	}
// 
// 	// free pipeline
// 	for (size_t i = p_count; i < count; i += vector_step) {
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			p_move_dst[(i + j) << 1] = p_move_lsrc[i + j];
// 			p_move_dst[((i + j) << 1) + 1] = p_move_hsrc[i + j];
// 		}
// 	}
// 	return;
// }

// 
// #include <cmath>
// void temp(double const * const source) {
// 	typedef unsigned int int_t;
// 	typedef std::make_unsigned<int_t>::type uint_t;
// 	typedef std::make_signed<int_t>::type sint_t;
// 	constexpr int corr = 1;
// 	static const size_t N = 16;
// 	double _result_D[N] = { 0 };
// 	double * const result_D = &_result_D[1];
// 	double result_C[N] = { 0 };
// 
// 	// TODO: vectorize. Get rid of function calls (or force inline)
// 	for (size_t i = 0; i < N; i += 2) {
// 		double temp1 = source[i] + source[i + 2];
// 		double temp2 = source[i - 2] + source[i + 4];
// 		temp1 = ldexp(temp1, -4 + corr) + ldexp(temp1, -1 + corr);
// 		temp2 = ldexp(temp2, -4 + corr);
// 		result_D[i >> 1] = source[i + 1] - ((((uint_t)(temp1 + temp2)) + 0x01) >> corr);
// 	}
// 	_result_D[0] = _result_D[1];
// 	for (size_t i = 0; i < N; i+=2) {
// 		result_C[i >> 1] = source[i] - (((sint_t)(-(ldexp(result_D[i >> 1] + result_D[i >> 1 - 1], -2 + corr)) + 0x01)) >> corr);
// 	}
// }
// 
// template <typename T> //, typename sufficient_integral<T>::type int_t>
// T v_fpexp(T val, size_t exp);
// 
// 
// 
// template <typename T = unsigned int>
// void v_fwd_dwt(T* const src, T* const hdst, T* const ldst, size_t count) {
// 	size_t alligned_size = ((count + 2 + 4) + (16 - 1)) & ((-1) << 4));
// 	size_t alligned_count = (count + (16 - 1)) & ((-1) << 4));
// 	T* alligned_src = src - 2;
// 	for (size_t i = 0; i < alligned_size; i += 16) {
// 		for (size_t j = 0; j < 16; j += 2) {
// 			// move even to lowpass buffer, odd to highpass buffer
// 			// Intermediate results for highpass are added to highpass buffer, but
// 			// input coefficents for highpass intermediate are even and therefore are
// 			// fetched from lowpass buffer.
// 			ldst[(i + j) >> 1] = alligned_src[i + j];
// 			hdst[(i + j) >> 1] = alligned_src[i + j + 1];
// 		}
// 	}
// 	// for (size_t j = (alligned_size & ((-1) << 4)); j < alligned_size; j += 2) {
// 	// 	hdst[j >> 1] = alligned_src[j];
// 	// 	ldst[j >> 1] = alligned_src[j + 1];
// 	// }
// 
// 	for (size_t i = 0; i < alligned_count; i += 16) {
//           for (size_t j = 0; j < 16; ++j) {
// 			  T htemp = (-ldst[i + j] + ldst[i + j + 1] + ldst[i + j + 2] - ldst[i + j + 3]) >> 3;
// 			  hdst[i + j] -= ((ldst[i + j + 1] + ldst[i + j + 2]) + htemp + 1) >> 1;
// 		  }
// 	}
// 	for (size_t i = 0; i < 16; i += 16) {
// 		for (size_t j = 0; j < 16; ++j) {
//             ldst[i + j] = ldst[i + j + 1] + ((hdst[i + j] + hdst[i + j + 1] + 2) >> 2);
// 		  }
// 	}
// 
// 	constexpr size_t vector_step = 16;
// 	constexpr size_t move_prepipline_boundary = 2 * 2 * vector_step;
//         constexpr size_t h_prepipline_boundary = 1 * vector_step;
// 		constexpr size_t l_prepipline_boundary = 0 * vector_step;
// 		constexpr size_t move_postpipline_boundary = 0 * 2 * vector_step;
// 		constexpr size_t h_postpipline_boundary = 0 * vector_step;
// 		constexpr size_t l_postpipline_boundary = 1 * vector_step;
// 
// 
// 		for (size_t i = 0; i < move_prepipline_boundary; i += 2 * vector_step) {
// 			for (size_t j = 0; j < 2 * vector_step; j += 2) {
// 				// move even to lowpass buffer, odd to highpass buffer
// 				// Intermediate results for highpass are added to highpass
// 				// buffer, but input coefficents for highpass intermediate
// 				// are even and therefore are fetched from lowpass buffer.
// 				ldst[(i + j) >> 1] = alligned_src[i + j];
// 				hdst[(i + j) >> 1] = alligned_src[i + j + 1];
// 			}
// 		}
// 		for (size_t i = 0; i < h_prepipline_boundary; i += vector_step) {
// 			for (size_t j = 0; j < vector_step; ++j) {
// 				T htemp = (-ldst[i + j] + ldst[i + j + 1] + ldst[i + j + 2] - ldst[i + j + 3]) >> 3;
// 				hdst[i + j] -= ((ldst[i + j + 1] + ldst[i + j + 2]) + htemp + 1) >> 1;
// 			}
// 		}
// 
// 		T restrict * p_move_src = alligned_src + move_prepipline_boundary;
// 		T restrict * p_move_hdst = hdst + move_prepipline_boundary / 2;
// 		T restrict * p_move_ldst = ldst + move_prepipline_boundary / 2;
// 		T restrict * p_hsrc = hdst + h_prepipline_boundary;
// 		T restrict * p_hdst = ldst + h_prepipline_boundary;
// 		T restrict * p_lsrc = hdst;
// 		T restrict * p_ldst = ldst;
// 		size_t p_count = count - vector_step;
// 		for (size_t i = 0; i < p_count; i += vector_step) {
// 			for (size_t j = 0; j < 2 * vector_step; j += 2) {
// 				p_move_ldst[(i + j) >> 1] = p_move_src[i + j];
// 				p_move_hdst[(i + j) >> 1] = p_move_src[i + j + 1];
// 			}
// 			for (size_t j = 0; j < vector_step; ++j) {
// 				T htemp = (-p_hsrc[i + j] + p_hsrc[i + j + 1] + p_hsrc[i + j + 2] - p_hsrc[i + j + 3]) >> 3;
// 				p_hdst[i + j] -= ((p_hsrc[i + j + 1] + p_hsrc[i + j + 2]) + htemp + 1) >> 1;
// 				p_ldst[i + j] = p_ldst[i + j + 1] + ((p_lsrc[i + j] + p_lsrc[i + j + 1] + 2) >> 2);
// 			}
// 		}
// 		for (size_t i = p_count; i < count; i += vector_step) {
// 			for (size_t j = 0; j < vector_step; ++j) {
// 				p_ldst[i + j] = p_ldst[i + j + 1] + ((p_lsrc[i + j] + p_lsrc[i + j + 1] + 2) >> 2);
// 			}
//         }
// 		return;
// }
// 
// #include <limits>
// void temp1(double const * const source) {
// 	typedef unsigned int int_t;
// 	typedef std::make_unsigned<int_t>::type uint_t;
// 	typedef std::make_signed<int_t>::type sint_t;
// 	constexpr int corr = 1;
// 	static const size_t N = 16;
// 	double _result_D[N] = { 0 };
// 	double * const result_D = &_result_D[1];
// 	double result_C[N] = { 0 };
// 
// 	for (size_t i = -2; i < N; i+=2) {
// 		result_C[i] = source[i];
// 		result_D[i] = source[i+1];
// 	}
// 	//	-2	0	2	4
// 	//	|___|___|___|___|___|___|___|___|___|
// 	//	  |	  +---+	  |	  |	  |
// 	//	  +-----------+	  |	  |
// 	//		  |	  +---+   |	  |
// 	//		  +-----------+	  |
// 	//			  |	  +---+	  |
// 	//			  +-----------+
// 	//
// 	//	  +---+   |   |   |   |
// 	//		  +---+	  |	  |	  |
// 	//			  +---+	  |	  |
// 	//				  +---+	  |
// 	//					  +---+
// 
// 	// TODO: vectorize. Get rid of function calls (or force inline)
// 	for (size_t i = 0; i < N; i += 2) {
// 		double temp1 = source[i] + source[i + 2];
// 		double temp2 = source[i - 2] + source[i + 4];
// 		temp1 = ldexp(temp1, -4 + corr) + ldexp(temp1, -1 + corr);
// 		temp2 = ldexp(temp2, -4 + corr);
// 		result_D[i >> 1] = source[i + 1] - ((((uint_t)(temp1 + temp2)) + 0x01) >> corr);
// 	}
// 	_result_D[0] = _result_D[1];
// 	for (size_t i = 0; i < N; i += 2) {
// 		result_C[i >> 1] = source[i] - (((sint_t)(-(ldexp(result_D[i >> 1] + result_D[i >> 1 - 1], -2 + corr)) + 0x01)) >> corr);
// 	}
// 
// 	//	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	
// 	//	|xxx|xxx|xxx|xxx|	|	|	|	|+a	|	|	|	|	|	|	|	|
// 	//	|xxx|xxx|xxx|xxx|	|	|	|D0	|D0	|D1	|D2	|D3	|	|	|	|	|
// 
// 	// allocate sufficient buffer to take into account allignment required for vectorization 
// 	double* dest_D;
// 	double* dest_C;
// 	double* dest;
// 	for (size_t i = 0; i < N; i+=8) {
// 		// TODO: get suffiscient unsigned integral type, shift left to match fp exp part, add
// 		// should vectorize well
// 		for (size_t j = 0; j < 8; ++j) {
// 			dest_D[i] = source[i] + source[i + 1];
// 
// 			// double* dest_o1 = &dest[1];
// 			dest[i] = source[i] + source[i + 2];
// 			dest[i] = v_fpexp(dest[i], -4) + v_fpexp(dest[i], -1);
// 			dest[i] -= v_fpexp(source[i - 2] + source[i + 4], -4);
// 			// round off implimentation
// 
// 		}
// 	}
// 
// 	// TODO notes:
// 	// transform uses only integer arithmetic, no fp operations
// 	// is it worthy to increase data density so that coefficients are fetched linearly one by one?
// 
// 	// increase density:
// 	// x2 memory units len for original data
// 	// x1 memory units for lowpass data
// 	// x1 memory units for highpass data
// 	// x1 memory units for temporal data of lowpass computation
// 
// 	// Memory transitions can be performed in the begining of the CPU piplene, though shoul not affect 
// 	// performance.
// 
// 	// lowpass and highpass sources can be allocated at a dst location and then reused
// 	// temporal storage for lowpass needed.
// 
// 	// lowpass required for mirroring if boundary is near (if mirroring is necessary, otherwise no additional 
// 	// operations)
// 	// Transform can be invoked before mirroring corrections are applied for highpass data. That is:
// 	// it is possible to replace a single possibly affected output coefficient (in the begining). All that is
// 	// needed is to write correct output coefficients starting from [1] and further (and this is easy to make)
// 	// Mirroring for end bound should be applied after all computations are done.
// 	// Mirroring for lowpass data is applied to source data, so it should be provided befor any computation.
// 
// 	// allighment:
// 	// should be provided before transformation is applied. Affects source data and output buffer allocation 
// 	// only. Should account for mirroring (source data for low-pass and single placeholder for highpass). 
// 
// 	// Two-level call: memory operational + transform itself. All memory-related stuff and corrections are
// 	// performed at memory level.
// 	// Density increase is incapsulated in transform level. 
// 
// 	// Transform level call definition has input parameters: src, hdst, ldst, len.
// 	// src points to the first data member that requires corresponding output coefficient computation, 
// 	// this buffer should have valid boundary extension on the left (negative indexes will be used)
// }
// 
// template <size_t size_bytes>
// struct _sufficient_integral;
// 
// template <>
// struct _sufficient_integral<2> {
// 	typedef int_fast16_t type;
// };
// 
// template <>
// struct _sufficient_integral<4> {
// 	typedef int_fast32_t type;
// };
// 
// template <>
// struct _sufficient_integral<8> {
// 	typedef int_fast64_t type;
// };
// 
// template <typename T>
// using sufficient_integral = _sufficient_integral<sizeof(T)>;
// 
// template <typename T> //, typename sufficient_integral<T>::type int_t>
// inline T v_fpexp(T val, size_t exp) {
// 	static_assert(std::is_floating_point(T)::value);
// 	typedef sufficient_integral<T>::type int_t;
// 	int_t a = *(int_t*)(&val);
// 	int_t b = exp;
// 	return *(T*)(a + (b << std::numeric_limits<T>::digits));
// }
// 
// 
// #include <string>
// void temp2() {
// 	std::char_traits<char16_t>;
// 	intmax_t a;
// 	sizeof(intmax_t);
// 	int_fast32_t b;
// 	uint_least64_t c;
// }
// 

// template <typename T = unsigned int>
// void v_fwd_dwt(T* src, T* hdst, T* ldst, size_t count) {
// 	// size_t alligned_size = ((count + 2 + 4) + (16 - 1)) & ((-1) << 4));
// 	// size_t alligned_count = (count + (16 - 1)) & ((-1) << 4));
// 	T* alligned_src = src - 2;
// 	constexpr size_t vector_step = 16;
// 	constexpr size_t move_prepipeline_boundary = 2 * /*2 **/ vector_step;
// 	constexpr size_t h_prepipeline_boundary = 1 * vector_step;
// 	constexpr size_t l_prepipeline_boundary = 0 * vector_step;
// 	constexpr size_t move_postpipeline_boundary = 0 * 2 * vector_step;	// TODO: check if needed
// 	constexpr size_t h_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed
// 	constexpr size_t l_postpipeline_boundary = 1 * vector_step;			// TODO: check if needed
// 
// 	// load pipeline 
// 	for (size_t i = 0; i < move_prepipeline_boundary; i += /*2 **/ vector_step) {
// 		for (size_t j = 0; j < /*2 **/ vector_step; ++j /* += 2*/) {
// 			// move even to lowpass buffer, odd to highpass buffer
// 			// Intermediate results for highpass are added to highpass
// 			// buffer, but input coefficents for highpass intermediate
// 			// are even and therefore are fetched from lowpass buffer.
// 			// ldst[(i + j) >> 1] = alligned_src[i + j];
// 			// hdst[(i + j) >> 1] = alligned_src[i + j + 1];
// 
// 			ldst[i + j] = alligned_src[(i + j) << 1];
// 			hdst[i + j] = alligned_src[((i + j) << 1) + 3];		// index -1 is never used, start from +1
// 		}
// 	}
// 	for (size_t i = 0; i < h_prepipeline_boundary; i += vector_step) {
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			T htemp = (-ldst[i + j] + ldst[i + j + 1] + ldst[i + j + 2] - ldst[i + j + 3]) >> 3;
// 			hdst[i + j] -= ((ldst[i + j + 1] + ldst[i + j + 2]) + htemp + 1) >> 1;
// 		}
// 	}
// 
// 	T* p_move_src = alligned_src + move_prepipeline_boundary * 2;
// 	T* p_move_hdst = hdst + move_prepipeline_boundary/* / 2*/;
// 	T* p_move_ldst = ldst + move_prepipeline_boundary/* / 2*/;
// 	T* p_hsrc = ldst + h_prepipeline_boundary;
// 	T* p_hdst = hdst + h_prepipeline_boundary;
// 	T* p_lsrc = hdst;
// 	T* p_ldst = ldst;
// 	size_t p_count = count - vector_step;	// TODO: ensure allignment
// 
// 	// run pipeline
// 	for (size_t i = 0; i < p_count; i += vector_step) {
// 		// for (size_t j = 0; j < 2 * vector_step; j += 2) {
// 		// 	p_move_ldst[(i + j) >> 1] = p_move_src[i + j];
// 		// 	p_move_hdst[(i + j) >> 1] = p_move_src[i + j + 1];
// 		// }
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			p_move_ldst[i + j] = p_move_src[(i + j) << 1];
// 			p_move_hdst[i + j] = p_move_src[((i + j) << 1) + 3];	// stil start from +1
// 			T htemp = (-p_hsrc[i + j] + p_hsrc[i + j + 1] + p_hsrc[i + j + 2] - p_hsrc[i + j + 3]) >> 3;
// 			p_hdst[i + j] -= ((p_hsrc[i + j + 1] + p_hsrc[i + j + 2]) + htemp + 1) >> 1;
// 			p_ldst[i + j] = p_ldst[i + j + 1] - ((-p_lsrc[i + j - 1] - p_lsrc[i + j] + 2) >> 2);
// 			// p_ldst[i + j] = p_ldst[i + j + 1] - ((-p_lsrc[i + j] - p_lsrc[i + j + 1] + 2) >> 2);
// 		}
// 	}
// 
// 	// free pipeline
// 	for (size_t i = p_count; i < count; i += vector_step) {
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			p_ldst[i + j] = p_ldst[i + j + 1] - ((-p_lsrc[i + j - 1] - p_lsrc[i + j] + 2) >> 2);
// 		}
// 	}
// 	return;
// }
// 
// template <typename T = unsigned int>
// void v_bwd_dwt(T* hsrc, T* lsrc, T* dst, size_t count) {
// 	// size_t alligned_size = ((count + 2 + 4) + (16 - 1)) & ((-1) << 4));
// 	// size_t alligned_count = (count + (16 - 1)) & ((-1) << 4));
// 	// T* alligned_lsrc = lsrc + 1;
// 	constexpr size_t vector_step = 1 << 4;
// 	constexpr size_t move_prepipeline_boundary = 0 * vector_step;
// 	constexpr size_t h_prepipeline_boundary = 1 * vector_step;
// 	constexpr size_t l_prepipeline_boundary = 2 * vector_step;
// 	constexpr size_t move_postpipeline_boundary = 1 * vector_step;		// TODO: check if needed
// 	constexpr size_t h_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed
// 	constexpr size_t l_postpipeline_boundary = 0 * vector_step;			// TODO: check if needed
// 
// 	T lbuffer[vector_step];
// 
// 	// load pipeline 
// 	for (size_t j = 0; j < vector_step; ++j) {
// 		// restores starting from 2j-2
// 		dst[j] = lsrc[j - 1] + ((-hsrc[j - 2] - hsrc[j - 1] + 2) >> 2);
// 		// dst[j] = lsrc[j - 1] - ((hsrc[j - 2] + hsrc[j - 1] - 2) >> 2);
// 	}
// 	for (size_t i = vector_step; i < l_prepipeline_boundary; i += 2 * vector_step) {	// TODO: verify dst and src indexing
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			// restores starting from 2j-2
// 			dst[i + j] = lsrc[i + j - 1] + ((-hsrc[i + j - 2] - hsrc[i + j - 1] + 2) >> 2);
// 			// dst[i + j] = lsrc[i + j - 1] - ((hsrc[i + j - 2] + hsrc[i + j - 1] - 2) >> 2);
// 			dst[i + j + vector_step] = dst[i + j];
// 		}
// 	}
// 	for (size_t i = 0; i < h_prepipeline_boundary; i += vector_step) {
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			// restores starting from 2j+1
// 			T htemp = (-dst[i + j] + dst[i + j + 1] + dst[i + j + 2] - dst[i + j + 3]) >> 3;
// 			lbuffer[j] = hsrc[i + j] + (((dst[i + j + 1] + dst[i + j + 2]) + htemp + 1) >> 1);
// 		}
// 	}
// 
// 	T* p_move_hsrc = dst + 1;
// 	T* p_move_dst = dst;
// 	T* p_hsrc = dst + 2 * h_prepipeline_boundary;
// 	T* p_hacc = hsrc + h_prepipeline_boundary;
// 	T* p_lsrc = hsrc + l_prepipeline_boundary;
// 	T* p_lacc = lsrc + l_prepipeline_boundary;
// 	T* p_ldst = dst + l_prepipeline_boundary + vector_step;
// 	size_t p_count = (count - 1) & (~(vector_step - 1));	// TODO: verify allignment
// 
// 	// run pipeline
// 	for (size_t i = 0, k = 0; i < p_count; i += vector_step, k += 2 * vector_step) {	// i is for sources, k is for dst
// 		for (size_t j = vector_step; j > 0; --j) {
// 			p_move_dst[k + (j << 1) - 2] = p_move_hsrc[k + j - 1];		// dst + 1 - 1 == dst
// 			p_move_dst[k + (j << 1) - 1] = lbuffer[j - 1];
// 		}
// 		for (size_t j = 0; j < vector_step; ++j) {
// 			p_ldst[k + j] = p_lacc[i + j - 1] + ((-p_lsrc[i + j - 2] - p_lsrc[i + j - 1] + 2) >> 2);
// 			// p_ldst[k + j] = p_lacc[i + j - 1] - ((p_lsrc[i + j - 2] + p_lsrc[i + j - 1] - 2) >> 2);
// 			p_ldst[k + j + vector_step] = p_ldst[k + j];
// 			T htemp = (-p_hsrc[k + j] + p_hsrc[k + j + 1] + p_hsrc[k + j + 2] - p_hsrc[k + j + 3]) >> 3;
// 			lbuffer[j] = p_hacc[i + j] + (((p_hsrc[k + j + 1] + p_hsrc[k + j + 2]) + htemp + 1) >> 1);
// 		}
// 	}
// 
// 	// free pipeline
// 	for (size_t k = 2 * p_count; k < p_count + count; k += 2 * vector_step) {
// 		for (size_t j = vector_step; j > 0; --j) {
// 			p_move_dst[k + (j << 1) - 2] = p_move_hsrc[k + j - 1];		// dst + 1 - 1 == dst
// 			p_move_dst[k + (j << 1) - 1] = lbuffer[j - 1];
// 		}
// 	}
// 	return;
// }
// 



// from DWT test unit:


//#include <iostream>
//#include <fstream>
//#include <sstream>
//#include <type_traits>
//
//#include "../EntropyCoding/EndianCoder.tpp"
//
//TEST(EntropyCoding, EndinanCoderSmoke) {
//	typedef unsigned int word_t;
//	word_t sample = 0x9c3382ac2ba4f835;
//	constexpr word_t expected = (0x35f8a42bac82339c >> ((8 - sizeof(word_t)) * 8));
//	word_t actual = EndianCoder<word_t>::apply(sample);
//	EXPECT_EQ(actual, expected);
//}
//
//TEST(EntropyCoding, EndinanCoderLong) {
//	typedef size_t word_t;
//	word_t sample = 0x9c3382ac2ba4f835;
//	constexpr word_t expected = (0x35f8a42bac82339c >> ((8 - sizeof(word_t)) * 8));
//	word_t actual = EndianCoder<word_t>::apply(sample);
//	EXPECT_EQ(actual, expected);
//}
//
//TEST(EntropyCoding, EndinanCoderB2L) {
//	typedef unsigned int word_t;
//	word_t sample = 0x35f8a42bac82339c;
//	constexpr word_t expected = (0x9c3382ac2ba4f835 >> ((8 - sizeof(word_t)) * 8));
//	word_t actual = EndianCoder<word_t>::apply(sample);
//	EXPECT_EQ(actual, expected);
//}
//
//
//#include "../EntropyCoding/isymbolstream.h"
//#include "../EntropyCoding/EntropyEncoder.tpp"
//
//TEST(EntropyCoding, EncoderSmoke) {
//	typedef unsigned int obuffer_t;
//	//typedef std::fstream stream_t;
//	typedef std::wstringstream stream_t;
//	obuffer_t test_sequence[6] = {
//		0x2ba4f835,
//		0x9c3382ac,
//		0x8d910e28,
//		0x2ba4f835,
//		0x9c3382ac,
//		0x8d910e28,
//	};
//	size_t encodedCount = 0;
//	//stream_t my_ostream("./buffer.txt");
//	stream_t my_ostream;
//	my_ostream << std::hex;
//	{
//		EndianCoder<obuffer_t> endianCoder;
//		for (size_t i = 0; i < sizeof(test_sequence) / sizeof(obuffer_t); ++i) {
//			test_sequence[i] = endianCoder.apply(test_sequence[i]);
//		}
//
//		isymbolstream my_istream(
//			std::vector<unsigned char>((unsigned char*)test_sequence,
//				(unsigned char*)test_sequence + sizeof(test_sequence)), 2 * sizeof(test_sequence));
//		EntropyEncoder<4, 2, obuffer_t, stream_t::char_type> my_encoder(my_ostream);
//		my_encoder.translate(my_istream);
//		encodedCount = my_encoder.close();
//	}
//	obuffer_t expected[] = {
//		// 0xc7320c9d
//		0b1100'0111'0011'0010'0000'1100'1001'1101,
//		// 0x4a0fc98c
//		0b0100'1010'0000'1111'1100'1001'1000'1100,
//		// 0x0204b60b
//		0b0000'0010'0000'0100'1011'0110'0000'1011,
//		// 0x131cc832
//		0b0001'0011'0001'1100'1100'1000'0011'0010,
//		// 0x75283f26
//		0b0111'0101'0010'1000'0011'1111'0010'0110,
//		// 0x300812b8
//		0b0011'0000'0000'1000'0001'0010'1101'1000,
//		// 0x2c400000
//		0b0010'1100'0100'0000'0000'0000'0000'0000
//	};
//
//	std::cout << "Encoded count: " << encodedCount << std::endl;
//	size_t index = 0;
//	obuffer_t temp;
//	my_ostream.read((stream_t::char_type*)&temp, sizeof(temp) / sizeof(stream_t::char_type));
//	while (my_ostream) {
//		// std::cout << std::hex << temp << " : " << expected[index] << std::endl;
//		EXPECT_EQ(temp, expected[index]);
//		++index;
//		my_ostream.read((stream_t::char_type*)&temp, sizeof(temp) / sizeof(stream_t::char_type));
//	}
//}
//
//#include "../EntropyCoding/EntropyDecoder.tpp"
//
//TEST(EntropyCoding, DecoderSmoke) {
//	typedef unsigned int obuffer_t;
//	typedef std::wstringstream stream_t;
//	obuffer_t test_sequence[6] = {
//		0x2ba4f835,
//		0x9c3382ac,
//		0x8d910e28,
//		0x2ba4f835,
//		0x9c3382ac,
//		0x8d910e28,
//	};
//
//	obuffer_t expected[6] = {
//		0x2ba4f835,
//		0x9c3382ac,
//		0x8d910e28,
//		0x2ba4f835,
//		0x9c3382ac,
//		0x8d910e28,
//	};
//
//	obuffer_t* actual = nullptr;
//	size_t actual_size = 0;
//	size_t decodedCount = 0;
//
//	stream_t my_ostream;
//	{
//		for (size_t i = 0; i < sizeof(test_sequence) / sizeof(obuffer_t); ++i) {
//			test_sequence[i] = EndianCoder<obuffer_t>::apply(test_sequence[i]);
//		}
//
//		isymbolstream my_istream(
//			std::vector<unsigned char>((unsigned char*)test_sequence,
//				(unsigned char*)test_sequence + sizeof(test_sequence)), 2 * sizeof(test_sequence));
//		EntropyEncoder<4, 2, obuffer_t, stream_t::char_type> my_encoder(my_ostream);
//		my_encoder.translate(my_istream);
//		my_encoder.close();
//	}
//	{
//		osymbolstream my_osstream;
//		EntropyDecoder<4, 2, obuffer_t, stream_t::char_type> my_decoder(my_ostream);
//		my_decoder.translate(my_osstream);
//		decodedCount = my_decoder.close();
//
//		actual_size = (my_osstream.capacity() + sizeof(obuffer_t) - 1) / sizeof(obuffer_t);
//		actual = new obuffer_t[actual_size];
//		memcpy(actual, my_osstream.data(), my_osstream.capacity());
//		for (size_t i = 0; i < actual_size; ++i) {
//			actual[i] = EndianCoder<obuffer_t>::apply(actual[i]);
//		}
//	}
//
//	std::cout << "Decoded count: " << decodedCount << std::endl;
//	for (size_t i = 0; i < sizeof(expected) / sizeof(*expected); ++i) {
//		// std::cout << std::hex << actual[i] << " : " << expected[i] << std::endl;
//		EXPECT_EQ(actual[i], expected[i]);
//	}
//	delete[] actual;
//}
//
//#include "../EntropyCoding/entropy_utils.h"
//#include "../EntropyCoding/symbol_utils.h"
//#include "../EntropyCoding/symbolstream.h"
//
//
//TEST(EntropyCoding, SymbolCycleChainSmoke) {
//	typedef size_t buffer_t;
//	typedef std::wstringstream stream_t;
//	buffer_t test_sequence[] = {
//		0x2ba4f8359c3382ac,
//		0x8d910e282ba4f835,
//		0x9c3382ac8d910e28,
//	};
//
//	buffer_t expected[] = {
//		0x2ba4f8359c3382ac,
//		0x8d910e282ba4f835,
//		0x9c3382ac8d910e28,
//	};
//
//	buffer_t* actual = nullptr;
//	size_t actual_size = 0;
//	size_t decodedCount = 0;
//	// std::cout << "Stream states: " << std::endl
//	// 	<< "\tstream_t::goodbit \t"	<< stream_t::goodbit << std::endl
//	// 	<< "\tstream_t::eofbit \t"	<< stream_t::eofbit << std::endl
//	// 	<< "\tstream_t::failbit \t"	<< stream_t::failbit << std::endl
//	// 	<< "\tstream_t::badbit \t"	<< stream_t::badbit << std::endl << std::endl;
//
//
//	stream_t bplane_stream;
//	{
//		symbolstream forward_symstream;
//		symbolstream backward_symstream;
//		stream_t entropy_stream;
//
//		// TODO: make template specializations (or simply wrappers with redirection) 
//		typedef code::code_type<4, 0, buffer_t, decltype(bplane_stream)::char_type> symbolCode_t;
//		typedef code::code_type<4, 1, buffer_t, decltype(entropy_stream)::char_type> entropyCode_t;
//
//		// std::cout << "1.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl;
//
//		for (size_t i = 0; i < sizeof(test_sequence) / sizeof(buffer_t); ++i) {
//			test_sequence[i] = EndianCoder<buffer_t>::apply(test_sequence[i]);
//			bplane_stream.write((decltype(bplane_stream)::char_type*) & (test_sequence[i]),
//				sizeof(*test_sequence) / sizeof(decltype(bplane_stream)::char_type));
//		}
//
//		// std::cout << "2.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl;
//
//		symbol::encoder<symbolCode_t> forward_symencoder(bplane_stream);
//		forward_symencoder.translate(forward_symstream);
//		forward_symencoder.close();
//
//		// std::cout << "3.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl;
//		bplane_stream.clear();
//		// std::cout << "4.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl;
//
//		entropy::encoder<entropyCode_t> forward_bitencoder(entropy_stream);
//		forward_bitencoder.translate(forward_symstream);
//		forward_bitencoder.close();
//
//		entropy::decoder<entropyCode_t> backward_bitdecoder(entropy_stream);
//		backward_bitdecoder.translate(backward_symstream);
//		backward_bitdecoder.close();
//
//		symbol::decoder<symbolCode_t> backward_symdecoder(bplane_stream);
//		backward_symdecoder.translate(backward_symstream);
//		// backward_symdecoder.translate(forward_symstream);
//		backward_symdecoder.close();
//
//		// std::cout << "5.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl << std::endl;
//		// std::cout << "bplane_stream tellp: \t" << bplane_stream.tellp() << std::endl;
//		// std::cout << "bplane_stream tellg: \t" << bplane_stream.tellg() << std::endl;
//		size_t bplane_size = ((size_t)bplane_stream.tellp()) - ((size_t)bplane_stream.tellg());
//		actual_size = (bplane_size * sizeof(decltype(bplane_stream)::char_type) + sizeof(buffer_t) - 1) / sizeof(buffer_t);
//		actual = new buffer_t[actual_size];
//
//		for (size_t i = 0; i < actual_size; ++i) {
//			buffer_t temp;
//			bplane_stream.read((decltype(bplane_stream)::char_type*) & temp,
//				sizeof(*test_sequence) / sizeof(decltype(bplane_stream)::char_type));
//			actual[i] = EndianCoder<buffer_t>::apply(temp);
//		}
//	}
//
//	for (size_t i = 0; i < sizeof(expected) / sizeof(*expected); ++i) {
//		// std::cout << std::hex << actual[i] << " : " << expected[i] << std::endl;
//		EXPECT_EQ(actual[i], expected[i]);
//	}
//	delete[] actual;
//}


// from DWT test unit, old dwtcore tests:

// TEST(WaveletTranform, 1dTrandformSmoke) {
// 	typedef long long item_t;
// 	item_t* input = new item_t[512] {0};
// 	item_t* l_output_target = new item_t[256] {0};
// 	item_t* h_output_target = new item_t[256] {0};
// 	item_t* reversed_target = new item_t[512] {0};
// 
// 	item_t* l_output_result = new item_t[256]{ 0 };
// 	item_t* h_output_result = new item_t[256]{ 0 };
// 	item_t* reversed_result = new item_t[512]{ 0 };
// 
// 	constexpr size_t sampleNum = 512 - 128;
// 	constexpr double pi = 3.141592653589793238;
// 	constexpr double stride = pi * 2 / sampleNum;
// 
// 	double argument = 0;
// 	for (size_t i = 0; i < sampleNum; i++) {
// 		input[i + 64] = (item_t)((cos(argument * 3) + sin(argument * 2)) * 1024*16);
// 		argument += stride;
// 	}
// 
// 	typedef sufficient_integral<item_t>::type uitem_t;
// 
// 	uitem_t* pi_input = (uitem_t*)(&input[64]);
// 	uitem_t* pi_h_output_target = (uitem_t*)(&h_output_target[32]);
// 	uitem_t* pi_l_output_target = (uitem_t*)(&l_output_target[32]);
// 	uitem_t* pi_reversed_target = (uitem_t*)(&reversed_target[64]);
// 	target_fwd_dwt_impl(pi_input, pi_h_output_target, pi_l_output_target, sampleNum / 2);
// 	target_bwd_dwt_impl(pi_h_output_target, pi_l_output_target, pi_reversed_target, sampleNum / 2);
// 
// 	uitem_t* pi_h_output_result = (uitem_t*)(&h_output_result[32]);
// 	uitem_t* pi_l_output_result = (uitem_t*)(&l_output_result[32]);
// 	uitem_t* pi_reversed_result = (uitem_t*)(&reversed_result[64]);
// 	v_fwd_dwt(pi_input, pi_h_output_result, pi_l_output_result, sampleNum / 4);
// 	v_fwd_dwt(pi_input + sampleNum / 2, pi_h_output_result + sampleNum / 4, pi_l_output_result + sampleNum / 4, sampleNum / 4);
// 	pi_l_output_result[sampleNum / 2] = 0;
// 	v_bwd_dwt(pi_h_output_result, pi_l_output_result, pi_reversed_result, sampleNum / 2);
// 
// 
// 	for (size_t i = 0; i < 512; ++i) {
// 		EXPECT_EQ((item_t)input[i], (item_t)reversed_target[i]) << " at index " << i;
// 	}
// 
// 	for (size_t i = 0; i < 512; ++i) {
// 		if (i == 445 || i == 447) continue;
// 		EXPECT_EQ((item_t)reversed_target[i], (item_t)reversed_result[i]) << " at index " << i;
// 	}
// 
// 	for (size_t i = 0; i < 256; ++i) {
// 		EXPECT_EQ((item_t)l_output_target[i], (item_t)l_output_result[i]) << " at index " << i;
// 		EXPECT_EQ((item_t)h_output_target[i], (item_t)h_output_result[i]) << " at index " << i;
// 	}
// 	delete [] reversed_result;
// 	delete [] reversed_target;
// 	delete [] l_output_result;
// 	delete [] h_output_result;
// 	delete [] l_output_target;
// 	delete [] h_output_target;
// 	delete [] input;
// }

// SegmentPreCoder::apply()
// size_t bufsize = (output[i].size + 3 * allignment - 1) & (~(allignment - 1));
// output[i].quantizedDc = std::vector<T>(bufsize);
// T* segmentDc = (T*)(((size_t)(output[i].quantizedDc.data()) + 2 * pallignment - 1) 
// 	& (~(pallignment - 1)));
// 
// std::vector<T> diff(bufsize);
// T* diffData = (T*)(((size_t)(diff.data()) + 2 * pallignment - 1) & (~(pallignment - 1)));



// SemgnetPostDecoder::apply()
// auto impl1 = [&]() {
// 	T feta = bound - (this->segments[i].quantizedDc[j - 1] ^ (this->segments[i].quantizedDc[j - 1] >> ((sizeof(T) << 3) - 1)));
// 	T predicate = this->segments[i].quantizedDc[j] - feta;
// 	if (predicate > feta) {
// 		T signmask = this->segments[i].quantizedDc[j] >> ((sizeof(T) << 3) - 1);
// 		this->segments[i].quantizedDc[j] = this->segments[i].quantizedDc[j - 1] - (predicate ^ signmask) + signmask;
// 	}
// 	else {
// 		T val = this->segments[i].quantizedDc[j];
// 		this->segments[i].quantizedDc[j] = this->segments[i].quantizedDc[j - 1] + ((-(val & 0x01)) ^ (val >> 1));
// 	}
// };
// 
// auto impl2 = [&]() {
// 	T feta = bound - (this->segments[i].quantizedDc[j - 1] ^ (this->segments[i].quantizedDc[j - 1] >> ((sizeof(T) << 3) - 1)));
// 	T predicate = this->segments[i].quantizedDc[j] - feta;
// 	T signmask = this->segments[i].quantizedDc[j] >> ((sizeof(T) << 3) - 1);
// 	T val = this->segments[i].quantizedDc[j];
// 	if (predicate > feta) {
// 		this->segments[i].quantizedDc[j] = this->segments[i].quantizedDc[j - 1] - (predicate ^ signmask) + signmask;
// 	}
// 	else {
// 		this->segments[i].quantizedDc[j] = this->segments[i].quantizedDc[j - 1] + ((-(val & 0x01)) ^ (val >> 1));
// 	}
// };
