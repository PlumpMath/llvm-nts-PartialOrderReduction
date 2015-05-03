#include <stdexcept>
#include <algorithm>

#include <libNTS/variables.hpp>

#include "logic_utils.hpp"

using std::logic_error;
using std::set;
using std::set_union;

using namespace nts;

using VariablePredicate = std::function < bool ( const nts::Variable & v ) >;

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

void used_variables ( const Transition & t, const VariablePredicate & p, Globals & g );

void used_variables ( const Term & t, const VariablePredicate & p, Globals & g )
{
	switch ( t.term_type() )
	{
		case Term::TermType::ArithmeticOperation:
		{
			auto & ao = static_cast < const ArithmeticOperation & > ( t );
			used_variables ( ao.term1(), p, g );
			used_variables ( ao.term2(), p, g );
			return;
		}

		case Term::TermType::MinusTerm:
		{
			auto & mt = static_cast < const MinusTerm & > ( t );
			used_variables ( mt.term(), p, g );
			return;
		}

		case Term::TermType::ArrayTerm:
		{
			auto & at = static_cast < const ArrayTerm & > ( t );
			used_variables ( at.array(), p, g );

			for ( const Term * t : at.indices() )
				used_variables ( *t, p, g );

			return;
		}

		case Term::TermType::Leaf:
		{
			auto & lf = static_cast < const Leaf & > ( t );
			switch ( lf.leaf_type() )
			{
				case Leaf::LeafType::UserConstant:
					return;

				case Leaf::LeafType::IntConstant:
					return;

				case Leaf::LeafType::ThreadID:
					return;

				case Leaf::LeafType::VariableReference:
				{
					auto & vr = static_cast < const VariableReference & > ( lf );
					const Variable * var = vr.variable().get();

					if ( p ( *var ) )
					{
						if ( vr.primed() )
							g.writes.insert ( var );
						else
							g.reads.insert ( var );
					}

					return;
				}

				default:
					throw logic_error ( "Unknown LeafType" );
			}
		}

		default:
			throw logic_error ( "Unknown TermType" );
	}
}


void used_variables ( const Formula & f, const VariablePredicate & p, Globals & g )
{

	switch ( f.type() )
	{
		case Formula::Type::FormulaBop:
		{
			auto & fb = static_cast < const FormulaBop & > ( f );
			used_variables ( fb.formula_1(), p, g );
			used_variables ( fb.formula_2(), p, g );
			return;
		}

		case Formula::Type::FormulaNot:
		{
			auto & fn = static_cast < const FormulaNot & > ( f );
			used_variables ( fn.formula(), p, g );
			return;
		}

		case Formula::Type::QuantifiedFormula:
		{
			auto & qf = static_cast < const QuantifiedFormula & > ( f );
			used_variables ( qf.formula(), p, g );

			const QuantifiedType & qtype = qf.list.qtype();
			const DataType       & dtype = qtype.type();
			for ( Term * t : dtype.idx_terms() )
				used_variables ( *t, p, g );
	
			if ( qtype.from() )
				used_variables ( *qtype.from(), p, g );

			if ( qtype.to() )
				used_variables ( *qtype.to(), p, g );

			return;
		}

		case Formula::Type::AtomicProposition:
		{
			auto & ap = static_cast < const AtomicProposition & > ( f );
			switch ( ap.aptype() )
			{
				case AtomicProposition::APType::BooleanTerm:
				{
					auto & bt = static_cast < const BooleanTerm & > ( ap );
					used_variables ( bt.term(), p, g );
					return;
				}

				case AtomicProposition::APType::Relation:
				{
					auto & re = static_cast < const Relation & > ( ap );
					used_variables ( re.term1(), p, g );
					used_variables ( re.term2(), p, g );
					return;
				}

				case AtomicProposition::APType::ArrayWrite:
				{
					auto & aw = static_cast < const ArrayWrite & > ( ap );

					if ( p ( *aw.array() ) )
						g.writes.insert ( aw.array() );

					for ( Term * t : aw.indices_1() )
						used_variables ( *t, p, g );

					for ( Term * t : aw.indices_2() )
						used_variables ( *t, p, g );

					for ( Term * t : aw.values() )
						used_variables ( *t, p, g );

					return;
				}

				case AtomicProposition::APType::Havoc:
				{
					auto & hv = static_cast < const Havoc & > ( ap );

					for ( const VariableUse & u : hv.variables )
					{
						if ( p ( *u ) )
							g.writes.insert ( u.get() );
					}

					return;
				}

				default:
					throw logic_error ( "Unknown APType" );
			}
		}

		default:
			throw logic_error ( "Unknown Formula::Type" );

	}
}

void used_variables ( const Transition & t, const VariablePredicate & p, Globals & g )
{
	const TransitionRule & tr = t.rule();
	switch ( tr.kind() )
	{
		case TransitionRule::Kind::Formula:
		{
			auto & fr = static_cast < const FormulaTransitionRule & > ( tr );
			const Formula & f = fr.formula();

			if ( ! havoc_in_toplevel_conjunction ( f ) )
				g.writes.insert_everything();

			used_variables ( f, p, g );
			return;
		}

		case TransitionRule::Kind::Call:
		{
			auto & cr = static_cast < const CallTransitionRule &> ( tr );
			for ( const Term * t : cr.terms_in() )
				used_variables ( *t, p, g );

			for ( const VariableUse & u : cr.variables_out() )
			{
				if ( p ( *u ) )
					g.writes.insert ( u.get() );
			}

			return;
		}

		default:
			throw logic_error ( "Unknown TransitionRule::Kind" );
	}
}

Globals used_global_variables ( const Nts & n, const Transition & t )
{
	VariablePredicate p = [ &n ] ( const Variable & v ) -> bool
	{
		// Not owned by anyone?
		if ( ! v.container() )
			return false;

		return v.container() == & n.variables();
	};

	Globals g;
	used_variables ( t, p, g );
	return g;
}

