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
	// First state does not have 'from' edge.
	// If .from is null, .t and .pid have no meaning
	// and are undefined.
	ControlState * from;
	ControlState & to;
	nts::Transition * t;
	unsigned int pid;

	CFGEdge ( ControlState * from, ControlState & to, nts::Transition * t, unsigned int pid ) :
		from ( from ), to ( to ), t ( t ), pid ( pid )
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
	struct DFSInfo
	{
		enum class St
		{
			New,
			On_stack,
			Closed
		};

		St st;
		ControlState * reached_from; //< Creates search stack
		unsigned int visited_next;

		DFSInfo();
	};

	/**
	 * Invariant P1: if d.st > St::New, this state is expanded.
	 */

	DFSInfo di;

	// One state per process
	std::vector < ProcessState > states;

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


class ControlFlowGraph;

class IEdgeVisitor
{
	public:
		IEdgeVisitor() = default;
		virtual ~IEdgeVisitor() = default;

		virtual void operator() ( const CFGEdge & edge ) = 0;
};

using EdgeVisitorGenerator = std::function < IEdgeVisitor * ( ControlFlowGraph & ) >;

class ControlFlowGraph
{
	private:
		std::unordered_set <
			ControlState *,
			std::function < size_t (const ControlState *) >,
			derref_equal < ControlState >
		> states;

		std::vector < CFGEdge * > edges;

		//std::set < ControlState * > unexplored_states;
		ControlState * initial;

		// If not null, current->di.st == On_stack
		ControlState * current;

		ControlFlowGraph();


		void explore ( ControlState * cs, unsigned int pid );
		void explore ( ControlState * cs );

		/**
		 * returns false if there is no edge to be explored
		 */
		bool explore_next_edge();

		IEdgeVisitor * _edge_visitor;

	public:

		~ControlFlowGraph();

		// It does not modify cs.
		bool has_state ( ControlState & cs ) const;

		// if CFG does not have given state, returns nullptr.
		ControlState * get_state ( ControlState & cs ) const;

		// Caller must own the cs. After calling, this class
		// owns given cs and it is responsible to destroy it.
		// Therefore, cs must be allocated on heap,
		// so it might be possible to call delete on it.
		//
		// If given state already exists in graph, returns the old version.
		// Otherwise returns given cs.
		//
		// Returned value belongs to ControlFlofGraph
		ControlState & insert_state ( ControlState & cs );

		static ControlFlowGraph * build ( const nts::Nts & n, const EdgeVisitorGenerator & g );
};

struct SimpleVisitor : public IEdgeVisitor
{
	protected:
		ControlFlowGraph & g;

	public:
		SimpleVisitor ( ControlFlowGraph & g );
		virtual ~SimpleVisitor();

		void explore ( ControlState & cs, unsigned int pid );
		
		virtual void explore ( ControlState & cs );
		virtual void operator() ( const CFGEdge & e ) override;

};

SimpleVisitor * SimpleVisitor_generator ( ControlFlowGraph & g );

class Tasks;
struct POVisitor : public SimpleVisitor
{
	private:
		nts::Nts & n;
		Tasks * t;

	public:
		POVisitor ( ControlFlowGraph & g, nts::Nts & n );
		virtual ~POVisitor();

		/**
		 * @pre  Q1: All transitions in .n must have computed TransitionInfo
		 */
		bool try_ample ( ControlState & cs, unsigned int pid );
		virtual void explore ( ControlState & cs ) override;

		struct generator;
};

struct POVisitor::generator
{
	nts::Nts & n;
	generator ( nts::Nts & n ) : n ( n ) { ; }

	POVisitor * operator() ( ControlFlowGraph & g );
};

#endif // POR_SRC_CONTROL_STATE_HPP
