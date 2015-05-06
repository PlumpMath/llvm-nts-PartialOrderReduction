#ifndef NTS_SEQ_HPP_
#define NTS_SEQ_HPP_
#pragma once

#include <memory>

#include <libNTS/nts.hpp>

namespace nts
{

enum class SeqMode
{
	Simple,
	PartialOrderReduction
};

std::unique_ptr < nts::Nts > sequentialize ( nts::Nts & n, SeqMode mode );

} // namespace nts

#endif // NTS_SEQ_HPP_
