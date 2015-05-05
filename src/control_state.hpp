#ifndef POR_SRC_CONTROL_STATE_HPP_
#define POR_SRC_CONTROL_STATE_HPP_
#pragma once

#include <functional>
#include <ostream>
#include <vector>
#include <unordered_set>
#include <set>

#include <libNTS/nts.hpp>


/**
 * Notes about some explicit-state information
 * ===
 * In current implementation, the explicit knowledge of values
 * is based in knowing, how our parellel model works.
 * In particular, we assume following:
 *
 * 1) If an instance does not use 'main' BasicNts,
 *    it represents worker threads.
 *
 * 2) BasicNts with name '__thread_create' takes first
 *    unused (worker) thread and assigns to it the task
 *    with given number.
 *
 * 3) We have somehow numbered tasks.
 *
 * 4) We know the initial and final states of tasks.
 *
 * 5) We know what variables __thread_pool_lock and __thread_pool_selected
 *    means and that only __thread_create and idle task
 *    can modify them.
 *
 *
 *
 * Later, we may like to use some solver to track these variables
 * in some less ad-hoc manner. But I think that actual solution
 * would still be more efficient.
 */


struct ProcessState
{
	/**
	 * If bnts_state == null,
	 * this proccess is not running any task.
	 *
	 * Note that main process is always running main task.
	 */
	nts::State * bnts_state;

	explicit ProcessState ( nts::State * s ) : bnts_state ( s ) { ; }

	bool operator== ( const ProcessState & other ) const;
	bool operator!= ( const ProcessState & other ) const;

	static std::size_t calculate_hash ( const ProcessState & st );
};

struct ControlState;

struct CFGEdge
{
	ControlState * to;
	nts::Transition * t;
	unsigned int pid;

	CFGEdge ( ControlState * to, nts::Transition *t, unsigned int pid ) :
		to ( to ), t ( t ), pid ( pid )
	{
		;
	}
	// Invariant:
	// to->states[pid].bnts_state == & t->to()
};

/**
 * @brief Describes control state of serialized NTS
 */
struct ControlState
{
	// One state per process
	std::vector < ProcessState > states;

	/**
	 * If null, this is the initial state.
	 * If not, then prev->states contains this
	 */
	ControlState * reached_from;


	/**
	 * States which could be reached from this state.
	 * invariant: \forall e in next,
	 * & e.t->from() == states[e.pid].bnts_state
	 */
	std::vector < CFGEdge > next;

	ControlState() { ; }
	ControlState ( const ControlState & ) = delete;
	ControlState ( ControlState && ) = delete;

	bool operator== ( const ControlState & other ) const;

	/**
	 * Is this state on search stack of given st?
	 */
	//ControlState * on_stack ( ControlState & st ) const;

	void print ( std::ostream & o ) const;

	static size_t calculate_hash ( const ControlState & cs );
	static size_t calculate_hash_p ( const ControlState * cs );

};

ControlState * initial_control_state ( const nts::Nts & n );

template < typename T>
struct derref_equal
{
	bool operator() ( const T * a, const T * b ) const
	{
		return *a == *b;
	}
};

// TODO Dat si pozor na to, kde v celem programu pouzivam uordered_set. I v ilineru a prekladu.

class ControlFlowGraph
{
	private:
		std::unordered_set <
			ControlState *,
			std::function < size_t (const ControlState *) >,
			derref_equal < ControlState >
		> states;

		std::set < ControlState * > unexplored_states;
		ControlState * initial;

		ControlFlowGraph();


		void explore ( ControlState * cs, unsigned int pid );
		void explore ( ControlState * cs );

		/**
		 * returns false if there is no state to be explored
		 */
		bool explore_next_state();

	public:

		static ControlFlowGraph * build ( const nts::Nts & n );
};


#endif // POR_SRC_CONTROL_STATE_HPP
