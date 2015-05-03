#include <stdexcept>

#include "control_state.hpp"

using std::logic_error;

using namespace nts;

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

