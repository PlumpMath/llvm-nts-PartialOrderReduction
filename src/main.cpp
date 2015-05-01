#include <iostream>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include <libNTS/nts.hpp>
#include <libNTS/inliner.hpp>
#include <llvm2nts/llvm2nts.hpp>

using std::cout;
using std::find_if;
using std::logic_error;
using std::make_pair;
using std::map;
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
Tasks * split_to_tasks ( Nts & n, const std::string & main_nts )
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

TransitionRule * VariableUser_get_transitionRule ( const VariableUser & v )
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
		case VariableUser::UserType::ArrayWrite:
			st = St::Formula;
			ptr.formula = v.user_ptr.arr_wr;
			break;

		case VariableUser::UserType::VariableReference:
			st = St::Term;
			ptr.term = v.user_ptr.vref;
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
	for ( const VariableUser & u : v.users() )
	{
		TransitionRule * tr = VariableUser_get_transitionRule ( u );
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
	Tasks * ts = split_to_tasks ( *nts, "main" );

	for ( Variable *v : nts->variables() )
	{
		cout << "Variable " << v->name << "\n";
		auto used_by = tasks_using_variable ( *v );

		for ( Task * t : used_by )
		{
			cout << "\tby " << t->name << "\n" ;
		}
	}
	

	delete ts;
	// TODO: Remove all user data

	cout << "done\n";
	return 0;
}
