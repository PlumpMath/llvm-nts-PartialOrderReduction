#include <utility>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>

#include <libNTS/logic.hpp>

#include "tasks.hpp"
#include "logic_utils.hpp"
#include "control_state.hpp"

using namespace nts;

using std::cout;
using std::find_if;
using std::logic_error;
using std::move;
using std::ostream;
using std::out_of_range;
using std::runtime_error;
using std::set;
using std::sort;
using std::string;
using std::stringstream;
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

Task::Task ( string name ) :
	name ( move ( name ) )
{
	cout << "new task: '" << this->name << "'\n";
	has_number = false;
}

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

	cout << "Task " << this->name << " uses:\n" << global;
}

Task::~Task()
{
	for ( StateInfo *s : states )
	{
		s->st->user_data = nullptr;
		delete s;
	}
}


//------------------------------------//
// Tasks                              //
//------------------------------------//

Tasks::Tasks ( Nts & n ) :
	n ( n )
{
	main_task = nullptr;
	idle_worker_task = new Task ( "idle_worker_task" );
	//tasks.push_back ( idle_worker_task );
	// Do not add it to map - there could be some task with the same name
}

Tasks::~Tasks()
{
	delete idle_worker_task;
	for ( Task * t : tasks )
	{
		delete t;
	}

	// Delete TransitionInfo
	for ( const BasicNts * bn : toplevel_bnts )
	{
		for ( Transition * t : bn->transitions() )
		{
			if ( !t->user_data )
				continue;

			TransitionInfo * ti = static_cast < TransitionInfo * > ( t->user_data );
			t->user_data = nullptr;
			delete ti;
		}
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
				idle_worker_task->states.push_back ( si );
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
			t = new Task( task_name );
			tasks.push_back ( t );
			name_to_task.insert ( make_pair ( task_name, t ) );

			if ( t->name == this->main_nts_name )
				this->main_task = t;

		} else {
			t = it->second;
		}

		si->t = t;
		t->states.push_back ( si );
	}
}

void Tasks::find_tasks_initial_final_states()
{
	// Initial state of task T is some state Si such that
	// there is a state from idle worker Sw such that
	// there is a transition Sw -> Si.
	//
	// Conversely, final state of task T is state Sf s.t.
	// exists state Sw in idle worker s.t.
	// there is a transition Sf -> Sw.
	
	for ( StateInfo * si : idle_worker_task->states )
	{
		//@ assert si->t == idle_worker_task;

		State * st = si->st;

		for ( const Transition * t : st->incoming() )
		{
			const State & from = t->from();
			auto fri = static_cast < StateInfo * > ( from.user_data ) ;
			

			// Transition from some task to idle_worker_task
			if ( fri->t != idle_worker_task )
			{
				cout << "Final state of task "
					 << fri->t->name
					 << " is "
					 << from.name << "\n";

				fri->t->final_states.push_back ( fri );
			}
		}

		for ( const Transition *t : st->outgoing() )
		{
			const State & to = t->to();
			auto toi = static_cast < StateInfo * > ( to.user_data );

			if ( toi->t != idle_worker_task )
			{	
				cout << "Initial state of task "
					 << toi->t->name
					 << " is "
					 << to.name << "\n";

				toi->t->initial_states.push_back ( toi );

				// HACK: we know that 'from' state
				// should have been named 's_running_X',
				// where X is the number of task
				// This name should be preserved in annotations
				AnnotString * as = find_annot_origin ( t->from().annotations );
				if ( !as )
					throw runtime_error ( "Missing 'origin' annotation" );

				string prefix ( "s_running_" );
				if ( as->value.compare ( 0, prefix.length(), prefix ) )
					throw runtime_error ( "Source state does not have 's_running_' prefix" );

				stringstream ss ( as->value.substr ( prefix.length() ) );
				unsigned int task_id;
				ss >> task_id;
				std::cout << "Task id: " << task_id << "\n";

				if ( toi->t->has_number && toi->t->number != task_id )
					throw runtime_error ( "Two IDs for one task?" );

				if ( task_id == 0 )
					throw runtime_error ( "ID 0 is reserved for main task" );

				toi->t->number = task_id;
				toi->t->has_number = true;

			}
		}
	}

	// One more thing: main task is instantiated directly,
	// so there is no transition from worker to main task.
	// => we have find it
	if ( !main_task )
		return;

	main_task->has_number = true;
	main_task->number = 0;

	for ( const BasicNts * bn : toplevel_bnts )
	{
		if ( bn->name == main_nts_name )
		{
			for ( const State * s : bn->states() )
			{
				auto si = static_cast < StateInfo * > ( s->user_data );

				if ( s->is_initial() )
				{
					cout << "Initial state of main task: " << s->name << "\n";
					main_task->initial_states.push_back ( si );
				}

				if ( s->is_final() )
				{
					cout << "Final state of main task: " << s->name << "\n";
					main_task->final_states.push_back ( si );
				}
			}

			break;
		}
	}
}

