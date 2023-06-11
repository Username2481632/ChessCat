#pragma once

#include "types.h"

#include <vector>

struct Position {
  bool white_to_move;
  int number;
  double evaluation;
  Board board;
  Castling castling;
  std::vector<Position*>* outcomes;
  int depth;
  Position* previous_move;
  Kings kings;
  int en_passant_target;
  size_t fifty_move_rule;

  Position(bool wtm, int n, double e, Board b, Castling c /*, time_t t*/,
           std::vector<Position*>* o, Position* pm, int d,
           /*Position* bm,*/ Kings k, int ept, size_t fmc);
  ~Position();

  static Position* StartingPosition();

  Position GenerateMovesCopy();
  Position* CreateDeepCopy();  
  Position* RealDeepCopy() const;
};
