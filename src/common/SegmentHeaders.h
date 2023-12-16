#pragma once
//CCSDS 122.0-B-2
// 
//	Part1A:	3 bytes
//	Part1B:	1 byte
//	Part2:	5 bytes
//	Part3:	3 bytes
//	Part4:	8 bytes

// TODO: check bit order in bitfields regarding to members defeninition order
// TODO: check out default value initialization (maybe C++20)

// Members aredefine in order:
//	starting from most-significant bits at the top of the definition 
//	ending with least-significant bits at the bottom of the definition
// In standard notation: from bit 0 (MSb) to bit N-1 (LSb)
// Transmission is performed starting from MSb.

// Table 4-4, Page 4-7
struct HeaderPart_1A {
public:
	unsigned int StartImgFlag : 1;		// = 0; 	// Flags initial segment in an image
	unsigned int EndImgFlag : 1;		// = 0; 	// Flags final segment in an image
	unsigned int SegmentCount : 8;		// = 0; 	// Segment counter value
	unsigned int BitDepthDC : 5;		// = 0; 	// Number of bits needed to represent DC coefficients in 2’s complement representation
	unsigned int BitDepthAC : 5;		// = 0; 	// Number of bits needed to represent absolute value of AC coefficients in unsigned integer representation
private:
	unsigned int __reserved_001 : 1;	// = 0;	// Check out default value initialization (maybe C++20)
public:
	unsigned int Part2Flag : 1;			// = 0; 	// Indicates presence of Part 2 header
	unsigned int Part3Flag : 1;			// = 0; 	// Indicates presence of Part 3 header
	unsigned int Part4Flag : 1;			// = 0; 	// Indicates presence of Part 4 header
	// 24 bits = 3 bytes
};

// Table 4-4, Page 4-7
struct HeaderPart_1B {
public:
	unsigned int PadRows : 3;			// = 0; 	// Number of ‘padding’ rows to delete after inverse DWT
private:
	unsigned int __reserved_001 : 5;	// = 0; 
	// 8 bits = 1 byte
};

// Table 4-5, Page 4-10
struct HeaderPart_2 {
public:
	unsigned int SegByteLimit : 27;		// = 0; 	// Maximum number of bytes in a coded segment
	unsigned int DCStop : 1; 			// = 0;		// Indicates whether coded segment stops after coding of quantized DC coefficients(4.3)
	unsigned int BitPlaneStop : 5;		// = 0; 	// Unused when DCStop = 1. When DCStop = 0, indicates limit on coding of DWT coefficient bit
		// planes. When BitPlaneStop = b and StageStop = s, coded segment terminates once stage s of bit plane b has been completed(see 4.5),
		// unless coded segment terminates earlier because of the coded segment byte limit(SegByteLimit)
	unsigned int StageStop : 2;			// = 0b11; 	// No description in source, codes stage 1/2/3/4 : 00/01/10/11
	unsigned int UseFill : 1;			// = 0; 	// Specifies whether fill bits will be used to produce SegByteLimit bytes in each coded segment
private:
	unsigned int __reserved_001 : 4;	// = 0; 
	// 40 bits = 5 bytes
};

// Table 4-6, Page 4-13
struct HeaderPart_3 {
public:
	unsigned int S : 20;				// = 0; 	// segment size in blocks
	unsigned int OptDCSelect : 1;		// = 1; 	// Specifies whether optimum or heuristic method is used to select 
		// value of k parameter for		coding quantized DC coefficient values (see 4.3.2)
	unsigned int OptACSelect : 1;		// = 1; 	// Specifies whether optimum or heuristic method is used to select 
		// value of k parameter for coding BitDepthAC(see 4.4)
private:
	unsigned int __reserved_001 : 2;	// = 0; 
	// 24 bits = 3 bytes
};

// Tools for CodeWordLength field of HeaderPart_4 struct
enum CodeWordLengthValues : unsigned int {
	w8bit = 0b000, 
	w16bit = 0b010, 
	w24bit = 0b100, 
	w32bit = 0b110, 
	w40bit = 0b001, 
	w48bit = 0b011, 
	w56bit = 0b101, 
	w64bit = 0b111
};

CodeWordLengthValues getWordLengthValue(const unsigned int bitNum) {
	if (bitNum & 0b0111 || bitNum > 64)
		throw "Invalid word length!";
	unsigned int &&bitNumShifted = bitNum >> 2; // most significant bit is moved to lowest position
	return (CodeWordLengthValues)((bitNumShifted & 0b0111) ^ ((bitNumShifted >> 3) & 0b01));
}

// Table 4-7, Page 4-15
struct HeaderPart_4 {
public:
	unsigned int DWTtype : 1; 			// = 1;		// Specifies DWT type
private: 
	unsigned int __reserved_001 : 1; 	// = 0; 
public: 
	unsigned int ExtendedPixelBitDepthFlag : 1;		// = 0;		// Indicates an input pixel bit depth larger than 16
	unsigned int SignedPixels : 1; 		// = 0; 	// Specifies whether input pixel values are signed or unsigned quantities
	unsigned int PixelBitDepth : 4;		// = 8; 	// Together with ExtendedPixelBitDepth Flag, indicates the input pixel bit depth
	unsigned int ImageWidth : 20;		// = 0; 	// Image width in pixels
	unsigned int TransposeImg : 1;		// = 0; 	// Indicates whether entire image should be transposed after reconstruction
	unsigned int CodeWordLength : 3;	// = w64bit; 	// Indicates the coded word length
	unsigned int CustomWtFlag : 1;		// = 0; 	// Indicates if weights in 3.9 used or user defined
	// Subband weights: {00, 01, 10, 11} => {2^0, 2^1, 2^2, 2^3}
	unsigned int CustomWtHH__1 : 2;		// = 0; 	// Weight of HH__1 subband
	unsigned int CustomWtHL__1 : 2;		// = 0; 	// Weight of HL__1 subband
	unsigned int CustomWtLH__1 : 2;		// = 0; 	// Weight of LH__1 subband
	unsigned int CustomWtHH__2 : 2;		// = 0; 	// Weight of HH__2 subband
	unsigned int CustomWtHL__2 : 2;		// = 0; 	// Weight of HL__2 subband
	unsigned int CustomWtLH__2 : 2;		// = 0; 	// Weight of LH__2 subband
	unsigned int CustomWtHH__3 : 2;		// = 0; 	// Weight of HH__3 subband
	unsigned int CustomWtHL__3 : 2;		// = 0; 	// Weight of HL__3 subband
	unsigned int CustomWtLH__3 : 2;		// = 0; 	// Weight of LH__3 subband
	unsigned int CustomWtLL__3 : 2;		// = 0; 	// Weight of LL__3 subband
private:
	unsigned int __reserved_002 : 11;	// = 0;
	// 64 bits = 8 bytes
};