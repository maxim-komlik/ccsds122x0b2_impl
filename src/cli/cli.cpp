#include <string_view>
#include <vector>
#include <iostream>
#include <iterator>
#include <algorithm>

#include "parameters_context.hpp"
#include "parsers/flag_parser.hpp"
#include "parameters/root.hpp"
#include "exception/exception.hpp"
#include "help_context.hpp"

cli::parameters::cmd_parameters parse_command_line_parameters(std::vector<std::string_view>& tokens);
void print_help_message(const help_context& help_cx);

int main(int argc, char** argv) {
	std::vector<std::string_view> tokens;
	tokens.reserve(argc);

	using namespace std::literals;
	constexpr std::string_view tokens_terminator = {};
	tokens.emplace_back(tokens_terminator);	// put some unparsible token as a terminator

	for (ptrdiff_t i = argc - 1; i > 0; --i) {
		tokens.emplace_back(argv[i]);
	}

	const auto tokens_original = tokens;

	cli::parameters::cmd_parameters parsing_result;

	try {
		auto result = parse_command_line_parameters(tokens);
	}
	catch (const cli::parsing_aborted& e) {
		std::cout << "Command line parsing stopped here:" << std::endl;
		auto last_token_it = std::make_reverse_iterator(std::next(tokens_original.cbegin(), tokens.size()));
		std::for_each(tokens_original.crbegin(), last_token_it,
			[](std::string_view item) -> void {
				std::cout << item << " ";
			});

		std::cout << "{" << tokens.back() << "} <--" << std::endl;

		return 1;
	} catch (const cli::exception& e) {
		std::cout << "Unhandled cli exception: " << std::endl;
		std::cout << '\t' << e.what() << std::endl;

		return 2;
	} catch (const std::exception& e) {
		std::cout << "Unhandled std exception: " << std::endl;
		std::cout << '\t' << e.what() << std::endl;

		return 3;
	} catch (...) {
		std::cout << "Unhandled exception of unknown type. " << std::endl;

		return 4;
	}

	return 0;
}


cli::parameters::cmd_parameters parse_command_line_parameters(std::vector<std::string_view>& tokens) {
	constexpr size_t exhausted_tokens_collection_size = 1; // due to unparsible token at tokens[0]

	constexpr char tab = '\t';

	try {
		// for second level error detail description, dispatched by exception type
		try {
			auto result = cli::parameters::cmd_parser::parse(tokens);

			if (tokens.size() != exhausted_tokens_collection_size) {
				std::string unknown_param = static_cast<std::string>(tokens.back());
				throw cli::parameter_unknown(std::move(unknown_param));
			}

			return result;
		} catch (const cli::help_requested& e) {
			print_help_message(e.get_help_context());
			throw;
		} catch (const cli::context_exit_requested& e) {
			throw;
		} catch (const cli::parsing_error& e) {
			std::cout << "Parameter " << e.parameter_name() << " parsing error: " << std::endl;
			throw;
		}
	} 
	// detailed error handling below:
	catch (const cli::parameter_mandatory& e) {
		std::cout << tab << e.what() << std::endl;
		throw cli::parsing_aborted{};
	} catch (const cli::parameter_invalid& e) {
		std::cout << tab << e.what() << std::endl;
		std::cout << tab << "Value passed: " << std::endl;
		std::cout << tab << tab << e.parameter_value() << std::endl;
		std::cout << tab << "Reason: " << std::endl;
		std::cout << tab << tab << e.reason() << std::endl;
		throw cli::parsing_aborted{};
	} catch (const cli::parameter_unknown& e) {
		std::cout << tab << e.what() << std::endl;
		throw cli::parsing_aborted{};
	}
}

