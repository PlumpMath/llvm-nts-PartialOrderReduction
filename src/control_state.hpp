#ifndef POR_SRC_CONTROL_STATE_HPP_
#define POR_SRC_CONTROL_STATE_HPP_
#pragma once

#include <vector>
#include <libNTS/nts.hpp>

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



#endif // POR_SRC_CONTROL_STATE_HPP
