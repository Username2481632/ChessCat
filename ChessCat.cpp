#include "utils.h"

#include <Windows.h>
#include <assert.h>
#include <process.h>
#include <psapi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <ratio>
#include <regex>
#include <semaphore>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#undef min
#undef max

std::map<char, double> piece_values = {
    {'P', 1.0}, {'N', 3.0}, {'B', 3.0}, {'R', 5.0}, {'Q', 9.0}};
const int knight_moves[8] = {-17, -15, -6, 10, 17, 15, 6, -10};
const int bishop_moves[4] = {-9, -7, 9, 7};
const int king_moves[8] = {-9, -8, -7, 1, 9, 8, 7, -1};
const int rook_moves[4] = {-8, 1, 8, -1};

std::binary_semaphore computed{0};
std::mutex position_mutex;


// enum class PieceType : char { Empty, Rook, Knight, Bishop, Queen, King, Pawn
// }; enum class ColorType : char { White, Black }; struct Piece {
//   PieceType type = PieceType::Empty;
//   ColorType color = ColorType::White;
// };
enum class Color : char { White, Black, Empty};
struct Piece {
  Color color;
  char type;
  
  bool IsEmpty() const { return color == Color::Empty;
  }
  bool operator==(const Piece& other) const {
    return color == other.color && type == other.type;
  }
  bool operator!=(const Piece& other) const {
    return color != other.color || type != other.type;
  }
  bool Equals(const Color& c, const char& t) const {
    return color == c && type == t;
  }
  void set(const Color& c, const char& t) {
    color = c;
    type = t;
  }
  void Empty() {
    color = Color::Empty;
    type = ' ';
  }
  Piece(const Color& c, const char& t) : color(c), type(t){};
};
//struct Point {
//  size_t row;
//  size_t column;
//  bool Equals(size_t r, size_t c) { return row == r && column == c; }
//  Point(size_t r, size_t c) : row(r), column(c) {}
//  //void clear() { row = column = -1;
//  //}
//  void Set(size_t r, size_t c) {
//    row = r;
//    column = c;
//  };
//  bool operator==(const Point& other) const {
//    return row == other.row && column == other.column;
//  }
//
//  bool operator<(const Point& other) const {
//    if (row < other.row) return true;
//    if (row > other.row) return false;
//    if (column < other.column) return true;
//    return false;
//  }
//  Point(){}
//};
struct Castling {
  bool white_o_o : 1;
  bool white_o_o_o : 1;
  bool black_o_o : 1;
  bool black_o_o_o : 1;

  bool operator!=(const Castling other) const {
    return white_o_o != other.white_o_o || white_o_o_o != other.white_o_o_o ||
           black_o_o != other.black_o_o || black_o_o_o != other.black_o_o_o;
  };
  bool operator==(const Castling other) const {
    return white_o_o == other.white_o_o && white_o_o_o == other.white_o_o_o &&
           black_o_o == other.black_o_o && black_o_o_o == other.black_o_o_o;
  };

};

using Board = std::array<Piece, 64>;
struct Kings {
  size_t white_king;
  size_t black_king;
  size_t& operator[](const Color& color) {
    return color == Color::White ? white_king : black_king;
  };
  Kings(size_t wk, size_t bk) : white_king(wk), black_king(bk){};
};

 //int g_copied_boards = 0;
 //int boards_created = 0;
 //int boards_destroyed = 0;

Board starting_board = {
    {Piece(Color::Black, 'R'), Piece(Color::Black, 'N'), Piece(Color::Black, 'B'),
       Piece(Color::Black, 'Q'), Piece(Color::Black, 'K'), Piece(Color::Black, 'B'),
       Piece(Color::Black, 'N'), Piece(Color::Black, 'R'),
     Piece(Color::Black, 'P'), Piece(Color::Black, 'P'), Piece(Color::Black, 'P'),
       Piece(Color::Black, 'P'), Piece(Color::Black, 'P'), Piece(Color::Black, 'P'),
       Piece(Color::Black, 'P'), Piece(Color::Black, 'P'),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
       Piece(Color::Empty, ' '), Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
       Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
       Piece(Color::Empty, ' '), Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
       Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
       Piece(Color::Empty, ' '), Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
       Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
       Piece(Color::Empty, ' '), Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
       Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::White, 'P'), Piece(Color::White, 'P'), Piece(Color::White, 'P'),
       Piece(Color::White, 'P'), Piece(Color::White, 'P'), Piece(Color::White, 'P'),
       Piece(Color::White, 'P'), Piece(Color::White, 'P'),
     Piece(Color::White, 'R'), Piece(Color::White, 'N'), Piece(Color::White, 'B'),
       Piece(Color::White, 'Q'), Piece(Color::White, 'K'), Piece(Color::White, 'B'),
       Piece(Color::White, 'N'), Piece(Color::White, 'R')}};

//struct IntPoint {
//  int row;
//  int column;
//  bool Equals(const int& r, const int& c) const {
//    return row == r && column == c;
//  }
//  IntPoint(int r, int c) : row(r), column(c) {}
//  void clear() { row = column = -1; }
//  void Set(int r, int c) {
//    row = r;
//    column = c;
//  };
//  bool operator==(const IntPoint& other) const {
//    return row == other.row && column == other.column;
//  }
//
//  bool operator<(const IntPoint& other) const {
//    if (row < other.row) return true;
//    if (row > other.row) return false;
//    if (column < other.column) return true;
//    return false;
//  }
//};


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



  Position(bool wtm, int n, double e, Board b,
           Castling c /*, time_t t*/,
           std::vector<Position*>* o,
           Position* pm, int d, /*Position* bm,*/Kings k, int ept, size_t fmc)
      : white_to_move(wtm),
        number(n),
        evaluation(e),
        board(b),
        castling(c),
        //time(t),
        outcomes(o),
        depth(d),
        previous_move(pm),
        //best_move(bm),
        kings(k),
        en_passant_target(ept),
        fifty_move_rule(fmc) {
    //boards_created++;
  }
  /*Position(const Board& b, const Castling& c, time_t ti, char to)
      : board(b), castling(c), time(ti), to_move(to) {}*/
  /*Position(const Board& b, const Castling& c)
      : Position(b, c, 0LL) {}*/
  // Possibilities possibilities;

  static Position* StartingPosition() {
    return new Position(true, 0, 0.0, starting_board, Castling(true, true, true, true), nullptr, nullptr, 0, Kings(60, 4), -1, 0);
  }

  Position GenerateMovesCopy() {
    return Position(!white_to_move, number + 1, round(evaluation), board, castling, nullptr /* outcomes */,
                    this /* previous_move */, -1 /*depth*/, /*
                    nullptr /* best_move */
                    /*, */ kings, -1, fifty_move_rule + 1);
  }
  ~Position() {
    // delete all outcomes pointers
    if (outcomes) {
      for (size_t i = 0; i < outcomes->size(); i++) {
        delete (*outcomes)[i];
        // boards_destroyed++;
      }
      delete outcomes;
      outcomes = nullptr;
    }

  };

  Position* CreateDeepCopy() {
    Position* new_position = new Position(white_to_move, number, evaluation, board, castling,
        nullptr /* outcomes */, previous_move /* previous_move */, depth /*,
        nullptr *//* best_move */, kings, en_passant_target, fifty_move_rule);
    return new_position;
  }
  Position* RealDeepCopy() const { // previous move is not a deep copy
    Position* new_position = new Position(
        white_to_move, number, evaluation, board, castling /*, time*/,
        nullptr /* outcomes */, previous_move /* previous_move */, depth /*,
        nullptr *//* best_move */, kings, en_passant_target, fifty_move_rule);

    if (outcomes) {
      new_position->outcomes = new std::vector<Position*>;
      for (size_t i = 0; i < outcomes->size(); i++) {
        new_position->outcomes->emplace_back((*outcomes)[i]->RealDeepCopy());
        //(*new_position->outcomes)[i]->previous_move = new_position;
      }
    }

    return new_position;
  }
};

using Pair = std::pair<Board, Castling>;
using HistoryType = std::vector<Position>;
using MovesType = std::vector<Pair>;
// using Possibilities = std::vector<Position>;


struct Check {
  bool in_check = false;
  bool king_must_move = false;
  std::map<size_t, std::set<size_t>> pinned;
  std::set<size_t> block;
};


void GetCheckInfo(Check& output, Position& position, Color side = Color::Empty) {
  if (side == Color::Empty) {
    side = position.white_to_move ? Color::White : Color::Black;
  }
  int new_i;
  /*if (!king_present(position)) {
    int i=1;
  }*/
  Color opponent;
  int found;
  std::set<size_t> done;
  output.in_check = false;
  /*if (position.kings[side][1] == -1) {
    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
        if (position.board[i][j][0] == side &&
            position.board[i][j][1] == 'K') {
          position.kings[side][0] = i;
          position.kings[side][1] = j;
          i = 8;
          break;
        }
      }
    }
  }*/
  size_t i = (size_t)position.kings[side];
  // pawn check
  if (side == Color::White) {
    opponent = Color::Black;
    // right check
    if (i > 7 && ((i & 7) != 7)) {
      new_i = (int)i - 7;
      if (position.board[(size_t)new_i].color == opponent &&
          position.board[(size_t)new_i].type == 'P') {
        output.in_check = true;
        output.block.insert((size_t)new_i);
      }
    }
    // left check
    if (i > 8 && (i & 7) != 0) {
      new_i = (int)i - 9;
      if (position.board[(size_t)new_i].color == opponent &&
          position.board[(size_t)new_i].type == 'P') {
        output.in_check = true;
        output.block.insert((size_t)new_i);
      }
    }
  } else {
    opponent = Color::White;
    // right check
    if (i < 55 && ((i & 7) != 7)) {
      new_i = (int)i + 9;
      if (position.board[(size_t)new_i].color == opponent &&
          position.board[(size_t)new_i].type == 'P') {
        output.in_check = true;
        output.block.insert((size_t)new_i);
      }
    }
    // left check
    if (i < 56 && (i & 7) != 0) {
      new_i = (int)i + 7;
      if (position.board[(size_t)new_i].color == opponent &&
          position.board[(size_t)new_i].type == 'P') {
        output.in_check = true;
        output.block.insert((size_t)new_i);
      }
    }
  }

  // knight check
  for (size_t k = 0; k < 8; k++) {
    new_i = (int)i + knight_moves[k];
    if (new_i >= 0 && new_i <= 63) {
      if (position.board[(size_t)new_i].type ==
              'N'&&
          position.board[(size_t)new_i].color == opponent          ) {
        if (output.in_check) {
          output.king_must_move = true;
        } else {
          output.in_check = true;
          output.block.insert((size_t)new_i);
        }
      }
    }
  }
  // bishop check
  for (size_t k = 0; k < 4; k++) {
    found = -1;
    done.clear();
    new_i = (int)i + bishop_moves[k];
    if (((new_i & 7) - ((int)i & 7)) > 1 || ((new_i & 7) - ((int)i & 7)) < -1 ||
        new_i > 63 || new_i < 0) {
      continue;
    }
    while (true) {
      done.insert((size_t)new_i);
      if (position.board[(size_t)new_i].color == side) {
        if (found != -1) {
          break;
        }
        found = new_i;
      } else if (position.board[(size_t)new_i].color == opponent) {
        if (position.board[(size_t)new_i].type == 'B' ||
            position.board[(size_t)new_i].type == 'Q') {
          if (found != -1) {
            //if (position.board[found.row][found.column].type == 'B' ||
            //    position.board[found.row][found.column].type == 'Q' /*need to account for pawns too*/) {
              // (position.board[(size_t)new_i][(size_t)new_j].type != 'Q' ? (position.board[found.row][found.column].type != position.board[(size_t)new_i][(size_t)new_j].type || (position.board[found.row][found.column].type == 'Q' && (position.board[(size_t)new_i][(size_t)new_j].type == 'B' || position.board[(size_t)new_i][(size_t)new_j].type == 'R'))) : (position.board[found.row][found.column].type == 'B' || position.board[found.row][found.column].type == 'R')
              output.pinned[(size_t)found] = done;
            //} else {
            //  output.pinned[found];
            //}
          } else {
            if (output.in_check) {
              output.king_must_move = true;
              output.block.clear();
            } else {
              output.in_check = true;
              output.block = done;
            }
          }
        }
        break;
      }
      if (new_i <= 8 || new_i >= 55 || (new_i & 7) == 7 || (new_i & 7) == 0) {
        break;
      }
      new_i += bishop_moves[k];
    }
  }
  // rook check


  for (size_t k = 0; k < 4; k++) {
    found = -1;
    done.clear();
    new_i = (int)i + rook_moves[k];
    if ((((new_i & 7) != ((int)i & 7)) &&
         ((new_i >> 3) != ((int)i >> 3))) ||
        new_i > 63 || new_i < 0) {
      continue;
    }
    while (true) {
      done.insert((size_t)new_i);
      if (position.board[(size_t)new_i].color ==
          side) {
        if (found != -1) {
          break;
        }
        found = new_i;

      } else if (position.board[(size_t)new_i].color ==
                 opponent) {
        if (position.board[(size_t)new_i].type == 'R' ||
            position.board[(size_t)new_i].type == 'Q') {
          if (found != -1) {
            output.pinned[(size_t)found] = done;
          } else {
            if (output.in_check) {
              output.king_must_move = true;
            } else {
              output.in_check = true;
              output.block = done;
            }
          }
        }
        break;
      }
      if (k == 0) {
        if (new_i <= 7) {
          break;
        }
      } else if (k == 1) {
        if ((new_i & 7) == 7) {
          break;
        }
      } else if (k == 2) {
        if (new_i >= 56) {
          break;
        }
      } else {
        if ((new_i & 7) == 0) {
          break;
        }
      }
      new_i += rook_moves[k];
    }
  }
}

bool MoveOk(Check check, size_t i, size_t new_i) {
  return ((check.in_check ? check.block.count(new_i)
          : true) && (check.pinned.contains(i)
              ? check.pinned[i].contains(new_i)
              : true));
}


