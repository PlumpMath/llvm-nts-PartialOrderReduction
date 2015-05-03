#include <iostream>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>
#include <libNTS/variables.hpp>

#include <llvm2nts/llvm2nts.hpp>

#include "tasks.hpp"
#include "logic_utils.hpp"

using std::cout;
using std::find_if;
using std::logic_error;
using std::make_pair;
using std::map;
using std::out_of_range;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace nts;


class PidTooLarge : public out_of_range
{
	public:
		PidTooLarge() : out_of_range ( "Given PID (process ID ) is too large" )
		{
			;
		}
};


TransitionRule * VariableUse_get_transitionRule ( const VariableUse & v )
{
	union
	{
		Transition * transition;
		Formula    * formula;
		Term       * term;
	} ptr;

	enum class St
	{
		Formula,
		Term
	};

	St st;

	switch ( v.user_type )
	{
		case VariableUse::UserType::ArrayWrite:
			st = St::Formula;
			ptr.formula = v.user_ptr.arr_wr;
			break;

		case VariableUse::UserType::VariableReference:
			st = St::Term;
			ptr.term = v.user_ptr.vref;
			break;

		case VariableUse::UserType::CallTransitionRule:
			return v.user_ptr.ctr;

		case VariableUse::UserType::Havoc:
			st = St::Formula;
			ptr.formula = v.user_ptr.hvc;
			break;

		default:
			throw logic_error ( "Unknown UserType" );

	}

	while ( true )
	{
		switch ( st )
		{
			case St::Formula:
				switch ( ptr.formula->_parent_type )
				{
					case Formula::ParentType::None:
						return nullptr;

					case Formula::ParentType::NtsInitialFormula:
						return nullptr;

					case Formula::ParentType::FormulaTransitionRule:
						return ptr.formula->_parent_ptr.ftr;

					case Formula::ParentType::Formula:
						st = St::Formula;
						ptr.formula = ptr.formula->_parent_ptr.formula;
						break;

					default:
						throw logic_error ( "Unknown Formula::ParentType" );
				}
				break;

			case St::Term:
				switch ( ptr.term->_parent_type )
				{
					case Term::ParentType::None:
						return nullptr;

					case Term::ParentType::Formula:
						st = St::Formula;
						ptr.formula = ptr.term->_parent_ptr.formula;
						break;

					case Term::ParentType::Term:
						st = St::Term;
						ptr.term = ptr.term->_parent_ptr.term;
						break;

					// This should not happen with global variable,
					// but it is possible and correct with parameter.
					// Now we do not support it
					case Term::ParentType::DataType:
						throw logic_error ( "Variable sized array? Not supported" );

					case Term::ParentType::CallTransitionRule:
						return ptr.term->_parent_ptr.crule;

					case Term::ParentType::QuantifiedType:
						st = St::Formula;
						ptr.formula = ptr.term->_parent_ptr.qtype->parent()->parent();
						break;
				}
				break;

			default:
				throw logic_error ( "Invariant failed" );
		}
	}
}

/**
 * @pre  Q1: All states from the same Nts as given variable
 *           have assigned their StateInfo structure
 * @psot R1: Each task, which can use given variable, is in the returned set.
 */
set < Task * > tasks_using_variable ( Variable & v )
{
	set < Task * > tasks;
	for ( const VariableUse * u : v.uses() )
	{
		TransitionRule * tr = VariableUse_get_transitionRule ( *u );
		if ( ! tr )
			continue;

		Transition * t = tr->transition();

		const State * from = & t->from();
		const State * to   = & t->to();

		StateInfo * fsi = ( StateInfo * ) from->user_data;
		StateInfo * tsi = ( StateInfo * ) to->user_data;

		if ( fsi->t )
			tasks.insert ( fsi->t );

		if ( tsi->t )
			tasks.insert ( tsi->t );
	}

	return tasks;
}

struct ControlState
{
	// One state per process
	vector < const State * > states;


	/**
	 * If null, this is the initial state.
	 * If not, then prev->states.size() == states.size()
	 */
	ControlState * prev;
	vector < ControlState * > next;
	unsigned int n_explored;


	bool expand();

	bool operator== ( const ControlState & other ) const;

	/**
	 * Is given state on search stack?
	 */
	bool on_stack ( const ControlState & st ) const;