void print_help_message(const help_context& help_cx) {
	constexpr char tab = '\t';
	size_t tab_size = 4; // should be configurable?

	std::cout << std::endl;
	if (help_cx.param_help) {
		const auto& param_desc = help_cx.param_help.value();

		std::cout << "Parameter";
		if (param_desc.if_mandatory) {
			std::cout << " (mandatory)";
		}
		std::cout << ": " << std::endl;

		std::cout << tab << tab << param_desc.name << " " << param_desc.placeholder << std::endl;

		std::cout << tab << param_desc.description << std::endl;
		std::cout << tab << param_desc.requirements << std::endl;

		if (!param_desc.if_mandatory) {
			std::cout << tab << tab << "default value: " << param_desc.default_value;
		}
		std::cout << std::endl;
	}
	
	// context parameters helper
	auto print_parameter_help = [tab_size](
			const help_context::description& item, size_t tab_offset) -> void {
		std::cout << tab << item.name << " ";
		size_t tabs_written = (item.name.size() + 1) / tab_size;
		for (; tabs_written < tab_offset; ++tabs_written) {
			std::cout << tab;
		}
		std::cout << item.description << std::endl;

		if (!item.if_mandatory) {
			std::cout << tab;
			for (ptrdiff_t i = 0; i < tab_offset; ++i) {
				std::cout << tab;
			}
			std::cout << "(default: " << item.default_value << ") " << std::endl;
		}
	};

	if (help_cx.cx_help) {
		const auto& cx_desc = help_cx.cx_help.value();

		std::cout << "Context: " << std::endl;
		std::cout << tab << tab << cx_desc.overview << std::endl;
		


		bool if_has_immediates = std::any_of(cx_desc.parameters_description.cbegin(),
			cx_desc.parameters_description.cend(),
			[](const auto& item) -> bool {
				return item.if_positional;
			});


		bool if_has_named = std::any_of(cx_desc.parameters_description.cbegin(),
			cx_desc.parameters_description.cend(),
			[](const auto& item) -> bool {
				return !item.if_positional & !item.if_vla;
			});

		if (if_has_immediates | if_has_named) {
			std::cout << std::endl;
			std::cout << "Parameters: " << std::endl;
		}

		if (if_has_immediates) {
			const auto& longest_name_param = std::max_element(cx_desc.parameters_description.cbegin(), 
				cx_desc.parameters_description.cend(), 
				[](const auto& lhs, const auto& rhs) -> bool {
					bool result = true;
					result &= rhs.if_positional;
					result &= !(!(lhs.name.size() < rhs.name.size()) & lhs.if_positional);

					//			p1		!p1
					//	p2	  s1<s2		t	
					// !p2		f		f	
					//
					// same as below:
					// 
					//	if (lhs.if_positional & rhs.if_positional) {
					//		return lhs.name.size() < rhs.name.size();
					//	} else {
					//		// the positional one is greater
					//		// 
					//		// p1,p2	-		handled above
					//		// p1,!p2	false	lhs is greater
					//		// !p1,!p2	false	equal
					//		// !p1,p2	true	rhs is greater
					//		//		=> p2
					//		//		or p1<p2 for branched version
					// 
					//		return rhs.if_positional;
					//	}

					return result;
				});
			
			size_t tab_offset = (longest_name_param->name.size() + 1 + (tab_size - 1)) / tab_size; // + 1 for trailing space
			for (const auto& item : cx_desc.parameters_description) {
				if (!item.if_positional) {
					continue;
				}
				print_parameter_help(item, tab_offset);
			}

			std::cout << std::endl;
		}

		if (if_has_named) {
			const auto& longest_name_param = std::max_element(cx_desc.parameters_description.cbegin(), 
				cx_desc.parameters_description.cend(), 
				[](const auto& lhs, const auto& rhs) -> bool {
					bool result = true;
					result &= !rhs.if_positional;
					result &= (lhs.name.size() < rhs.name.size()) | lhs.if_positional;

					//		p1		!p1
					//	p2	f		f	
					// !p2	t	  s1<s2	
					//
					// same as below:
					// 
					//	if (!lhs.if_positional & !rhs.if_positional) {
					//		return lhs.name.size() < rhs.name.size();
					//	} else {
					//		// the named one is greater
					//		// 
					//		// !p1,!p2	-		handled above
					//		// !p1,p2	false	lhs is greater
					//		// p1,p2	false	equal
					//		// p1,!p2	true	rhs is greater
					//		//		=> !p2
					//	
					//		return !rhs.if_positional;
					//	}

					return result;
				});


			size_t tab_offset = (longest_name_param->name.size() + 1 + (tab_size - 1)) / tab_size; // + 1 for trailing space
			for (const auto& item : cx_desc.parameters_description) {
				if (item.if_positional) {
					continue;
				}
				print_parameter_help(item, tab_offset);
			}

			std::cout << std::endl;
		}
	}

	if (help_cx.global_cx_help) {
		const auto& cx_global_desc = help_cx.global_cx_help.value();

		std::cout << "Options recognized globally: " << std::endl;

		const auto& longest_name_param = std::max_element(cx_global_desc.parameters_description.cbegin(),
			cx_global_desc.parameters_description.cend(),
			[](const auto& lhs, const auto& rhs) -> bool {
				return lhs.name.size() < rhs.name.size();
			});

		size_t tab_offset = (longest_name_param->name.size() + 1 + (tab_size - 1)) / tab_size; // + 1 for trailing space
		for (const auto& item : cx_global_desc.parameters_description) {
			print_parameter_help(item, tab_offset);
		}
	}
}

