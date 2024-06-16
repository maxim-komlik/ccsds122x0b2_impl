
void fncore()
{
}


// SegmentPreCoder.tpp
// template <typename T, size_t alignment>
// size_t bdepth(bitmap<T> src) {
// 	// alligned segment data
// 	T* segment = nullptr;
// 	size_t segmentSize = 64;
// 	T buffer[alignment] = { 0 };
// 	// size_t vectorizable = segmentSize & (~(alignment - 1));
// 
// 	for (size_t i = 0; i < segmentSize; i += alignment) {
// 		for (size_t j = 0; j < alignment; ++j) {
// 			buffer[j] |= altitude<T>(segment[i + j]);
// 		}
// 	}
// 
// 	for (size_t i = alignment >> 1; i > 0; i >>= 1) {
// 		for (size_t j = 0; j < i; ++j) {
// 			buffer[j] |= buffer[i + j];
// 		}
// 	}
// 	return buffer[0];
// }



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
// 	// allocate sufficient buffer to take into account alignment required for vectorization 
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
// 	size_t p_count = count - vector_step;	// TODO: ensure alignment
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
// 	size_t p_count = (count - 1) & (~(vector_step - 1));	// TODO: verify alignment
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

// SegmentAssembler::apply()
// size_t bufsize = (output[i].size + 3 * alignment - 1) & (~(alignment - 1));
// output[i].quantizedDc = std::vector<T>(bufsize);
// T* segmentDc = (T*)(((size_t)(output[i].quantizedDc.data()) + 2 * palignment - 1) 
// 	& (~(palignment - 1)));
// 
// std::vector<T> diff(bufsize);
// T* diffData = (T*)(((size_t)(diff.data()) + 2 * palignment - 1) & (~(palignment - 1)));



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



