#include <iostream>
#include <utility>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>

#include "control_flow_graph.hpp"
#include "nts-seq.hpp"

using std::unique_ptr;

namespace nts
{

using namespace nts::seq;


unique_ptr < Nts > sequentialize ( Nts & n, SeqMode mode )
{
	ControlFlowGraph * cfg = nullptr;
	switch ( mode )
	{
		case SeqMode::Simple:
			cfg = ControlFlowGraph::build ( n, SimpleVisitor_generator );
			break;

		case SeqMode::PartialOrderReduction:
			cfg = ControlFlowGraph::build ( n, POVisitor::generator ( n ) );
			break;
	}
	unique_ptr < Nts > result = cfg->compute_nts();
	delete cfg;
	return result;
}

}
