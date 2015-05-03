#include <stdexcept>
#include <algorithm>

#include <libNTS/variables.hpp>

#include "logic_utils.hpp"

using std::logic_error;
using std::set;
using std::set_union;

using namespace nts;

bool VariableUsage::operator== ( const VariableUsage & other ) const
{
	return ( var == other.var ) && ( modify == other.modify );
}

bool VariableUsage::operator< ( const VariableUsage & snd ) const
{
	if ( var < snd.var )
		return true;
	
	if ( var > snd.var )
		return false;

	return ( modify < snd.modify );
}

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

bool can_modify_all_variables ( const Formula & f )
{
	return !havoc_in_toplevel_conjunction ( f );
}

namespace
{
	void add ( set < VariableUsage > & s1, const set < VariableUsage > & s2 )
	{
		s1.insert ( s1.cbegin(), s2.cend() );
#if 0
		set_union (
				s1.begin(), s1.end(),
				s2.begin(), s2.end(),
				s1.begin()
		);
#endif
	}
}

set < VariableUsage > used_variables ( const Term & t )
{
	switch ( t.term_type() )
	{
		case Term::TermType::ArithmeticOperation:
		{
			auto & ao = static_cast < const ArithmeticOperation & > ( t );
			set < VariableUsage > s;
			add ( s, used_variables ( ao.term1() ) );
			add ( s, used_variables ( ao.term2() ) );
			return s;
		}

		case Term::TermType::MinusTerm:
		{
			auto & mt = static_cast < const MinusTerm & > ( t );
			return used_variables ( mt.term() );
		}

		case Term::TermType::ArrayTerm:
		{
			auto & at = static_cast < const ArrayTerm & > ( t );
			set < VariableUsage > s;
			add ( s, used_variables ( at.array() ) );
			for ( const Term * t : at.indices() )
				add ( s, used_variables ( *t ) );
			return s;
		}

		case Term::TermType::Leaf:
		{
			auto & lf = static_cast < const Leaf & > ( t );
			switch ( lf.leaf_type() )
			{
				case Leaf::LeafType::UserConstant:
					return set < VariableUsage > {};

				case Leaf::LeafType::IntConstant:
					return set < VariableUsage > {};

				case Leaf::LeafType::ThreadID:
					return set < VariableUsage > {};

				case Leaf::LeafType::VariableReference:
				{
					auto & vr = static_cast < const VariableReference & > ( lf );
					return set < VariableUsage > {
						VariableUsage ( vr.variable().get(), vr.primed() )
					};
				}

				default:
					throw logic_error ( "Unknown LeafType" );
			}
		}

		default:
			throw logic_error ( "Unknown TermType" );
	}
}


set < VariableUsage > used_variables ( const Formula & f )
{

	switch ( f.type() )
	{
		case Formula::Type::FormulaBop:
		{
			set < VariableUsage > s;
			auto & fb = static_cast < const FormulaBop & > ( f );
			add ( s, used_variables ( fb.formula_1() ) );
			add ( s, used_variables ( fb.formula_2() ) );
			return s;
		}

		case Formula::Type::FormulaNot:
		{
			auto & fn = static_cast < const FormulaNot & > ( f );
			return used_variables ( fn.formula() );
		}

		case Formula::Type::QuantifiedFormula:
		{
			set < VariableUsage > s;
			auto & qf = static_cast < const QuantifiedFormula & > ( f );
			add ( s, used_variables ( qf.formula() ) );

			const QuantifiedType & qtype = qf.list.qtype();
			const DataType       & dtype = qtype.type();
			for ( Term * t : dtype.idx_terms() )
			{
				add ( s, used_variables ( *t ) );
			}

			if ( qtype.from() )
				add ( s, used_variables ( *qtype.from() ) );

			if ( qtype.to() )
				add ( s, used_variables ( *qtype.to() ) );

			return s;
		}

		case Formula::Type::AtomicProposition:
		{
			auto & ap = static_cast < const AtomicProposition & > ( f );
			switch ( ap.aptype() )
			{
				case AtomicProposition::APType::BooleanTerm:
				{
					auto & bt = static_cast < const BooleanTerm & > ( ap );
					return used_variables ( bt.term() );
				}

				case AtomicProposition::APType::Relation:
				{
					auto & re = static_cast < const Relation & > ( ap );
					set < VariableUsage > s;
					add ( s, used_variables ( re.term1() ) );
					add ( s, used_variables ( re.term2() ) );
					return s;
				}

				case AtomicProposition::APType::ArrayWrite:
				{
					auto & aw = static_cast < const ArrayWrite & > ( ap );
					set < VariableUsage > s;

					s.insert ( VariableUsage ( aw.array(), true ) );

					for ( Term * t : aw.indices_1() )
						add ( s, used_variables ( *t ) );

					for ( Term * t : aw.indices_2() )
						add ( s, used_variables ( *t ) );

					for ( Term * t : aw.values() )
						add ( s, used_variables ( *t ) );

					return s;
				}

				case AtomicProposition::APType::Havoc:
				{
					auto & hv = static_cast < const Havoc & > ( ap );
					set < VariableUsage > s;

					for ( const VariableUse & u : hv.variables )
						s.insert ( VariableUsage ( u.get(), true ) );

					return s;
				}

				default:
					throw logic_error ( "Unknown APType" );
			}
		}

		default:
			throw logic_error ( "Unknown Formula::Type" );

	}
}

set < VariableUsage > used_variables ( const Transition & t )
{
	const TransitionRule & tr = t.rule();
	switch ( tr.kind() )
	{
		case TransitionRule::Kind::Formula:
		{
			auto & fr = static_cast < const FormulaTransitionRule & > ( tr );
			return used_variables ( fr.formula() );
		}

		case TransitionRule::Kind::Call:
		{
			auto & cr = static_cast < const CallTransitionRule &> ( tr );
			set < VariableUsage > s;
			for ( const Term * t : cr.terms_in() )
				add ( s, used_variables ( *t ) );

			for ( const VariableUse & u : cr.variables_out() )
				s.insert ( VariableUsage ( u.get(), true ) );

		}

		default:
			throw logic_error ( "Unknown TransitionRule::Kind" );
	}
}
