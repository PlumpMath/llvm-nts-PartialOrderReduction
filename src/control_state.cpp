#include <iostream>
#include <stdexcept>
#include <regex>
#include <utility>

#include "tasks.hpp"
#include "control_state.hpp"

using std::hash;
using std::regex;
using std::logic_error;
using std::ostream;
using std::cout;
using std::size_t;

using namespace nts;

//------------------------------------//
// ProcessState                       //
//------------------------------------//

bool ProcessState::operator== ( const ProcessState & other ) const
{
	if ( bnts_state != other.bnts_state )
		return false;

	return true;
}

bool ProcessState::operator!= ( const ProcessState & other ) const
{
	return ( ! ( *this == other ) );
}

// from http://stackoverflow.com/a/21062236
template<typename Tval>
struct MyTemplatePointerHash1 {
	    size_t operator()(const Tval* val) const {
			        static const size_t shift = (size_t)log2(1 + sizeof(Tval));
					        return (size_t)(val) >> shift;
							    }
};

size_t ProcessState::calculate_hash ( const ProcessState & st )
{
	MyTemplatePointerHash1<State> h;
	return h ( st.bnts_state );
}

//------------------------------------//
// ControlState                       //
//------------------------------------//

bool ControlState::operator== ( const ControlState & other ) const
{
	//cout << "Comparing:\n\t";
	//print ( cout );
	//cout << "\n\t";
	//other.print ( cout );
	//cout << "\n";

	if ( states.size() != other.states.size() )
	{
		//cout << "Different size\n";
		return false;
	}

	for ( unsigned int i = 0; i < states.size(); i++ )
	{
		if ( states[i] != other.states[i] )
		{
			//cout << "Process " << i << " missmatch\n";
			return false;
		}
	}

	//cout << "same\n";
	return true;
}

void ControlState::print ( ostream & o ) const
{
	o << "( ";
	if ( states.empty() )
	{
		o << ")";
		return;
	}

	for ( unsigned int i = 0; i < states.size() - 1; i++ )
	{
		o << states[i].bnts_state->name << " | ";
	}

	o << states.back().bnts_state->name << " )";
}

#if 0
ControlState * ControlState::on_stack ( ControlState & other ) const
{
	ControlState * st = & other;
	while ( st )
	{
		if ( *st == *this )
		{
			return st;
		}

		st = st->reached_from;
	}

	return nullptr;
}
#endif

// from http://stackoverflow.com/a/2595226
template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
	    std::hash<T> hasher;
		    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

size_t ControlState::calculate_hash ( const ControlState & cs )
{
	size_t h = 0;
	for ( const ProcessState & p : cs.states )
	{
		hash_combine ( h, ProcessState::calculate_hash ( p ) );
	}
	return h;
}

size_t ControlState::calculate_hash_p ( const ControlState * cs )
{
	size_t h = calculate_hash ( *cs );
	//cout << "Hash of ";
	//cs->print ( cout );
	//cout << " is " << h << "\n";
	return h;
}

#if 0

regex origin_is_initial_thread_create ( "^([a-zA-Z]+\\:[0-9]+\\:)*__thread_create\\:[0-9]+" );

bool calls_thread_create ( const Transition & t )
{
	const AnnotString * as = find_annot_origin ( t.to().annotations );
	// Example:
	// some_function:1:__thread_create:0:si
	// toplevel called some_function at least twice,
	// and now we are in second call.
	// Then some_function called __thread__create first time,
	// and we are in its initial state (named si)
	
	return std::regex_search ( as->value, origin_is_initial_thread_create );
}

#endif

ControlState * initial_control_state ( const Nts & n )
{
	ControlState * cs = new ControlState();

	for ( const Instance * i : n.instances() )
	{
		const BasicNts & bn = i->basic_nts();
		State * initial_state = nullptr;

		for ( State * s : bn.states() )
		{
			if ( s->is_initial() )
			{
				if ( initial_state )
					throw logic_error ( "Only one initial state is supported" );
				initial_state = s;
			}
		}

		for ( unsigned int j = 0; j < i->n; j++ )
			cs->states.push_back ( ProcessState ( initial_state ) );

	}

	return cs;
}

//------------------------------------//
// ControlState::DFSInfo              //
//------------------------------------//

ControlState::DFSInfo::DFSInfo()
{
	this->st= St::New;
	this->reached_from = nullptr;
	this->visited_next = 0;
}

