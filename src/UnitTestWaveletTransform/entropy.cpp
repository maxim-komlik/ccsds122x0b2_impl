#include "gtest/gtest.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <type_traits>

#include "../EntropyCoding/EndianCoder.tpp"

TEST(endiannes, EndinanCoderSmoke) {
	typedef unsigned int word_t;
	word_t sample = 0x9c3382ac2ba4f835;
	constexpr word_t expected = (0x35f8a42bac82339c >> ((8 - sizeof(word_t)) * 8));
	word_t actual = EndianCoder<word_t>::apply(sample);
	EXPECT_EQ(actual, expected);
}

TEST(endiannes, EndinanCoderLong) {
	typedef size_t word_t;
	word_t sample = 0x9c3382ac2ba4f835;
	constexpr word_t expected = (0x35f8a42bac82339c >> ((8 - sizeof(word_t)) * 8));
	word_t actual = EndianCoder<word_t>::apply(sample);
	EXPECT_EQ(actual, expected);
}

TEST(endiannes, EndinanCoderB2L) {
	typedef unsigned int word_t;
	word_t sample = 0x35f8a42bac82339c;
	constexpr word_t expected = (0x9c3382ac2ba4f835 >> ((8 - sizeof(word_t)) * 8));
	word_t actual = EndianCoder<word_t>::apply(sample);
	EXPECT_EQ(actual, expected);
}


#include "../EntropyCoding/isymbolstream.h"
#include "../EntropyCoding/EntropyEncoder.tpp"

TEST(entropy, EncoderSmoke) {
	typedef unsigned int obuffer_t;
	//typedef std::fstream stream_t;
	typedef std::wstringstream stream_t;
	obuffer_t test_sequence[6] = {
		0x2ba4f835,
		0x9c3382ac,
		0x8d910e28,
		0x2ba4f835,
		0x9c3382ac,
		0x8d910e28,
	};
	size_t encodedCount = 0;
	//stream_t my_ostream("./buffer.txt");
	stream_t my_ostream;
	my_ostream << std::hex;
	{
		EndianCoder<obuffer_t> endianCoder;
		for (size_t i = 0; i < sizeof(test_sequence) / sizeof(obuffer_t); ++i) {
			test_sequence[i] = endianCoder.apply(test_sequence[i]);
		}

		isymbolstream my_istream(
			std::vector<unsigned char>((unsigned char*)test_sequence,
				(unsigned char*)test_sequence + sizeof(test_sequence)), 2 * sizeof(test_sequence));
		EntropyEncoder<4, 2, obuffer_t, stream_t::char_type> my_encoder(my_ostream);
		my_encoder.translate(my_istream);
		encodedCount = my_encoder.close();
	}
	obuffer_t expected[] = {
		// 0xc7320c9d
		0b1100'0111'0011'0010'0000'1100'1001'1101,
		// 0x4a0fc98c
		0b0100'1010'0000'1111'1100'1001'1000'1100,
		// 0x0204b60b
		0b0000'0010'0000'0100'1011'0110'0000'1011,
		// 0x131cc832
		0b0001'0011'0001'1100'1100'1000'0011'0010,
		// 0x75283f26
		0b0111'0101'0010'1000'0011'1111'0010'0110,
		// 0x300812b8
		0b0011'0000'0000'1000'0001'0010'1101'1000,
		// 0x2c400000
		0b0010'1100'0100'0000'0000'0000'0000'0000
	};

	// std::cout << "Encoded count: " << encodedCount << std::endl;
	size_t index = 0;
	obuffer_t temp;
	my_ostream.read((stream_t::char_type*)&temp, sizeof(temp) / sizeof(stream_t::char_type));
	while (my_ostream) {
		// std::cout << std::hex << temp << " : " << expected[index] << std::endl;
		EXPECT_EQ(temp, expected[index]);
		++index;
		my_ostream.read((stream_t::char_type*)&temp, sizeof(temp) / sizeof(stream_t::char_type));
	}
}

#include "../EntropyCoding/EntropyDecoder.tpp"

TEST(entropy, DecoderSmoke) {
	typedef unsigned int obuffer_t;
	typedef std::wstringstream stream_t;
	obuffer_t test_sequence[6] = {
		0x2ba4f835,
		0x9c3382ac,
		0x8d910e28,
		0x2ba4f835,
		0x9c3382ac,
		0x8d910e28,
	};

	obuffer_t expected[6] = {
		0x2ba4f835,
		0x9c3382ac,
		0x8d910e28,
		0x2ba4f835,
		0x9c3382ac,
		0x8d910e28,
	};

	obuffer_t* actual = nullptr;
	size_t actual_size = 0;
	size_t decodedCount = 0;

	stream_t my_ostream;
	{
		for (size_t i = 0; i < sizeof(test_sequence) / sizeof(obuffer_t); ++i) {
			test_sequence[i] = EndianCoder<obuffer_t>::apply(test_sequence[i]);
		}

		isymbolstream my_istream(
			std::vector<unsigned char>((unsigned char*)test_sequence,
				(unsigned char*)test_sequence + sizeof(test_sequence)), 2 * sizeof(test_sequence));
		EntropyEncoder<4, 2, obuffer_t, stream_t::char_type> my_encoder(my_ostream);
		my_encoder.translate(my_istream);
		my_encoder.close();
	}
	{
		osymbolstream my_osstream;
		EntropyDecoder<4, 2, obuffer_t, stream_t::char_type> my_decoder(my_ostream);
		my_decoder.translate(my_osstream);
		decodedCount = my_decoder.close();

		actual_size = (my_osstream.capacity() + sizeof(obuffer_t) - 1) / sizeof(obuffer_t);
		actual = new obuffer_t[actual_size];
		memcpy(actual, my_osstream.data(), my_osstream.capacity());
		for (size_t i = 0; i < actual_size; ++i) {
			actual[i] = EndianCoder<obuffer_t>::apply(actual[i]);
		}
	}

	// std::cout << "Decoded count: " << decodedCount << std::endl;
	for (size_t i = 0; i < sizeof(expected) / sizeof(*expected); ++i) {
		// std::cout << std::hex << actual[i] << " : " << expected[i] << std::endl;
		EXPECT_EQ(actual[i], expected[i]);
	}
	delete[] actual;
}

