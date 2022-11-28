/*
 *  Rapfi, a Gomoku/Renju playing engine supporting piskvork protocol.
 *  Copyright (C) 2022  Rapfi developers
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "../../core/types.h"

#include <array>
#include <cmath>

namespace Search::AB {

// -------------------------------------------------
// Search limits

constexpr int MAX_DEPTH = 200;
constexpr int MAX_PLY   = 256;

// -------------------------------------------------
// Depth & Value Constants

constexpr Value MARGIN_INFINITE             = Value(INT16_MAX);
constexpr Depth ASPIRATION_DEPTH            = 5.0f;
constexpr Depth IID_DEPTH[RULE_NB]          = {12.86f, 12.12f, 12.68f};
constexpr Depth IIR_REDUCTION[RULE_NB]      = {0.93f, 0.69f, 0.51f};
constexpr Depth IIR_REDUCTION_PV[RULE_NB]   = {2.15f, 2.09f, 1.61f};
constexpr Depth SE_DEPTH[RULE_NB]           = {6.68f, 6.14f, 8.75f};
constexpr Depth SE_TTE_DEPTH[RULE_NB]       = {2.33f, 2.62f, 2.77f};
constexpr Depth LMR_DEPTH[RULE_NB]          = {2.78f, 2.51f, 2.54f};
constexpr Depth RAZOR_PRUN_DEPTH[RULE_NB]   = {2.89f, 2.16f, 2.74f};
constexpr Depth TRIVIAL_PRUN_DEPTH[RULE_NB] = {5.88f, 4.45f, 4.95f};

// -------------------------------------------------
// Dynamic margin & reduction functions/LUTs

/// Aspiration window delta. When prevDelta is zero, returns the initial
/// aspiration window size. Otherwise returns the next expanded window
/// size for the given previous delta.
constexpr Value nextAspirationWindowDelta(Value prevDelta = VALUE_ZERO)
{
    return prevDelta ? prevDelta * 3 / 2 + 5 : Value(17);
}

/// Razoring depth & margins
constexpr Value razorMargin(Depth d)
{
    return d < 3.36f ? static_cast<Value>(std::max(int(0.125f * d * d + 46 * d) + 49, 0))
                     : MARGIN_INFINITE;
}

/// Razoring verification margin
constexpr Value razorVerifyMargin(Depth d)
{
    return razorMargin(d - 2.9f);
}

/// Static futility pruning depth & margins
constexpr Value futilityMargin(Depth d, bool improving)
{
    return Value(std::max(int(54 * (d - improving)), 0));
}

/// Null move pruning margin
constexpr Value nullMoveMargin(Depth d)
{
    return d >= 8.0f ? Value(680 - 27 * std::min(int(d), 20)) : MARGIN_INFINITE;
}

/// Null move search depth reduction. The result of a null move will be
/// tested using reduced depth search.
constexpr Depth nullMoveReduction(Depth d)
{
    return 3.21f + 0.27f * d;
}

/// Internal iterative deepening depth reduction.
constexpr Depth iidDepthReduction(Depth d)
{
    return 7.0f;
}

/// Fail high reduction margin
constexpr Value failHighMargin(Depth d, int oppo4)
{
    return Value(40 * (int(d) + bool(oppo4) * 2));
}

/// Fail low reduction margin
constexpr Value failLowMargin(Depth d)
{
    return Value(100 + int(50 * d));
}

// Lookup tables used for move count based pruning, initialized at startup
inline const auto FutilityMC = []() {
    std::array<int, MAX_MOVES + 1> MC {0};  // [depth]
    for (size_t i = 1; i < MC.size(); i++)
        MC[i] = 3 + int(std::pow(i, 1.4));
    return MC;
}();

/// Move count based pruning. When we already have a non-losing move,
/// and opponent is not making a four at last step, moves that exceeds
/// futility move count will be directly pruned.
constexpr int futilityMoveCount(Depth d, bool improving)
{
    return FutilityMC[std::max(int(d), 0)] / (2 - improving);
}

/// Singular extension margin
constexpr Value singularMargin(Depth d, bool formerPv)
{
    return Value((2 + formerPv) * d);
}

/// Depth reduction for singular move test search
constexpr Depth singularReduction(Depth d, bool formerPv)
{
    return d * 0.5f - formerPv;
}

/// Margin for double singular extension
constexpr Value doubleSEMargin(Depth d)
{
    return Value(70 - std::min(int(d) / 2, 20));
}

/// Delta pruning margin for QVCF search
constexpr Value qvcfDeltaMargin(Rule rule, Depth d)  // note: d <= 0
{
    return Value(std::max((rule == RENJU ? 4000 : 2500) + 64 * int(d), 600));
}

/// LMR move count. For non-PV all node, moves that exceeds late move count
/// will be searched will late move reduction even without other condition.
constexpr int lateMoveCount(Depth d, bool improving)
{
    return 1 + 2 * improving + int((improving ? 1.35f : 1.2f) * d);
}

/// Init Reductions table according to num threads.
inline void initReductionLUT(std::array<Depth, MAX_MOVES + 1> &lut, int numThreads = 1)
{
    double factor     = 1.0 / std::sqrt(1.95);
    double threadBias = 0.1 * std::log(numThreads);
    lut[0]            = 0.0f;
    for (size_t i = 1; i < lut.size(); i++)
        lut[i] = float(factor * (std::log(i) + threadBias));
}

/// Basic depth reduction in LMR search
template <bool PvNode>
constexpr Depth reduction(const std::array<Depth, MAX_MOVES + 1> &lut,
                          Depth                                   d,
                          int                                     moveCount,
                          int                                     improvement,
                          Value                                   delta,
                          Value                                   rootDelta)
{
    assert(d > 0.0f);
    assert(moveCount > 0 && moveCount < lut.size());
    Depth r = lut[(int)d] * lut[moveCount];
    if constexpr (PvNode)
        return std::max(r - Depth(delta) / Depth(rootDelta), 0.0f);
    else
        return r + (improvement <= 0 && r > 1.0f);
}

constexpr Depth CR1[RULE_NB]                  = {0.01f * 8.475f, 0.01f * 9.0f, 0.01f * 7.200f};
constexpr Depth CR2[RULE_NB]                  = {0.01f * 4.143f, 0.01f * 4.0f, 0.01f * 3.628f};
constexpr Depth CR3[RULE_NB]                  = {0.01f * 2.189f, 0.01f * 2.0f, 0.01f * 1.950f};
constexpr Depth CR4[RULE_NB]                  = {0.01f * 0.719f, 0.01f * 0.7f, 0.01f * 0.681f};
constexpr Depth PolicyReductionScale[RULE_NB] = {2.818f, 3.2f, 3.469f};
constexpr Depth PolicyReductionBias[RULE_NB]  = {3.724f, 5.0f, 5.205f};
constexpr Depth PolicyReductionMax[RULE_NB]   = {3.696f, 4.0f, 4.047f};

template <Rule R>
constexpr Depth complexityReduction(bool trivialMove, bool importantMove, bool distract)
{
    return (trivialMove ? (distract ? CR1 : CR2) : !importantMove ? CR3 : CR4)[R];
}

}  // namespace Search::AB
