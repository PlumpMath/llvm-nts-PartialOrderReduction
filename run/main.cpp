#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>

#include <llvm2nts/llvm2nts.hpp>
#include "../src/nts-seq.hpp"
#include "optionparser.h"


using std::cout;
using std::cerr;
using std::ofstream;
using std::ostream;
using std::string;
using std::unique_ptr;

using namespace nts;

enum Option
{
	Output,
	InlOutput,
	N_Threads,
	Help,
	NoPOR,
	Unknown
};

struct Arg: public option::Arg
{
	static option::ArgStatus Numeric(const option::Option& option, bool msg)
	{
		char* endptr = 0;
		if (option.arg != 0 && strtol(option.arg, &endptr, 10)){};
		if (endptr != option.arg && *endptr == 0)
			return option::ARG_OK;

		if (msg) std::cerr << "Option '" << string ( option.name ) << "' requires a numeric argument\n";
		return option::ARG_ILLEGAL;
	}

	static option::ArgStatus Required(const option::Option& option, bool msg)
	{
		if (option.arg != 0)
			return option::ARG_OK;

		if (msg) std::cerr << "Option '" << string ( option.name ) << "' requires an argument\n";
		return option::ARG_ILLEGAL;
	}
};

const option::Descriptor usage[] = 
{
	{ Option::Unknown,   0,  "",               "", Arg::None,     "Usage: run [options] file.ll\n\n"
		"Options:"                          },
	{ Option::Output,    0, "o",         "output", Arg::Required, "  --output, -o       Where to write sequentialized nts." },
	{ Option::InlOutput, 0,  "", "inliner-output", Arg::Required, "  --inliner-output   Where to write inlined nts (mainly for debug purposes)" },
	{ Option::N_Threads, 0,  "",        "threads", Arg::Numeric,  "  --threads          Number of threads in thread pool" },
	{ Option::NoPOR,     0,  "",         "no-por", Arg::None,     "  --no-por           Do not use Partial Order reduction " },
	{ Option::Help,      0, "h",           "help", Arg::None,     "  --help             Print usage and exit." },
	{ Option::Unknown,   0,  "",               "", Arg::None,     "\nExample: run -o seq.nts parallel.ll" },

	{ 0, 0, 0, 0, 0, 0 }
};

int main ( int argc, char **argv )
{
	argc-=(argc>0); argv+=(argc>0); // skip program name argv[0] if present
	option::Stats  stats(usage, argc, argv);
	option::Option options[stats.options_max], buffer[stats.buffer_max];
	option::Parser parse(usage, argc, argv, options, buffer);

	if (parse.error())
	{
		cerr << "Error parsing arguments\n";
		return 1;
	}

	if (options[Option::Help] || argc == 0) {

		option::printUsage ( std::cout, usage );
		return 0;
	}

	llvm2nts_options opts;
	opts.thread_poll_size = 1;

	if ( options[N_Threads] )
	{
		std::stringstream ss ( options[N_Threads].arg );
		ss >> opts.thread_poll_size;
	}

	SeqMode mode = SeqMode::PartialOrderReduction;
	if ( options[NoPOR ] )
		mode = SeqMode::Simple;


	if ( parse.nonOptionsCount() != 1 )
	{
		option::printUsage ( cout, usage );
		return 0;
	}

	std::string filename = parse.nonOption ( 0 );

	ostream * out = & cout;
	ofstream fout;
	if ( options[Output] )
	{
		fout.open ( options[Output].arg );
		out = &fout;
	}

	
	unique_ptr < Nts > nts = llvm_file_to_nts ( filename, & opts );
	if ( ! nts )
	{
		cerr << "Can not convert llvm file to nts\n";
		return 1;
	}
	inline_calls_simple ( *nts );

	if ( options[Option::InlOutput] )
	{
		ofstream fino ( options[Option::InlOutput].arg );
		fino << *nts;
		fino.close();
		//cout << *nts;
	}

	unique_ptr < Nts > result = sequentialize (
			* nts,
			mode
			);

	if ( ! result )
	{
		cerr << "An error happened\n";
		return 2;
	}


	*out << *result;
	if ( fout.is_open() )
		fout.close();

	return 0;
}