#include "../EntropyCoding/entropy_utils.h"
#include "../EntropyCoding/symbol_utils.h"
#include "../EntropyCoding/symbolstream.h"


TEST(entropy, SymbolCycleChainSmoke) {
	typedef size_t buffer_t;
	typedef std::wstringstream stream_t;
	buffer_t test_sequence[] = {
		0x2ba4f8359c3382ac,
		0x8d910e282ba4f835,
		0x9c3382ac8d910e28,
	};

	buffer_t expected[] = {
		0x2ba4f8359c3382ac,
		0x8d910e282ba4f835,
		0x9c3382ac8d910e28,
	};

	buffer_t* actual = nullptr;
	size_t actual_size = 0;
	size_t decodedCount = 0;
	// std::cout << "Stream states: " << std::endl
	// 	<< "\tstream_t::goodbit \t"	<< stream_t::goodbit << std::endl
	// 	<< "\tstream_t::eofbit \t"	<< stream_t::eofbit << std::endl
	// 	<< "\tstream_t::failbit \t"	<< stream_t::failbit << std::endl
	// 	<< "\tstream_t::badbit \t"	<< stream_t::badbit << std::endl << std::endl;


	stream_t bplane_stream;
	{
		symbolstream forward_symstream;
		symbolstream backward_symstream;
		stream_t entropy_stream;

		// TODO: make template specializations (or simply wrappers with redirection) 
		typedef code::code_type<4, 0, buffer_t, decltype(bplane_stream)::char_type> symbolCode_t;
		typedef code::code_type<4, 1, buffer_t, decltype(entropy_stream)::char_type> entropyCode_t;

		// std::cout << "1.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl;

		for (size_t i = 0; i < sizeof(test_sequence) / sizeof(buffer_t); ++i) {
			test_sequence[i] = EndianCoder<buffer_t>::apply(test_sequence[i]);
			bplane_stream.write((decltype(bplane_stream)::char_type*) &(test_sequence[i]),
				sizeof(*test_sequence) / sizeof(decltype(bplane_stream)::char_type));
		}

		// std::cout << "2.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl;

		symbol::encoder<symbolCode_t> forward_symencoder(bplane_stream);
		forward_symencoder.translate(forward_symstream);
		forward_symencoder.close();

		// std::cout << "3.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl;
		bplane_stream.clear();
		// std::cout << "4.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl;

		entropy::encoder<entropyCode_t> forward_bitencoder(entropy_stream);
		forward_bitencoder.translate(forward_symstream);
		forward_bitencoder.close();

		entropy::decoder<entropyCode_t> backward_bitdecoder(entropy_stream);
		backward_bitdecoder.translate(backward_symstream);
		backward_bitdecoder.close();

		symbol::decoder<symbolCode_t> backward_symdecoder(bplane_stream);
		backward_symdecoder.translate(backward_symstream);
		// backward_symdecoder.translate(forward_symstream);
		backward_symdecoder.close();

		// std::cout << "5.\tbplane_stream state: " << bplane_stream.rdstate() << std::endl << std::endl;
		// std::cout << "bplane_stream tellp: \t" << bplane_stream.tellp() << std::endl;
		// std::cout << "bplane_stream tellg: \t" << bplane_stream.tellg() << std::endl;
		size_t bplane_size = ((size_t)bplane_stream.tellp()) - ((size_t)bplane_stream.tellg());
		actual_size = (bplane_size * sizeof(decltype(bplane_stream)::char_type) + sizeof(buffer_t) - 1) / sizeof(buffer_t);
		actual = new buffer_t[actual_size];

		for (size_t i = 0; i < actual_size; ++i) {
			buffer_t temp;
			bplane_stream.read((decltype(bplane_stream)::char_type*) &temp,
				sizeof(*test_sequence) / sizeof(decltype(bplane_stream)::char_type));
			actual[i] = EndianCoder<buffer_t>::apply(temp);
		}
	}

	for (size_t i = 0; i < sizeof(expected) / sizeof(*expected); ++i) {
		// std::cout << std::hex << actual[i] << " : " << expected[i] << std::endl;
		EXPECT_EQ(actual[i], expected[i]);
	}
	delete[] actual;
}