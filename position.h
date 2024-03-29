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
  unsigned char fifty_move_rule;
  bool was_capture;

  Position(bool wtm, int n, double e, Board b, Castling c /*, time_t t*/,
           std::vector<Position*>* o, Position* pm, int d,
           /*Position* bm,*/ Kings k, int ept, unsigned char fmc, bool wc);
  ~Position();

  static Position* StartingPosition();

  Position GenerateMovesCopy();
  Position* CreateDeepCopy();  
  Position* RealDeepCopy() const;
};

struct Tool {
  std::string name;
  void (*display)(Position&);
  bool on;
  //Tool(const std::string& n, void (*d)(Position&), const bool& o)
  //    : name(n), display(d), on(o){};
};
using Tools = std::array<Tool, 5>;