bool InCheck(Position& position, const Color& side, int ki = -1) {
    int new_i;
    /*if (!king_present(position)) {
      int i=1;
    }*/
    Color opponent;
    /*if (position.kings[side][1] == -1) {
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
          if (position.board[i][j][0] == side &&
              position.board[i][j][1] == 'K') {
            position.kings[side][0] = i;
            position.kings[side][1] = j;
            i = 8;
            break;
          }
        }
      }
    }*/
    size_t i;
    if (ki == -1) {
    i = (size_t)position.kings[side];
    } else {
    i = (size_t)ki;
    }
    // pawn check
    if (side == Color::White) {
      opponent = Color::Black;
      // right check
      if (i > 7 && ((i & 7) != 7)) {
        new_i = (int)i - 7;
        if (position.board[(size_t)new_i].color == opponent &&
            position.board[(size_t)new_i].type == 'P') {
        return true;
        }
      }
      // left check
      if (i > 8 && (i & 7) != 0) {
        new_i = (int)i - 9;
        if (position.board[(size_t)new_i].color == opponent &&
            position.board[(size_t)new_i].type == 'P') {
        return true;
        }
      }
    } else {
      opponent = Color::White;
      // right check
      if (i < 55 && ((i & 7) != 7)) {
        new_i = (int)i + 9;
        if (position.board[(size_t)new_i].color == opponent &&
            position.board[(size_t)new_i].type == 'P') {
        return true;
        }
      }
      // left check
      if (i < 56 && (i & 7) != 0) {
        new_i = (int)i + 7;
        if (position.board[(size_t)new_i].color == opponent &&
            position.board[(size_t)new_i].type == 'P') {
        return true;
        }
      }
    }

    // knight check
    for (size_t k = 0; k < 8; k++) {
      new_i = (int)i + knight_moves[k];
      if (new_i >= 0 && new_i <= 63) {
        if (position.board[(size_t)new_i].type == 'N' &&
            position.board[(size_t)new_i].color == opponent) {
        return true;
        }
      }
    }
    // bishop check
    for (size_t k = 0; k < 4; k++) {
      new_i = (int)i + bishop_moves[k];
      if (((new_i & 7) - ((int)i & 7)) > 1 ||
          ((new_i & 7) - ((int)i & 7)) < -1 || new_i > 63 || new_i < 0) {
        continue;
      }
      while (true) {
        if (position.board[(size_t)new_i].color == side) {
        break;
        } else if (position.board[(size_t)new_i].color == opponent) {
          if (position.board[(size_t)new_i].type == 'B' ||
              position.board[(size_t)new_i].type == 'Q') {
          return true;
          }
        }
        if (new_i <= 8 || new_i >= 55 || (new_i & 7) == 7 || (new_i & 7) == 0) {
          break;
        }
        new_i += bishop_moves[k];
      }
    }
    // rook check

    for (size_t k = 0; k < 4; k++) {
      new_i = (int)i + rook_moves[k];
      if ((((new_i & 7) != ((int)i & 7)) && ((new_i >> 3) != ((int)i >> 3))) ||
          new_i > 63 || new_i < 0) {
        continue;
      }
      while (true) {
        if (position.board[(size_t)new_i].color == side) {
          break;
        } else if (position.board[(size_t)new_i].color == opponent) {
          if (position.board[(size_t)new_i].type == 'R' ||
              position.board[(size_t)new_i].type == 'Q') {
          return true;
          }
        }
        if (k == 0) {
          if (new_i <= 7) {
          break;
          }
        } else if (k == 1) {
          if ((new_i & 7) == 7) {
          break;
          }
        } else if (k == 2) {
          if (new_i >= 56) {
          break;
          }
        } else {
          if ((new_i & 7) == 0) {
          break;
          }
        }
        new_i += rook_moves[k];
      }
    }
    return false;
}



const int king_value = 1000;


//double EvaluateMobility(Position& position) {
//  Color opponent;
//  Check white_check;
//  Check black_check;
//  GetCheckInfo(white_check, position);
//  GetCheckInfo(black_check, position);
//  int new_i, new_j;
//  double mobility_score = 0;
//  Check* check;
//
//  for (size_t i = 0; i <= 7; i++) {
//      if (position.board[i].color == empty) {
//        continue;
//      }
//      double moves = 0;
//      if (position.board[i].type == 'K') {
//        for (size_t k = 0; k < 8; k++) {
//          new_i = (int)i + king_moves[k][0];
//          new_j = (int)j + king_moves[k][1];
//          if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 && !InCheck(position, position.board[i][j].color, new_i, new_j)) {
//            moves++;
//          }
//        }
//        mobility_score += (double)moves / (double)king_value;
//        continue;
//      }
//      if (position.board[i].color == white) {
//        check = &white_check;
//      } else {
//        check = &black_check;
//      }
//      // counting moves
//      //int addition = position.board[i][j].color == 'W' ? 1 : -1;
//      opponent = position.board[i].color == white ? black : white;
//      switch (position.board[i].type) {
//        case 'P': {
//          if (check->king_must_move) {
//            break;
//          }
//          int multiplier;
//          size_t promotion_row, starting_row, jump_row;
//          if (position.board[i].color == white) {
//            multiplier = -1;
//            starting_row = 6;
//            jump_row = 4;
//            promotion_row = 1;
//          } else {
//            multiplier = 1;
//            starting_row = 1;
//            jump_row = 3;
//            promotion_row = 6;
//          }
//          bool left_capture =
//              j > 0 && position.board[i + multiplier][j - 1].color == opponent;
//          bool right_capture =
//              j < 7 && position.board[i + multiplier][j + 1].color == opponent;
//          new_i = (int)i + multiplier;
//          if (i == promotion_row) {
//            // promotion
//            // one square
//
//            if (position.board[(size_t)new_i][j].color == empty) {
//              if (MoveOk(*check, i, j, (size_t)new_i, j)) {
//                moves += 2;
//              }
//            }
//            // captures to the left
//            if (left_capture && MoveOk(*check, i, j, (size_t)new_i, j - 1)) {
//              moves += 4;
//            }
//            // captures to the right
//            if (right_capture && MoveOk(*check, i, j, (size_t)new_i, j + 1)) {
//              moves += 4;
//            }
//          } else if (position.board[(size_t)new_i][j].color == empty) {
//            // one square forward
//            if (MoveOk(*check, i, j, (size_t)new_i, j)) {
//              moves += 0.5;
//            }
//            // two squares forward
//            if (i == starting_row &&
//                position.board[i + multiplier][j].color == empty && 
//                position.board[jump_row][j].color == empty &&
//                MoveOk(*check, i, j, jump_row, j)) {
//              moves += 0.5;
//            }
//          } else {
//            if (left_capture && MoveOk(*check, i, j, (size_t)new_i, j - 1)) {
//              moves++;
//            }
//            // captures to the right
//            if (left_capture && MoveOk(*check, i, j, (size_t)new_i, j + 1)) {
//              moves++;
//            }
//            // en passant to the left
//            size_t en_passant_start = jump_row + multiplier;
//            if (i == en_passant_start && j > 0 &&
//                position.board[i][j - 1].color == opponent &&
//                position.board[i][j - 1].type == 'P' &&
//                position.en_passant_target.Equals(new_i, (int)j - 1) &&
//                MoveOk(*check, i, j, (size_t)new_i, j - 1)) {
//              int di = (int)position.kings[position.board[i][j].color].row - (int)i;
//              int dj =
//                  (int)position.kings[position.board[i][j].color].column - (int)j - 1;
//              bool good = true;
//              if (position.kings[position.board[i][j].color].row == i) {
//                if (position.kings[position.board[i][j].color].column > j) {
//                  int iter_j =
//                      (int)position.kings[position.board[i][j].color].column;
//                  while (iter_j >= 0) {
//                    if (position.board[i][(size_t)iter_j].color ==
//                        position.board[i][j].color) {
//                      break;
//                    }
//                    if (position.board[i][(size_t)iter_j].color == opponent) {
//                      if (position.board[i][(size_t)iter_j].type == 'R' ||
//                          position.board[i][(size_t)iter_j].type == 'Q') {
//                        good = false;
//                        break;
//                      }
//                    }
//                    iter_j--;
//                  }
//                } else {
//                  int iter_j =
//                      (int)position.kings[position.board[i][j].color].column;
//                  while (iter_j <= 7) {
//                    if (position.board[i][(size_t)iter_j].color ==
//                        position.board[i][j].color) {
//                      break;
//                    }
//                    if (position.board[i][(size_t)iter_j].color == opponent) {
//                      if (position.board[i][(size_t)iter_j].type == 'R' ||
//                          position.board[i][(size_t)iter_j].type == 'Q') {
//                        good = false;
//                      }
//                      break;
//                    }
//                    iter_j++;
//                  }
//                }
//              }
//              if (good && std::abs(di) == std::abs(dj)) {
//                di = di > 0 ? 1 : -1;
//                dj = dj > 0 ? 1 : -1;
//                int iter_i = (int)i;
//                int iter_j = (int)j;
//
//                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
//                       iter_j <= 7) {
//                  if (position.board[(size_t)iter_i][(size_t)iter_j].color ==
//                      position.board[i][j].color) {
//                    break;
//
//                  } else if (position.board[(size_t)iter_i][(size_t)iter_j].color == opponent) {
//                    if (position.board[(size_t)iter_i][(size_t)iter_j].type == 'B' ||
//                        position.board[(size_t)iter_i][(size_t)iter_j].type == 'Q') {
//                      good = false;
//                    }
//                    break;
//                  }
//                  iter_i += di;
//                  iter_j += dj;
//                }
//              }
//              if (good) {
//                moves++;
//              }
//            }
//            // en passant to the right
//            if (i == en_passant_start && j < 7 &&
//                position.board[i][j + 1].color == opponent &&
//                position.board[i][j + 1].type == 'P' &&
//                position.en_passant_target.Equals(new_i, (int)j + 1) &&
//                MoveOk(*check, i, j, (size_t)new_i, j + 1)) {
//              int di = (int)position.kings[position.board[i][j].color].row - (int)i;
//              int dj = (int)position.kings[position.board[i][j].color].column -
//                       (int)j + 1;
//              bool good = true;
//              if (position.kings[position.board[i][j].color].row == i) {
//                if (position.kings[position.board[i][j].color].column > j) {
//                  int iter_j =
//                      (int)position.kings[position.board[i][j].color].column;
//                  while (iter_j >= 0) {
//                    if (position.board[i][(size_t)iter_j].color ==
//                        position.board[i][j].color) {
//                      break;
//                    }
//                    if (position.board[i][(size_t)iter_j].color == opponent) {
//                      if (position.board[i][(size_t)iter_j].type == 'R' ||
//                          position.board[i][(size_t)iter_j].type == 'Q') {
//                        good = false;
//                        break;
//                      }
//                    }
//                    iter_j--;
//                  }
//                } else {
//                  int iter_j =
//                      (int)position.kings[position.board[i][j].color].column;
//                  while (iter_j <= 7) {
//                    if (position.board[i][(size_t)iter_j].color ==
//                        position.board[i][j].color) {
//                      break;
//                    }
//                    if (position.board[i][(size_t)iter_j].color == opponent) {
//                      if (position.board[i][(size_t)iter_j].type == 'R' ||
//                          position.board[i][(size_t)iter_j].type == 'Q') {
//                        good = false;
//                      }
//                      break;
//                    }
//                    iter_j++;
//                  }
//                }
//              }
//              if (good && std::abs(di) == std::abs(dj)) {
//                di = di > 0 ? 1 : -1;
//                dj = dj > 0 ? 1 : -1;
//                int iter_i = (int)i;
//                int iter_j = (int)j;
//
//                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
//                       iter_j <= 7) {
//                  if (position.board[(size_t)iter_i][(size_t)iter_j].color ==
//                      position.board[i][j].color) {
//                    break;
//
//                  } else if (position.board[(size_t)iter_i][(size_t)iter_j].color == opponent) {
//                    if (position.board[(size_t)iter_i][(size_t)iter_j].type == 'B' ||
//                        position.board[(size_t)iter_i][(size_t)iter_j].type == 'Q') {
//                      good = false;
//                    }
//                    break;
//                  }
//                  iter_i += di;
//                  iter_j += dj;
//                }
//              }
//              if (good) {
//                moves++;
//              }
//            }
//          }
//          break;
//        }
//        case 'N':
//          if (check->king_must_move) {
//            break;
//          }
//          for (size_t k = 0; k < 8; k++) {
//            new_i = (int)i + knight_moves[k][0];
//            new_j = (int)j + knight_moves[k][1];
//            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
//                position.board[(size_t)new_i][(size_t)new_j].color !=
//                    position.board[i][j].color) {
//              if (MoveOk(*check, i, j, (size_t)new_i, (size_t)new_j)) {
//                moves++;
//              }
//            }
//          }
//          break;
//        case 'Q':
//        case 'B':
//          if (check->king_must_move) {
//            break;
//          }
//          for (size_t k = 0; k < 4; k++) {
//            new_i = (int)i + bishop_moves[k][0];
//            new_j = (int)j + bishop_moves[k][1];
//
//            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
//              if (position.board[(size_t)new_i][(size_t)new_j].color ==
//                  position.board[i][j].color) {
//                break;
//              }
//              if (MoveOk(*check, i, j, (size_t)new_i, (size_t)new_j)) {
//                moves++;
//              }
//              if (position.board[(size_t)new_i][(size_t)new_j].color == opponent) {
//                break;
//              }
//              new_i += bishop_moves[k][0];
//              new_j += bishop_moves[k][1];
//            }
//          }
//          if (position.board[i][j].type == 'B') {
//            break;
//          }
//        case 'R':
//          if (check->king_must_move) {
//            break;
//          }
//          for (size_t k = 0; k < 4; k++) {
//            new_i = (int)i + rook_moves[k][0];
//            new_j = (int)j + rook_moves[k][1];
//            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
//              if (position.board[(size_t)new_i][(size_t)new_j].color ==
//                  position.board[i][j].color) {
//                break;
//              }
//              if (MoveOk(*check, i, j, (size_t)new_i, (size_t)new_j)) {
//                moves++;
//              }
//              if (position.board[(size_t)new_i][(size_t)new_j].color == opponent) {
//                break;
//              }
//              new_i = new_i + rook_moves[k][0];
//              new_j = new_j + rook_moves[k][1];
//            }
//          }
//          break;
//
//      }
//
//      if (position.board[i][j].color == white) {
//        mobility_score +=
//            ((double)moves / piece_values[position.board[i][j].type]);
//      } else {
//        mobility_score -=
//            ((double)moves / piece_values[position.board[i][j].type]);
//      }
//    }
//  }
//
//  return mobility_score / 1000.0;
//}
bool CheckEnPassantRights(Position& position, size_t i, int new_i, int target, Color& opponent) {
    size_t king_i = position.kings[position.board[i].color];
    int iter_i;
    assert(i <= 63);
    for (size_t k = 0; k < 4; k++) {
      iter_i = (int)king_i + bishop_moves[k];
      if (((iter_i & 7) - ((int)king_i & 7)) > 1 ||
          ((iter_i & 7) - ((int)king_i & 7)) < -1 || iter_i > 63 ||
          iter_i < 0) {
        continue;
      }
      while (true) {
        if ((size_t)iter_i == i || (size_t)iter_i == (i + 1)) {
          iter_i += bishop_moves[k];
          continue;
        }
        if (position.board[(size_t)iter_i].color ==
                position.board[king_i].color ||
            iter_i == target) {
          break;
        } else if (position.board[(size_t)iter_i].color == opponent) {
          if (position.board[(size_t)iter_i].type == 'B' ||
              position.board[(size_t)iter_i].type == 'Q') {
          return false;
          }
        }
        if (iter_i <= 8 || iter_i >= 55 || (iter_i & 7) == 7 ||
            (iter_i & 7) == 0) {
          break;
        }
        iter_i += bishop_moves[k];
      }
    }
    // rook check

    for (size_t k = 0; k < 4; k++) {
      iter_i = (int)king_i + rook_moves[k];
      if ((((iter_i & 7) != ((int)king_i & 7)) && ((iter_i >> 3) != ((int)king_i >> 3))) ||
          iter_i > 63 || iter_i < 0) {
        continue;
      }
      while (true) {
        assert(iter_i >= 0 && iter_i <= 63);
        if (position.board[(size_t)iter_i].color == position.board[i].color) {
          break;
        } else if (position.board[(size_t)new_i].color == opponent) {
          if (position.board[(size_t)iter_i].type == 'R' ||
              position.board[(size_t)iter_i].type == 'Q') {
          return false;
          }
        }
        if (k == 0) {
          if (iter_i <= 7) {
          break;
          }
        } else if (k == 1) {
          if ((iter_i & 7) == 7) {
          break;
          }
        } else if (k == 2) {
          if (iter_i >= 56) {
          break;
          }
        } else {
          if ((iter_i & 7) == 0) {
          break;
          }
        }
        iter_i += rook_moves[k];
      }
    }
    return true;
}


