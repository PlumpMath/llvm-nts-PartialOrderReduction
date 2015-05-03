#ifndef POR_SRC_CONTROL_STATE_HPP_
#define POR_SRC_CONTROL_STATE_HPP_
#pragma once

#include <ostream>
#include <vector>
#include <libNTS/nts.hpp>


/**
 * @brief Holds explicit values of selected variables.
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
struct ExplicitState
{

	ExplicitState ( unsigned int n_worker_threads );
};

/**
 * @brief Describes control state of serialized NTS
 */
struct ControlState
{
	// One state per process
	std::vector < const nts::State * > states;


	/**
	 * If null, this is the initial state.
	 * If not, then prev->states.size() == states.size()
	 */
	ControlState * prev;
	std::vector < ControlState * > next;
	std::vector < ControlState * > back_loop;
	unsigned int n_explored;


	bool expand();

	bool operator== ( const ControlState & other ) const;

	/**
	 * Is this state on search stack of given st?
	 */
	ControlState * on_stack ( ControlState & st ) const;

	/**
	 * @brief Does this state have a successor under pid,
	 *        which is on the stack, induced by 'st'?
	 */
	bool successor_on_stack ( const ControlState & st, unsigned int pid ) const;

	void print ( std::ostream & o ) const;
};

ControlState * initial_control_state ( const nts::Nts & n );


#endif // POR_SRC_CONTROL_STATE_HPP