// // 38 words total compound block context
// 
// struct block_bpe_meta {
// 	// TODO: consider splitting the structure into 3 dedicated structs
// 	// for each relevant encoding stage.
// 
// 	bpe_variadic_length_word types_P;
// 	std::array<bpe_variadic_length_word, F_num> types_C;
// 	std::array<std::array<bpe_variadic_length_word, HxpF_num>, F_num> types_H;
// 
// 	bpe_variadic_length_word signs_P;
// 	std::array<bpe_variadic_length_word, F_num> signs_C;
// 	std::array<std::array<bpe_variadic_length_word, HxpF_num>, F_num> signs_H;
// 
// 	bpe_variadic_length_word tran_B;
// 	bpe_variadic_length_word tran_D;
// 	bpe_variadic_length_word tran_G;
// 	std::array<bpe_variadic_length_word, F_num> tran_H;
// 
// 	uint64_t bplane_mask_transit;
// 
// 	bool encode_descendants = false;
// };
// 
// std::vector<block_bpe_meta> block_states(input.size);
// 
// ptrdiff_t i = 0;
// for (; i < input_size_truncated; i += items_per_gaggle) {
// 	for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
// 		std::array<decltype(family_masks)::value_type, family_masks.size()> prepared_block = { 0 };
// 		for (ptrdiff_t k = 0; k < family_masks.size(); ++k) {
// 			prepared_block[k] = current_bplane[i + j] & family_masks[k];
// 		}
// 		// bool descendants_selected = (prepared_block[B_index] > 0);
// 		// size_t tran_B_len = (bplane_mask[i + j] & family_masks[B_index]) > 0;
// 		// block_states[i + j].tran_B = { !(block_states[i + j].encode_descendants), descendants_selected };
// 		// block_states[i + j].encode_descendants |= descendants_selected;
// 
// 		size_t tran_B_val = (prepared_block[B_index] > 0);
// 		size_t tran_B_len = (bplane_mask[i + j] & family_masks[B_index]) > 0; // see,s there is a typo and should be == 0
// 		block_states[i + j].tran_B = { tran_B_len, tran_B_val };	// TODO
// 		bool skip_descendants = ((tran_B_val == 0) & (tran_B_len == 1));
// 
// 		uint64_t effective_mask_P = (family_masks[P_index] & (~bplane_mask[i + j]));
// 		std::tie(
// 			std::get<0>(block_states[i + j].types_P),
// 			std::get<1>(block_states[i + j].types_P)
// 		) = combine_bword(prepared_block[P_index], effective_mask_P);
// 		std::tie(
// 			std::get<0>(block_states[i + j].signs_P),
// 			std::get<1>(block_states[i + j].signs_P)
// 		) = combine_bword(block_signs[i + j], (prepared_block[P_index] & effective_mask_P));
// 		block_states[i + j].bplane_mask_transit |= effective_mask_P;
// 
// 		if (!skip_descendants) {
// 			// move descendant word computation here
// 			// TODO: clean tran_D state (may remain from the previous iteration)
// 			std::get<0>(block_states[i + j].tran_D) = 0;
// 			std::get<0>(block_states[i + j].tran_G) = 0;
// 			for (ptrdiff_t k = 0; k < F_num; ++k) {
// 				size_t tran_Dx_val = prepared_block[(k * F_step) + D_offset] > 0;
// 				size_t tran_Dx_len = ((bplane_mask[i + j] & family_masks[(k * F_step) + D_offset]) == 0);
// 
// 				bool skip_tran_Gx = ((tran_Dx_val == 0) & (tran_Dx_len == 1));
// 
// 				size_t tran_Gx_val = prepared_block[(k * F_step) + G_offset] > 0;
// 				size_t tran_Gx_len = ((bplane_mask[i + j] & family_masks[(k * F_step) + G_offset]) == 0);
// 				tran_Gx_len &= !skip_tran_Gx;
// 
// 				bool skip_tran_Hx = ((tran_Gx_val == 0) & (tran_Gx_len == 1));
// 
// 				std::get<1>(block_states[i + j].tran_D) <<= tran_Dx_len;
// 				std::get<1>(block_states[i + j].tran_D) |= (tran_Dx_len & tran_Dx_val);
// 				std::get<0>(block_states[i + j].tran_D) += tran_Dx_len;
// 
// 				std::get<1>(block_states[i + j].tran_G) <<= tran_Gx_len;
// 				std::get<1>(block_states[i + j].tran_G) |= (tran_Gx_len & tran_Gx_val);
// 				std::get<0>(block_states[i + j].tran_G) += tran_Gx_len;
// 
// 				uint64_t effective_mask_C =
// 					(family_masks[(k * F_step) + C_offset] & (~bplane_mask[i + j])) & (-(!skip_tran_Gx));
// 				std::tie(
// 					std::get<0>(block_states[i + j].types_C[k]),
// 					std::get<1>(block_states[i + j].types_C[k])
// 				) = combine_bword(prepared_block[(k * F_step) + C_offset], effective_mask_C);
// 				std::tie(
// 					std::get<0>(block_states[i + j].signs_C[k]),
// 					std::get<1>(block_states[i + j].signs_C[k])
// 				) = combine_bword(block_signs[i + j], (prepared_block[(k * F_step) + C_offset] & effective_mask_C));
// 				block_states[i + j].bplane_mask_transit |= effective_mask_C;
// 
// 				std::get<0>(block_states[i + j].tran_H[k]) = 0;
// 				for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
// 					size_t tran_Hx_val = prepared_block[(k * F_step) + Hx_offset + l] > 0;
// 					size_t tran_Hx_len = ((bplane_mask[i + j] & family_masks[(k * F_step) + Hx_offset + l]) == 0);
// 					tran_Hx_len &= !skip_tran_Hx;
// 
// 					bool skip_Hx = ((tran_Hx_val == 0) & (tran_Hx_len == 1));
// 
// 					std::get<1>(block_states[i + j].tran_H[k]) <<= tran_Hx_len;
// 					std::get<1>(block_states[i + j].tran_H[k]) |= (tran_Hx_len & tran_Hx_val);
// 					std::get<0>(block_states[i + j].tran_H[k]) += tran_Hx_len;
// 
// 					uint64_t effective_mask_H =
// 						(family_masks[(k * F_step) + Hx_offset + l] & (~bplane_mask[i + j])) & (-(!skip_Hx));
// 					std::tie(
// 						std::get<0>(block_states[i + j].types_H[k][l]),
// 						std::get<1>(block_states[i + j].types_H[k][l])
// 					) = combine_bword(prepared_block[(k * F_step) + Hx_offset + l], effective_mask_H);
// 					std::tie(
// 						std::get<0>(block_states[i + j].signs_H[k][l]),
// 						std::get<1>(block_states[i + j].signs_H[k][l])
// 					) = combine_bword(block_signs[i + j], (prepared_block[(k * F_step) + Hx_offset + l] & effective_mask_H));
// 					block_states[i + j].bplane_mask_transit |= effective_mask_H;
// 				}
// 			}
// 			// but actually better to omit other words computation if tran_B == '0'
// 			// std::get<0>(block_states[i + j].tran_D) &=
// 			// 	std::get<0>(block_states[i + j].tran_B) & (~(std::get<1>(block_states[i + j].tran_B)));
// 
// 			// 21 variable length encoded words per block.
// 			// 3 words have maximum length = 3
// 			// 18 words have maximum length = 4
// 			// 
// 			// max number of variable len words in the gaggle with length = 4:
// 			//	18 * 16 = 288
// 			// max bitlen of variable len words in the gaggle with length = 4:
// 			//	288 * 4 = 1152
// 			// 
// 			// max number of variable len words in the gaggle with length = 3:
// 			//	3 * 16 = 48
// 			//	5 * 16 = 80
// 			//	10 * 16 = 160
// 			//	16 * 16 = 256
// 			// max bitlen of variable len words in the gaggle with length = 3:
// 			//	256 * 3 = 768
// 			// 
// 			// max number of variable len words in the gaggle with length = 2:
// 			//	3 * 16 = 48
// 			//	8 * 16 = 128
// 			//	13 * 16 = 208
// 			//	16 * 16 = 256
// 			// max bitlen of variable len words in the gaggle with length = 2:
// 			//	256 * 2 = 512
// 			//
// 
// 			constexpr size_t bpe_vlw_length_index = 0;
// 			constexpr size_t bpe_vlw_value_index = 1;
// 
// 			std::array<tempsymbolstream, 3> vlwstreams{
// 				tempsymbolstream(std::vector<std::byte>(144));
// 				tempsymbolstream(std::vector<std::byte>(128));
// 				tempsymbolstream(std::vector<std::byte>(128));
// 			};
// 
// 			auto addVlwToCollection = [&](bpe_variadic_length_word& vlw) -> void {
// 				constexpr size_t vlwstreams_index_bias = 2;
// 				size_t vlw_len = std::get<bpe_vlw_length_index>(vlw);
// 				if (vlw_len >= 2) {
// 					vlwstreams[vlw_len - vlwstreams_index_bias].put(std::get<bpe_vlw_value_index>(vlw));
// 				}
// 				};
// 
// 			auto computeTranVlw = [&](bpe_variadic_length_word& vlw, size_t index, bool skip) -> bool {
// 				size_t tran_val = prepared_block[index] > 0;
// 				size_t tran_len = ((bplane_mask[i + j] & family_masks[index]) == 0);
// 				tran_len &= !skip;
// 
// 				std::get<bpe_vlw_value_index>(vlw) <<= tran_len;
// 				std::get<bpe_vlw_value_index>(vlw) |= (tran_len & tran_val);
// 				std::get<bpe_vlw_length_index>(vlw) += tran_len;
// 
// 				addVlwToCollection(vlw);
// 
// 				return ((tran_val == 0) & (tran_len == 1));
// 				};
// 
// 			auto computeTerminalVlw = [&](bpe_variadic_length_word& typesVlw, bpe_variadic_length_word& signsVlw,
// 				size_t index, bool skip) {
// 					uint64_t effective_mask = (family_masks[index] & (~bplane_mask[i + j])) & (-(!skip));
// 
// 					std::tie(
// 						std::get<bpe_vlw_length_index>(typesVlw),
// 						std::get<bpe_vlw_value_index>(typesVlw)
// 					) = combine_bword(prepared_block[index], effective_mask);
// 					std::tie(
// 						std::get<bpe_vlw_length_index>(signsVlw),
// 						std::get<bpe_vlw_value_index>(signsVlw)
// 					) = combine_bword(block_signs[i + j], (prepared_block[index] & effective_mask));
// 
// 					addVlwToCollection(typesVlw);
// 					block_states[i + j].bplane_mask_transit |= effective_mask;
// 				};
// 		}
// 	}
// }
// for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {}