size_t CountPieceMoves(Position& position, size_t& i, const Check& check_info) {
    size_t moves = 0;
    int new_i;
    Color opponent = position.board[i].color == Color::White ? Color::Black : Color::White;
    switch (position.board[i].type) {
      case 'P': {
        if (check_info.king_must_move) {
          break;
        }
        int multiplier;
        size_t promotion_row_a, promotion_row_b, starting_row_a, starting_row_b,
            en_passant_start_a, en_passant_start_b;
        if (position.board[i].color == Color::White) {
          multiplier = -8;
          starting_row_a = 48;
          starting_row_b = 55;
          promotion_row_a = 8;
          promotion_row_b = 15;
          en_passant_start_a = 24;
          en_passant_start_b = 31;
        } else {
          multiplier = 8;
          promotion_row_a = 48;
          promotion_row_b = 55;
          starting_row_a = 8;
          starting_row_b = 15;
          en_passant_start_a = 32;
          en_passant_start_b = 39;
        }
        bool left_capture =
            ((i & 7) != 0) &&
            position.board[i + multiplier - 1].color == opponent;
        bool right_capture =
            ((i & 7) != 7) &&
            position.board[i + multiplier + 1].color == opponent;
        new_i = (int)i + multiplier;
        if (i >= promotion_row_a && i < promotion_row_b) {
          // promotion
          for (size_t k = 0; k < 4; k++) {
          // one square
          if (position.board[(size_t)new_i].color == Color::Empty &&
              MoveOk(check_info, i, (size_t)new_i)) {
            moves++;
          }
          // captures to the left
          if (left_capture && MoveOk(check_info, i, (size_t)new_i - 1)) {
            moves++;
          }
          // captures to the right
          if (right_capture && MoveOk(check_info, i, (size_t)new_i + 1)) {
            moves++;
          }
          }
        } else {
          if (position.board[(size_t)new_i].color == Color::Empty) {
          // one square forward
          if (MoveOk(check_info, i, (size_t)new_i)) {
            moves++;
          }
          // two squares forward
          if (i >= starting_row_a && i <= starting_row_b &&
              position.board[(size_t)new_i + multiplier].color == Color::Empty &&
              MoveOk(check_info, i, (size_t)new_i + multiplier)) {
            moves++;
          }
          }
          // captures to the left
          if (left_capture && MoveOk(check_info, i, (size_t)new_i - 1)) {
          moves++;
          }
          // captures to the right
          if (right_capture && MoveOk(check_info, i, (size_t)new_i + 1)) {
          moves++;
          }
          // en passant to the left
          if (i >= en_passant_start_a + 1 && i <= en_passant_start_b &&
              position.board[i - 1].color == opponent &&
              position.board[i - 1].type == 'P' &&
              position.en_passant_target == new_i - 1 &&
              MoveOk(check_info, i, (size_t)new_i - 1) &&
              CheckEnPassantRights(position, i, new_i, new_i - 1, opponent)) {
            moves++;
          }
          // en passant to the right
          if (i >= en_passant_start_a && i <= (en_passant_start_b - 1) &&
              position.board[i + 1].color == opponent &&
              position.board[i + 1].type == 'P' &&
              position.en_passant_target == new_i + 1 &&
              MoveOk(check_info, i, (size_t)new_i + 1) &&
              CheckEnPassantRights(position, i, new_i, new_i + 1, opponent)) {
            moves++;
          }
        }
        break;
      }
      case 'N':
        if (check_info.king_must_move) {
          break;
        }
        for (size_t k = 0; k < 8; k++) {
          new_i = (int)i + knight_moves[k];
          if (new_i >= 0 && new_i <= 63 && ((new_i & 7) - ((int)i & 7)) <= 2 &&
              (new_i & 7) - ((int)i & 7) >= -2 &&
              position.board[(size_t)new_i].color != position.board[i].color &&
              MoveOk(check_info, i, (size_t)new_i)) {
          moves++;
          }
        }
        break;
      case 'Q':
      case 'B':
        if (check_info.king_must_move) {
          break;
        }
        for (size_t k = 0; k < 4; k++) {
          new_i = (int)i + bishop_moves[k];
          if (((new_i & 7) - ((int)i & 7)) > 1 ||
              ((new_i & 7) - ((int)i & 7)) < -1 || new_i > 63 || new_i < 0) {
          continue;
          }
          while (true) {
          if (position.board[(size_t)new_i].color == position.board[i].color) {
            break;
          }
          if (MoveOk(check_info, i, (size_t)new_i)) {
            moves++;
          }
          if (position.board[(size_t)new_i].color == opponent) {
            break;
          }
          if (new_i <= 8 || new_i >= 55 || (new_i & 7) == 7 ||
              (new_i & 7) == 0) {
            break;
          }
          new_i += bishop_moves[k];
          }
        }
        if (position.board[i].type == 'B') {
          break;
        }
      case 'R':
        if (check_info.king_must_move) {
          break;
        }
        for (size_t k = 0; k < 4; k++) {
          new_i = (int)i + rook_moves[k];
          if ((((new_i & 7) != ((int)i & 7)) &&
               ((new_i >> 3) != ((int)i >> 3))) ||
              new_i > 63 || new_i < 0) {
          continue;
          }
          while (true) {
          if (position.board[(size_t)new_i].color == position.board[i].color) {
            break;
          }
          if (MoveOk(check_info, i, (size_t)new_i)) {
            moves++;
          }
          if (position.board[(size_t)new_i].color == opponent) {
            break;
          }
          if (k == 0) {
            if (new_i <= 8) {
              break;
            }
          } else if (k == 1) {
            if ((new_i & 7) == 7) {
              break;
            }
          } else if (k == 2) {
            if (new_i >= 56) {
              break;
            }
          } else {
            if ((new_i & 7) == 0) {
              break;
            }
          }
          new_i += rook_moves[k];
          }
        }
        break;
      case 'K':
        for (size_t k = 0; k < 8; k++) {
          new_i = (int)i + king_moves[k];
          if (new_i >= 0 && new_i <= 63 && ((new_i & 7) - ((int)i & 7)) <= 2 &&
              (new_i & 7) - ((int)i & 7) >= -2 &&
              position.board[(size_t)new_i].color != position.board[i].color &&
              !InCheck(position, position.board[i].color, new_i)) {
          moves++;
          //} else {
          //  delete new_position;
          //}
          }
        }
        if ((position.board[i].color == Color::White ? position.castling.white_o_o
                                    : position.castling.black_o_o) &&
            position.board[i + 1].color == Color::Empty &&
            position.board[i + 2].color == Color::Empty && !check_info.in_check &&
            !InCheck(position, position.board[i].color, (int)i + 1) &&
            !InCheck(position, position.board[i].color, (int)i + 2)) {
          moves++;
        }
        if ((position.board[i].color == Color::White ? position.castling.white_o_o_o
                                    : position.castling.black_o_o_o) &&
            position.board[i - 1].color == Color::Empty &&
            position.board[i - 2].color == Color::Empty &&
            position.board[i - 3].color == Color::Empty && !check_info.in_check &&
            !InCheck(position, position.board[i].color, (int)i - 1) &&
            !InCheck(position, position.board[i].color, (int)i - 2)) {
          moves++;
        }

        break;
    }
    return moves;
}


size_t CountMoves(Position& position) {
  Check white_check;
  Check black_check;
  GetCheckInfo(white_check, position);
  GetCheckInfo(black_check, position);
  int new_i;
  size_t moves = 0;
  Check& check_info = white_check;
  for (size_t i = 0; i <= 63; i++) {
      if (position.board[i].color == Color::Empty) {
        continue;
      }
      if (position.board[i].type == 'K') {
        for (size_t k = 0; k < 8; k++) {
          new_i = (int)i + king_moves[k];
          if (new_i >= 0 && new_i <= 63 &&
              !InCheck(position, position.board[i].color, new_i)) {
            moves++;
          }
        }
        continue;
      }
      if (position.board[i].color == Color::White) {
        check_info = white_check;
      } else {
        check_info = black_check;
      }
      // counting moves
      // int addition = position.board[i][j].color == 'W' ? 1 : -1;
      moves += CountPieceMoves(position, i, check_info);
    }
  

  return moves;
}


          //
//bool was_capture(const Board& board1, const Board& board2) {
//  for (size_t i = 0; i < 8; i++) {
//    for (size_t j = 0; j < 8; j++) {
//      if (board1[i][j] != board2[i][j] && board1[i][j].color != ' ' &&
//          board2[i][j].color != ' ') {
//        return true;
//      }
//    }
//  }
//  return false;
//}


const char promotion_pieces[4] = {'Q', 'R', 'B', 'N'};


const std::set<char> pieces = {'K', 'Q', 'R', 'B', 'N'};

const int mate = 20000;




bool board_exists(const Position& position, const Position& new_position) {
  if (position.outcomes == nullptr) {
    return false;
  }
  for (size_t i = 0; i < position.outcomes->size(); i++) {
    if ((*(*position.outcomes)[i]).board == new_position.board) {
      return true;
    }
  }
  return false;
}








