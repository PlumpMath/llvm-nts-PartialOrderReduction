#include <vector>
#include <iostream>
#include <stdexcept>
#include <regex>
#include <utility>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>
#include <libNTS/sugar.hpp>

#include "tasks.hpp"
#include "control_state.hpp"

using std::hash;
using std::regex;
using std::logic_error;
using std::ostream;
using std::cout;
using std::size_t;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

using namespace nts;
using namespace nts::sugar;

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
	    size_t operator()(const Tval* val) const
		{
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

void ControlState::create_nts_state ( string name )
{
	if ( nts_state )
		throw std::logic_error ( "Already have nts state" );

	nts_state = new State ( move ( name ) );
	// Add some annotations
	
	std::stringstream ss;
	ss << "( ";
	unsigned int count = states.size();
	for ( const ProcessState & ps : states )
	{
		--count;
		AnnotString * as = find_annot_origin ( ps.bnts_state->annotations );
		if ( ! as )
			ss << "-";
		else
			ss << as->value;
		
		if ( count != 0 )
			ss << " | ";
	}

	ss << " )";

	AnnotString * as = new AnnotString ( "origin", ss.str());
	//cout << "it was named: " << as->value << "\n";
	as->insert_to ( nts_state->annotations );
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


ControlFlowGraph::ControlFlowGraph ( const Nts & orig_nts ) :
	original_nts ( orig_nts ),
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

	edge.to.di.reached_from = current;

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

ControlState * ControlFlowGraph::get_state ( ControlState & cs ) const
{
	auto found = states.find ( & cs );

	if ( found != states.cend() )
		return *found;

	return nullptr;
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
	ControlFlowGraph * cfg = new ControlFlowGraph ( n );
	cfg->_edge_visitor =  gen ( *cfg );

	ControlState * initial = initial_control_state ( n );
	cfg->states.insert ( initial );
	(*cfg->_edge_visitor) ( CFGEdge ( nullptr, *initial, nullptr, 0 ) );
	cfg->current = initial;

	while ( cfg->explore_next_edge() )
		;

	delete cfg->_edge_visitor;

	cout << "Total states: " << cfg->states.size()
		 << " edges: " << cfg->edges.size() << "\n";

	return cfg;
}

// ComputeNts variable information
// original variable points to this
struct CNVariableInfo
{
	bool global; //< this was a global variable

	// For local variables. Indexed by thread id.
	// (there may be multiple copies of the same variable ).
	vector < Variable * > instances;

	// For global variables
	Variable * var;

	// Constructor for global variables
	CNVariableInfo ();

	// Constructor for local variables
	CNVariableInfo ( unsigned int processes );
};

CNVariableInfo::CNVariableInfo ( unsigned int processes ) :
	global ( false )
{
	instances.resize ( processes );
}

CNVariableInfo::CNVariableInfo ()
	: global ( true )
{
	;
}

/**
 * @pre  Q1 Given VariableUse shall not be empty
 *       Q2 u->user_data must point to valid CNVariableInfo
 */
void variable_use_switch_by_cnvariableinfo ( const CFGEdge * e,  VariableUse & u )
{
	if ( ! u->user_data )
		return;

	CNVariableInfo * i = static_cast < CNVariableInfo * > ( u->user_data );
	Variable * dest;

	if ( i->global )
		dest = i->var;
	else
		dest = i->instances.at ( e->pid );

	u.set ( dest );
}

/**
 * @brief Returns copy of given variable with prefixed 'origin'
 * @pre  Q1 Given variable must have 'origin' annotation
 */
Variable & clone_with_prefix ( const Variable & v, const string & prefix )
{
	Variable * cl = v.clone();
	AnnotString * as = find_annot_origin ( cl->annotations );
	if ( ! as )
		throw logic_error ( "Precondition Q1 is not met" );
	as->value = prefix + as->value;
	return *cl;
}

/**
 * @brief Clones local variable to given thread
 * @pre  Q1 Variable must have 'origin' annotation
 *       Q2 Variable must have associated valid 'CNVariableInfo' structure
 */
Variable & clone_to_thread ( const Variable & v, string bnts_name, unsigned int tid )
{
	CNVariableInfo * cni = static_cast < CNVariableInfo * > ( v.user_data );
	//@ assert cni->global == false
	
	// Resulting 'origin' have form:
	// "name_of_bnts [ thread_id ] :: original_name"
	string prefix = move ( bnts_name ) + " [ " + to_string ( tid ) + " ] :: ";
	Variable & cl = clone_with_prefix ( v, prefix );
	cni->instances.at(tid) = & cl;
	return cl;
}


class ControlFlowGraph::NtsGenerator
{
	private:
		const ControlFlowGraph & _cfg;

		Nts      * dest_nts;
		BasicNts * dest_bn;

		NtsGenerator ( const ControlFlowGraph & cfg );
		void generate_nts();
		void clone_local_variables();
		void clone_global_variables();
		void create_states();

		/**
		 * @pre  Q1: destination Nts have all variables
		 *       Q2: all variables from source nts points to
		 *           variables in destination nts (through CNVariableInfo).
	     */
		void create_edges();


		/**
		 * @pre  Q1: Every ControlState in ControlFlowGraph
		 *           points to some state in dest_bn.
		 *           
		 * @post R1: Every ControlState have its nts_state null.
		 */
		void clear_state_mapping();

		/**
		 * @pre  Q1: Every variable in dest_nts,
		 *           including local variables (in dest_bn),
		 *           should point to valid, heap-allocated CNVariableInfo.
		 *
		 * @poot Q1: Every variable has its user_data null.
		 */
		void clear_variable_info();

	public:
		static unique_ptr < Nts > generate ( const ControlFlowGraph & cfg );
};

ControlFlowGraph::NtsGenerator::NtsGenerator ( const ControlFlowGraph & cfg ) :
	_cfg ( cfg )
{
	dest_nts = nullptr;
	dest_bn = nullptr;
}

unique_ptr < Nts > ControlFlowGraph::NtsGenerator::generate ( const ControlFlowGraph & cfg )
{
	NtsGenerator g ( cfg );
	g.generate_nts();

	unique_ptr < Nts > n ( g.dest_nts );
	g.dest_nts = nullptr;
	g.dest_bn = nullptr;
	return n;
}

void ControlFlowGraph::NtsGenerator::generate_nts()
{
	dest_nts = new Nts ( "sequenced" );
	dest_bn  = new BasicNts ( "main " );
	dest_bn->insert_to ( *dest_nts );

	Instance * i = new Instance ( dest_bn, 1 );
	i->insert_to ( *dest_nts );

	clone_local_variables();
	clone_global_variables();
	create_states();
	create_edges();

	clear_state_mapping();
	clear_variable_info();
}

/**
 * @brief Clones local variables for every thread
 * @pre  Q1 Given nts must be flat
 *       Q2 Every local variable has its user_data null.
 *       Q3 Every local variable must have 'origin' annotation
 *
 * @post R1 Every local variable's user_data
 *          point to CNVariableInfo.
 *       R2 Every variable from CNVariableInfo 
 *          is owned by given dest_bn.
 *          
 */
void ControlFlowGraph::NtsGenerator::clone_local_variables ( )
{
	const Nts & orig = _cfg.original_nts;
	const unsigned int n_threads = orig.n_threads();

	unsigned int var_id = 0;
	unsigned int thread_id = 0;
	for ( Instance * inst : orig.instances () )
	{
		const string & bnts_name = inst->basic_nts().name;

		for ( Variable * v : inst->basic_nts().variables() )
		{
			if ( !v->user_data )
				v->user_data = ( void * ) new CNVariableInfo ( n_threads );

			for ( unsigned int i = 0; i < inst->n; i++ )
			{
				Variable & cl = clone_to_thread ( *v, bnts_name, thread_id + i);
				cl.name = string ( "var_" ) + to_string ( var_id++ );
				cl.insert_to ( *dest_bn );
			}	
		}

		thread_id += inst->n;
	}
}

/**
 *  Btw also creates mapping from CFG states to Nts states
 */
void ControlFlowGraph::NtsGenerator::create_states()
{
	unsigned int st_id = 0;
	for ( ControlState * s : _cfg.states )
	{
		s->create_nts_state ( string ( "st_" ) + to_string ( st_id ) );
		s->nts_state->insert_to ( *dest_bn );
		st_id++;
	}

	 // TODO: Initial state, final state, error states.
}

void ControlFlowGraph::NtsGenerator::clone_global_variables()
{
	unsigned int gvar_id = 0;
	for ( Variable * v :_cfg.original_nts.variables() )
	{
		// We keep its annotations as they are
		Variable * cl = v->clone();
		cl->insert_to ( *dest_nts );
		cl->name = string ( "gvar_" ) + to_string ( gvar_id++ );

		CNVariableInfo * i = new CNVariableInfo ( ); // global variable
		i->var = cl;
		v->user_data = ( void * ) i;
	}
}

void ControlFlowGraph::NtsGenerator::create_edges()
{
	for ( const CFGEdge * e : _cfg.edges )
	{
		State * from;
		if ( e->from )
			from = e->from->nts_state;
		else
			from = _cfg.initial->nts_state;

		if ( !from )
			throw logic_error ( "State not found" );

		TransitionRule * tr = e->t->rule().clone();
	
		// It seems like after first calling of lambda, the capture data are cleaned
		// So we have to put that lambda into some holder, like VariableUse::visitor
		VariableUse::visitor visitor = [e] ( VariableUse & u )
		{
			variable_use_switch_by_cnvariableinfo ( e, u );
		};

		visit_variable_uses modifier ( visitor );
		modifier.visit ( *tr );

		Transition & t = ( *from ->* *e->to.nts_state ) ( *tr );
		t.insert_to ( *dest_bn );
	}
}

void ControlFlowGraph::NtsGenerator::clear_state_mapping()
{
	for ( ControlState * cs : _cfg.states )
	{
		if ( !cs->nts_state )
			throw logic_error ( "Precondition Q1 failed" );

		cs->nts_state = nullptr;
	}
}

void for_variables_owned_by ( const BasicNts & n, const std::function < void ( Variable * v ) > f )
{
	for ( Variable * v : n.variables() )
		f ( v );

	for ( Variable * v : n.params_out() )
		f ( v );

	for ( Variable * v : n.params_in() )
		f ( v );

	for ( Variable * v : n.pars() )
		f ( v );
}

void for_variables_owned_by ( const Nts & n, const std::function < void ( Variable * v ) > f )
{
	for ( Variable * v : n.variables() )
		f ( v );

	for ( Variable * v : n.parameters() )
		f ( v );
}

void ControlFlowGraph::NtsGenerator::clear_variable_info()
{
	const std::function < void ( Variable * v ) > deleter =
	[] ( Variable * v )
	{
		if ( !v->user_data )
		{
			cout << "Warning: no user_data on " << v->name << "\n";
			return;
		}

		CNVariableInfo * i = static_cast < CNVariableInfo * > ( v->user_data );
		v->user_data = nullptr;
		delete i;
	};

	for_variables_owned_by ( _cfg.original_nts,  deleter );
	for ( BasicNts * n : _cfg.original_nts.basic_ntses() )
		for_variables_owned_by ( *n, deleter );
}

/**
 * @pre Visitor must have cleaned all its data
 */
unique_ptr < Nts > ControlFlowGraph::compute_nts()
{
	return NtsGenerator::generate ( *this );
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
			cout << "Exploring ";
			e.to.print ( cout );
			cout << "\n";
			explore ( e.to );
			break;

		case ControlState::DFSInfo::St::On_stack:
			cout << "**Reached state on stack. Ignoring.\n";
			break;

		case ControlState::DFSInfo::St::Closed:
			cout << "**Reached closed state. Ignoring.\n";
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

		cout << "\tDiscovered: ";
		cs_new->print ( cout );
		cout << "\n";

		ControlState & reached = g.insert_state ( *cs_new );
		cs.next.push_back ( CFGEdge ( & cs, reached, t, pid ) );
	}
}

//------------------------------------//
// POVisitor - partial order          //
//------------------------------------//

POVisitor * POVisitor::generator::operator() ( ControlFlowGraph & g )
{
	return new POVisitor ( g, n );
}

POVisitor::POVisitor ( ControlFlowGraph & g, Nts & n ) :
	SimpleVisitor ( g ), n ( n )
{
	t = Tasks::compute_tasks ( n, "main" );
}

POVisitor::~POVisitor()
{
	delete t;
}

namespace
{
struct mystate
{
	ControlState * st;
	bool is_my;
	Transition & t;
	mystate ( ControlState * st, bool my, Transition & t ) :
		st ( st ), is_my ( my ), t ( t ) { ; }
};
// Well, there might be two edges leading to the same state,
// but it should not be bad.
struct mystates : public vector < mystate >
{
	~mystates()
	{
		for ( mystate & m : *this )
		{
			if ( m.is_my )
				delete m.st;
		}
	}
};

}

bool POVisitor::try_ample ( ControlState & cs, unsigned int pid )
{
	
	// Calculate all possible next states
	// We do not want to add them to set of states (yet).

	// Also calculate set of read / modified global variables

	mystates my_states;
	Globals gs; //< Modified by some transition in this possible ample set

	const ProcessState & s = cs.states[pid];
	for ( Transition *t : s.bnts_state->outgoing() )
	{

		const TransitionInfo * ti = ( const TransitionInfo * ) t->user_data;
		gs.union_with ( ti->global );

		auto cs_new = new ControlState();
		cs_new->states = cs.states; // Copy
		cs_new->states[pid].bnts_state = & t->to();

		ControlState * next = g.get_state ( *cs_new );
		if ( next )
		{
			// We already have this state
			delete cs_new;
			my_states.push_back ( mystate ( next, false, *t ) );
		} else {
			my_states.push_back ( mystate ( cs_new, true, *t ) );
		}
	}

	cout << "process " << pid << " uses following variables:\n";
	cout << gs;

	// Check C3: Is some of these states on stack?
	for ( mystate & ms : my_states )
	{
		ControlState * s = ms.st;

		if ( s->di.st == ControlState::DFSInfo::St::On_stack )
			return false;

		// But note that state 'cs' is not marked as on stack.
		if ( &cs == s )
		{
			cout << "self loop\n";
			return false;
		}
	}

	// FIXME: Now we assume that no task can cause another task to start.
	//std::set < Task * > running_tasks;
	Globals other_tasks_globals;
	for ( unsigned int i = 0; i < cs.states.size(); i++ )
	{
		if ( i == pid )
			continue;

		ProcessState & ps = cs.states[i];
		StateInfo * si = static_cast < StateInfo * > ( ps.bnts_state->user_data );

		//running_tasks.insert ( si->t );
		other_tasks_globals.union_with ( si->t->global );
	}

	if ( other_tasks_globals.may_collide_with ( gs ) )
		return false;

	cout << "other processes use:\n" << other_tasks_globals;

	cout << "Possible ample set: pid " << pid << "\n";

	// TODO: add it
	//return false;
	
	while ( !my_states.empty() )
	{
		mystate & ms = my_states.back();
		cs.next.push_back ( CFGEdge ( &cs, *ms.st, & ms.t, pid ) );
		g.insert_state ( * ms.st );
		ms.st = nullptr;
		my_states.pop_back();
	}


	return true;
}

void POVisitor::explore ( ControlState & cs )
{
	// Try to use one of the process states as ample set
	for ( unsigned int i = 0; i < cs.states.size(); i++ )
	{
		if ( try_ample ( cs, i ) )
			return;
	}

	cout << "Can not find ample set\n";
	// We must expand all states
	SimpleVisitor::explore ( cs );
}