// template <typename T, size_t alignment>
// void BitPlaneEncoder<T, alignment>::kOptimal(const segment<T>& input, obitwrapper<size_t>& output_stream) {
// 	// TODO: check N=1 option
// 	// implement heuristic k computation
// 	//
// 	// segment<T> input;
// 	// obitwrapper<size_t> output_stream;
// 
// 	constexpr size_t max_N_value = 10;
// 	size_t N = input.bdepthDc - input.q; // N is expected to be in range [1, 10]
// 	// N is a property of a whole segment. 
// 	// Therefore k_bound value is the same for every gaggle in the segment
// 	ptrdiff_t k_bound = ((ptrdiff_t)(N)) - 2;
// 
// 	// compute lengths for all gaggles for all k encoding parameters;
// 	// k has at most 9 different values for encoded data (0000 - 1000).
// 	// max bit length is 2^10 - 1 (max DC value) * 16 (max gaggle length)
// 	//	+ 16 additional bits (for '1' bit at the end of variable length sequence)
// 	//	= 16384. That requires 14 bits; 2-byte integer may be used
// 	// 
// 
// 	// For implementations with small-width simd instructions computing all 
// 	// possible stream lengths for all k values may be redundant and may
// 	// hurt performance; in this case well-predictable if branch is needed.
// 	//
// 
// 	constexpr size_t gaggle_len = 16;
// 	constexpr size_t gaggle_scale_factor = 2;
// 
// 	// zero placehoder for DC reference samble. 
// 	// zero remaining placeholders for dc elements at the end of the segmnet to extend 
// 	// the last gaggle to len=16
// 	{
// 		input.quantizedDc[-1] = 0;
// 		constexpr size_t gaggle_mask = gaggle_len - 1;
// 		ptrdiff_t dc_boundary = input.size + ((~input.size) & gaggle_mask);
// 		for (ptrdiff_t i = input.size; i < dc_boundary; ++i) {
// 			input.quantizedDc[i] = 0;
// 		}
// 	}
// 
// 	constexpr size_t k_simd_width = 8;
// 	std::vector<std::pair<size_t, ptrdiff_t>> k_options((input.size >> 4) + (gaggle_scale_factor * 2)); // extended just in case
// 
// 	uint_least16_t simdr_k_bound_mask[k_simd_width] = { 0 };
// 	for (ptrdiff_t i = k_bound; i < k_simd_width; ++i) {
// 		// clear sign bit to make comparison operate correctly later
// 		simdr_k_bound_mask[i] = ((uint_least16_t)((ptrdiff_t)(-1))) >> 1;
// 	}
// 	constexpr uint_least16_t simdr_shift[k_simd_width] = { 1, 2, 3, 4, 5, 6, 7, 8 };
// 	// requires input.quantizedDc to have gaggle_len * gaggle_scale_factor additional elements at the end (at most)
// 	for (ptrdiff_t i = 0; i < input.size; i += gaggle_len * gaggle_scale_factor) {
// 		uint_least16_t simdr_acc[gaggle_scale_factor][k_simd_width] = { 0 };
// 		uint_fast16_t gpr_acc[gaggle_scale_factor] = { 0 };
// 
// 		// attempt to increase pipline load uniformity; repeat uniform operation blocks several times
// 		// should be beneficial for branch prediction in the second block as well
// 		for (ptrdiff_t j = 0; j < gaggle_scale_factor; ++j) {
// 			for (ptrdiff_t ii = 0; ii < gaggle_len; ++ii) {
// 				uint_fast16_t gpr = input.quantizedDc[i + (j * gaggle_len) + ii - 1]; // -1 here because of reference sample
// 				gpr_acc[j] += gpr;
// 				uint_least16_t simdr[k_simd_width];
// 				for (ptrdiff_t jj = 0; jj < k_simd_width; ++jj) {
// 					simdr[jj] = gpr;
// 					simdr[jj] >>= simdr_shift[jj];
// 					simdr_acc[j][jj] += simdr[jj];
// 				}
// 			}
// 			for (ptrdiff_t jj = 0; jj < k_simd_width; ++jj) {
// 				simdr_acc[j][jj] |= simdr_k_bound_mask[jj];
// 			}
// 		}
// 
// 		ptrdiff_t k_option_index = i << 4;
// 		for (ptrdiff_t j = 0; j < gaggle_scale_factor; ++j) {
// 			// There is no straitforward way to compare with uncoded option reliably due to different 
// 			// uncoded stream length for the first gaggle, subsequent gaggles and the last one. 
// 			// Encoded stream lengths can be compared safely on the other hand as the diff value due to
// 			// additional '1' bit is the same for all k options for a given gaggle.
// 			decltype(k_options)::value_type current_k = { gpr_acc[j], 0 };
// 			// hopefully this can be vectorized or at least well-predicted
// 			for (ptrdiff_t jj = 0; jj < k_simd_width; ++jj) {
// 				if (simdr_acc[j][jj] < current_k.first) {
// 					current_k.first = simdr_acc[j][jj];
// 					current_k.second = simdr_shift[j][jj];
// 				}
// 			}
// 			k_options[k_option_index + j] = current_k;
// 		}
// 
// 		// PMINSW x86-64: vectorized min for 16-bit words
// 		// several simd iterations per one loop cycle for 2+ gaggles that lays continiously
// 		// compute simd bitmask to reflect k_bound to invalidate simd items with index > k_bound (make max signed numbers)
// 		// 
// 		// store gpr and min simd k results in a temporal buffer. Buffer length is known before processing
// 		// (but we need k value ~ index rather then pre-computed length itself)
// 		// 
// 	}
// 
// 	// adjust parameters for the first gaggle (see 4.3.2.8 and 4.3.2.10, figures 4-8 and 4-9)
// 	{
// 		size_t k_bitsize = std::bit_width(N - 1);
// 		constexpr ptrdiff_t k_uncoded_opt = -1;
// 		// take into account '1' bits inserted into the stream after the first part of the codewords
// 		// to make it comparable with encoded stream lengths
// 		size_t k_uncoded_len = N * gaggle_len - gaggle_len;
// 		size_t k_uncoded_len_first = k_uncoded_len + 1 - N; // TODO: ensure that unsigned arithmetic is not reordered
// 
// 		ptrdiff_t gaggle_index = 0;
// 		if (k_options[gaggle_index].first >= k_uncoded_len_first) {
// 			k_options[gaggle_index].second = k_uncoded_opt;
// 		}
// 		output_stream << {k_bitsize, k_options[gaggle_index].second};
// 		output_stream << {N, input.referenceSample};
// 
// 		ptrdiff_t i;
// 		bool execute_preamble = false;
// 		for (i = -1; i < input.size - gaggle_len; i += gaggle_len) {
// 			if (execute_preamble) {
// 				gaggle_index = (i >> 4) + 1;
// 				if (k_options[gaggle_index].first >= k_uncoded_len) {
// 					k_options[gaggle_index].second = k_uncoded_opt;
// 				}
// 				output_stream << {k_bitsize, k_options[gaggle_index].second};
// 			}
// 			execute_preamble = true;
// 
// 			if (k_options[gaggle_index].second != k_uncoded_opt) {
// 				for (ptrdiff_t j = (i < 0); j < gaggle_len; ++j) {
// 					output_stream << {(input.quantizedDc[i + j] >> k_options[gaggle_index].second) + 1, 1};
// 				}
// 			} else {
// 				// WTF is that? I don't remeber the reason I wrote this code
// 				// is it casting signed k value to unsigned alternative of k encoding?
// 				// k_options[gaggle_index].second = (~(((ptrdiff_t)(-1)) << k_bitsize)) + 1;
// 				k_options[gaggle_index].second = N;
// 			}
// 
// 			for (ptrdiff_t j = (i < 0); j < gaggle_len; ++j) {
// 				output_stream << {k_options[gaggle_index].second, input.quantizedDc[i + j]};
// 			}
// 		}
// 
// 		ptrdiff_t last_gaggle_len = input.size - i;
// 		size_t k_uncoded_len_last = N * last_gaggle_len - last_gaggle_len;
// 		if (execute_preamble) {
// 			gaggle_index = (i >> 4) + 1;
// 			if (k_options[gaggle_index].first >= k_uncoded_len_last) {
// 				k_options[gaggle_index].second = k_uncoded_opt;
// 			}
// 			output_stream << {k_bitsize, k_options[gaggle_index].second};
// 		}
// 		execute_preamble = true;
// 
// 		if (k_options[gaggle_index].second != k_uncoded_opt) {
// 			for (ptrdiff_t j = (i < 0); j < last_gaggle_len; ++j) {
// 				output_stream << {(input.quantizedDc[i + j] >> k_options[gaggle_index].second) + 1, 1};
// 			}
// 		} else {
// 			k_options[gaggle_index].second = N;
// 		}
// 
// 		for (ptrdiff_t j = (i < 0); j < last_gaggle_len; ++j) {
// 			output_stream << {k_options[gaggle_index].second, input.quantizedDc[i + j]};
// 		}
// 	}
// }
// 
// template <typename T, size_t alignment>
// void BitPlaneEncoder<T, alignment>::kHeuristic(const segment<T>& input, obitwrapper<size_t>& output_stream) {
// 	size_t N = input.bdepthDc - input.q; // N is expected to be in range [1, 10]
// 	size_t k_bitsize = std::bit_width(N - 1);
// 	constexpr ptrdiff_t k_uncoded_option = -1;
// 	constexpr size_t gaggle_len = 16;
// 	ptrdiff_t J = 15;
// 	for (ptrdiff_t i = 0; i < input.size; i += J) {
// 		if (i > 0) {
// 			J = std::min(gaggle_len, input.size - i);
// 		}
// 		size_t delta = 0;
// 		for (ptrdiff_t j = 0; j < J; ++j) {
// 			delta += input.quantizedDc[i + j];
// 		}
// 
// 		std::array<size_t, 3> predicates1 = {
// 			delta * 64, 
// 			J * 207, 
// 			J * (1 << (N + 5)), 
// 		};
// 		std::array<size_t, 3> predicates2 = {
// 			J * 23 * (1 << N), 
// 			delta * 128, 
// 			(delta * 128) + (J * 49), 
// 		};
// 
// 		ptrdiff_t k = k_uncoded_option;
// 		if (predicate1[0] >= predicate2[0]) {
// 			k = k_uncoded_option;
// 		} else if (predicate1[1] > predicate2[1]) {
// 			k = 0;
// 		} else if (predicate1[2] <= predicate2[2]) {
// 			k = N - 2;
// 		} else {
// 			// TODO: check underflow/arithmetic/input values, potential UB
// 			k = std::bit_width(predicate2[2] / J) - 7;
// 		}
// 
// 		output_stream << { k_bitsize, k };
// 		if (i == 0) {
// 			output_stream << { N, input.referenceSample };
// 		}
// 		if (k != k_uncoded_option) {
// 			for (ptrdiff_t j = 0; j < J; ++j) {
// 				output_stream << { (input.quanitzedDc[i + j] >> k) + 1, 1 };
// 			}
// 		} else {
// 			k = N;
// 		}
// 		for (ptrdiff_t j = 0; j < J; ++j) {
// 			output_stream << { k, input.qiantizedDc[i + j]};
// 		}
// 	}
// }
// 
// template <typename T, size_t alignment>
// void BitPlaneEncoder<T, alignment>::encodeDc(const segment<T>& input) {
// 	obitwrapper<size_t> output_stream;
// 	bool use_heuristic;
// 
// 	// TODO: check if std::max is needed with 1 as a second parameter for N computation
// 	// see 4.3.2.1
// 	using sbdepth_t = std::make_signed_t<decltype(input.bdepthDc)>;
// 	size_t N = std::max(((sbdepth_t)(input.bdepthDc)) - input.q, 1); // N is expected to be in range [1, 10]
// 	if (N > 1) {
// 		if (use_heuristic) {
// 			this->kHeuristic(input, output_stream);
// 		} else {
// 			this->kOptimal(input, output_stream);
// 		}
// 	} else {
// 		// N == 1
// 		for (ptrdiff_t i = 0; i < input.size; ++i) {
// 			output_stream << { N, input.quantizedDc[i] };
// 		}
// 	}
// }