void new_generate_moves(Position& position) {
  Position base_position = position.GenerateMovesCopy();
  // base_position.previous_move = &position;
  Color opponent = position.white_to_move ? Color::Black : Color::White;
  Position* new_position;
  size_t moves = 0;
  if (position.outcomes == nullptr) {
    position.outcomes = new std::vector<Position*>;
  }
  Check check_info;
  GetCheckInfo(check_info, position);

  int new_i;
  for (size_t i = 0; i < 63; i++) {
    if (position.board[i].color == Color::Empty) {
      continue;
    }
    moves = 0;
    if (position.board[i].color == (position.white_to_move ? Color::Black : Color::White)) {
      

      moves = CountPieceMoves(position, i, check_info);




      if (!position.white_to_move) {
          if (position.board[i].type == 'K') {
            position.evaluation +=
                ((double)moves / (double)king_value) / 1000.0;
          } else {
            position.evaluation +=
                ((double)moves / (double)piece_values[position.board[i].type]) /
                1000.0;
          }
      } else {
          if (position.board[i].type == 'K') {
            position.evaluation +=
                ((double)moves / (double)king_value) / 1000.0;
          } else {
            position.evaluation -=
                ((double)moves / (double)piece_values[position.board[i].type]) /
                1000.0;
          }
      }
      continue;
    }
    int evaluation_multiplier = position.board[i].color == Color::White ? 1 : -1;
    switch (position.board[i].type) {
      case 'P': {
          if (check_info.king_must_move) {
          break;
          }
          int multiplier;
          size_t starting_row_a,
              starting_row_b, en_passant_start_a, en_passant_start_b;
          if (position.board[i].color == Color::White) {
          multiplier = -8;
          starting_row_a = 48;
          starting_row_b = 55;
          en_passant_start_a = 24;
          en_passant_start_b = 31;
          } else {
          multiplier = 8;
          starting_row_a = 8;
          starting_row_b = 15;
          en_passant_start_a = 32;
          en_passant_start_b = 39;
          }
          bool left_capture =
              ((i & 7) != 0) &&
              position.board[i + multiplier - 1].color == opponent;
          assert((i + multiplier + 1) <= 63);
          bool right_capture =
              ((i & 7) != 7) &&
              position.board[i + multiplier + 1].color == opponent;
          new_i = (int)i + multiplier;
          if (new_i < 8 || new_i > 55) {
          // promotion
          for (size_t k = 0; k < 4; k++) {
            // one square
            if (position.board[(size_t)new_i].color == Color::Empty &&
                MoveOk(check_info, i, (size_t)new_i)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i].color =
                  new_position->board[i].color;
              new_position->board[(size_t)new_i].type = promotion_pieces[k];
              new_position->board[i].Empty();
              new_position->evaluation += piece_values[promotion_pieces[k]] - 1;
              new_position->fifty_move_rule = 0;
              moves++;
              position.outcomes->emplace_back(new_position);
            }
            // captures to the left
            if (left_capture && MoveOk(check_info, i, (size_t)new_i - 1)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i - 1].color =
                  new_position->board[i].color;
              new_position->board[(size_t)new_i - 1].type = promotion_pieces[k];
              new_position->board[i].Empty();
              new_position->fifty_move_rule = 0;
              if (position.white_to_move) {
                new_position->evaluation +=
                    (piece_values[position.board[(size_t)new_i - 1].type]) +
                    piece_values[promotion_pieces[k]] - 1.0;
              } else {
                new_position->evaluation -=
                    (piece_values[position.board[(size_t)new_i - 1].type]) +
                    piece_values[promotion_pieces[k]] - 1;
              }
              moves++;
              position.outcomes->emplace_back(new_position);
            }
            // captures to the right
            if (right_capture && MoveOk(check_info, i, (size_t)new_i + 1)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i + 1].color =
                  new_position->board[i].color;
              new_position->board[(size_t)new_i + 1].type = promotion_pieces[k];
              new_position->board[i].Empty();
              new_position->fifty_move_rule = 0;
              if (position.white_to_move) {
                new_position->evaluation +=
                    (piece_values[position.board[(size_t)new_i + 1].type]) +
                    piece_values[promotion_pieces[k]] - 1;
              } else {
                new_position->evaluation -=
                    (piece_values[position.board[(size_t)new_i + 1].type]) +
                    piece_values[promotion_pieces[k]] - 1;
              }
              moves++;
              position.outcomes->emplace_back(new_position);
            }
          }
          } else {
          if (position.board[(size_t)new_i].color == Color::Empty) {
            // one square forward
            if (MoveOk(check_info, i, (size_t)new_i)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i] = new_position->board[i];
              new_position->board[i].Empty();
              new_position->fifty_move_rule = 0;
              moves++;
              position.outcomes->emplace_back(new_position);
            }
            // two squares forward
            if (i >= starting_row_a && i <= starting_row_b &&
                position.board[(size_t)new_i + multiplier].color == Color::Empty &&
                MoveOk(check_info, i, (size_t)new_i + multiplier)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i + multiplier] =
                  new_position->board[i];
              new_position->fifty_move_rule = 0;
              new_position->board[i].Empty();
              new_position->en_passant_target = new_i + multiplier;
              moves++;
              position.outcomes->emplace_back(new_position);
            }
          }
          // captures to the left
          if (left_capture && MoveOk(check_info, i, (size_t)new_i - 1)) {
            new_position = base_position.CreateDeepCopy();
            new_position->board[(size_t)new_i - 1] = new_position->board[i];
            new_position->board[i].Empty();
            if (position.white_to_move) {
              new_position->evaluation +=
                  (piece_values[position.board[(size_t)new_i - 1].type]);
            } else {
              new_position->evaluation -=
                  (piece_values[position.board[(size_t)new_i - 1].type]);
            }
            moves++;
            position.outcomes->emplace_back(new_position);
          }
          // captures to the right
          if (right_capture && MoveOk(check_info, i, (size_t)new_i + 1)) {
            new_position = base_position.CreateDeepCopy();
            new_position->board[(size_t)new_i + 1] = new_position->board[i];
            new_position->board[i].Empty();
            if (position.white_to_move) {
              new_position->evaluation +=
                  (piece_values[position.board[(size_t)new_i + 1].type]);
            } else {
              new_position->evaluation -=
                  (piece_values[position.board[(size_t)new_i + 1].type]);
            }
            moves++;
            position.outcomes->emplace_back(new_position);
          }
          // en passant to the left
          if (i >= en_passant_start_a + 1 && i <= en_passant_start_b &&
              position.board[i - 1].color == opponent &&
              position.board[i - 1].type == 'P' &&
              position.en_passant_target == new_i - 1 &&
              MoveOk(check_info, i, (size_t)new_i - 1) &&
              CheckEnPassantRights(position, i, new_i, new_i - 1, opponent)) {
           
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i - 1] = new_position->board[i];
              new_position->board[i].Empty();
              new_position->board[i - 1].Empty();
              new_position->evaluation += evaluation_multiplier;
              new_position->fifty_move_rule = 0;
              moves++;
              position.outcomes->emplace_back(new_position);
           
          }
          // en passant to the right
          if (i >= en_passant_start_a && i <= en_passant_start_b - 1 &&
              position.board[i + 1].color == opponent &&
              position.board[i + 1].type == 'P' &&
              position.en_passant_target == new_i + 1 &&
              MoveOk(check_info, i, (size_t)new_i + 1) && CheckEnPassantRights(position, i, new_i, new_i + 1, opponent)) {

              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i + 1] = new_position->board[i];
              new_position->board[i].Empty();
              new_position->board[i + 1].Empty();
              new_position->evaluation += position.white_to_move ? 1 : -1;
              new_position->fifty_move_rule = 0;
              moves++;
              position.outcomes->emplace_back(new_position);
          }
          }
          break;
      }
      case 'N':
          if (check_info.king_must_move) {
          break;
          }
          for (size_t k = 0; k < 8; k++) {
          new_i = (int)i + knight_moves[k];
          if (new_i >= 0 && new_i <= 63 &&
              ((new_i & 7) - ((int)i & 7)) <= 2 && (new_i & 7) - ((int)i & 7) >= -2 &&
              position.board[(size_t)new_i].color != position.board[i].color &&
              MoveOk(check_info, i, (size_t)new_i)) {
            new_position = base_position.CreateDeepCopy();
            new_position->board[(size_t)new_i] = position.board[i];
            new_position->board[i].Empty();
            if (!position.board[(size_t)new_i].IsEmpty()) {
              new_position->evaluation +=
                  (evaluation_multiplier *
                   piece_values[position.board[(size_t)new_i].type]);
              new_position->fifty_move_rule = 0;
            }
            moves++;
            position.outcomes->emplace_back(new_position);
          }
          }
          break;
      case 'Q':
      case 'B':
          if (check_info.king_must_move) {
          break;
          }
          for (size_t k = 0; k < 4; k++) {
          new_i = (int)i + bishop_moves[k];
          if (((new_i & 7) - ((int)i & 7)) > 1 ||
                  ((new_i & 7) - ((int)i & 7)) <-1 || new_i > 63 || new_i < 0) {
            continue;
          }
          while (true) {
            assert(new_i >= 0 && new_i <= 63);
            if (position.board[(size_t)new_i].color ==
                position.board[i].color) {
              break;
            }
            if (MoveOk(check_info, i, (size_t)new_i)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i] = position.board[i];
              new_position->board[i].Empty();
              if (!position.board[(size_t)new_i].IsEmpty()) {
                new_position->fifty_move_rule = 0;
                new_position->evaluation +=
                    (evaluation_multiplier *
                     piece_values[position.board[(size_t)new_i].type]);
              }
              moves++;
              position.outcomes->emplace_back(new_position);
            }
            if (position.board[(size_t)new_i].color == opponent) {
              break;
            }
            if (new_i <= 8 || new_i >= 55 || (new_i & 7) == 7 ||
                (new_i & 7) == 0) {
              break;
            }
            new_i += bishop_moves[k];
          }
          }
          if (position.board[i].type == 'B') {
          break;
          }
      case 'R':
          if (check_info.king_must_move) {
          break;
          }
          for (size_t k = 0; k < 4; k++) {
          new_i = (int)i + rook_moves[k];
          if ((((new_i & 7) != ((int)i & 7)) &&
               ((new_i >> 3) != ((int)i >> 3))) ||
              new_i > 63 || new_i < 0) {
            continue;
          }
          while (true) {
            assert(new_i >= 0 && new_i <= 63);
            if (position.board[(size_t)new_i].color ==
                position.board[i].color) {
              break;
            }
            if (MoveOk(check_info, i, (size_t)new_i)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i] = position.board[i];
              new_position->board[i].Empty();
              if (position.board[63].Equals(Color::White, 'R') &&
                  new_position->board[63].IsEmpty()) {
                new_position->castling.white_o_o = false;
              } else if (position.board[56].Equals(Color::White, 'R') &&
                         new_position->board[56].IsEmpty()) {
                new_position->castling.white_o_o_o = false;
              } else if (position.board[7].Equals(Color::White, 'R') &&
                         new_position->board[7].IsEmpty()) {
                new_position->castling.black_o_o = false;
              } else if (position.board[0].Equals(Color::White, 'R') &&
                         new_position->board[0].IsEmpty()) {
                new_position->castling.black_o_o_o = false;
              }
              if (!position.board[(size_t)new_i].IsEmpty()) {
                new_position->fifty_move_rule = 0;
                new_position->evaluation +=
                    (evaluation_multiplier *
                     piece_values[position.board[(size_t)new_i].type]);
              }
              moves++;
              position.outcomes->emplace_back(new_position);
            }
            if (position.board[(size_t)new_i].color == opponent) {
              break;
            }
            if (k == 0) {
              if (new_i <= 7) {
                break;
              }
            } else if (k == 1) {
              if ((new_i & 7) == 7) {
                break;
              }
            } else if (k == 2) {
              if (new_i >= 56) {
                break;
              }
            } else {
              if ((new_i & 7) == 0) {
                break;
              }
            }
            new_i += rook_moves[k];
          }
          }
          break;
      case 'K':
          for (size_t k = 0; k < 8; k++) {
          new_i = (int)i + king_moves[k];
          if (new_i >= 0 && new_i <= 63 && (new_i & 7) - ((int)i &7)< 2 &&
              (new_i & 7) - ((int)i & 7) > -2 &&
              position.board[(size_t)new_i].color != position.board[i].color &&
              !InCheck(position, position.board[i].color, new_i)) {
            new_position = base_position.CreateDeepCopy();
            new_position->board[(size_t)new_i] = position.board[i];
            new_position->board[i].Empty();
            new_position->kings[position.board[i].color] = (size_t)new_i;
            // if (!InCheck(*new_position, position.to_move)) {
            if (position.white_to_move) {
              new_position->castling.white_o_o =
                  new_position->castling.white_o_o_o = false;
            } else {
              new_position->castling.black_o_o =
                  new_position->castling.black_o_o_o = false;
            }
            if (!position.board[(size_t)new_i].IsEmpty()) {
              new_position->fifty_move_rule = 0;
              new_position->evaluation +=
                  (evaluation_multiplier *
                   piece_values[position.board[(size_t)new_i].type]);
            }
            moves++;
            position.outcomes->emplace_back(new_position);
            //} else {
            //  delete new_position;
            //}
          }
          }
          if ((position.white_to_move ? position.castling.white_o_o
                                      : position.castling.black_o_o) &&
              position.board[i + 1].color == Color::Empty &&
              position.board[i + 2].color == Color::Empty && !check_info.in_check &&
              !InCheck(position, position.board[i].color, (int)i + 1) &&
              !InCheck(position, position.board[i].color, (int)i + 2)) {
          new_position = base_position.CreateDeepCopy();
          new_position->board[i + 2].set(position.board[i].color, 'K');
          new_position->board[i + 1].set(position.board[i].color, 'R');
          new_position->kings[position.board[i].color] = i + 2;
          new_position->board[i].Empty();
          new_position->board[i + 3].Empty();
          if (position.white_to_move) {
            new_position->castling.white_o_o =
                new_position->castling.white_o_o_o = false;
          } else {
            new_position->castling.black_o_o =
                new_position->castling.black_o_o_o = false;
          }
          moves++;
          position.outcomes->emplace_back(new_position);
          }
          if ((position.white_to_move ? position.castling.white_o_o_o
                                      : position.castling.black_o_o_o) &&
              position.board[i - 1].color == Color::Empty &&
              position.board[i - 2].color == Color::Empty &&
              position.board[i - 3].color == Color::Empty && !check_info.in_check &&
              !InCheck(position, position.board[i].color, (int)i - 1) &&
              !InCheck(position, position.board[i].color, (int)i - 2)) {
          new_position = base_position.CreateDeepCopy();
          new_position->board[i - 2].set(position.board[i].color, 'K');
          new_position->board[i - 1].set(position.board[i].color, 'R');
          new_position->board[i - 4].Empty();
          new_position->board[i].Empty();
          new_position->kings[position.white_to_move ? Color::White : Color::Black] = i - 2;
          if (position.white_to_move) {
            new_position->castling.white_o_o =
                new_position->castling.white_o_o_o = false;
          } else {
            new_position->castling.black_o_o =
                new_position->castling.black_o_o_o = false;
          }
          moves++;
          position.outcomes->emplace_back(new_position);
          }

          break;
    }
    if (position.white_to_move) {
      if (position.board[i].type == 'K') {
          position.evaluation += ((double)moves / (double)king_value) / 1000.0;
      } else {
          position.evaluation +=
              ((double)moves / (double)piece_values[position.board[i].type]) / 1000.0;
      }
    } else {
      if (position.board[i].type == 'K') {
          position.evaluation += ((double)moves / (double)king_value) / 1000.0;
      } else {
          position.evaluation -=
              ((double)moves / (double)piece_values[position.board[i].type]) / 1000.0;
      }
    }
  }

}

