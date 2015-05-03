#include <iostream>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>
#include <libNTS/variables.hpp>

#include <llvm2nts/llvm2nts.hpp>

using std::cout;
using std::find_if;
using std::logic_error;
using std::make_pair;
using std::map;
using std::out_of_range;
using std::string;
using std::unique_ptr;
using std::unordered_set;
using std::vector;

using namespace nts;

// Task is a basic organisation unit.
// It constitutes of states and transitions between them.
// During execution, instances of task can be assigned to threads.
// Task uses some subset of global variables. Also, a task
// may cause another task to be run. This can be computed using static analysis,
// before POR is run.

struct Task
{
	string name;
	// Set of global variables, which can be used by this task.
	unordered_set < Variable * > globals;
	
	// Set of tasks, which can be caused to run directly by this task.
	unordered_set < Task * > tasks_may_be_run;

	// Which task can activate this task?
	unordered_set < Task * > can_be_run_by;
};

struct Tasks
{
	vector < Task * > tasks;
	map < string, Task * > name_to_task;
	Task * idle_worker_task;

	void split_to_tasks ( BasicNts & bn, bool split_by_annot );

	Tasks();
	~Tasks();

};

Tasks::Tasks()
{
	idle_worker_task = new Task();
	idle_worker_task->name = "idle_worker_task";
	// Do not add it to map - there could be some task with the same name
}

Tasks::~Tasks()
{
	for ( Task * t : tasks )
	{
		delete t;
	}
}

// Each variable is associated (through its user pointer)
// to an instance of this class.
struct GlobalVariableInfo
{
	Variable * var;

	// Which tasks (directly) use associated variable.
	unordered_set < Task * > users;

};

struct StateInfo
{
	State * st;
	Task * t;
};

AnnotString * find_annot_origin ( Annotations & ants )
{
	auto it = find_if ( ants.begin(), ants.end(), [] ( Annotation * a ) {
		if ( a->type() != Annotation::Type::String )
			return false;
		if ( a->name != "origin" )
			return false;
		return true;
	});

	if ( it == ants.end() )
		return nullptr;

	return static_cast<AnnotString *> ( *it );
}

/**
 * @pre R1 If split_by_annot is true, "origin" annotation must be composed of two parts.
 * @pre R2, R3, R4 as in  "split_to_tasks ( Nts & , const std::string & )"
 *
 * @post All states in given BasicNts have associated StateInfo.
 *       But this does not meain they have associated task!
 *
 *
 * @param split_by_annot - true  => "origin" annotation is used to split
 *                                  states to task
 *                         false => task is assigned by name of given BasicNts
 *
 * If state's 'origin' consist only of one part and split_by_annot is true,
 * the state is not assigned to any task. For example, if pthreads are used,
 * then __thread_create BasicNts is generated. This BasicNts
 * is instantiated in toplevel and its control states are annotated only by their name.
 */
void Tasks::split_to_tasks ( BasicNts & bn, bool split_by_annot )
{
	for ( State * s : bn.states() )
	{
		if ( s->user_data )
			throw logic_error ( "Precondition R4 failed" );

		StateInfo * si = new StateInfo();
		si->t = nullptr;
		si->st = s;
		s->user_data = ( void * ) si;

		string task_name;

		if ( split_by_annot )
		{
			AnnotString * origin = find_annot_origin ( s->annotations );
			if ( !origin )
				throw logic_error ( "Precondition R3 failed" );

			// TODO Add it to default task
			// Examples:
			// "thread_func:0:st_0_0" - from task 'thread_func'
			// "s_running_1"          - no task
			size_t pos = origin->value.find ( ':' );
			if ( pos == string::npos )
			{
				si->t = idle_worker_task;
				continue;
			}

			task_name = string ( origin->value, 0, pos );
		} else {
			task_name = bn.name;
		}

		Task * t;
		auto it = name_to_task.find ( task_name );
	
		if ( it == name_to_task.end() )
		{
			cout << "New task with name: '" << task_name << "'\n";
			t = new Task();
			t->name = task_name;
			tasks.push_back ( t );
			name_to_task.insert ( make_pair ( task_name, t ) );
		} else {
			cout << "Found task with name: '" << task_name << "'\n";
			t = it->second;
		}

		si->t = t;
	}
}

/**
 * @pre R1: Nts contains only BasicNtses, which are instantiated.
 *      R2: Each BasicNts is flat (i.e. it does not contain call rule)
 *      R3: Each state contains an "origin" annotation (see inliner).
 *      R4: All user pointers are null.
 *      R5: Nobody 'calls' main nts (there is no origin annotation starting with "main::").
 *
 * @post Q1: Each state has associated StateInfo structure.
 *
 * @param main_nts   name of main BasicNts. All states and transition in this BasicNts
 *                   are treated as one task
 */
Tasks * nts_split_to_tasks ( Nts & n, const std::string & main_nts )
{
	Tasks * tasks = new Tasks();

	for ( Instance * i : n.instances() )
	{
		if ( i->basic_nts().name == main_nts )
		{
			tasks->split_to_tasks ( i->basic_nts(), false );
		}
		else
		{
			tasks->split_to_tasks ( i->basic_nts(), true );
		}
	}

	return tasks;
}

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
 * @pre R1 All states from the same Nts as given variable
 *         have assigned their StateInfo structure
 */
unordered_set < Task * > tasks_using_variable ( Variable & v )
{
	unordered_set < Task * > tasks;
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

	unordered_set < Variable * > visible_global_vars;

	// Btw sets all global variables as visible
	PartialOrderReduction ( Nts & n );
	~PartialOrderReduction();

	void split_to_tasks();
	void calc_variable_users();

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

/**
 * @brief It is possible to use set of transitions of process with given id
 *        as an ample set?
 */
bool PartialOrderReduction::can_ample (
		const ControlState & s,
		unsigned int process_id ) const
{
	if ( process_id >= s.states.size() )
		throw out_of_range ( "process_id too large" );

	// Check C3
	if ( s.successor_on_stack ( s, process_id ) )
		return false;

	// For C2 and C1 we need to have the set of variables, accessed by 
	// some of transitions, leading from state s.

	// Check C2

}

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

void PartialOrderReduction::split_to_tasks()
{
	tasks = nts_split_to_tasks ( n, "main" );
}

unique_ptr < Nts * > reduct ( Nts & n )
{
	PartialOrderReduction por ( n );
	por.split_to_tasks();
	por.calc_variable_users();

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