// Initial decoding quantized DC coeffs implementation
// 
// // T bound = this->segments[i].bdepthDc - this->segments[i].q;
// // bound = ((-1) << bound) ^ ((-1) << (bound - 1));
// 
// T mask = ~(-1 << (this->segments[i].bdepthDc - this->segments[i].q));
// for (ptrdiff_t j = 0; j < this->segments[i].size - 1; ++j) {
// 	// T theta = bound - (this->segments[i].quantizedDc[j - 1] ^ (this->segments[i].quantizedDc[j - 1] >> ((sizeof(T) << 3) - 1)));
// 	// T predicate = this->segments[i].quantizedDc[j] - theta;
// 	// T signmask = this->segments[i].quantizedDc[j] >> ((sizeof(T) << 3) - 1);
// 	// T val = this->segments[i].quantizedDc[j];
// 	// T diff = ((-(val & 0x01)) ^ (val >> 1));
// 	// if (predicate > theta) {
// 	// 	diff = -(predicate ^ signmask) + signmask;
// 	// }
// 
// 	T signmask = diffData[j - 1] >> ((sizeof(T) << 3) - 1);
// 	T theta = (diffData[j - 1] ^ (~signmask)) & mask;
// 	T predicate = diffData[j] - theta;
// 	T val = diffData[j];
// 	T diff = ((-(val & 0x01)) ^ (val >> 1));
// 	if (predicate > theta) {
// 		diff = -(predicate ^ signmask) + signmask;
// 	}
// 
// 	diffData[j] = diffData[j - 1] + diff;
// 	this->segments[i].data[j + 1].content[0] |= diffData[j] << this->segments[i].q;
// }
// 
// 
// T mask = ~(-1 << bdepth);
// // norm is either 0 either half of static range
// T norm = (1 << (bdepth & normalize)) >> 1;
// 