	/**
	 * @brief Does this state have a successor under pid,
	 *        which is on the stack, induced by 'st'?
	 */
	bool successor_on_stack ( const ControlState & st, unsigned int pid ) const;

};

bool ControlState::operator== ( const ControlState & other ) const
{
	if ( states.size() != other.states.size() )
		return false;

	for ( unsigned int i = 0; i < states.size(); i++ )
		if ( states[i] != other.states[i] )
			return false;

	return true;
}

bool ControlState::on_stack ( const ControlState & other ) const
{
	const ControlState * st = this;
	while ( st )
	{
		if ( *st == other )
			return false;

		st = st->prev;
	}

	return false;
}

/*
 * def [ pid-successor ]: For any two control states S_1, S_2, we say
 * S_2 is a pid-successor of S_1, if and only if
 * control state of procces with id 'pid' (i.e. S_1.states[pid] )
 * has a successor 'suc' such that if we substitute S_1.states[pid]
 * with 'suc', we got S_2.
 */

bool ControlState::successor_on_stack (
		const ControlState & st,
		unsigned int pid ) const
{
	if ( states.size() != st.states.size() )
		throw logic_error ( "Comparing control states of different width" );

	const ControlState *s;
	/**
	 * loop invariant: Having the path
	 * S_k, S_{k-1}, ..., S_1, S_0
	 * where S_0 = & st, S_k = s and for each i; 0 <= i < k; S_{i+1} = S_i->prev,
	 * there is no control state S_j where 0 <= j < k such that
	 * some pid-successor of this state equals S_j.
	 */
	for ( s = & st; s != nullptr; s = s->prev )
	{
		unsigned int i;
		// States of all processes except the one with id 'pid' should match
		for ( i = 0; i < states.size(); i++ )
		{
			if ( i == pid )
				continue;

			if ( states[i] != s->states[i] )
				break;
		}

		// One of states is different
		if ( i != states.size() )
			continue;

		// All states (except the selected) matches
		for ( Transition *t : states[pid]->outgoing() )
		{
			if ( & t->to() == s->states[pid] )
				return true;
		}
	}

	return false;
}

bool ControlState::expand()
{
	// No reduction (for now)
	n_explored = 0;
	for ( unsigned int i = 0; i < states.size(); i++ )
	{
		const State * s = states[i];
		for ( Transition *t : s->outgoing() )
		{
			auto cs_new = new ControlState();
			cs_new->states = states;
			cs_new->states[i] = & t->to();
			// TODO: Check whether this already exists on stack
			

			// if not, add it
			next.push_back ( cs_new );
		}
	}

	return true;
}

ControlState initial_control_state ( const Nts & n )
{
	ControlState cs;
	cs.prev = nullptr;
	cs.n_explored = 0;

	for ( const Instance * i : n.instances() )
	{
		const BasicNts & bn = i->basic_nts();
		const State * initial_state = nullptr;

		for ( const State * s : bn.states() )
		{
			if ( s->is_initial() )
			{
				if ( initial_state )
					throw logic_error ( "Only one initial state is supported" );
				initial_state = s;
			}
		}

		for ( unsigned int j = 0; j < i->n; j++ )
			cs.states.push_back ( initial_state );

	}

	return cs;
}

struct PartialOrderReduction
{
	Nts & n;
	Tasks * tasks;

	ControlState * initial_state;
	ControlState * current_state;

	set < Variable * > visible_global_vars;

	// Btw sets all global variables as visible
	PartialOrderReduction ( Nts & n );
	~PartialOrderReduction();

	void split_to_tasks();
	void calc_variable_users();

	/**
	 * @brief Can the process 'pid' access given variable?
	 */
	bool can_be_used ( Variable *v, const ControlState & s, unsigned int pid ) const;

	/**
	 * @brief Can the process 'pid' modify given variable?
	 * @pre  Q1: Variable 'v' is a global variable
	 *       Q2: and it has associated VariableInfo
	 *       Q3: with calculated set of tasks using it.
	 */
	bool can_be_modified ( Variable *v, const ControlState & s, unsigned int pid ) const;


	/*
	 * @pre R1 list of visible global variables must be calculated
	 */
	bool can_ample ( const ControlState & s, unsigned int process_id ) const;
};

