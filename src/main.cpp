#include <iostream>
#include <utility>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>

#include <llvm2nts/llvm2nts.hpp>

#include "control_flow_graph.hpp"

using std::cout;
using std::unique_ptr;

using namespace nts;

enum class SerializationMode
{
	Simple,
	PartialOrderReduction
};

unique_ptr < Nts > serialize ( Nts & n, SerializationMode mode )
{
	ControlFlowGraph * cfg = nullptr;
	switch ( mode )
	{
		case SerializationMode::Simple:
			cfg = ControlFlowGraph::build ( n, SimpleVisitor_generator );
			break;

		case SerializationMode::PartialOrderReduction:
			cfg = ControlFlowGraph::build ( n, POVisitor::generator ( n ) );
			break;
	}
	unique_ptr < Nts > result = cfg->compute_nts();
	delete cfg;
	return result;
}

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
	unique_ptr < Nts > result = serialize (
			* nts,
			SerializationMode::PartialOrderReduction
	);
	if ( result )
	{
		cout << "** Serialized:\n";
		cout << * result;
	}

	cout << "done\n";
	return 0;
}