Position* FindPosition(const Board& board, const Position& position) {
  if (position.outcomes) {
    for (size_t i = 0; i < position.outcomes->size(); i++) {
      if ((*position.outcomes)[i]->board == board) {
        return (*position.outcomes)[i];
      }
    }
  }
  return nullptr;
}

struct Move {
  bool check;
  size_t dest;
  std::vector<int> start;
  char piece;
  bool capture;
  bool checkmate;
  char promotion;
  Move()
      : check(false),
        start({-1, -1}),
        piece(' '),
        capture(false),
        checkmate(false),
        promotion(' ') {}
};

Board GetBoard(Move info, Position& position) {
  Board board = position.board;
  Check check_info;
  GetCheckInfo(check_info, position);
  Color to_move = position.white_to_move ? Color::White : Color::Black;
  size_t count = 0;
  size_t found = 64;
  switch (info.piece) {
    case 'P': {
      int multiplier = to_move == Color::White ? -8 : 8;
      if (info.capture) {
        if (board[info.dest].color == Color::Empty) {
          // en passant
          board[info.dest] = board[(((size_t)info.dest - multiplier) & (-8)) +
                                   (size_t)info.start[1]];
          board[(((size_t)info.dest - multiplier) & (-8)) +
                (size_t)info.start[1]]
              .Empty();
          board[info.dest - multiplier].Empty();
        } else {
          board[(size_t)info.dest] = board[(size_t)info.dest - multiplier];
          board[(((size_t)info.dest - multiplier) & (-8)) + (size_t)info.start[1]].Empty();
        }
      } else if (board[(size_t)info.dest - multiplier].type ==
                 'P') {
        board[info.dest] =
            board[info.dest - multiplier];
        board[info.dest - multiplier].Empty();
      } else {
        board[info.dest] = board[info.dest - (size_t)(multiplier * 2)];
        board[info.dest - (size_t)(multiplier * 2)].Empty();
      }
      if (info.promotion != ' ') {
        board[info.dest].type = info.promotion;
      }
      return board;
    }
    case 'N':
      for (size_t k = 0; k < 8; k++) {
        int i = (int)info.dest + knight_moves[k];
        if (i >= 0 && i <= 63 && ((i & 7) - ((int)info.dest & 7)) <= 2 &&
            (i & 7) - ((int)info.dest & 7) >= -2 && board[(size_t)i].type == 'N' &&
            board[(size_t)i].color == to_move &&
            (info.start[0] == -1 || ((i >> 3) == info.start[0])) && 
            (info.start[1] == -1 || ((i & 7) == info.start[1])) &&
            MoveOk(check_info, (size_t)i, info.dest)) {
          found = (size_t)i;
          count++;
        }
      }
        if (count == 1) {
        board[info.dest] = board[found];
        board[found].Empty();
        }
          return board;

    case 'Q':
    case 'B': {

          for (size_t k = 0; k < 4; k++) {
        int i = (int)info.dest + bishop_moves[k];
        if (((i & 7) - ((int)info.dest & 7)) > 1 ||
            ((i & 7) - ((int)info.dest & 7)) < -1 || i > 63 || i < 0) {
          continue;
        }
        while (true) {
          if ((board[(size_t)i].type == 'B' || board[(size_t)i].type == 'Q') &&
              board[(size_t)i].color == to_move &&
              (info.start[0] == -1 || ((i >> 3) == info.start[0])) &&
              (info.start[1] == -1 || ((i & 7) == info.start[1]))&&
              MoveOk(check_info, (size_t)i, info.dest)) {
            found = (size_t)i;
            count++;
          }
          if (i <= 8 || i >= 55 || (i & 7) == 7 ||
              (i & 7) == 0) {
            break;
          }
          i += bishop_moves[k];
        }
          }
          if (info.piece == 'B') {
        if (count == 1) {
          board[info.dest] = board[found];
          board[found].Empty();
        }
        return board;
          }

          [[fallthrough]];
    }
    case 'R': {
          for (size_t k = 0; k < 4; k++) {
        int i = (int)info.dest + rook_moves[k];
        if ((((i & 7) != ((int)info.dest & 7)) &&
            ((i >> 3) != ((int)info.dest >> 3))) || i > 63 || i < 0) {
          continue;
        }
        while (true) {
          if ((board[(size_t)i].type == 'R' || board[(size_t)i].type == 'Q') &&
              board[(size_t)i].color == to_move &&
              (info.start[0] == -1 || ((i >> 3) == info.start[0])) &&
              (info.start[1] == -1 || ((i & 7) == info.start[1]))&&
              MoveOk(check_info, (size_t)i, info.dest)) {
            found = (size_t)i;
            count++;
          }
          if (k == 0) {
            if (i <= 7) {
              break;
            }
          } else if (k == 1) {
            if ((i & 7) == 7) {
              break;
            }
          } else if (k == 2) {
            if (i >= 56) {
              break;
            }
          } else {
            if ((i & 7) == 0) {
              break;
            }
          }
          i += rook_moves[k];
        }
          }
        if (count == 1) {
          board[info.dest] = board[found];
          board[found].Empty();
        }
        return board;
    }
    case 'K': {
      int new_i;
      for (size_t k = 0; k < 8; k++) {
          new_i = (int)info.dest + king_moves[k];
        if (new_i >= 0 && new_i <=  63) {
          if (position.board[(size_t)new_i].type == 'K') {
            board[info.dest] =
                board[(size_t)new_i];
            board[(size_t)new_i].Empty();
            return board;
          }
        }
      }
      return board;
    }

  }
}

Position* MoveToPosition(Position& position, const std::string& move) {
  Board new_board = position.board;
  if (move == "0-0" || move == "O-O") {
    if (position.white_to_move) {
      new_board[62].set(Color::White, 'K');
      new_board[61].set(Color::White, 'R');
      new_board[60].Empty();
      new_board[63].Empty();
    } else {
      new_board[6].set(Color::Black, 'K');
      new_board[5].set(Color::Black, 'R');
      new_board[4].Empty();
      new_board[7].Empty();
    }
    return FindPosition(new_board, position);
  }
  if (move == "O-O-O" || move == "0-0-0") {
    if (position.white_to_move) {
      new_board[58].set(Color::White, 'K');
      new_board[59].set(Color::White, 'R');
      new_board[60].Empty();
      new_board[56].Empty();
    } else {
      new_board[2].set(Color::Black, 'K');
      new_board[3].set(Color::Black, 'R');
      new_board[4].Empty();
      new_board[0].Empty();
    }
    return FindPosition(new_board, position);
  }
  Move info;
  int current = (int)move.length() - 1;
  if (pieces.count(move[0])) {
    info.piece = move[0];
  } else {
    info.piece = 'P';
  }
  if (pieces.count(move[(size_t)current])) {
    info.promotion = move[(size_t)current];
    current -= 2;
  }
  if (move[(size_t)current] == '+') {
    info.check = true;
    current--;
  }
  if (move[(size_t)current] == '#') {
    info.checkmate = true;
    current--;
  }
  info.dest = size_t(8 * (8 - char_to_int(move[(size_t)current])));
  current--;
  info.dest += size_t(move[(size_t)current] - 'a');
  current--;
  if (current == -1) {
    return FindPosition(GetBoard(info, position), position);
  }
  if (move[(size_t)current] == 'x') {
    info.capture = true;
    current--;
    if (current == -1) {
      return FindPosition(GetBoard(info, position), position);
    }
  }
  if (isdigit(move[(size_t)current])) {
    info.start[0] = 8 - char_to_int(move[(size_t)current]);
    current--;
    if (current == -1) {
      return FindPosition(GetBoard(info, position), position);
    }
  }
  if (current != 0 || info.piece == 'P') {
    info.start[1] = move[(size_t)current] - 'a';
  }
  return FindPosition(GetBoard(info, position), position);
}



std::string MakeString(const Position& position, const bool& white_on_bottom) {
  std::stringstream result;
  if (white_on_bottom) {
    for (size_t i = 0; i < 64; i++) {
      if (i % 8 == 0) {
        result << "  +----+----+----+----+----+----+----+----+\n"
               << (8 - (i >> 3)) << " |";
      }
      char color;

        switch (position.board[i].color) {
          case Color::White:
            color = 'W';
            break;
          case Color::Black:
            color = 'B';
            break;
          case Color::Empty:
            color = ' ';
            break;
          default:
            std::cout << std::endl << "Unknown color error in MakeString.";
            exit(1);
        }
        result << ' ' << color << position.board[i].type
               << " |";
        if ((i & 7) == 7) {
          result << '\n';
        }
    }
  }
  else {
    for (int i = 63; i >= 0; i--) {
        if ((i + 1) % 8 == 0) {
          result << "  +----+----+----+----+----+----+----+----+\n"
                 << (8 - (i >> 3)) << " |";
        }
        char color;
        
        switch (position.board[(size_t)i].color) {
          case Color::White:
            color = 'W';
            break;
          case Color::Black:
            color = 'B';
            break;
          case Color::Empty:
            color = ' ';
            break;
          default:
            std::cout << std::endl << "Unknown color error in MakeString.";
            exit(1);
        }
        result << ' ' << color << position.board[(size_t)i].type
               << " |";
        if (((i + 1) & 7) == 0) {
          result << '\n';
        }
    }
  }
  result << "  +----+----+----+----+----+----+----+----+";
    if (white_on_bottom) {
    result << "\n    a    b    c    d    e    f    g    h    ";
    }
    else {
    result << "\n    h    g    f    e    d    c    b    a    ";
    }
  return result.str();
}


//bool LessOutcome(const Position* position1, const Position* position2) {
//  return position1->evaluation < position2->evaluation;
//}
bool LessOutcome(const Position* position1, const Position* position2) {
  return position1->evaluation < position2->evaluation;
  //if (position1->evaluation != position2->evaluation) {
  //  return position1->evaluation < position2->evaluation;
  //} else if (position1->outcomes && position2->outcomes) {
  //  std::vector<Position*> a = *position1->outcomes;
  //  std::vector<Position*> b = *position2->outcomes;
  //  std::sort(a.begin(), a.end(),
  //            [](const Position* x, const Position* y) {
  //              return x->evaluation < y->evaluation;
  //            });
  //  std::sort(b.begin(), b.end(), [](const Position* x, const Position* y) {
  //    return x->evaluation < y->evaluation;
  //  });
  //  // Iterate over outcomes and compare second best move, third best move, etc.
  //  for (int i = 1; i < a.size() && i < b.size(); i++) {
  //    Position* a_outcome = a[i];
  //    Position* b_outcome = b[i];
  //    if (a_outcome->evaluation != b_outcome->evaluation) {
  //      return a_outcome->evaluation < b_outcome->evaluation;
  //    }
  //  }
  //  // If outcomes have same evaluation, recursively compare outcomes of
  //  // outcomes
  //  for (int i = 0; i < a.size() && i < b.size(); i++) {
  //    Position* a_outcome = a[i];
  //    Position* b_outcome = b[i];
  //    if (a_outcome->outcomes && b_outcome->outcomes) {
  //      return LessOutcome(a_outcome, b_outcome);
  //    }
  //  }
  //}
  //return false;
}
double EvaluateMaterial(const Position& position) {
  double material = 0;
  for (size_t i = 0; i < 64; i++) {
    if (position.board[i].color == Color::Empty || position.board[i].type == 'K') {
    continue;
    }
    if (position.board[i].color == Color::White) {
    material += piece_values[position.board[i].type];
    } else {
    material -= piece_values[position.board[i].type];
    }
  }
  return material;
}

bool ComplicatedLessOutcome(const Position* position1,
                            const Position* position2) {
  // return position1->evaluation < position2->evaluation;
   if (position1->evaluation != position2->evaluation) {
     return position1->evaluation < position2->evaluation;
   } else if (position1->outcomes && position2->outcomes) {
     std::vector<Position*> a = *position1->outcomes;
     std::vector<Position*> b = *position2->outcomes;
     std::sort(a.begin(), a.end(),
               [](const Position* x, const Position* y) {
                 return x->evaluation < y->evaluation;
               });
     std::sort(b.begin(), b.end(), [](const Position* x, const Position* y) {
       return x->evaluation < y->evaluation;
     });
     // Iterate over outcomes and compare second best move, third best move, etc.
     for (size_t i = 1; i < a.size() && i < b.size(); i++) {
       Position* a_outcome = a[i];
       Position* b_outcome = b[i];
       if (a_outcome->evaluation != b_outcome->evaluation) {
         return a_outcome->evaluation < b_outcome->evaluation;
       }
     }
     // If outcomes have same evaluation, recursively compare outcomes of
     // outcomes
     for (size_t i = 0; i < a.size() && i < b.size(); i++) {
       Position* a_outcome = a[i];
       Position* b_outcome = b[i];
       if (a_outcome->outcomes && b_outcome->outcomes) {
         return LessOutcome(a_outcome, b_outcome);
       }
     }
   }
   return false;
}
int deleting = 0;

bool GreaterOutcome(const Position* position1, const Position* position2) {
  return position1->evaluation > position2->evaluation;
}
Position* GetBestMove(const Position* position) {
  for (size_t i = 0; i < position->outcomes->size(); i++) {
     if ((*position->outcomes)[i]->evaluation == position->evaluation) {
       return (*position->outcomes)[i];
     }
  }
  return nullptr;
}

int default_max_depth_extension = -3;
int min_depth = 4;  // no less than 2
//int64_t max_gigabytes = 15;
//uint64_t max_bytes = (1 << 30) * max_gigabytes;
//uint64_t critical_bytes = (1 << 30) * (uint64_t) 13;
// int max_depth_extension = default_max_depth_extension;
//
//
// uint64_t safe_bytes = (1 << 30) * (uint64_t)8;
//const uint64_t minimum_bytes = (1 << 30) * (uint64_t)4;
const uint64_t max_bytes = (1 << 30) * (uint64_t)16;
//const int deepening_depth = 2;

