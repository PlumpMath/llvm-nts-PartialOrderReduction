#include <iostream>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>
#include <libNTS/variables.hpp>

#include <llvm2nts/llvm2nts.hpp>

#include "tasks.hpp"
#include "logic_utils.hpp"
#include "control_state.hpp"

using std::cout;
using std::find_if;
using std::logic_error;
using std::make_pair;
using std::map;
using std::out_of_range;
using std::set;
using std::string;
using std::unique_ptr;
using std::unordered_set;
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

#if 0
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
#endif

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
#if 0
unique_ptr < Nts * > serialize_simple ( Nts & n )
{
	ControlState * cs = initial_control_state ( n );
	cs->print ( cout );
	cout << "\n";
	cs->expand();
	cout << "After expansion: " << cs->next.size() << "\n";


	unsigned int depth = 0;
	ControlState * current = cs;
	while ( current->n_explored < current->next.size() || current->prev )
	{
		//cout << "Depth: " << depth << "state: ";
		//current->print ( cout );
		//cout << "\n";
		if ( current->n_explored >= current->next.size() )
		{
			depth--;
			current = current->prev;
			current->n_explored++;
			cout << "Up to ";
			current->print ( cout );
			cout << ", remaining: " << (current->next.size() - current->n_explored) << "\n";
			continue;
		}

		ControlState * next = current->next [ current->n_explored ];
		current->print ( cout );
		cout << " -> ";
		next->print ( cout );
		cout << "\n";
		current = next;
		current->expand();
		//cout << "After expansion: " << current->next.size() << "\n";
		depth++;
	}

	cout << "Stopped.\n"
		 << "\tExplored: " << current->n_explored << "\n"
		 << "\tnext: " << current->next.size() << "\n"
		 << "\trpev: " << current->prev << "\n";

	return nullptr;
}
#endif
enum class SerializationMode
{
	Simple,
	PartialOrderReduction
};

unique_ptr < Nts * > serialize ( Nts & n, SerializationMode mode )
{
	POVisitor::generator g ( n );
	ControlFlowGraph * cfg = ControlFlowGraph::build ( n, g /* SimpleVisitor_generator */ );
	delete cfg;
	return nullptr;
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
	unique_ptr < Nts * > result = serialize (
			* nts,
			SerializationMode::Simple
	);
	if ( result )
		cout << * result;

	cout << "done\n";
	return 0;
}

