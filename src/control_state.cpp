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
	cs->reached_from = nullptr;

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

ControlFlowGraph::ControlFlowGraph() :
	states (1000, ControlState::calculate_hash_p) 
{
	;
}

void ControlFlowGraph::explore ( ControlState * cs, unsigned int pid )
{
	const ProcessState & s = cs->states[pid];
	cout << "process " << pid << ": possible " << s.bnts_state->outgoing().size() << " folowers\n";
	for ( Transition *t : s.bnts_state->outgoing() )
	{
		auto cs_new = new ControlState();
		cs_new->states = cs->states; // Copy
		cs_new->states[pid].bnts_state = & t->to();

		cout << "Discovered: ";
		cs_new->print ( cout );
		cout << "\n";

		// Does this state exist?
		auto found = states.find ( cs_new );

		// Found?
		if ( found != states.cend() )
		{
			cout << "Found some older state\n";
			cs->next.push_back ( CFGEdge ( *found, t, pid ) );
			delete cs_new;
		}
		else
		{
			cout << "New state\n";
			cs_new->reached_from = cs;
			cs->next.push_back ( CFGEdge ( cs_new, t, pid ) );
			states.insert ( cs_new );
			unexplored_states.insert ( cs_new );
		}
	}
}

void ControlFlowGraph::explore ( ControlState * cs )
{
	for ( unsigned int i = 0; i < cs->states.size(); i++ )
	{
		const ProcessState & s = cs->states[i];

		// Skip processes without assigned task
		if ( s.bnts_state == nullptr )
			continue;

		explore ( cs, i );
	}
}

bool ControlFlowGraph::explore_next_state()
{
	cout << "Unexplored states: " << unexplored_states.size() << "\n";

	if ( unexplored_states.empty() )
		return false;

	auto b = unexplored_states.begin();
	ControlState * s = *b;
	unexplored_states.erase ( b );
	explore ( s );

	return ! unexplored_states.empty();
}

ControlFlowGraph * ControlFlowGraph::build ( const Nts & n )
{
	ControlFlowGraph * cfg = new ControlFlowGraph();
	ControlState * initial = initial_control_state ( n );
	cfg->states.insert ( initial );
	cfg->unexplored_states.insert ( initial );

#if 0
	// Create new initial state
	ControlState * i2 = initial_control_state ( n );
	auto it = cfg->states.find ( i2 );
	if ( it == cfg->states.cend() )
	{
		cout << "NOT FOUND\n";
	} else {
		cout << "OK\n";
	}
	return nullptr;
#endif

	while ( cfg->explore_next_state() )
		;

	return cfg;
}