// ccsds122x0b2_impl:
//	help
//	compress
//	restore
// 
// compress:
//	= parameters:
//		source data type
//		source data path
//		destination data type
//		destination data format
//		 signed image
//		dwt type
//		custom shifts
//		 segment params:
//		  segment id
//		  segment size
//		  heuristic DC
//		  heuristic AC bdepth
//		compression params:
//		 byte limit
//		 DC stop
//		 bplane stop
//		 stage stop
//		 fill
//		transpose 
// 
// 
// restore:
//		source data type
//		source data format [deducible]
//		source data path
//		destination data type
//		destination data format
//		destination data path
// 
// 
// compress:source data type
//		- describes input type
//	generated noise
//		+ image dimensions
//		+ image channel depth
//		+ channel num
//		+ generation seed
//		+ image underlying type/signness
//		+ constant fill value?
//	[
//		file:
//			bmp
//		network:
//				- but what the reason? to be dumped to filesystem anyway, no rationale to try to dump to process memory
//			download url
//			format
//	]
// 
// compress:source data path
//		- describes input location, opaque value used as parameter for acquisition logic identified by source type
//		? may be merged with [source data type] by use of uri/url
//	[alphanumeric character string]
// 
// compress:destination data type
//		- describes sink type
//		! may have subcommand parameters
//	file
//	memory
//	[
//		network
//	]
//	
// compress:destination data format
//		- describes protocol
//		! destination paths are generated by implementation
//		! may have subcommand parameters
//	file
//	standard
//	memory
// 
// compress:signed image
//		- describes whether interpret input pixels intensity values as signed integers
//		* default: false
//	# flag
// 
// compress:dwt type
//		- determines dwt type for transformation
//		# default: int
//	int
//	fp
// 
// compress:custom shifts
//		- set/array of values representing custom shifts
//	[0-3]*10
//		e.g. 3332221110 (?)
// 
// compress:segment size
//		# default: 0
//	[non-negative integer]
// 
// compress:byte limit
//	[positive integer]
// 
// compress:DC stop
//		# default: false
//	# flag
// 
// compress:bplane stop
//		# default: 0
//	[non-negative integer]
// 
// compress:stage stop
//		# default: 4
//	[re{0-4} / integer 0-4]
// 
// compress:heuristic DC
//		# default: false
//	# flag
// 
// compress:heuristic AC bdepth
//		# default: false
//	# flag
// 
// compress:fill
//		# default: false
//	# flag
// 
// compress:transpose
//		# default: false
//		! action performed by client cli code that parses the image, before the image is passed to core implementation
//	# flag
// 
// 
// 
// token 
//	-> [classification/match] {context-dependent, passes to parent context on failure, maybe finalization is handled}
//		-> [immediate parameters lexical parse]
//		-> [match]
// 
//	lexical parse implies that requirements/constraints exist, that is, token is classified already
// 
// parsing context:
//	1. immediate/anonimous arguments, lexical restriction apply
//		= identified by position
//		= every argument is mandatory
//			=> if default value is desirable, make it ordinary named parameter
//	2. parameters
//		does parameter parsing create child context?
//	3. global tokens:
//		+ optional context exit separator, '--'
//			= participates in token parsing only if parameter list is not empty for current context
//				or for variable length immediate arguments array
//		+ help description for current context, '--help'
// 
// 
// 
// compress:segment
//		[variable length argument array]
//	start id
//  end id (?)
//  channel id
//  segment size
//  heuristic DC
//  heuristic AC bdepth
// 
// compress:compress params
//		[variable length argument array]
//  start id
//  end id (?)
//  channel id
//  byte limit
//  DC stop
//  bplane stop
//  stage stop
//  fill
// 
// 
// all parameter names and types are known at compile time. control flow is static, no dynamic dispatch.
// 
// 
// 
//


// for some parameters "unspecified" means no value really, not some kind of default. 
// E.g.:
//	compress:shifts: it has defaults defined for integer transform, but for fp transform the parameter 
//		is not valid.
//	compress:generate_noise: if not specified, no values for generation are valid and no action taken.
//	any root parameter/command: no values are valid if not specified
// 
// So it is needed to wrap them all into optional somehow?
// 
// 
// generation is data source type, all generation parameters are optional data source type parsing 
// parameters; similar parameters are allowed for any other data source type. I.e. data source type 
// parameters are variants.
// 
// destination data format parameters:
// destination data format is structure, it has main enum member and variant parameters member, value 
// of parameters depends on enum value parsed. 
// destination data format parsing context will have tuple item 
// 
// 
// parser == type representation of logic that parses tokens into constrained data objects
// context == set of valid tokens
// structure == logically joint set of mutually constrained values, i.e. invariant
// tuple == set of storage items for tokens of the current context, for the purpose of context analysis, e.g. 
//		defaults assignment for unspecified values
// 

// parsing immediates: 
//	parsed in order:
//		if parsing fails, and 
//			if no default defined, then fatal error issued. 
//				No more parsing makes sense, we need diagnostics for user. 
//				throw exception invalid_command_line? Pass context-dependent parsing error details in 
//				exception message, other error description is common: invalid tokens at position [pos],
//				print parsed tokens and mark/highlight erroneus one.
//			otherwise assign default and continue successful parsing scenario.
//		otherwise continue parsing next immediate parameter in the sequence.
//			if no immediate parameters left, start parsing named parameters.
// 
// can same parsers be used in named context and in immediate context?
// named context parsing is done by the keys first, and key match search failure is not an error, but 
// rather end of context mark.
// 
//