void Tasks::sort_tasks_by_id()
{
	// TODO: Check every task has ID
	for ( Task * t : tasks )
	{
		if ( !t->has_number )
		{
			cout << "Task " << t->name << "does not have id\n" ;
			throw logic_error ( "Precondition failed" );
		}
	}
	
	auto cmp = []( const Task * t1, const Task * t2 ) -> bool
	{
		return t1->number < t2->number;
	};

	sort ( tasks.begin(), tasks.end(), cmp );

	// Check postcondition
	for ( unsigned int i = 0; i < tasks.size(); i++ )
	{
		cout << i << ": task " << tasks[i]->number << " (" << tasks[i]->name << ")\n";
		if ( tasks[i]->number != i )
			throw runtime_error ( "Task numbering problem" );
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
	tasks->find_tasks_initial_final_states();
	tasks->sort_tasks_by_id();

	// Assume R1 is true. Now lets calculate R2
	tasks->compute_transition_info();
	//tasks->print_transition_info( cout );
	tasks->compute_task_structure();

	return tasks;
}

//------------------------------------//
// GlobalReads                        //
//------------------------------------//

bool GlobalReads::contains ( const Variable * v ) const
{
	return cend() != find ( v );
}

ostream & operator<< ( ostream & o, const GlobalReads & gr )
{
	o << "{ ";
	for ( const Variable * v : gr )
		o << v->name << ", ";
	o << "}";
	return o;
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

ostream & operator<< ( ostream & o, const GlobalWrites & gw )
{
	if ( gw.everything )
	{
		o << "everything";
		return o;
	}

	o << "{ ";
	for ( const Variable * v : gw.vars )
		o << v->name << ", ";
	o << "}";
	return o;
}

//------------------------------------//
// Globals                            //
//------------------------------------//

void Globals::union_with ( const Globals & other )
{
	writes.union_with ( other.writes );
	reads.insert ( other.reads.begin(), other.reads.end() );
}

bool Globals::may_collide_with ( const Globals & other ) const
{
	if ( writes.everything || other.writes.everything )
		return true;

	for ( const Variable * r : writes.vars )
	{
		if ( other.writes.contains ( r ) )
			return true;
	
		if ( other.reads.contains ( r ) )
			return true;
	}

	for ( const Variable * r : other.writes.vars )
	{
		if ( writes.contains ( r ) )
			return true;

		if ( reads.contains ( r ) )
			return true;
	}

	return false;
}

ostream & operator<< ( ostream & o, const Globals & gs )
{
	o << "\treads:  " << gs.reads  << "\n";
	o << "\twrites: " << gs.writes << "\n";
	return o;
}

//------------------------------------//
// InlinedProcedureCalls              //
//------------------------------------//

ControlState * InlinedProcedureCalls::next ( const ControlState *c, unsigned int pid ) const
{
	if ( pid >= c->states.size() )
		throw out_of_range ( "PID too large" );

	const ProcessState & ps = c->states[pid];

	// This thread is not running at all
	if ( ps.bnts_state == nullptr )
		return nullptr;

	const StateInfo * si = static_cast < const StateInfo * > ( ps.bnts_state->user_data );
	auto it = calls.find ( si );
	if ( it == calls.end() )
		return nullptr;

	return it->second->next ( c, pid );
}

//------------------------------------//
// thread_create()                    //
//------------------------------------//

ControlState * thread_create_function ( const ControlState *st, unsigned int pid )
{
	const ProcessState & ps = st->states[pid];
	// There should be exactly one 
}