//------------------------------------//
// ControlFlowGraph                   //
//------------------------------------//


ControlFlowGraph::ControlFlowGraph() :
	states (1000, ControlState::calculate_hash_p) 
{
	;
}

ControlFlowGraph::~ControlFlowGraph()
{
	for ( ControlState * x : states )
	{
		delete x;
	}
}

bool ControlFlowGraph::explore_next_edge()
{
	// Go back to top state, which is not closed yet	
	while ( current && current->di.visited_next >= current->next.size() )
	{
		ControlState * up = current->di.reached_from;
		current->di.st = ControlState::DFSInfo::St::Closed;
		current->di.reached_from = nullptr;
		current = up;
	}

	if ( ! current )
		return false;

	// now current->di.visited_next < current->next.size()
	// So visit next
	CFGEdge & edge = current->next[ current->di.visited_next ];
	current->di.visited_next++;

	// Each edge is visited exactly once
	edges.push_back ( & edge );

	if ( _edge_visitor )
		(*_edge_visitor) ( edge );
	
	if ( edge.to.di.st == ControlState::DFSInfo::St::New )
	{
		edge.to.di.st = ControlState::DFSInfo::St::On_stack;
		current = & edge.to;
	}

	return true;
}

bool ControlFlowGraph::has_state ( ControlState & cs ) const
{
	auto found = states.find ( & cs );
	return found != states.cend();
}

ControlState & ControlFlowGraph::insert_state ( ControlState & cs )
{
	auto found = states.find ( & cs );

	if ( found == states.cend() )
	{
		states.insert ( & cs );
		return cs;
	} else {
		delete & cs;
		return **found;
	}
}

ControlFlowGraph * ControlFlowGraph::build ( const Nts & n, const EdgeVisitorGenerator & gen )
{
	ControlFlowGraph * cfg = new ControlFlowGraph();
	cfg->_edge_visitor =  gen ( *cfg );

	ControlState * initial = initial_control_state ( n );
	(*cfg->_edge_visitor) ( CFGEdge ( nullptr, *initial, nullptr, 0 ) );
	cfg->states.insert ( initial );
	cfg->current = initial;

	while ( cfg->explore_next_edge() )
		;

	delete cfg->_edge_visitor;

	cout << "Total states: " << cfg->states.size()
		 << " edges: " << cfg->edges.size() << "\n";

	return cfg;
}

//------------------------------------//
// Simple visitor - no reduction      //
//------------------------------------//

SimpleVisitor * SimpleVisitor_generator ( ControlFlowGraph & g )
{
	return new SimpleVisitor ( g );
}

SimpleVisitor::SimpleVisitor ( ControlFlowGraph & g ) :
	g ( g )
{
	;
}

SimpleVisitor::~SimpleVisitor()
{
	;
}

void SimpleVisitor::operator() ( const CFGEdge & e )
{
	( void ) e;

	switch ( e.to.di.st )
	{
		case ControlState::DFSInfo::St::New:
			cout << "Reached new state. Expanding.\n";
			explore ( e.to );
			break;

		case ControlState::DFSInfo::St::On_stack:
			cout << "Reached state on stack. Ignoring.\n";
			break;

		case ControlState::DFSInfo::St::Closed:
			cout << "Reached closed state. Ignoring.\n";
			break;
	}
}

void SimpleVisitor::explore ( ControlState & cs )
{
	for ( unsigned int i = 0; i < cs.states.size(); i++ )
	{
		const ProcessState & s = cs.states[i];

		// Skip processes without assigned task
		if ( s.bnts_state == nullptr )
			continue;

		explore ( cs, i );
	}
}
void SimpleVisitor::explore ( ControlState & cs, unsigned int pid )
{
	const ProcessState & s = cs.states[pid];
	cout << "process " << pid << ": possible " << s.bnts_state->outgoing().size() << " folowers\n";
	for ( Transition *t : s.bnts_state->outgoing() )
	{
		auto cs_new = new ControlState();
		cs_new->states = cs.states; // Copy
		cs_new->states[pid].bnts_state = & t->to();

		cout << "Discovered: ";
		cs_new->print ( cout );
		cout << "\n";

		ControlState & reached = g.insert_state ( *cs_new );
		cs.next.push_back ( CFGEdge ( & cs, reached, t, pid ) );
	}
}
// TODO: POR visitor.