//int reasonability_limit = 8;


void minimax(Position& position, int depth, double alpha, double beta,
             bool* stop/*, bool reasonable_extension*//*, bool selective_deepening*//*, int initial_material*/) {
  if (*stop || position.depth >= depth) {
     return;
  }
  if (position.fifty_move_rule >= 6) {
     int occurrences = 1;
     Position* previous_move = &position;
     for (size_t i = 0; i < position.fifty_move_rule; i += 2) {
       previous_move = previous_move->previous_move->previous_move;
       if (previous_move->board == position.board &&
           previous_move->castling == position.castling &&
           previous_move->en_passant_target == position.en_passant_target) {
         occurrences++;
         if (occurrences == 3) {
          position.evaluation = 0.0;
          position.depth = 1000;
          return;
         }
       }
    }
  
  }
  double initial_evaluation = position.evaluation;
  bool h = false;
  if (!position.outcomes) {

    new_generate_moves(position);
  }
  else if (depth == 0) {
     h = true;
  }

  if (position.outcomes->size() == 0) {
    position.depth = 1000;
    if (InCheck(position, position.white_to_move ? Color::White : Color::Black)) {
       position.evaluation =
           position.white_to_move
               ? (double)-mate + (double)position.number
               : (double)mate - (double)position.number;  // checkmate
    } else {
       position.evaluation = 0;  // draw by stalemate
    }
    return;
  }
  if (position.fifty_move_rule >= 100) {  // fifty move rule
    position.evaluation = 0;
    position.depth = 1000;
    return;
  }
  if (position.outcomes->size() <= 3) {
    depth++;
  }
  if (depth == 0 && (*position.outcomes)[0]->outcomes) {
       depth = 1;
  }
  

  if (depth >= (min_depth - 1)) {
     PROCESS_MEMORY_COUNTERS_EX pmc{};
     GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc,
                          sizeof(pmc));
     if (deleting == 0 && pmc.WorkingSetSize > max_bytes) {
       *stop = true;
     }
  }

  std::sort(position.outcomes->begin(), position.outcomes->end(),
            position.white_to_move ? GreaterOutcome : LessOutcome);
  if (depth == 0 &&
      position.outcomes->back()->evaluation == initial_evaluation) {
     //initial_evaluation = position.evaluation;
     if (position.white_to_move) {
       if (position.evaluation > alpha) {
         alpha = position.evaluation;
       }
     } else {
       if (position.evaluation < beta) {
         beta = position.evaluation;
       }
     }
     if (beta <= alpha) {
       return;
     }

  } else {
     position.evaluation = position.white_to_move ? INT_MIN : INT_MAX;
  }
  //if (depth == 0) {
  //  for (size_t i = 0; i < position.outcomes->size(); i++) {
  //     if ((*position.outcomes)[i]->evaluation != initial_evaluation && !(*position.outcomes)[i]->outcomes) {
  //       new_generate_moves(*(*position.outcomes)[i]);
  //       if ((*position.outcomes)[i]->outcomes->size() > 0) {
  //        (*position.outcomes)[i]->evaluation =
  //            (*std::max_element(
  //                 (*position.outcomes)[i]->outcomes->begin(),
  //                 (*position.outcomes)[i]->outcomes->end(),
  //                 ((*position.outcomes)[i]->white_to_move ? GreaterOutcome
  //                                                         : LessOutcome)))
  //                ->evaluation;

  //        if ((*position.outcomes)[i]->evaluation == initial_evaluation) {
  //          (*position.outcomes)[i]->evaluation += 0.0000000001;
  //        }
  //       }
  //     }

  //   }
  //  std::sort(position.outcomes->begin(), position.outcomes->end(),
  //            position.white_to_move ? GreaterOutcome : LessOutcome);
  //}
  for (size_t c = 0; c < position.outcomes->size(); c++) {

     if (depth == 0) {
       if ((*position.outcomes)[c]->evaluation == initial_evaluation) {
         continue;

       } else {
         //assert(round(initial_evaluation) !=
         //       round((*position.outcomes)[c]->evaluation));
         minimax(*(*position.outcomes)[c], depth, alpha, beta, stop/*,
                 reasonable_extension *//*, initial_material*/);
       }
     } else {
       minimax(*(*position.outcomes)[c], depth - 1, alpha, beta, stop);
     }
    
     if (position.white_to_move) {



       if ((*position.outcomes)[c]->evaluation > position.evaluation) {
         position.evaluation = (*position.outcomes)[c]->evaluation;
         // position.best_move = (*position.outcomes)[c];
         /*output.castling = position_value.castling;*/
       }
       if ((*position.outcomes)[c]->evaluation > alpha) {
         alpha = (*position.outcomes)[c]->evaluation;
       }
     } else {
       if ((*position.outcomes)[c]->evaluation < position.evaluation) {
         position.evaluation = (*position.outcomes)[c]->evaluation;
       }
       if ((*position.outcomes)[c]->evaluation < beta) {
         beta = (*position.outcomes)[c]->evaluation;
       }
     }
     if (beta <= alpha) {
       position.depth = -1;
       return;
     }
  }
  if (!*stop) {
      position.depth = depth;
  } else {
      position.depth = -1;
  }
}

int thread_count = 0;




std::mutex t_c_mutex;
std::mutex cv_mtx;
std::mutex making_move;
bool threads_maxed = false;
int max_threads = 6;
std::condition_variable threads_maxed_cv;

struct ThreadArg {
  Position* position;
  Position* to_keep;
};

void DeleteOutcomes(void* void_arg) {
  deleting++;
  //Sleep(10000);
  ThreadArg* arg = static_cast<ThreadArg*>(void_arg);
  //printf("[DeleteOutcomes %d] enter.  outcomes=%p, size=%d\n",
  //       GetCurrentThreadId(), arg->position->outcomes,
  //       arg->position->outcomes->size());
  for (size_t i = 0; i < arg->position->outcomes->size(); i++) {
    if ((*arg->position->outcomes)[i] != arg->to_keep) {
      delete (*arg->position->outcomes)[i];
    }
  }
  arg->position->outcomes->clear();
  arg->position->outcomes->emplace_back(arg->to_keep);
  delete arg;
  t_c_mutex.lock();
  thread_count--;
  if (thread_count < max_threads) {
    threads_maxed = false;
  }
  t_c_mutex.unlock();
  deleting--;
  //printf("[DeleteOutcomes %d] exit\n", GetCurrentThreadId());
  _endthread();
}



using TrashType = std::vector<std::pair<Position*, Position*>>;
//std::mutex working;
std::condition_variable stop_cv;
std::mutex stop_cv_mutex;
std::binary_semaphore waiting{0};

struct ThreadInfo {
  Position* position;
  bool* stop;
  TrashType* trash;
  char side;
  ThreadInfo(Position* p, bool* s, TrashType* t, char c)
      : position(p), stop(s), trash(t), side(c) {}
};
bool done = false;
std::condition_variable done_cv;
std::mutex done_cv_mtx;
std::mutex stop_mutex;

// search 'printf' to find



std::vector<Position*> GetLine(Position* position) {
  std::vector<Position*> line;
  //line.emplace_back(position);
  while (position->outcomes && GetBestMove(position)) {
    position = GetBestMove(position);
    line.emplace_back(position);
  }
  return line;
}

int CheckGameOver(Position* position) {
  if (position->outcomes->size() == 0) {
    if (position->white_to_move && InCheck(*position, Color::White)) {
      return -1;
    }
    if (!position->white_to_move && InCheck(*position, Color::Black)) {
      return 1;
    }
    return 0;
  }
  if (position->fifty_move_rule >= 100) {
    return 0;
  }
  if (position->fifty_move_rule >= 6) {
    int occurrences = 1;
    Position* previous_move = position;
    for (size_t i = 0; i < position->fifty_move_rule; i += 2) {
      previous_move = previous_move->previous_move->previous_move;
      if (previous_move->board == position->board &&
          previous_move->castling == position->castling &&
          previous_move->en_passant_target == position->en_passant_target) {
         occurrences++;
         if (occurrences == 3) {
          return 0;
         }
      }
    }
  }
  return 2;
}


const double aspiration_window = 0.0025;


void SearchPosition(Position* position, int minimum_depth, bool* stop) {
  int depth = position->depth + 1;
  double alpha, beta;
  double alpha_aspiration_window = aspiration_window;
  double beta_aspiration_window = aspiration_window;
  double approximate_evaluation = position->evaluation;
  if (!position->outcomes) {
    new_generate_moves(*position);
  }
  while (depth <= minimum_depth && !*stop && CheckGameOver(position) == 2) {
    if (depth > 2) {
      alpha = position->evaluation - aspiration_window;
      beta = position->evaluation + aspiration_window;
    } else {
      alpha = std::numeric_limits<double>::lowest();
      beta = INT_MAX;
    }
    minimax(*position, depth, alpha, beta,
            stop);
    if (position->evaluation <= alpha) {
      alpha_aspiration_window *= 4; // unexpected advantage for black
    } else if (position->evaluation >= beta) {
      beta_aspiration_window *= 4; // unexpected advantage for white
    } else {
      alpha_aspiration_window = beta_aspiration_window = aspiration_window;
      approximate_evaluation = position->evaluation;
      depth++;
    }
  }
}



void calculate_moves(void* varg) {
  //double minimum_time = 3;
  //int64_t max_memory = 13;
  //int64_t max_bytes = 1073741824 * max_memory;
  //int max_depth = /*floor(log2(max_bytes / sizeof(Position)) / log2(32));*/ 6;
  //std::chrono::duration<double> time_span = duration_cast<std::chrono::duration<double>::zero>;

  ThreadInfo* input = (ThreadInfo*)varg;

  while (true) {
    if (!*input->stop) {

      if ((input->position->white_to_move ? 'W' : 'B') == input->side) {
         double alpha, beta;
         double alpha_aspiration_window = aspiration_window;
         double beta_aspiration_window = aspiration_window;
         double approximate_evaluation = input->position->evaluation;
        while (input->position->depth < min_depth && !*input->stop) {
          if (input->position->depth > 2) {
            alpha = input->position->evaluation - aspiration_window;
            beta = input->position->evaluation + aspiration_window;
          } else {
            alpha = std::numeric_limits<double>::lowest();
            beta = DBL_MAX;
          }
          minimax(*input->position, input->position->depth + 1, alpha, beta, input->stop);
          if (input->position->evaluation <= alpha) {
            alpha_aspiration_window *= 4; // unexpected advantage for black
          } else if (input->position->evaluation >= beta) {
            beta_aspiration_window *= 4; // unexpected advantage for white
          } else {
            alpha_aspiration_window = beta_aspiration_window =
                aspiration_window;
            approximate_evaluation = input->position->evaluation;
          }
        }

        done = true;
        *input->stop = true;
        done_cv.notify_one();

      } else {
        while (!*input->stop) {
          for (size_t i = 0;
               i < input->position->outcomes->size() && !*input->stop; i++) {
            SearchPosition(
                (*input->position->outcomes)[i],
                std::max(min_depth, (*input->position->outcomes)[i]->depth + 1), input->stop);  // do position->depth + 1 while
                                                 // position->depth < min_depth
          }
        }
      }
    }
    if (*input->stop) {
      waiting.release();
      // printf("[calculate_moves %d] block.\n", GetCurrentThreadId());
      std::unique_lock lk(stop_cv_mutex);
      stop_cv.wait(lk, [input] { return !*input->stop; });
      if (!input->position->outcomes) {
        new_generate_moves(*input->position);
      }
      if ((input->position->white_to_move ? 'W' : 'B') ==
              input->side && /*(input->position->depth >= min_depth ||*/
          input->position->outcomes->size() == 1 /*)*/) {
        input->position->evaluation =
            (*input->position->outcomes)[0]->evaluation;
        done = true;
        *input->stop = true;
        done_cv.notify_one();
      }
    }

    

    for (size_t i = 0; i < input->trash->size(); i++) {
      ThreadArg* arg = new ThreadArg();
      arg->position = (*input->trash)[i].first;
      arg->to_keep = (*input->trash)[i].second;
      {
        std::unique_lock lk(cv_mtx);
        threads_maxed_cv.wait(lk, [] { return !threads_maxed; });
      }
      t_c_mutex.lock();
      thread_count++;
      if (thread_count >= max_threads) {
        threads_maxed = true;
      }
      t_c_mutex.unlock();
      _beginthread(DeleteOutcomes, 0, arg);
    }
    if (input->trash->size() > 0) {
      input->trash->clear();
    }
  }
  
}



using VBoard = std::vector<std::vector<std::string>>;


