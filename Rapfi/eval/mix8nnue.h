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

#include "evaluator.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Evaluation::mix8 {

using namespace Evaluation;

constexpr uint32_t ArchHashBase     = 0x00712850;
constexpr size_t   Alignment        = 32;
constexpr int      ShapeNum         = 708588;
constexpr int      PolicyDim        = 32;
constexpr int      ValueDim         = 96;
constexpr int      FeatureDim       = std::max(PolicyDim, ValueDim);
constexpr int      FeatureDWConvDim = 32;
constexpr int      NumBuckets       = 1;

struct alignas(Alignment) Mix8Weight
{
    // 1  mapping layer
    int16_t mapping[ShapeNum][FeatureDim];

    // 2  PReLU after mapping
    int16_t map_prelu_weight[FeatureDim];

    // 3  Depthwise conv
    int16_t feature_dwconv_weight[9][FeatureDWConvDim];
    int16_t feature_dwconv_bias[FeatureDWConvDim];

    // 4  Value sum scale
    float value_sum_scale_after_conv;
    float value_sum_scale_direct;
    char  __padding_to_32bytes_0[24];

    struct HeadBucket
    {
        // 5  Policy depthwise conv
        int16_t policy_dwconv_weight[33][PolicyDim];
        int16_t policy_dwconv_bias[PolicyDim];

        // 6  Policy dynamic pointwise conv
        float policy_pwconv_weight_layer_weight[ValueDim][PolicyDim];
        float policy_pwconv_weight_layer_bias[PolicyDim];

        // 7  Value MLP (layer 1,2,3)
        float value_l1_weight[ValueDim * 2][ValueDim];  // shape=(in, out)
        float value_l1_bias[ValueDim];
        float value_l2_weight[ValueDim][ValueDim];
        float value_l2_bias[ValueDim];
        float value_l3_weight[ValueDim][3];
        float value_l3_bias[3];

        // 8  Policy PReLU
        float policy_neg_weight;
        float policy_pos_weight;
        char  __padding_to_32bytes_1[12];
    } buckets[NumBuckets];
};

struct Mix8WeightTwoSide
{
    Mix8Weight *sideWeight[SIDE_NB];

    Mix8WeightTwoSide(std::unique_ptr<Mix8Weight> common) : sideWeight {common.get(), common.get()}
    {
        common.release();
    }
    Mix8WeightTwoSide(std::unique_ptr<Mix8Weight> black, std::unique_ptr<Mix8Weight> white)
        : sideWeight {black.release(), white.release()}
    {}
    ~Mix8WeightTwoSide()
    {
        if (sideWeight[BLACK] != sideWeight[WHITE])
            delete sideWeight[WHITE];
        delete sideWeight[BLACK];
    }
    const Mix8Weight &operator[](size_t i) const { return *sideWeight[i]; }
};

class alignas(Alignment) Mix8Accumulator
{
public:
    Mix8Accumulator(int boardSize);
    ~Mix8Accumulator();

    /// Init accumulator state to empty board.
    void clear(const Mix8Weight &w);
    /// Incremental update mix6 network state.
    enum UpdateType { MOVE, UNDO };
    template <UpdateType UT>
    void update(const Mix8Weight              &w,
                Color                          pieceColor,
                int                            x,
                int                            y,
                std::array<int32_t, ValueDim> *valueSumBoardBackup);

    /// Calculate value (win/loss/draw tuple) of current network state.
    std::tuple<float, float, float> evaluateValue(const Mix8Weight      &w,
                                                  const Mix8Weight      &oppoW,
                                                  const Mix8Accumulator &oppoAccumulator);
    /// Calculate policy value of current network state.
    void evaluatePolicy(const Mix8Weight &w, PolicyBuffer &policyBuffer);

private:
    friend class Mix8Evaluator;
    //=============================================================
    // Mix8 network states

    /// Value feature sum of the full board
    std::array<int32_t, ValueDim> valueSum;  // [ValueDim] (aligned to 32)
    /// Index table to convert line shape to map feature
    std::array<uint32_t, 4> *indexTable;  // [H*W, 4] (unaligned)
    /// Sumed map feature of four directions
    std::array<int16_t, FeatureDim> *mapSum;  // [H*W, MapDim] (aligned)
    /// Map feature after depth wise conv
    std::array<int16_t, FeatureDWConvDim> *mapAfterDWConv;  // [(H+2)*(W+2), DWConvDim] (aligned)

    //=============================================================
    int   boardSize;
    int   fullBoardSize;  // (boardSize + 2)
    float boardSizeScale;

    void initIndexTable();
    int  getBucketIndex() { return 0; }
};

class Mix8Evaluator : public Evaluator
{
public:
    Mix8Evaluator(int boardSize, Rule rule, std::filesystem::path weightPath);
    ~Mix8Evaluator();

    void initEmptyBoard();
    void beforeMove(const Board &board, Pos pos);
    void afterUndo(const Board &board, Pos pos);

    ValueType evaluateValue(const Board &board);
    void      evaluatePolicy(const Board &board, PolicyBuffer &policyBuffer);

private:
    struct MoveCache
    {
        Color  oldColor, newColor;
        int8_t x, y;

        friend bool isContraryMove(MoveCache a, MoveCache b)
        {
            bool isSameCoord = a.x == b.x && a.y == b.y;
            bool isContrary  = a.oldColor == b.newColor && a.newColor == b.oldColor;
            return isSameCoord && isContrary;
        }
    };

    /// Clear all caches to sync accumulator state with current board state.
    void clearCache(Color side);
    /// Record new board action, but not update accumulator instantly.
    void addCache(Color side, int x, int y, bool isUndo);

    Mix8WeightTwoSide /* non-owning ptr */    *weight;
    std::unique_ptr<Mix8Accumulator>           accumulator[2];
    std::vector<MoveCache>                     moveCache[2];
    std::vector<std::array<int32_t, ValueDim>> valueSumBoardHistory[2];
};

}  // namespace Evaluation::mix8