// 
// struct BitOrderTranslator {
// private:
// 	std::tuple <std::array<size_t, (1 << 1)>,
// 		std::array<size_t, (1 << 2)>,
// 		std::array<size_t, (1 << 3)>,
// 		std::array<size_t, (1 << 4)>> codes =
// 	{
// 		{ 0x00, 0x01 },
// 		{ 0x00, 0x02, 0x01, 0x03 },
// 		{ 0x00, 0x04, 0x02, 0x06, 0x01, 0x05, 0x03, 0x07 },
// 		{ 0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e, 0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f}
// 	};
// 
// 	std::array<size_t*, 5> mapping = {
// 		std::get<0>(codes).data(),
// 		std::get<0>(codes).data(),
// 		std::get<1>(codes).data(),
// 		std::get<2>(codes).data(),
// 		std::get<3>(codes).data()
// 	};
// public:
// 	void translate(dense_vlw_t& vlw) {
// 		vlw.value = this->mapping[vlw.length][vlw.value];
// 	}
// };


//	constexpr size_t gaggle_mask = items_per_gaggle - 1; // 0x0f
// 	size_t input_size_truncated = input.size & (~gaggle_mask);
// 	size_t last_gaggle_size = input.size & gaggle_mask;
// 
// 	size_t b = input.bdepthAc - 1;
// 	for (; b >= max_subband_shift; --b) {
// 		if (current_bplane.empty()) {
// 			// TODO: the logic below is relevant less significant bits; as per the loop conditions 
// 			// b is guaranteed to be more than or equal to 3, that means at least 4 bitplanes are
// 			// available always. 
// 			// 
// 			// extracts next 4 bplanes at most
// 			size_t bplane_num_to_extract = std::min(b, (decltype(b))(3));
// 			++bplane_num_to_extract;
// 
// 			// fills current_bplane container
// 			bplaneEncode(raw_block_data, raw_block_size, b, bplane_num_to_extract, current_bitplane_bitwrapper);
// 		}
// 
// 		// stage 0
// 		if (b < input.q) {
// 			// q may be less than bdepthAc, e.g.:
// 			// bdepthAc = 3; bdepthDc = 10; bitShift(LL3) = 0
// 			// => 
// 			// q = 2
// 			//
// 			bplaneEncode(input.plainDc.data(), input.size, b, 1, output); // the output buffer can still not be flushed yet
// 		}
// 
// 		// interleaving bpe stages
// 		{
// 			// the words not encoded further:
// 			// (all computed on a dedicated signs data set)
// 			// signs_P
// 			// signs_C0
// 			// signs_C1
// 			// signs_C2
// 			// signs_H00
// 			// signs_H01
// 			// signs_H02
// 			// signs_H03
// 			// signs_H10
// 			// signs_H11
// 			// signs_H12
// 			// signs_H13
// 			// signs_H20
// 			// signs_H21
// 			// signs_H22
// 			// signs_H23
// 			//
// 			// the words encoded further:
// 			// types_P
// 			// types_C0
// 			// types_C1
// 			// types_C2
// 			// types_H00
// 			// types_H01
// 			// types_H02
// 			// types_H03
// 			// types_H10
// 			// types_H11
// 			// types_H12
// 			// types_H13
// 			// types_H20
// 			// types_H21
// 			// types_H22
// 			// types_H23
// 			//
// 			// transition words encoded further:
// 			// tran_B	// but actually max len is 1, therefore never encoded. But still suits this set
// 			// tran_D
// 			// tran_G
// 			// tran_H0
// 			// tran_H1
// 			// tran_H2
// 			//
// 
// 			ptrdiff_t i = 0;
// 			for (; i < input_size_truncated; i += items_per_gaggle) {
// 				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
// 					std::array<decltype(family_masks)::value_type, family_masks.size()> prepared_block = { 0 };
// 					for (ptrdiff_t k = 0; k < family_masks.size(); ++k) {
// 						prepared_block[k] = current_bplane[i + j] & family_masks[k];
// 					}
// 
// 					auto addVlwToCollection = [&](dense_vlw_t& vlw) -> void {
// 					// auto addVlwToCollection = [&](bpe_variadic_length_word& vlw, size_t vlw_len) -> void {
// 						constexpr size_t vlwstreams_index_bias = 2;
// 						size_t vlw_len = vlw.length;
// 						// 
// 						// PERFMARK:	TODO:
// 						// Keep branch. If lambda's operator() gets inlined, then there should be
// 						// enough instruction queue clocks to elide branch prediction. 
// 						// // Hence explicit
// 						// // function parameter for word length to avoid transitive dependincies through
// 						// // vlw structure.
// 						//
// 						if (vlw_len >= 2) {
// 							vlwstreams[vlw_len - vlwstreams_index_bias].put(vlw.value);
// 						}
// 					};
// 
// 					auto accumulateTranVlw = [&](dense_vlw_t& vlw, size_t index, bool skip = false) -> bool {
// 						// Accumulates transition word with next signle bit. Since the word
// 						// is not complete yet, the correspnding stream for length estimation
// 						// is not populated, therefore the resulting symbol should be inserted
// 						// into the stream when the complete word is ready.
// 						//
// 
// 						size_t tran_val = prepared_block[index] > 0;
// 						size_t tran_len = ((bplane_mask[i + j] & family_masks[index]) == 0);
// 						tran_len &= !skip;
// 
// 						// tran_len value is either 0 or 1, and used as both mask and len
// 						vlw.value <<= tran_len;
// 						vlw.value |= (tran_len & tran_val);
// 						vlw.length += tran_len;
// 
// 						return ((tran_val == 0) & (tran_len == 1));
// 					};
// 
// 					auto computeTerminalVlw = [&](dense_vlw_t& typesVlw, dense_vlw_t& signsVlw, size_t index, bool skip = false) {
// 						// All active elements that are not of type 2 (or -1) yet, mask is applicable
// 						// to get values '0' and '1' of dedicated bits
// 						uint64_t effective_mask = (family_masks[index] & (~bplane_mask[i + j])) & (-((int64_t)(!skip)));
// 
// 						typesVlw = combine_bword(prepared_block[index], effective_mask);
// 						// Actually the same as below, but may relief contention for a single item 
// 						// in current_bplane[i + j] and use masked copy instead, that should be just
// 						// used by computing transition word
// 						// 
// 						// ) = combine_bword(current_bplane[i + j], effective_mask);
// 						signsVlw = combine_bword(block_signs[i + j], (prepared_block[index] & effective_mask));
// 
// 						block_states[i + j].bplane_mask_transit |= (prepared_block[index] & effective_mask);
// 					};
// 
// 					// set tran words to completely clean states to mitigate interfering with leftovers
// 					// of the previous bitplane. Also allows write operations only with no need to read
// 					// the old state and elide assosiated memory ops dependencies.
// 					block_states[i + j].tran_B = dense_vlw_t{ 0, 0 };
// 					bool skip_descendants = accumulateTranVlw(block_states[i + j].tran_B, B_index);
// 					computeTerminalVlw(block_states[i + j].types_P, block_states[i + j].signs_P, P_index);
// 					symbol_translator.translate(block_states[i + j].types_P);
// 					addVlwToCollection(block_states[i + j].types_P);
// 
// 					if (!skip_descendants) {
// 						block_states[i + j].tran_D = dense_vlw_t{ 0, 0 };
// 						block_states[i + j].tran_G = dense_vlw_t{ 0, 0 };
// 						for (ptrdiff_t k = 0; k < F_num; ++k) {
// 							// (skip = false) is implied because the case is covered by control flow dependency
// 							// introduced by branch few lines above
// 							bool skip_tran_Gx = accumulateTranVlw(block_states[i + j].tran_D, (k * F_step) + D_offset);
// 							bool skip_tran_Hx = accumulateTranVlw(block_states[i + j].tran_G, (k * F_step) + G_offset, skip_tran_Gx);
// 
// 							computeTerminalVlw(block_states[i + j].types_C[k], block_states[i + j].signs_C[k],
// 								(k * F_step) + C_offset, skip_tran_Gx);
// 							symbol_translator.translate(block_states[i + j].types_C[k]);
// 							addVlwToCollection(block_states[i + j].types_C[k]);
// 
// 							block_states[i + j].tran_H[k] = dense_vlw_t{ 0, 0 };
// 							for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
// 								bool skip_Hx = accumulateTranVlw(block_states[i + j].tran_H[k], (k * F_step) + Hx_offset + l, skip_tran_Hx);
// 								computeTerminalVlw(block_states[i + j].types_H[k][l], block_states[i + j].signs_H[k][l],
// 									(k * F_step) + Hx_offset + l, skip_Hx);
// 								symbol_translator.translate<decltype(symbol_translator)::types_H_codeparam>(
// 									block_states[i + j].types_H[k][l]);
// 								addVlwToCollection(block_states[i + j].types_H[k][l]);
// 							}
// 						}
// 					}
// 
// 					symbol_translator.translate<decltype(symbol_translator)::tran_D_codeparam>(block_states[i + j].tran_D);
// 					addVlwToCollection(block_states[i + j].tran_D);
// 					symbol_translator.translate(block_states[i + j].tran_G);
// 					addVlwToCollection(block_states[i + j].tran_G);
// 					for (size_t k = 0; k < block_states[i + j].tran_H.size(); ++k) {
// 						symbol_translator.translate<decltype(symbol_translator)::tran_H_codeparam>(block_states[i + j].tran_H[k]);
// 						addVlwToCollection(block_states[i + j].tran_H[k]);
// 					}
// 				}
// 
// 				{
// 					std::array<std::pair<size_t, ptrdiff_t>, vlwstreams.size()> optimal_code_options = { {
// 						{ vlwstreams[0]._size() * 2, -1 },
// 						{ vlwstreams[1]._size() * 3, -1 },
// 						{ vlwstreams[2]._size() * 4, -1 }
// 					} };
// 
// 					auto update_optimal_code_option = [&](size_t index, size_t blen, size_t option) -> void {
// 						size_t current_bitsize = 0;
// 						while (vlwstreams[index]) {
// 							current_bitsize += entropy_translator.word_length(vlwstreams[index].get(), blen, option);
// 						}
// 						if (current_bitsize < optimal_code_options[index].first) {
// 							optimal_code_options[index] = { current_bitsize, option };
// 						}
// 						vlwstreams[index].restart();
// 					};
// 
// 					update_optimal_code_option(0, 2, 0);
// 					update_optimal_code_option(1, 3, 0);
// 					update_optimal_code_option(1, 3, 1);
// 					update_optimal_code_option(2, 4, 0);
// 					update_optimal_code_option(2, 4, 1);
// 					update_optimal_code_option(2, 4, 2);
// 
// 					for (ptrdiff_t k = 0; k < vlwstreams.size(); ++k) {
// 						constexpr size_t codeoptions_bias = 2;
// 						gaggle_states[i >> 4].entropy_codeoptions[k + codeoptions_bias] = optimal_code_options[k].second;
// 						gaggle_states[i >> 4].code_marker_written[k + codeoptions_bias] = false;
// 						vlwstreams[k].reset();
// 					}
// 				}
// 			}
// 			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {}
// 			
// 			// The code below writes the computed words to the output stream. Per 4.5.3.1.7 and 
// 			// 4.5.3.1.8 some words shall not be written to the output stream upon certain 
// 			// conditions dependent on other words' states. The stages encoding code section 
// 			// below ignores these dependencies as they are already taken into accoung when 
// 			// the words are computed (code section above), and all the words that shall not 
// 			// be written to the stream have length 0, that is, these words are null-words. 
// 			// All the words are initialized with length 0 in the beginning, and then are 
// 			// eventually computed when necessary on a later bitplane. Once computed, they're 
// 			// computed in a subsequent bitplanes also, until evaluate to null-word in some 
// 			// less signigicant bitplane.
// 			//
// 
// 			auto translateEncodeVlw = [&](dense_vlw_t& vlw) -> void {
// 				// TODO: warning, i is captured by reference
// 				size_t vlw_length = vlw.length;
// 				ptrdiff_t codeoption = gaggle_states[i >> 4].entropy_codeoptions[vlw_length];
// 				if (!gaggle_states[i >> 4].code_marker_written[vlw_length]) {
// 					// for word length 0 and 1 predicate is always false, should
// 					// never be executed for these lengths
// 					gaggle_states[i >> 4].code_marker_written[vlw_length] = true;
// 					size_t codeoption_bitsize = std::bit_width((size_t)(vlw_length - 1));
// 					output << vlw_t{ codeoption_bitsize, (vlw_t::type)(codeoption) };
// 				}
// 				output << entropy_translator.translate(vlw, codeoption);
// 			};
// 
// 			// stage 1
// 			i = 0;
// 			for (; i < input_size_truncated; i += items_per_gaggle) {
// 				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
// 					translateEncodeVlw(block_states[i + j].types_P);
// 					output << block_states[i + j].signs_P;
// 				}
// 			}
// 			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {}
// 
// 			// stage 2
// 			i = 0;
// 			for (; i < input_size_truncated; i += items_per_gaggle) {
// 				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
// 					output << block_states[i + j].tran_B;
// 					translateEncodeVlw(block_states[i + j].tran_D);
// 					for (ptrdiff_t k = 0; k < F_num; ++k) {
// 						translateEncodeVlw(block_states[i + j].types_C[k]);
// 						output << block_states[i + j].signs_C[k];
// 					}
// 				}
// 			}
// 			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {}
// 
// 			// stage 3
// 			i = 0;
// 			for (; i < input_size_truncated; i += items_per_gaggle) {
// 				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
// 					translateEncodeVlw(block_states[i + j].tran_G);
// 					for (ptrdiff_t k = 0; k < F_num; ++k) {
// 						translateEncodeVlw(block_states[i + j].tran_H[k]);
// 						for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
// 							translateEncodeVlw(block_states[i + j].types_H[k][l]);
// 							output << block_states[i + j].signs_H[k][l];
// 						}
// 					}
// 				}
// 			}
// 			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {}
// 
// 			// stage 4
// 			for (ptrdiff_t j = 0; j < input.size; ++j) {
// 				output << combine_bword(current_bplane[j], bplane_mask[j]);
// 				bplane_mask[j] |= block_states[j].bplane_mask_transit;
// 				block_states[j].bplane_mask_transit = 0;
// 			}
// 		}
// 		{
// 			// fast forward to the next bplane, will make the container empty
// 			// if the current bplane was the last extracted.
// 			// Since the elements at the beginning only are erased, no moves/copies
// 			// occur and no iteraters are invalidated (as guaranteed by std::deque)
// 
// 			current_bplane.erase(current_bplane.begin(), current_bplane.begin() + input.size);
// 		}
// 	}