PartialOrderReduction::PartialOrderReduction ( Nts & n ) :
	n ( n )
{
	for ( Variable * v : n.variables() )
		visible_global_vars.insert ( v );
}

PartialOrderReduction::~PartialOrderReduction()
{
	for ( Variable * v : n.variables() )
	{
		if ( !v->user_data )
			continue;

		GlobalVariableInfo * gvi = ( GlobalVariableInfo * ) v->user_data;
		delete gvi;
		v->user_data = nullptr;
	}

	delete tasks;
}

bool PartialOrderReduction::can_be_used (
		Variable * v,
		const ControlState & cs,
		unsigned int pid ) const
{
	if ( pid >= cs.states.size() )
		throw PidTooLarge();

	const State * st = cs.states[pid];

	if ( !st->user_data )
		throw std::logic_error ( "State does not have StateInfo" );

	const StateInfo * si = ( StateInfo * ) st->user_data;

	if ( !si->t )
		throw std::logic_error ( "State does not belong to any task" );

	const Task * t = si->t;

}

/**
 * @brief It is possible to use set of transitions of process with given id
 *        as an ample set?
 *
 * @pre Q1: All transitions have associated calculated TransitionInfo
 * @assigns nothing
 */
bool PartialOrderReduction::can_ample (
		const ControlState & s,
		unsigned int process_id ) const
{
	if ( process_id >= s.states.size() )
		throw PidTooLarge();


	auto & transitions = s.states[process_id]->outgoing();

	// Part of C1
	// TODO: If any of outgoing transitions modifies all global variables
	// because it does not contain havoc, it can be in ample set
	// only if none of other running tasks uses global variables.
	

	// Check C3
	if ( s.successor_on_stack ( s, process_id ) )
		return false;

	// For C2 and C1 we need to have the set of variables, accessed by 
	// some of transitions, leading from state s.
	
	Globals gs;
	for ( Transition * t : transitions )
	{
		if ( !t->user_data )
			throw logic_error ( "Precondition Q1 failed" );

		const TransitionInfo * ti = ( const TransitionInfo * ) t->user_data;
		gs.union_with ( ti->global );
	}

	// Check C2
	// TODO: Now we do not care much
	

	// Check C1
	// Is there a variable, which is:
	// 1. used by some outgoing transition < and > used by some running task
	// 2. and modified by some outgoing transition < or > by some running task ?
	//
	// TODO: We should check all tasks, which can be run by some running task.
	//
	
	return false;
}

#if 0
/**
 * @pre R1: User data of global variables are null.
 * @post Q1: Each global variable has assigned GlobalVariableInfo structure
 *       Q2: In that structure there is a valid list of tasks, which use the variable.
 *
 * TODO: Handle missing havoc(). Now the behaviour is the same as if there was
 * one havoc() in toplevel of each transition formula.
 */
void PartialOrderReduction::calc_variable_users()
{
	for ( Variable *v : n.variables() )
	{
		cout << "Variable " << v->name << "\n";
		auto used_by = tasks_using_variable ( *v );

		for ( Task * t : used_by )
		{
			cout << "\tby " << t->name << "\n" ;
		}

		auto * gvi = new GlobalVariableInfo();
		gvi->var = v;
		gvi->users = move ( used_by );

		if ( v->user_data )
			throw logic_error ( "Precondition R1 failed: user_data are not null" );

		v->user_data = ( void * ) gvi;
	}
	
}

#endif

void PartialOrderReduction::split_to_tasks()
{
	tasks = Tasks::compute_tasks ( n, "main" );
}

unique_ptr < Nts * > reduct ( Nts & n )
{
	PartialOrderReduction por ( n );
	por.split_to_tasks();
	//por.calc_variable_users();

	return nullptr;
}

int main ( int argc, char **argv )
{
	if ( argc <= 1 )
	{
		cout << "usage: " << argv[0] << " file.ll\n";
		return 0;
	}

	const char * file = argv[1];
	unique_ptr < Nts > nts = llvm_file_to_nts ( file );
	inline_calls_simple ( *nts );
	cout << *nts;
	unique_ptr < Nts * > reduced = reduct ( * nts );
	if ( reduced )
		cout << * reduced;


	cout << "done\n";
	return 0;
}

