#ifndef LOGIC_UTILS_HPP_
#define LOGIC_UTILS_HPP_
#pragma once

#include <set>

#include <libNTS/nts.hpp>
#include <libNTS/logic.hpp>

struct VariableUsage
{
	nts::Variable * var;
	bool modify; // Whether this usage modifies it

	VariableUsage ( nts::Variable * var, bool modify ) :
		var ( var ), modify ( modify ) { ; }
	VariableUsage ( const VariableUsage & orig ) = default;
	VariableUsage ( VariableUsage && old ) = default;

	bool operator== ( const VariableUsage & other ) const;

	VariableUsage & operator= ( const VariableUsage & orig ) = default;
	VariableUsage & operator= ( VariableUsage && old ) = default;
	bool operator< ( const VariableUsage & snd ) const;
};


bool can_modify_all_variables ( const nts::Formula & f );
std::set < VariableUsage > used_variables ( const nts::Formula & f );















#endif // LOGIC_UTILS_HPP
