#include <iostream>
#include <utility>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>

#include <llvm2nts/llvm2nts.hpp>
#include "../src/nts-seq.hpp"

using std::cout;
using std::unique_ptr;

using namespace nts;

int main ( int argc, char **argv )
{
	if ( argc <= 1 )
	{
		cout << "usage: " << argv[0] << " file.ll\n";
		return 0;
	}

	llvm2nts_options opts;
	opts.thread_poll_size = 1;

	const char * file = argv[1];
	unique_ptr < Nts > nts = llvm_file_to_nts ( file, & opts );
	inline_calls_simple ( *nts );
	cout << *nts;
	unique_ptr < Nts > result = sequentialize (
			* nts,
			SeqMode::Simple
	);
	if ( result )
	{
		cout << "** Serialized:\n";
		cout << * result;
	}

	cout << "done\n";
	return 0;
}

