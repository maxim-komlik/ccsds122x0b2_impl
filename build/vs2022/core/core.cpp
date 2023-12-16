// core.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "framework.h"

// TODO: This is an example of a library function
void fncore()
{
}


// SegmentPreCoder.tpp
// template <typename T, size_t allignment>
// size_t bdepth(bitmap<T> src) {
// 	// alligned segment data
// 	T* segment = nullptr;
// 	size_t segmentSize = 64;
// 	T buffer[allignment] = { 0 };
// 	// size_t vectorizable = segmentSize & (~(allignment - 1));
// 
// 	for (size_t i = 0; i < segmentSize; i += allignment) {
// 		for (size_t j = 0; j < allignment; ++j) {
// 			buffer[j] |= altitude<T>(segment[i + j]);
// 		}
// 	}
// 
// 	for (size_t i = allignment >> 1; i > 0; i >>= 1) {
// 		for (size_t j = 0; j < i; ++j) {
// 			buffer[j] |= buffer[i + j];
// 		}
// 	}
// 	return buffer[0];
// }
