
#include <libNTS/variables.hpp>
#include <libNTS/inliner.hpp>

#include "logic_utils.hpp"


namespace nts {
namespace seq {


bool havoc_in_toplevel_conjunction ( const Formula & f )
{
	if ( f.type() == Formula::Type::AtomicProposition )
	{
		auto & ap = static_cast < const AtomicProposition & > ( f );
		if ( ap.aptype() == AtomicProposition::APType::Havoc )
			return true;
	}

	if ( f.type() == Formula::Type::FormulaBop )
	{
		auto & fb = static_cast < const FormulaBop & > ( f );
		if ( fb.op() == BoolOp::And )
		{
			return havoc_in_toplevel_conjunction ( fb.formula_1() )
				|| havoc_in_toplevel_conjunction ( fb.formula_2() ) ; 
		}
	}

	return false;
}

Globals used_global_variables ( const Nts & n, const Transition & t )
{
	Globals g;

	if ( t.rule().kind() == TransitionRule::Kind::Formula )
	{
		auto & ftr = static_cast < const FormulaTransitionRule & > ( t.rule() );
		if ( ! havoc_in_toplevel_conjunction ( ftr.formula() ) )
			g.writes.insert_everything();
	}

	VariableUse::visitor v = [ &g, &n ] ( const VariableUse & u )
	{
		if ( u->container() == & n.variables() )
		{
			if ( u.modifying )
				g.writes.insert ( u.get() );
			else
				g.reads.insert ( u.get() );
		}
	};

	visit_variable_uses vvu ( v );
	vvu.visit ( t.rule() );

	return g;
}

} // namespace seq
} // namespace nts
