#include <iostream>
#include <utility>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>
#include <libNTS/variables.hpp>

#include <llvm2nts/llvm2nts.hpp>

#include "tasks.hpp"
#include "logic_utils.hpp"
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
	POVisitor::generator g ( n );
	ControlFlowGraph * cfg = ControlFlowGraph::build ( n, g /* SimpleVisitor_generator */ );
	unique_ptr < Nts > result = cfg->compute_nts();
	delete cfg;
	return result;
#if 0
	switch ( mode )
	{
		case SerializationMode::Simple:
			return serialize_simple ( n );

		case SerializationMode::PartialOrderReduction:
			return reduct ( n );

		default:
			return nullptr;
	}
#endif
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
			SerializationMode::Simple
	);
	if ( result )
	{
		cout << "** Serialized:\n";
		cout << * result;
	}

	cout << "done\n";
	return 0;
}

