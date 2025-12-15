#pragma once

#include <memory>

#include "core_types.hpp"

#include "sink.hpp"


// compression context is created as soon as segment data is constructed
// by segment assembler; it owns constructed segment data. Sink and 
// associated protocols are initialized before the segment data is passed 
// to bpe encoder, and constructed obitwrapper of a specific sink is used
// as a parameter of bpe calls.
// 
// all compression contexts are owned by session context and stored as a 
// collection. Compression contexts are strictly ordered (as well as 
// owned segments which have ordered segment indexes). Compression can be 
// performed on several compression contexts simultaneously. Number of 
// compression contexts that exist simultaneously cannot exceed 256 due 
// to SegmentCount field width limitation of 8 bits in segment header 
// part 1a.
// 
// compression contexts in session context's collection hold dependencies.
// Compression settings and segment settings are effective starting from 
// the context they first defined at until the next redefinition in one 
// of the consequent contexts or the very last context in the end of the
// session, whatever comes first.
// 
// when fine-tuning compression parameters transmitted via header part 3 
// after actual compression is done (for example, to hint necessary 
// amount of memory to hold the whole segment via SegByteLimit field), 
// care should be taken to avoid applying the settings to all the 
// consequent compression contexts in the collection transitively. E.g. 
// compression settings for the next compression context should be 
// restored by setting old values explicitly if possible, otherwise 
// such fine-tuning should be discarded.
// 


template <typename dstT>
struct compression_context {
	std::unique_ptr<sink<dstT>> dst; // TODO: template parameter? Would like sink to be plain type, not template
	std::unique_ptr<segment<dstT>> segment_data;
	size_t id;
};
