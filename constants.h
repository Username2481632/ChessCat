#pragma once

#include <map>

#include "types.h"

extern std::map<char, double> piece_values;

extern const int knight_moves[8];
extern const int bishop_moves[4];
extern const int king_moves[8];
extern const int rook_moves[4];
extern const Board starting_board;
extern const int mate;

extern const double adaptive_off_threshold;