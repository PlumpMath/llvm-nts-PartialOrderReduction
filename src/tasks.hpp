#ifndef POR_TASKS_HPP_
#define POR_TASKS_HPP_
#pragma once

#include <functional>
#include <list>
#include <map>
#include <ostream>
#include <string>
#include <vector>
#include <set>

#include <libNTS/nts.hpp>

/**
 * @brief Represents set of global variables, which can be modified by something.
 *
 * invariant: I1: If .everything is  true, then .vars.size() == 0.
 *            I2: Each item in .vars points to valid global variable.
 */
struct GlobalWrites
{
	std::set < const nts::Variable * > vars;
	bool everything;


	GlobalWrites() : everything ( false ) { ; }

	GlobalWrites ( const GlobalWrites & orig ) = default;
	GlobalWrites ( GlobalWrites && old ) = default;


	GlobalWrites & operator= ( const GlobalWrites & orig ) = default;
	GlobalWrites & operator= ( GlobalWrites && old ) = default;

	void union_with ( const GlobalWrites & other );

	/**
	 * @pre Variable must be global variable.
	 */
	bool contains ( const nts::Variable * var ) const;

	void insert ( const nts::Variable * var );
	void insert_everything();

	void clear();
};

std::ostream & operator<< ( std::ostream & o, const GlobalWrites & gw );

struct GlobalReads : public std::set < const nts::Variable * >
{
	bool contains ( const nts::Variable * var ) const;
};

std::ostream & operator<< ( std::ostream & o, const GlobalReads & gw );

struct Globals
{
	GlobalReads  reads;
	GlobalWrites writes;

	void union_with ( const Globals & other );

	/**
	 * Commutative.
	 * True iff there exists some global variable, which is
	 * read or modified by one Globals and modified by second Globals.
	 */
	bool may_collide_with ( const Globals & other ) const;
};

std::ostream & operator<< ( std::ostream & o, const Globals & gs );

/**
 * @brief Additional information about transition.
 * Each transition belongs to the task,
 * to which belongs transition's 'from' state.
 *
 * predicate computed:
 * 	.transition points to valid Transition,
 * 	.transition->user_data points to this TransitionInfo,
 * 	.global.reads contains set of global variables, which are read by .transition,
 * 	and .global.writes contains set of global variables, which can possibly be
 * 	changed by executing .transition.
 *
 */
struct TransitionInfo
{
	nts::Transition * transition;
	Globals global;
};


struct StateInfo;

struct ControlState;
using NextStateFunction = std::function < ControlState * ( const ControlState *, unsigned int ) >;

struct InlinedProcedureCall
{
	TransitionInfo * transition;
	StateInfo * final_state;
	const NextStateFunction & next;
};

struct InlinedProcedureCalls
{
	std::map < const StateInfo *, InlinedProcedureCall * > calls;

	/**
	 * If it is possible to simulate transition for proccess 'pid',
	 * then returns new control state.
	 * If not, returns null.
	 *
	 */
	ControlState * next ( const ControlState * orig, unsigned int pid ) const;
};

struct StateInfo;

// Task is a basic organisation unit.
// It constitutes of states and transitions between them.
// During execution, instances of task can be assigned to threads.
// Task uses some subset of global variables. Also, a task
// may cause another task to be run. This can be computed using static analysis,
// before POR is run.

/**
 * predicate "states_assigned":
 *   Each state, which through its StateInfo points to this task,
 *   has its StateInfo once in this list.
 *
 * predicate "globals_computed":
 *   global.reads and global.writes are computed
 *
 * predicate "boundary_states_computed"
 *   .initial points to initial state,
 *   .final points to final state.
 *   There is no other initial or final state.
 */
struct Task
{
	const std::string name;
	std::vector < StateInfo * > states;
	Globals global;

	std::vector < StateInfo * > initial_states;
	std::vector < StateInfo * > final_states;

	bool has_number;
	unsigned int number;


	Task ( std::string name );
	~Task();

#if 0
	// Set of tasks, which can be caused to run directly by this task.
	std::set < Task * > tasks_may_be_run;

	// Which task can activate this task?
	std::set < Task * > can_be_run_by;
#endif

	/**
	 * @pre  Q1: "states_assigned" must be true
	 *       Q2: Each transition must have associated its TransitionInfo.
	 *       Q3: Each transition must have computed its globals.
	 *
	 * @post R1: "globals_computed" is true.
	 */
	void compute_globals();
};

class Tasks
{
	private:
		nts::Nts & n;
		std::set < nts::BasicNts * > toplevel_bnts;
		std::string main_nts_name;

		/**
		 * @pre  Nothing.
		 * @post Member field 'toplevel_bnts' contains set of all BasicNts-es,
		 *       which are instantiated in 'bn'.
		 * @assigns toplevel_bnts
		 */
		void calculate_toplevel_bnts();

		/**
		 * @pre  Q1: Calculated toplevel_bnts. 
		 *       Q2: All transitions should have null their user_data.
		 *       Q3 = compute_tasks's R1
		 *
		 * @post R1: All Transitions have associated computed TransitionInfo
		 *           (see compute_tasks's R2 ).
		 * @assigns Only transitions.
		 */
		void compute_transition_info();


		void print_transition_info ( std::ostream & o ) const;

		/**
		 * @pre   Q1: Every transition has associated computed TransitionInfo.
		 *        Q2: Every state has associated computed StateInfo.
		 *            In particular, every state belongs to one task.
		 *  @post R1: Every task has computed its structure.
		 */
		void compute_task_structure();

		/**
		 * @pre Q1: Postconditions POST_1 and POST_2 of split_to_task()
		 *          must hold, i.e. each state belongs to task
		 *          and each task has list of its states.
		 *
		 * @post R1: For each task T in .tasks,
		 *           boundary_states_computed ( T ).
		 *       R2: Each task have assigned its number.
		 *
		 */
		void find_tasks_initial_final_states();

		/**
		 * @pre  Tasks must have assigned their number
		 * @post Task with id 'i' is placed '.tasks[i]'
		 */
		void sort_tasks_by_id();

		void split_to_tasks();

		void split_to_tasks ( nts::BasicNts & bn, bool split_by_annot );

		Tasks ( nts::Nts & n );

	public:
		std::vector < Task * > tasks;
		std::map < std::string, Task * > name_to_task;
		Task * main_task;
		Task * idle_worker_task;


		/**
		 * @pre  Q1: Nts contains only BasicNtses, which are instantiated.
		 *       Q2: Each BasicNts is flat (i.e. it does not contain call rule)
		 *       Q3: Each state contains an "origin" annotation (see inliner).
		 *       Q4: All user pointers are null.
		 *       Q5: Nobody 'calls' main nts (there is no origin annotation starting with "main::").
		 *
		 * @post R1: Each state has associated (through its user_data) computed StateInfo structure.
		 *       R2: Each transition has associated (through its user_data) computed TransitionInfo structure.
		 *       R3: Each task has its Task structure computed.
		 *
		 * @param main_nts   name of main BasicNts. All states and transition in this BasicNts
		 *                   are treated as one task
		 */
		static Tasks * compute_tasks ( nts::Nts & n, const std::string & main_nts );

		~Tasks();
};


struct StateInfo
{
	nts::State * st;
	Task * t;
};



// Each variable is associated (through its user pointer)
// to an instance of this class.
struct GlobalVariableInfo
{
	nts::Variable * var;

	std::set < Task * > read_users;
	std::set < Task * > write_users;
};


nts::AnnotString * find_annot_origin ( nts::Annotations & ants );



#endif // POR_TASKS_HPP