std::string GetMove(Position& position1, Position& position2) {


  
  std::vector<size_t> differences;
  for (size_t i = 0; i < 64; i++) {
      if (position1.board[i] != position2.board[i]) {
        differences.emplace_back(i);
      }
    
  }
  if (differences.size() == 4) { // optimization: use position1 catling rights != position2 castling rights
    if ((position2.board[62].type == 'K' &&
        position1.board[62].color == Color::Empty) || (position2.board[6].type == 'K' &&
        position1.board[6].color == Color::Empty)) {
      return "O-O";
    }
    if ((position2.board[58].type == 'K' &&
        position1.board[58].color == Color::Empty) || (position2.board[2].type == 'K' &&
        position1.board[2].color == Color::Empty)) {
      return "O-O-O";
    }
  }
  if (differences.size() == 3) {
    if (position2
            .board[(size_t)differences[0]]
      .color == Color::Empty) {
        return std::string({char('a' + (((differences[1] == differences[0] + 8)
                                             ? differences[2]
                                             : differences[1]) &
                                        7)),
                            'x', char('a' + (differences[0] & 7)),
                            char('0' + 8 - (differences[0] >> 3))});
    } else {
        return std::string({char('a' + (((differences[1] == differences[2] + 8)
                                             ? differences[0]
                                             : differences[1]) &
                                        7)),
                            'x', char('a' + (differences[2] & 7)),
                            char('0' + 8 - (differences[2] >> 3))});
    }

    
  }
  size_t origin, dest;
  if (position2.board[differences[0]].color == Color::Empty) {
    origin = differences[0];
    dest = differences[1];
  } else {
    origin = differences[1];
    dest = differences[0];
  }
  Check check_info;
  if (position1.board[origin].type != 'P') {
    GetCheckInfo(check_info, position1);
  }
  std::stringstream output;
  std::vector<size_t> found;
  switch (position1.board[origin].type) {
    case 'P':
      if (position1.board[dest].color != Color::Empty) {
        // capture
        output << (char) ('a' + (origin & 7)) << 'x' << char('a' + (dest & 7))
               << (8 - (dest >> 3));
      } else {
        output << char('a' + (dest & 7)) << (8 - (dest >> 3));
      }
      if (position2.board[dest].type != 'P') {
        // promotion
        output << '='
               << position2.board[dest].type;
      }
      break;
    case 'N':
      output << 'N';
      for (size_t k = 0; k < 8; k++) {
        int new_i = (int)dest + knight_moves[k];
        if (new_i >= 0 && new_i <= 63 &&
            position1.board[(size_t)new_i].color ==
                    position2.board[dest]
                    .color &&
            position1.board[(size_t)new_i].type == 'N' &&
            (size_t)new_i !=
                origin &&
            MoveOk(check_info, (size_t)new_i, dest)) {
          found.emplace_back((size_t)new_i);
        }
      }
      if (found.size() > 0) {
        if (std::none_of(found.begin(), found.end(), [origin](size_t p) {
              return (p & 7) == (origin & 7);
            })) {
          output << (char) ('a' + (origin & 7));
        } else if (std::none_of(found.begin(), found.end(), [origin](size_t p) {
                     return (p >> 3) == (origin >> 3);
                   })) {
          output << 8 - (origin >> 3);
        } else {
          output << static_cast<char>('a' + (origin & 7)) << (origin >> 3);
        }
      }
      if (position1.board[dest].color != Color::Empty) {
        output << 'x';
      }
      output << char('a' + (dest & 7)) << (8 - (dest >> 3));
      break;
    case 'Q':
    case 'B':
      output << position1.board[origin].type;
      for (size_t k = 0; k < 4; k++) {
        int new_i = (int)dest + bishop_moves[k];
        if (((new_i & 7) - ((int)dest & 7)) > 1 ||
            ((new_i & 7) - ((int)dest & 7)) < -1 || new_i > 63 || new_i < 0) {
          continue;
        }
        while (true) {
          if (position1.board[(size_t)new_i].color == (position2.white_to_move ? Color::White : Color::Black)) {
            break;
          }
          if (position1.board[(size_t)new_i].color ==
                  (position1.white_to_move ? Color::White : Color::Black)&&
              position1.board[(size_t)new_i].type ==
                  position1.board[origin].type &&
              (new_i != (int)origin) &&
              MoveOk(check_info, (size_t)new_i,dest)) {
            found.emplace_back((size_t)new_i);
          }
          if (position1.board[(size_t)new_i].color ==
              position2.board[dest].color || new_i <= 8 || new_i >= 55 || (new_i & 7) == 7 ||
              (new_i & 7) == 0) {
            break;
          }
          new_i += bishop_moves[k];
        }
      }
      if (position1.board[origin].type ==
          'B') {
        if (found.size() > 0) {
          if (std::none_of(found.begin(), found.end(), [origin](size_t p) {
                return (p & 7) == (origin & 7);
              })) {
            output << (char)('a' + (origin & 7));
          } else if (std::none_of(
                         found.begin(), found.end(),
                                  [origin](size_t p) {
                                    return (p >> 3) == (origin >> 3);
                                  })) {
            output << 8 - (origin >> 3);
          } else {
            output << (char)('a' + (origin & 7)) << (origin >> 3);
          }
        }
        if (position1.board[dest].color !=
            Color::Empty) {
          output << 'x';
        }
        output << (char)('a' + (dest & 7)) << (8 - (dest >> 3));
        break;
      }
    case 'R':
      if (position1.board[origin].type ==
          'R') {
        output << 'R';
      }
      for (size_t k = 0; k < 4; k++) {
        int new_i = (int)dest + rook_moves[k];
        if ((((new_i & 7) != ((int)dest & 7)) &&
             ((new_i >> 3) != ((int)dest >> 3))) ||
            new_i > 63 || new_i < 0) {
          continue;
        }
        while (true) {
          if (position1.board[(size_t)new_i].color ==
              (position2.white_to_move ? Color::White : Color::Black)) {
            break;
          }
          if (position1.board[(size_t)new_i].color ==
                  (position1.white_to_move ? Color::White : Color::Black) &&
              position1.board[(size_t)new_i].type ==
                  position1.board[origin].type &&
              (size_t)new_i != origin &&
              MoveOk(check_info, (size_t)new_i,dest)) {
            found.emplace_back((size_t)new_i);
          }
          if (position1.board[(size_t)new_i].color ==
              position2.board[dest].color) {
            break;
          }
          if (k == 0) {
            if (new_i <= 7) {
              break;
            }
          } else if (k == 1) {
            if ((new_i & 7) == 7) {
              break;
            }
          } else if (k == 2) {
            if (new_i >= 56) {
              break;
            }
          } else {
            if ((new_i & 7) == 0) {
              break;
            }
          }
          new_i += rook_moves[k];
        }
      }
      if (found.size() > 0) {
        if (std::none_of(found.begin(), found.end(), [origin](size_t p) {
              return (p & 7) == (origin&7);
            })) {
          output << (char)('a' + (origin&7));
        } else if (std::none_of(found.begin(), found.end(), [origin](size_t p) {
                     return (p >> 3) == (origin >> 3);
                   })) {
          output << 8 - (origin >> 3);
        } else {
          output << char('a' + (origin&7)) << (8 - (origin>>3));
        }
      }
      if (position1.board[dest].color != Color::Empty) {
        output << 'x';
      }
      output << (char)('a' + (dest & 7)) << (8 - (dest >> 3));
      break;
    case 'K':
      output << 'K';
      if (position1.board[dest].color != Color::Empty) {
        output << 'x';
      }
      output << (char)('a' + (dest & 7)) << (8 - (dest >> 3));
      break;
      


  }
  if (std::abs(position2.evaluation) == (position2.white_to_move ? (mate - position2.number) : (-mate + position2.number))) {
    output << '#';
  } else if (InCheck(position2, position2.white_to_move ? Color::White : Color::Black)) {
    output << '+';
  }
  return output.str();
}

void ReadFEN(Position& position, std::string FEN) {

std::stringstream elements_line(FEN);
  std::vector<std::string> elements;
  for (int i = 0; i < 6; i++) {
    std::string s;
    elements_line >> s;
    elements.push_back(s);
  }
  size_t i = 0;
  for (size_t k = 0; k < elements[0].length(); k++) {
    if (elements[0][k] == '/') {
      i++;
      continue;
    }
    if (i > 63) {
      std::cout << "FEN error";
      exit(1);
    }
    if (isdigit(elements[0][k])) {
      for (int l = char_to_int(elements[0][k]); l > 0; l--) {
        if (i > 63) {
          std::cout << "FEN error";
          exit(1);
        }
        position.board[i].Empty();
        i++;
      }
    } else {
      if (islower(elements[0][k])) {
        position.board[i].color = Color::White;
      } else {
        position.board[i].color = Color::White;
      }
      position.board[i].type = (char)toupper(elements[0][k]);
      i++;
    }
  }
  position.white_to_move = toupper(elements[1][0]) == 'W';
  if (elements[2][0] == 'K') {
    position.castling.white_o_o = true;
  }
  if (elements[2][0] == 'Q') {
    position.castling.white_o_o_o = true;
  }
  if (elements[2][0] == 'k') {
    position.castling.black_o_o = true;
  }
  if (elements[2][0] == 'q') {
    position.castling.black_o_o_o = true;
  }
  if (elements[3] != "-") {
    position.en_passant_target =
        ((8 - char_to_int(elements[3][1])) * 8) + (elements[3][0] - 'a');
  } else {
    position.en_passant_target = -1;
  }
  position.fifty_move_rule = (size_t)stoi(elements[4]);
}


Position* ReadPGN(Position* position) {
  std::ifstream file;
  file.open("PGN_position.txt");
  if (!file.is_open()) {
    std::cout << "Failed to open file.";
    exit(1);
  }
  std::string line;
  std::regex rgx(R"regex(\[(\S+) \"(.*)\"\])regex");
  do {
    getline(file, line);
    if (line[0] != '[') {
      break;
    }
    std::smatch match;
    if (std::regex_search(line, match, rgx) && match.size() == 3) {
      if (match[1] == "FEN") {
        ReadFEN(*position, match[2]);
      }
      //std::cout << "Parsed match 1: " << match[1] << std::endl;
      //std::cout << "Parsed match 2: " << match[2] << std::endl;
    } else {
      std::cout << "Invalid line: " << line;
    }
  } while (line[0] == '[');
  std::string moves_line;
  getline(file, moves_line);
  if (moves_line == "*") {
    return position;
  }
  std::regex moves_regex(
      R"regex(\d{1,}\.{1,3}\s?(?:([Oo0]-[Oo0](?:-[Oo0])?|[KQRBN]?[a-h]?[1-8]?x?[a-h][1-8](?:\=[QRBN])?[+#]?)(?:\s?\{.+?\})?(?:\s(?:1-0|0-1|1\/2-1\/2))?\s?)(?:([Oo0]-[Oo0](?:-[Oo0])?|[KQRBN]?[a-h]?[1-8]?x?[a-h][1-8](?:\=[QRBN])?[+#]?)(?:\s?\{.+?\})?(?:\s(?:1-0|0-1|1\/2-1\/2))?\s?)?)regex");
  std::smatch moves_match;
  std::string moves_line_copy = moves_line;
  int position_number = 0;
  while (std::regex_search(moves_line_copy, moves_match, moves_regex)) {
    for (size_t i = 1; (i < moves_match.size() && moves_match[i].matched); i++) {
      position_number++;
      new_generate_moves(*position);
      assert(MoveToPosition(*position, moves_match[i]));
      position = MoveToPosition(*position, moves_match[i]);
      //moves_match[i];
    }
    moves_line_copy = moves_match.suffix().str();
  }
  position->number = position_number;
  position->depth = 0;
  file.close();
  return position;
}



std::string Convert(double e) {
  int rounding = 10000000;
  std::stringstream output;
  double approximation = round(e * rounding) / rounding;
  if ((int)approximation == approximation) {
    return std::to_string((int)approximation);
  }
  output << std::setprecision(10) << std::fixed << approximation;
  std::string output_string = output.str();
  size_t last_digit = output_string.find_last_not_of('0') + 1;
  if (last_digit < output_string.length()) {
    output_string.erase(last_digit);
  }
  return output_string;


  
  }


void UpdatePGN(Position* new_position, std::string move_string) {
  std::ifstream file;
  file.open("PGN_position.txt");
  std::string line;
  std::vector<std::string> lines;
  do {
    getline(file, line);
    lines.emplace_back(line);
  } while (line != "");
  getline(file, line);
  std::regex moves_regex(
      R"regex(\d{1,}\.{1,3}\s?(?:([Oo0]-[Oo0](?:-[Oo0])?|[KQRBN]?[a-h]?[1-8]?x?[a-h][1-8](?:\=[QRBN])?[+#]?)(?:\s?\{.+?\})?(?:\s(?:1-0|0-1|1\/2-1\/2))?\s?)(?:([Oo0]-[Oo0](?:-[Oo0])?|[KQRBN]?[a-h]?[1-8]?x?[a-h][1-8](?:\=[QRBN])?[+#]?)(?:\s?\{.+?\})?(?:\s(?:1-0|0-1|1\/2-1\/2))?\s?)?)regex");
  std::smatch moves_match;
  std::string line_copy = line;
  int position_number = 0;
  while (std::regex_search(line_copy, moves_match, moves_regex) &&
         position_number < new_position->number - 1) {
    for (size_t i = 1; (i < moves_match.size() && moves_match[i].matched); i++) {
      position_number++;
      if (position_number == new_position->number - 1) {
        if (i == 1) {
          line.erase(line.length() -
                     (line_copy.length() - moves_match[i].length() - 2 -
                      std::to_string((int) floor((double)new_position->number / 2.0))
                          .length()));
          line += " *";
        } else {
          line.erase(line.length() - (moves_match.suffix().str().length() + 1));
          line += " *";
        }
      }
    }
    line_copy = moves_match.suffix().str();
  }
  if (!new_position->white_to_move) {
    if ((line.length() > 1)) {
      line.insert(
          (line.length() - 2),
          " " +
              std::to_string((int)((double)new_position->number / 2.0 + 0.5)) +
              ". " + move_string);
    } else {
      line.insert(
          0, std::to_string((int)((double)new_position->number / 2.0 + 0.5)) +
                 ". " + move_string + " ");
    }
  } else {
    line.insert(((line.length() > 1) ? (line.length() - 2) : 0),
                " " + move_string);
  }
  file.close();
  lines.emplace_back(line);
  std::ofstream output_file;
  output_file.open("PGN_position.txt");
  for (size_t i = 0; i < lines.size(); i++) {
    output_file << lines[i] << (i != (lines.size() - 1) ? "\n" : "");
  }
  output_file.close();
}

using SeekResult = std::pair<bool, Position*>;

