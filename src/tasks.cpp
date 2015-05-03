#include <utility>
#include <iostream>
#include <stdexcept>
#include <algorithm>

#include <libNTS/logic.hpp>

#include "tasks.hpp"
#include "logic_utils.hpp"

using namespace nts;

using std::cout;
using std::find_if;
using std::logic_error;
using std::move;
using std::ostream;
using std::set;
using std::string;
using std::vector;

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

//------------------------------------//
// Task                               //
//------------------------------------//

void Task::compute_globals()
{
	global.reads.clear();
	global.writes.clear();

	for ( StateInfo * si : states )
	{
		for ( Transition * t : si->st->outgoing() )
		{
			if ( !t->user_data )
				throw logic_error ( "Precondition Q2 failed" );

			TransitionInfo * ti = static_cast < TransitionInfo * > ( t->user_data );
			global.union_with ( ti->global );
		}
	}
}


//------------------------------------//
// Tasks                              //
//------------------------------------//

Tasks::Tasks ( Nts & n ) :
	n ( n )
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

void Tasks::calculate_toplevel_bnts()
{
	toplevel_bnts.clear();
	for ( const Instance * in : n.instances() )
		toplevel_bnts.insert ( & in->basic_nts() );
}

/**
 * @pre R1 If split_by_annot is true, "origin" annotation must be composed of two parts.
 * @pre R2, R3, R4 as in  "split_to_tasks ( Nts & , const std::string & )"
 *
 * @post POST_1: All states in given BasicNts have associated StateInfo,
 *               which is pointing to associated task.
 *       POST_2: All tasks have their structure, which has 'states_assigned'.
 *       
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

			// Add it to default task
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
		t->states.push_back ( si );
	}
}

void Tasks::compute_transition_info()
{
	for ( /* const */ BasicNts * bn : toplevel_bnts )
	{
		for ( Transition * t : bn->transitions() )
		{
			if ( t->user_data )
				throw logic_error ( "Precondition Q2 does not hold" );

			TransitionInfo * ti = new TransitionInfo();
			ti->transition = t;
			t->user_data = ( void * ) ti;

			ti->global = used_global_variables ( n, *t );
		}
	}
}

void Tasks::print_transition_info ( ostream & o ) const
{
	o << "** Transitions **\n";
	for ( /* const */ BasicNts * bn : toplevel_bnts )
	{
		o << "* toplevel " << bn->name << "\n";
		for ( Transition * t : bn->transitions() )
		{
			o << "\ttransition " << *t << "\n";
			TransitionInfo * ti = static_cast < TransitionInfo * > ( t->user_data );

			o << "\t\treads: ";
			for ( const Variable * v : ti->global.reads )
				o  << v->name << " ";

			if ( ti->global.writes.everything )
				o << "\n\t\twrites everything\n";
			else
			{
				o << "\n\t\twrites: ";
				for ( const Variable * v : ti->global.writes.vars )
				{
					o << v->name << " ";
				}
				o << "\n";
			}
		}
	}	
}

void Tasks::compute_task_structure()
{
	for ( Task *t : tasks )
	{
		t->compute_globals();
	}
}

void Tasks::split_to_tasks()
{
	for ( BasicNts * bn : toplevel_bnts )
	{
		if ( bn->name == main_nts_name )
			split_to_tasks ( *bn, false );
		else
			split_to_tasks ( *bn, true );
	}
}

Tasks * Tasks::compute_tasks ( nts::Nts & n, const std::string & main_nts )
{
	Tasks * tasks = new Tasks ( n );
	tasks->main_nts_name = main_nts;
	tasks->calculate_toplevel_bnts();
	tasks->split_to_tasks();

	// Assume R1 is true. Now lets calculate R2
	tasks->compute_transition_info();
	//tasks->print_transition_info( cout );
	tasks->compute_task_structure();

	return tasks;
}

//------------------------------------//
// GlobalWrites                       //
//------------------------------------//

void GlobalWrites::clear()
{
	vars.clear();
	everything = false;
}

void GlobalWrites::union_with ( const GlobalWrites & other )
{
	if ( other.everything &&  !everything )
		insert_everything();

	if ( everything )
		return;

	vars.insert ( other.vars.cbegin(), other.vars.cend() );
}

void GlobalWrites::insert_everything()
{
	vars.clear();
	everything = true;
}

bool GlobalWrites::contains ( const Variable * v ) const
{
	if ( everything )
		return true;

	auto it = vars.find ( v );
	return it != vars.cend();
}

void GlobalWrites::insert ( const Variable * v )
{
	if ( everything )
		return;

	vars.insert ( v );
}


//------------------------------------//
// Globals                            //
//------------------------------------//

void Globals::union_with ( const Globals & other )
{
	writes.union_with ( other.writes );
	reads.insert ( other.reads.begin(), other.reads.end() );
}