SeekResult SeekPosition(Position* position, int moves) {
  int new_position_number = position->number + moves;
  if (new_position_number < 0) {
    return std::pair(false, position);
  }
  std::ifstream file;
  file.open("PGN_position.txt");
  if (!file.is_open()) {
    std::cout << "Failed to open file.";
    exit(1);
  }
  std::string line;
  std::regex rgx(R"regex(\[(\S+) \"(.*)\"\])regex");
  Position* new_position = Position::StartingPosition();
  do {
    getline(file, line);
    if (line[0] != '[') {
      break;
    }
    std::smatch match;
    if (std::regex_search(line, match, rgx) && match.size() == 3) {
      if (match[1] == "FEN") {
        ReadFEN(*position, match[2]);
      }

    } else {
      std::cout << "Invalid line: " << line;
    }
  } while (line[0] == '[');
  getline(file, line);
  std::regex moves_regex(
      R"regex(\d{1,}\.{1,3}\s?(?:([Oo0]-[Oo0](?:-[Oo0])?|[KQRBN]?[a-h]?[1-8]?x?[a-h][1-8](?:\=[QRBN])?[+#]?)(?:\s?\{.+?\})?(?:\s(?:1-0|0-1|1\/2-1\/2))?\s?)(?:([Oo0]-[Oo0](?:-[Oo0])?|[KQRBN]?[a-h]?[1-8]?x?[a-h][1-8](?:\=[QRBN])?[+#]?)(?:\s?\{.+?\})?(?:\s(?:1-0|0-1|1\/2-1\/2))?\s?)?)regex");
  std::smatch moves_match;

  int position_number = 0;
  while (std::regex_search(line, moves_match, moves_regex) && position_number < new_position_number) {
    for (size_t i = 1; (i < moves_match.size() && moves_match[i].matched); i++) {
      position_number++;
      new_generate_moves(*new_position);
      new_position = MoveToPosition(*new_position, moves_match[i]);
      if (position_number == new_position_number) {
        break;
      }
      // moves_match[i];
    }
    line = moves_match.suffix().str();
  }
  file.close();
  if (new_position_number == position_number) {
    return std::pair(true, new_position);
  } else {
    return std::pair(false, position);
  }


}



double CountMaterial(const Position& position) {
  double count = 0;
  for (size_t i = 0; i < 64; i++) {
      if (position.board[i].color != Color::Empty) {
        if (position.board[i].type == 'K') {
          count += 4;
        } else {
          count += piece_values[position.board[i].type];
        }
      }
  }
  return count;
}

void LogGameEnd(const Position& position, const int &game_status) {
  switch (game_status) {
    case -1:
      std::cout << "Black wins by checkmate." << std::endl;
      break;
    case 0:
      if (position.fifty_move_rule == 50) {
        std::cout << "Draw by fifty move rule." << std::endl;
      } else if (position.outcomes->size() == 0) {
        std::cout << "Draw by stalemate" << std::endl;
      } else {
        std::cout << "Draw by repetition." << std::endl;
      }
      break;
    case 1:
      std::cout << "White wins by checkmate." << std::endl;
      break;
    default:
      std::cout << "Unexpected end of game." << std::endl;
      exit(1);
  }
}

const int max_positions = 2600000;

void UpdateMinDepth(Position& position) {
  min_depth = (int)std::max(
      4.0, round(log2(max_positions) / log2((double)CountMoves(position) / 2.0)));
}

int main() {
  thread_count++;

  //int max_threads = 6; // 12

  //VBoard v_board = {{"BR", "BN", "BB", "BQ", "BK", "BB", "BN", "BR"},
  //                  {"BP", "BP", "BP", "BP", "BP", "BP", "BP", "BP"},
  //                  {"  ", "  ", "  ", "  ", "  ", "  ", "  ", "  "},
  //                  {"  ", "  ", "  ", "  ", "  ", "  ", "  ", "  "},
  //                  {"  ", "  ", "  ", "  ", "  ", "  ", "  ", "  "},
  //                  {"  ", "  ", "  ", "  ", "  ", "  ", "  ", "  "},
  //                  {"WP", "WP", "WP", "WP", "WP", "WP", "WP", "WP"},
  //                  {"WR", "WN", "WB", "WQ", "WK", "WB", "WN", "WR"}};
  //Board board;
  //for (size_t i = 0; i < 8; i++) {
  //  std::vector<Piece> row;
  //  for (size_t j = 0; j < 8; j++) {
  //    Color color;
  //    switch (v_board[i][j][0]) { case 'W':
  //        color = white;
  //        break;
  //      case 'B':
  //        color = black;
  //        break;
  //      case ' ':
  //        color = empty;
  //        break;
  //      default:
  //        std::cout << "v_board conversion fail.";
  //        exit(1);
  //    }
  //    row.emplace_back(color, v_board[i][j][1]);
  //  }
  //  board.emplace_back(row);
  //}

  //bool white_O_O = true;
  //bool white_O_O_O = true;
  //bool black_O_O = true;
  //bool black_O_O_O = true;
  //int number = 0;
  //Kings kings = Kings(Point(7, 4), Point(0, 4));
  //Castling castling = {white_O_O, white_O_O_O, black_O_O, black_O_O_O};
  //Color to_move = white;
  ////Position* previous_move = NULL;
  //std::vector<Position*>* outcomes = nullptr;
  //Position* position =
  //    new Position(to_move == white, number, 0.0, board, castling/*, 0LL*/,
  //                 outcomes /*, previous_move*/, nullptr, 0, /*nullptr,*/ kings,
  //                 IntPoint(-1, -1), 0);
  Position* position = Position::StartingPosition();
  std::cout << std::setprecision(7);
  
  bool mental_chess;
  std::cout << "Mental chess? ";
  std::cin >> mental_chess;


  bool resume;
  std::cout << "Resume? ";
  std::cin >> resume;
  if (resume) {
    //ReadPosition(*position);
    position = ReadPGN(position);
  }
  else {
    std::ofstream file;
    file.open("PGN_position.txt");
    file << "[Event \"?\"]\n[Site \"?\"]\n[Date \"????.??.??\"]\n[Round \"?\"]\n[White \"?\"]\n[Black \"?\"]\n[Result \"*\"]\n\n*";
  }
  for (size_t i = 0; i < 64; i++) {
      if (position->board[i].type == 'K') {
        position->kings[position->board[i].color] = i;
      }
  }
  //std::cout << InCheck(*position, position->to_move);
  
  // standard notation: type the coordinates of the starting and end point (such as 6444 for e4). if you type one more point, that point becomes an empty square (for en passant). For castling, type the king move and the rook move right after each other. For promotions, add =[insert piece type] after the pawn move (such as 1404=Q) piece types are Q, B, R, N.

  char color;
  bool game_over = false;
  std::cout << "What color am I playing? ";
  std::cin >> color;
  color = (char)toupper(color);
  while (color != 'W' && color != 'B') {
    std::cout << "What color am I playing (enter \"W\" or \"B\")? ";
    std::cin >> color;
    color = (char)toupper(color);
  }
  bool engine_white = color == 'W';
  char bottom_board;
  std::cout << "What color is on the bottom of the board? ";
  std::cin >> bottom_board;
  bool white_on_bottom = bottom_board == 'W' || bottom_board == 'w';

  ThreadInfo* info = new ThreadInfo(position, new bool(false), new TrashType, color);
  std::string move;
  Board new_board = position->board;
  bool engine_on = true;
  Position* new_position = nullptr;
  int game_status;
  std::string lower_move;
  //new_generate_moves(*position);
  SeekResult result;
  std::cout << MakeString(*position, white_on_bottom) << std::endl;
  if (engine_white == position->white_to_move) {
    //std::cout << "My move: ";
    UpdateMinDepth(*position);
  } else {
    // minimax(*position, 1, INT_MIN, INT_MAX, info->stop, false); // TODO: delete this line
    new_generate_moves(*position);
    game_status = CheckGameOver(position);
    if (game_status != 2) {
      game_over = true;
      switch (game_status) {
        case -1:
          std::cout << "Black wins by checkmate." << std::endl;
          break;
        case 0:
          if (position->fifty_move_rule == 50) {
            std::cout << "Draw by fifty move rule." << std::endl;
          } else {
            std::cout << "Draw by stalemate" << std::endl;
          }
          break;
        case 1:
          std::cout << "White wins by checkmate." << std::endl;
          break;
      }
    }
  }
  t_c_mutex.lock();
  thread_count++;
  if (thread_count >= max_threads) {
    threads_maxed = true;
  }
  t_c_mutex.unlock();
  _beginthread(calculate_moves, 0, info);
  while (true) {
    if (engine_white == position->white_to_move && engine_on) {
      //printf("[main thread %d] block.\n", GetCurrentThreadId());
      std::cout << "My move: ";

      {
        std::unique_lock lk(done_cv_mtx);
        done_cv.wait(lk, [] { return done; });
      }
      waiting.acquire();
      game_status = CheckGameOver(position);
      Position* best_move;

      if (game_status != 2) {
        std::cout << std::endl;
        game_over = true;
        LogGameEnd(*position, game_status);
      } else {
        best_move = GetBestMove(position);
        if (!best_move->outcomes) {
          new_generate_moves(*best_move);
        }
        assert(best_move->evaluation == position->evaluation);
        /*} else {
  best_move = *std::min_element(position->outcomes->begin(),
                                position->outcomes->end(),
ComplicatedLessOutcome);
}*/
        std::string move_string =
            GetMove(*position, *best_move);

        game_status = CheckGameOver(best_move);


        UpdatePGN(best_move, move_string);

        info->trash->emplace_back(position, best_move);
        info->position = best_move;
        // printf("[main thread %d] wakeup.  outcomes=%p, size=%d\n",
        //        GetCurrentThreadId(), info->position->outcomes,
        //        info->position->outcomes->size());
        if (!best_move->outcomes) {
          new_generate_moves(*best_move);
        }
        std::cout << move_string
                  << std::endl;  // sometimes dies with this line being "the
                                 // next statement to execute when this thread
                                 // returns from the current function"
        if (!mental_chess) { // died with this
          std::cout << MakeString(*best_move, white_on_bottom) << std::endl;
          std::cout << "Material evaluation: " << EvaluateMaterial(*best_move) << std::endl;
          std::cout << "Evaluation on depth "
                    << ((best_move->depth == -1)
                            ? "?"
                            : std::to_string(best_move->depth))
                    << ": " << Convert(best_move->evaluation) << std::endl;
          std::cout << "Line: ";
          std::vector<Position*> line = GetLine(position);
          //std::cout << "[got line] ";
          if (line.size() == 0) {
            std::cout << std::endl << "Line was size 0";
            exit(1);
          }
          std::cout << GetMove(*position, *line[0]);
          //std::cout << "[got first move] ";
          for (size_t i = 1; i < line.size(); i++) {
            std::cout << ' ' << GetMove(*line[i - 1], *line[i]);
            //std::cout << "[got next move] ";
          }
          std::cout/* << "[finished line]"*/ << std::endl;
          std::cout << "Moves: " << best_move->outcomes->size() << std::endl
                    << "Material: " << CountMaterial(*best_move) / 2.0 << std::endl;
        }
        position = best_move;
        if (game_status != 2) {
          game_over = true;
          LogGameEnd(*position, game_status);
        }
      }

      done = false;
      // stop_mutex.lock();
      *info->stop = false;
      // stop_mutex.unlock();
      stop_cv.notify_one();
      /*making_move.unlock();
      working.unlock();*/

    }
    // assert(!stop);
    // HANDLE thread_handle = (HANDLE)_beginthread(Premoves, 0, position);
    std::cout << "Your move: ";
    std::cin >> move;

     lower_move = ToLower(move);
    if (lower_move == "disable") {
      engine_on = false;
      continue;
    } else if (lower_move == "enable") {
      engine_on = true;
      continue;
    } else if (move.length() == 1) {
      new_position = nullptr;
    } else if (move[0] == '+' || move[0] == '-') {
      result = SeekPosition(
            position, (move[0] == '+' ? 1 : -1) * stoi(move.substr(1, (move.length() - 1))));
      if (!result.first) {
        std::cout << "Invalid seek." << std::endl;
        continue;
      } else {
        engine_on = false;
            new_position = result.second;
            new_position->depth = 0;
      }
      stop_mutex.lock();
      *info->stop = true;
      stop_mutex.unlock();
      waiting.acquire();

      if (!mental_chess) {
            std::cout << MakeString(*new_position, white_on_bottom) << std::endl;
            std::cout << Convert(new_position->evaluation) << std::endl;
      }
      game_over = false;
      info->trash->emplace_back(position, nullptr);
      position = new_position;
      info->position = position;
      done = false;
      *info->stop = false;

      stop_cv.notify_one();
      continue;
    } else if (game_over) {
      new_position = nullptr;
    } else {
      new_position = MoveToPosition(*position, move);
    }
    if (new_position != nullptr) {
      // Position* played_position = new_position;
      stop_mutex.lock();
      *info->stop = true;
      stop_mutex.unlock();
      //printf("[main thread %d] acquire.\n", GetCurrentThreadId());
      waiting.acquire();
      UpdatePGN(new_position, GetMove(*position, *new_position));
      if (!new_position->outcomes) {
        new_generate_moves(*new_position);
      }



      //printf("[main thread %d] acquire complete.\n", GetCurrentThreadId());
      if (!mental_chess) {
        std::cout << MakeString(*new_position, white_on_bottom) << std::endl;
        std::cout << "Material evaluation: " << EvaluateMaterial(*new_position) << std::endl;
        std::cout << "Evaluation on depth " << ((new_position->depth <= 0)
            ? "?" : std::to_string(new_position->depth)) << ": "
                  << Convert(new_position->evaluation) << std::endl;
        std::cout << "Moves: " << new_position->outcomes->size() << std::endl
                  << "Material: " << (float)CountMaterial(*new_position) / 2.0
                  << std::endl;
      }

      info->trash->emplace_back(position, new_position);
      position = new_position;
      info->position = position;
      if (!new_position->outcomes) {
        new_generate_moves(*new_position);
      }

          UpdateMinDepth(*position);
      done = false;
      // stop_mutex.lock();
      *info->stop = false;
      // stop_mutex.unlock();
      stop_cv.notify_one();
    } else {
      std::cout << "Invalid move." << std::endl;
    }
    // wait for premoves to finish
    // WaitForSingleObject(thread_handle, INFINITE);
    // std::cout << "[main thread] premoves thread ended";
    // stop = false;
  }

  return 0;
}