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

std::map<char, int> piece_values = {
    {'P', 1}, {'N', 3}, {'B', 3}, {'R', 5}, {'Q', 9}};
const int knight_moves[8][2] = {{-2, -1}, {-2, 1}, {-1, 2},  {1, 2},
                                {2, 1},   {2, -1}, {-1, -2}, {1, -2}};
const int bishop_moves[4][2] = {{-1, -1}, {-1, 1}, {1, 1}, {1, -1}};
const int king_moves[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, 1},
                              {1, 1},   {1, 0},  {1, -1}, {0, -1}};
const int rook_moves[4][2] = {{0, -1}, {-1, 0}, {0, 1}, {1, 0}};

std::binary_semaphore computed{0};
std::mutex position_mutex;


// enum class PieceType : char { Empty, Rook, Knight, Bishop, Queen, King, Pawn
// }; enum class ColorType : char { White, Black }; struct Piece {
//   PieceType type = PieceType::Empty;
//   ColorType color = ColorType::White;
// };
enum Color { white, black, empty};
struct Piece {
  Color color;
  char type;
  
  bool IsEmpty() const { return color == empty;
  }
  bool operator==(const Piece& other) const {
    return color == other.color && type == other.type;
  }
  bool operator!=(const Piece& other) const {
    return color != other.color || type != other.type;
  }
  bool operator==(const char* p) const { return color == p[0] && type == p[1]; }
  void set(const Color& c, const char& t) {
    color = c;
    type = t;
  }
  void Empty() {
    color = empty;
    type = ' ';
  }
  Piece(const Color& c, const char& t) : color(c), type(t){};
};
struct Point {
  size_t row;
  size_t column;
  bool Equals(size_t r, size_t c) { return row == r && column == c; }
  Point(size_t r, size_t c) : row(r), column(c) {}
  //void clear() { row = column = -1;
  //}
  void Set(size_t r, size_t c) {
    row = r;
    column = c;
  };
  bool operator==(const Point& other) const {
    return row == other.row && column == other.column;
  }

  bool operator<(const Point& other) const {
    if (row < other.row) return true;
    if (row > other.row) return false;
    if (column < other.column) return true;
    return false;
  }
  Point(){}
};
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

using Board = std::array<std::array<Piece, 8>, 8>;
struct Kings {
  Point white_king;
  Point black_king;
  Point& operator[](const Color& color) {
    return color == white ? white_king : black_king;
  };
  Kings(Point wk, Point bk) : white_king(wk), black_king(bk){};
};

 //int g_copied_boards = 0;
 //int boards_created = 0;
 //int boards_destroyed = 0;

Board starting_board = {
    {{{Piece(black, 'R'), Piece(black, 'N'), Piece(black, 'B'),
       Piece(black, 'Q'), Piece(black, 'K'), Piece(black, 'B'),
       Piece(black, 'N'), Piece(black, 'R')}},
     {{Piece(black, 'P'), Piece(black, 'P'), Piece(black, 'P'),
       Piece(black, 'P'), Piece(black, 'P'), Piece(black, 'P'),
       Piece(black, 'P'), Piece(black, 'P')}},
     {{Piece(empty, ' '), Piece(empty, ' '), Piece(empty, ' '),
       Piece(empty, ' '), Piece(empty, ' '), Piece(empty, ' '),
       Piece(empty, ' '), Piece(empty, ' ')}},
     {{Piece(empty, ' '), Piece(empty, ' '), Piece(empty, ' '),
       Piece(empty, ' '), Piece(empty, ' '), Piece(empty, ' '),
       Piece(empty, ' '), Piece(empty, ' ')}},
     {{Piece(empty, ' '), Piece(empty, ' '), Piece(empty, ' '),
       Piece(empty, ' '), Piece(empty, ' '), Piece(empty, ' '),
       Piece(empty, ' '), Piece(empty, ' ')}},
     {{Piece(empty, ' '), Piece(empty, ' '), Piece(empty, ' '),
       Piece(empty, ' '), Piece(empty, ' '), Piece(empty, ' '),
       Piece(empty, ' '), Piece(empty, ' ')}},
     {{Piece(white, 'P'), Piece(white, 'P'), Piece(white, 'P'),
       Piece(white, 'P'), Piece(white, 'P'), Piece(white, 'P'),
       Piece(white, 'P'), Piece(white, 'P')}},
     {{Piece(white, 'R'), Piece(white, 'N'), Piece(white, 'B'),
       Piece(white, 'Q'), Piece(white, 'K'), Piece(white, 'B'),
       Piece(white, 'N'), Piece(white, 'R')}}}};

struct IntPoint {
  int row;
  int column;
  bool Equals(const int& r, const int& c) const {
    return row == r && column == c;
  }
  IntPoint(int r, int c) : row(r), column(c) {}
  void clear() { row = column = -1; }
  void Set(int r, int c) {
    row = r;
    column = c;
  };
  bool operator==(const IntPoint& other) const {
    return row == other.row && column == other.column;
  }

  bool operator<(const IntPoint& other) const {
    if (row < other.row) return true;
    if (row > other.row) return false;
    if (column < other.column) return true;
    return false;
  }
};


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
  IntPoint en_passant_target;
  size_t fifty_move_rule;



  Position(bool wtm, int n, double e, Board b,
           Castling c /*, time_t t*/,
           std::vector<Position*>* o,
           Position* pm, int d, /*Position* bm,*/Kings k, IntPoint ept, size_t fmc)
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
    return new Position(true, 0, 0.0, starting_board, Castling(true, true, true, true), nullptr, nullptr, 0, Kings(Point(7, 4), Point(0, 4)), IntPoint(-1, -1), 0);
  }

  Position GenerateMovesCopy() {
    return Position(!white_to_move, number + 1, evaluation, board, castling, nullptr /* outcomes */,
                    this /* previous_move */, -1 /*depth*/, /*
                    nullptr /* best_move */
                    /*, */ kings, IntPoint(-1, -1), fifty_move_rule + 1);
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
  std::map<IntPoint, std::set<Point>> pinned;
  std::set<Point> block;
};


void GetCheckInfo(Check& output, Position& position, Color side = empty) {
  if (side == empty) {
    side = position.white_to_move ? white : black;
  }
  int new_i, new_j;
  /*if (!king_present(position)) {
    int i=1;
  }*/
  Color opponent;
  IntPoint found(-1, -1);
  std::set<Point> done;
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
  size_t i = (size_t)position.kings[side].row;
  size_t j = (size_t)position.kings[side].column;
  // pawn check
  if (side == white) {
    opponent = black;
    new_i = (int)i - 1;
  } else {
    opponent = white;
    new_i = (int)i + 1;
  }
  // right
  new_j = (int)j + 1;
  if (new_i >= 0 && new_i <= 7 && j < 7 && position.board[(size_t)new_i][j + 1].color == opponent &&
        position.board[(size_t)new_i][j + 1].type == 'P') {
    output.in_check = true;
    output.block.insert(Point((size_t)new_i, (size_t)new_j));
  }
  // left
  new_j = (int)j - 1;
  if (new_i >= 0 && new_i <= 7 && j > 0 && position.board[(size_t)new_i][(size_t)new_j].color == opponent &&
      position.board[(size_t)new_i][(size_t)new_j].type == 'P') {
    if (output.in_check) {
      output.king_must_move = true;
    } else {
      output.in_check = true;
      output.block.insert(Point((size_t)new_i, (size_t)new_j));
    }
  }

  // knight check
  for (size_t k = 0; k < 8; k++) {
    new_i = (int)i + knight_moves[k][0];
    if (new_i >= 0 && new_i <= 7) {
      new_j = (int)j + knight_moves[k][1];
      if (new_j >= 0 && new_j <= 7 &&
          position.board[(size_t)new_i][(size_t)new_j].color == opponent &&
          position.board[(size_t)new_i][(size_t)new_j].type == 'N') {
        if (output.in_check) {
          output.king_must_move = true;
        } else {
          output.in_check = true;
          output.block.insert(Point((size_t)new_i, (size_t)new_j));
        }
      }
    }
  }
  // bishop check
  for (size_t k = 0; k < 4; k++) {
    found.row = -1;
    found.column = -1;
    done.clear();
    new_i = (int)i + bishop_moves[k][0];
    new_j = (int)j + bishop_moves[k][1];

    while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
      done.insert(Point((size_t)new_i, (size_t)new_j));
      if (position.board[(size_t)new_i][(size_t)new_j].color == side) {
        if (found.row != -1) {
          break;
        }
        found.row = new_i;
        found.column = new_j;

      } else if (position.board[(size_t)new_i][(size_t)new_j].color == opponent) {
        if (position.board[(size_t)new_i][(size_t)new_j].type == 'B' ||
            position.board[(size_t)new_i][(size_t)new_j].type == 'Q') {
          if (found.row != -1) {
            //if (position.board[found.row][found.column].type == 'B' ||
            //    position.board[found.row][found.column].type == 'Q' /*need to account for pawns too*/) {
              // (position.board[(size_t)new_i][(size_t)new_j].type != 'Q' ? (position.board[found.row][found.column].type != position.board[(size_t)new_i][(size_t)new_j].type || (position.board[found.row][found.column].type == 'Q' && (position.board[(size_t)new_i][(size_t)new_j].type == 'B' || position.board[(size_t)new_i][(size_t)new_j].type == 'R'))) : (position.board[found.row][found.column].type == 'B' || position.board[found.row][found.column].type == 'R')
              output.pinned[found] = done;
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
      new_i += bishop_moves[k][0];
      new_j += bishop_moves[k][1];
    }
  }
  // rook check


  for (size_t k = 0; k < 4; k++) {
    found.row = -1;
    found.column = -1;
    done.clear();
    new_i = (int)i + rook_moves[k][0];
    new_j = (int)j + rook_moves[k][1];

    while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
      done.insert(Point((size_t)new_i, (size_t)new_j));
      if (position.board[(size_t)new_i][(size_t)new_j].color ==
          position.board[i][j].color) {
        if (found.row != -1) {
          break;
        }
        found.row = new_i;
        found.column = new_j;

      } else if (position.board[(size_t)new_i][(size_t)new_j].color ==
                 opponent) {
        if (position.board[(size_t)new_i][(size_t)new_j].type == 'R' ||
            position.board[(size_t)new_i][(size_t)new_j].type == 'Q') {
          if (found.row != -1) {
            if (position.board[(size_t)found.row][(size_t)found.column].type ==
                    'R' ||
                position.board[(size_t)found.row][(size_t)found.column].type ==
                    'Q' ||
                position.board[(size_t)found.row][(size_t)found.column].type ==
                    'P') {
              // (position.board[(size_t)new_i][(size_t)new_j].type != 'Q' ?
              // (position.board[found.row][found.column].type !=
              // position.board[(size_t)new_i][(size_t)new_j].type ||
              // (position.board[found.row][found.column].type == 'Q' &&
              // (position.board[(size_t)new_i][(size_t)new_j].type == 'B' ||
              // position.board[(size_t)new_i][(size_t)new_j].type == 'R'))) :
              // (position.board[found.row][found.column].type == 'B' ||
              // position.board[found.row][found.column].type == 'R')
              output.pinned[found] = done;
            } else {
              output.pinned[found];
            }
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
      new_i += rook_moves[k][0];
      new_j += rook_moves[k][1];
    }
  }
}

bool MoveOk(Check check, size_t i, size_t j, size_t new_i, size_t new_j) {
  IntPoint origin = IntPoint((int)i, (int)j);
  Point dest = Point(new_i, new_j);
  return ((check.in_check ? check.block.count(dest)
          : true) && (check.pinned.contains(origin)
              ? check.pinned[origin].contains(dest)
              : true));
}
bool InCheck(Position& position, const Color& side, int ki = -1, int kj = -1) {
  int new_i, new_j;
  /*if (!king_present(position)) {
    int i=1;
  }*/
  Color opponent = side == white ? black : white;
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
  size_t i = ki == -1 ? position.kings[side].row : (size_t)ki;
  size_t j = kj == -1 ? position.kings[side].column : (size_t)kj;
  // pawn check
  new_i = (int)i + (side == white ? -1 : 1);
  if (new_i >= 0 && new_i <= 7 &&
      ((j < 7 && position.board[(size_t)new_i][j + 1].color == opponent &&
        position.board[(size_t)new_i][j + 1].type == 'P') ||
       (j > 0 && position.board[(size_t)new_i][j - 1].color == opponent &&
        position.board[(size_t)new_i][j - 1].type == 'P'))) {
    return true;
  }

  // knight check
  for (size_t k = 0; k < 8; k++) {
    new_i = (int)i + knight_moves[k][0];
    if (new_i >= 0 && new_i <= 7) {
      new_j = (int)j + knight_moves[k][1];
      if (new_j >= 0 && new_j <= 7 &&
          position.board[(size_t)new_i][(size_t)new_j].color == opponent &&
          position.board[(size_t)new_i][(size_t)new_j].type == 'N') {
        return true;
      }
    }
  }
  // bishop check
  for (size_t k = 0; k < 4; k++) {
    new_i = (int)i + bishop_moves[k][0];
    new_j = (int)j + bishop_moves[k][1];

    while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
      if (position.board[(size_t)new_i][(size_t)new_j].color == side &&
          position.board[(size_t)new_i][(size_t)new_j].type != 'K') {
        break;
      }

      if (position.board[(size_t)new_i][(size_t)new_j].color == opponent) {
        if (position.board[(size_t)new_i][(size_t)new_j].type == 'B' ||
            position.board[(size_t)new_i][(size_t)new_j].type == 'Q') {
          return true;
        }
        break;
      }
      new_i += bishop_moves[k][0];
      new_j += bishop_moves[k][1];
    }
  }
  // rook check
  // left
  for (new_j = (int)j - 1; new_j >= 0; new_j--) {
    if (position.board[i][(size_t)new_j].color == opponent) {
      if ((position.board[i][(size_t)new_j].type == 'R' ||
           position.board[i][(size_t)new_j].type == 'Q')) {
        return true;
      }
      break;
    }
    if (position.board[i][(size_t)new_j].color == side &&
        position.board[i][(size_t)new_j].type != 'K') {
      break;
    }
  }

  // up
  for (new_i = (int)i - 1; new_i >= 0; new_i--) {
    if (position.board[(size_t)new_i][j].color == opponent) {
      if ((position.board[(size_t)new_i][j].type == 'R' ||
           position.board[(size_t)new_i][j].type == 'Q')) {
        return true;
      }
      break;
    }
    if (position.board[(size_t)new_i][j].color == side &&
        position.board[(size_t)new_i][j].type != 'K') {
      break;
    }
  }

  // right
  for (new_j = (int)j + 1; new_j <= 7; new_j++) {
    if (position.board[i][(size_t)new_j].color == opponent) {
      if ((position.board[i][(size_t)new_j].type == 'R' ||
           position.board[i][(size_t)new_j].type == 'Q')) {
        return true;
      }
      break;
    }
    if (position.board[i][(size_t)new_j].color == side &&
        position.board[i][(size_t)new_j].type != 'K') {
      break;
    }
  }

  // down
  for (new_i = (int)i + 1; new_i <= 7; new_i++) {
    if (position.board[(size_t)new_i][j].color == opponent) {
      if ((position.board[(size_t)new_i][j].type == 'R' ||
           position.board[(size_t)new_i][j].type == 'Q')) {
        return true;
      }
      break;
    }
    if (position.board[(size_t)new_i][j].color == side &&
        position.board[(size_t)new_i][j].type != 'K') {
      break;
    }
  }
  // king check
  for (size_t k = 0; k < 8; k++) {
    new_i = (int)i + king_moves[k][0];
    new_j = (int)j + king_moves[k][1];
    if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
        position.board[(size_t)new_i][(size_t)new_j].type == 'K' &&
        position.board[(size_t)new_i][(size_t)new_j].color == opponent) {
      return true;
    }

  }
  return false;
}


const int king_value = 1000;


double EvaluateMobility(Position& position) {
  Color opponent;
  Check white_check;
  Check black_check;
  GetCheckInfo(white_check, position);
  GetCheckInfo(black_check, position);
  int new_i, new_j;
  double mobility_score = 0;
  Check* check;

  for (size_t i = 0; i <= 7; i++) {
    for (size_t j = 0; j <= 7; j++) {
      if (position.board[i][j].color == empty) {
        continue;
      }
      double moves = 0;
      if (position.board[i][j].type == 'K') {
        for (size_t k = 0; k < 8; k++) {
          new_i = (int)i + king_moves[k][0];
          new_j = (int)j + king_moves[k][1];
          if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 && !InCheck(position, position.board[i][j].color, new_i, new_j)) {
            moves++;
          }
        }
        mobility_score += (double)moves / (double)king_value;
        continue;
      }
      if (position.board[i][j].color == white) {
        check = &white_check;
      } else {
        check = &black_check;
      }
      // counting moves
      //int addition = position.board[i][j].color == 'W' ? 1 : -1;
      opponent = position.board[i][j].color == white ? black : white;
      switch (position.board[i][j].type) {
        case 'P': {
          if (check->king_must_move) {
            break;
          }
          int multiplier;
          size_t promotion_row, starting_row, jump_row;
          if (position.board[i][j].color == white) {
            multiplier = -1;
            starting_row = 6;
            jump_row = 4;
            promotion_row = 1;
          } else {
            multiplier = 1;
            starting_row = 1;
            jump_row = 3;
            promotion_row = 6;
          }
          bool left_capture =
              j > 0 && position.board[i + multiplier][j - 1].color == opponent;
          bool right_capture =
              j < 7 && position.board[i + multiplier][j + 1].color == opponent;
          new_i = (int)i + multiplier;
          if (i == promotion_row) {
            // promotion
            // one square

            if (position.board[(size_t)new_i][j].color == empty) {
              if (MoveOk(*check, i, j, (size_t)new_i, j)) {
                moves += 2;
              }
            }
            // captures to the left
            if (left_capture && MoveOk(*check, i, j, (size_t)new_i, j - 1)) {
              moves += 4;
            }
            // captures to the right
            if (right_capture && MoveOk(*check, i, j, (size_t)new_i, j + 1)) {
              moves += 4;
            }
          } else if (position.board[(size_t)new_i][j].color == empty) {
            // one square forward
            if (MoveOk(*check, i, j, (size_t)new_i, j)) {
              moves += 0.5;
            }
            // two squares forward
            if (i == starting_row &&
                position.board[i + multiplier][j].color == empty && 
                position.board[jump_row][j].color == empty &&
                MoveOk(*check, i, j, jump_row, j)) {
              moves += 0.5;
            }
          } else {
            if (left_capture && MoveOk(*check, i, j, (size_t)new_i, j - 1)) {
              moves++;
            }
            // captures to the right
            if (left_capture && MoveOk(*check, i, j, (size_t)new_i, j + 1)) {
              moves++;
            }
            // en passant to the left
            size_t en_passant_start = jump_row + multiplier;
            if (i == en_passant_start && j > 0 &&
                position.board[i][j - 1].color == opponent &&
                position.board[i][j - 1].type == 'P' &&
                position.en_passant_target.Equals(new_i, (int)j - 1) &&
                MoveOk(*check, i, j, (size_t)new_i, j - 1)) {
              int di = (int)position.kings[position.board[i][j].color].row - (int)i;
              int dj =
                  (int)position.kings[position.board[i][j].color].column - (int)j - 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                      }
                      break;
                    }
                    iter_j++;
                  }
                }
              }
              if (good && std::abs(di) == std::abs(dj)) {
                di = di > 0 ? 1 : -1;
                dj = dj > 0 ? 1 : -1;
                int iter_i = (int)i;
                int iter_j = (int)j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[(size_t)iter_i][(size_t)iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[(size_t)iter_i][(size_t)iter_j].color == opponent) {
                    if (position.board[(size_t)iter_i][(size_t)iter_j].type == 'B' ||
                        position.board[(size_t)iter_i][(size_t)iter_j].type == 'Q') {
                      good = false;
                    }
                    break;
                  }
                  iter_i += di;
                  iter_j += dj;
                }
              }
              if (good) {
                moves++;
              }
            }
            // en passant to the right
            if (i == en_passant_start && j < 7 &&
                position.board[i][j + 1].color == opponent &&
                position.board[i][j + 1].type == 'P' &&
                position.en_passant_target.Equals(new_i, (int)j + 1) &&
                MoveOk(*check, i, j, (size_t)new_i, j + 1)) {
              int di = (int)position.kings[position.board[i][j].color].row - (int)i;
              int dj = (int)position.kings[position.board[i][j].color].column -
                       (int)j + 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                      }
                      break;
                    }
                    iter_j++;
                  }
                }
              }
              if (good && std::abs(di) == std::abs(dj)) {
                di = di > 0 ? 1 : -1;
                dj = dj > 0 ? 1 : -1;
                int iter_i = (int)i;
                int iter_j = (int)j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[(size_t)iter_i][(size_t)iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[(size_t)iter_i][(size_t)iter_j].color == opponent) {
                    if (position.board[(size_t)iter_i][(size_t)iter_j].type == 'B' ||
                        position.board[(size_t)iter_i][(size_t)iter_j].type == 'Q') {
                      good = false;
                    }
                    break;
                  }
                  iter_i += di;
                  iter_j += dj;
                }
              }
              if (good) {
                moves++;
              }
            }
          }
          break;
        }
        case 'N':
          if (check->king_must_move) {
            break;
          }
          for (size_t k = 0; k < 8; k++) {
            new_i = (int)i + knight_moves[k][0];
            new_j = (int)j + knight_moves[k][1];
            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
                position.board[(size_t)new_i][(size_t)new_j].color !=
                    position.board[i][j].color) {
              if (MoveOk(*check, i, j, (size_t)new_i, (size_t)new_j)) {
                moves++;
              }
            }
          }
          break;
        case 'Q':
        case 'B':
          if (check->king_must_move) {
            break;
          }
          for (size_t k = 0; k < 4; k++) {
            new_i = (int)i + bishop_moves[k][0];
            new_j = (int)j + bishop_moves[k][1];

            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[(size_t)new_i][(size_t)new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(*check, i, j, (size_t)new_i, (size_t)new_j)) {
                moves++;
              }
              if (position.board[(size_t)new_i][(size_t)new_j].color == opponent) {
                break;
              }
              new_i += bishop_moves[k][0];
              new_j += bishop_moves[k][1];
            }
          }
          if (position.board[i][j].type == 'B') {
            break;
          }
        case 'R':
          if (check->king_must_move) {
            break;
          }
          for (size_t k = 0; k < 4; k++) {
            new_i = (int)i + rook_moves[k][0];
            new_j = (int)j + rook_moves[k][1];
            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[(size_t)new_i][(size_t)new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(*check, i, j, (size_t)new_i, (size_t)new_j)) {
                moves++;
              }
              if (position.board[(size_t)new_i][(size_t)new_j].color == opponent) {
                break;
              }
              new_i = new_i + rook_moves[k][0];
              new_j = new_j + rook_moves[k][1];
            }
          }
          break;

      }

      if (position.board[i][j].color == white) {
        mobility_score +=
            ((double)moves / piece_values[position.board[i][j].type]);
      } else {
        mobility_score -=
            ((double)moves / piece_values[position.board[i][j].type]);
      }
    }
  }

  return mobility_score / 1000.0;
}


int CountMoves(Position& position) {
  Color opponent;
  Check white_check;
  Check black_check;
  GetCheckInfo(white_check, position);
  GetCheckInfo(black_check, position);
  int new_i, new_j;
  int moves = 0;
  Check* check;

  for (size_t i = 0; i <= 7; i++) {
    for (size_t j = 0; j <= 7; j++) {
      if (position.board[i][j].color == empty) {
        continue;
      }
      if (position.board[i][j].type == 'K') {
        for (size_t k = 0; k < 8; k++) {
          new_i = (int)i + king_moves[k][0];
          new_j = (int)j + king_moves[k][1];
          if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
              !InCheck(position, position.board[i][j].color, new_i, new_j)) {
            moves++;
          }
        }
        continue;
      }
      if (position.board[i][j].color == white) {
        check = &white_check;
      } else {
        check = &black_check;
      }
      // counting moves
      // int addition = position.board[i][j].color == 'W' ? 1 : -1;
      opponent = position.board[i][j].color == white ? black : white;
      switch (position.board[i][j].type) {
        case 'P': {
          if (check->king_must_move) {
            break;
          }
          int multiplier;
          size_t promotion_row, starting_row, jump_row;
          if (position.board[i][j].color == white) {
            multiplier = -1;
            starting_row = 6;
            jump_row = 4;
            promotion_row = 1;
          } else {
            multiplier = 1;
            starting_row = 1;
            jump_row = 3;
            promotion_row = 6;
          }
          bool left_capture =
              j > 0 && position.board[i + multiplier][j - 1].color == opponent;
          bool right_capture =
              j < 7 && position.board[i + multiplier][j + 1].color == opponent;
          new_i = (int)i + multiplier;
          if (i == promotion_row) {
            // promotion
            // one square

            if (position.board[(size_t)new_i][j].color == empty) {
              if (MoveOk(*check, i, j, (size_t)new_i, j)) {
                moves += 4;
              }
            }
            // captures to the left
            if (left_capture && MoveOk(*check, i, j, (size_t)new_i, j - 1)) {
              moves += 4;
            }
            // captures to the right
            if (right_capture && MoveOk(*check, i, j, (size_t)new_i, j + 1)) {
              moves += 4;
            }
          } else if (position.board[(size_t)new_i][j].color == empty) {
            // one square forward
            if (MoveOk(*check, i, j, (size_t)new_i, j)) {
              moves++;
            }
            // two squares forward
            if (i == starting_row &&
                position.board[i + multiplier][j].color == empty &&
                position.board[jump_row][j].color == empty &&
                MoveOk(*check, i, j, jump_row, j)) {
              moves++;
            }
          } else {
            if (left_capture && MoveOk(*check, i, j, (size_t)new_i, j - 1)) {
              moves++;
            }
            // captures to the right
            if (left_capture && MoveOk(*check, i, j, (size_t)new_i, j + 1)) {
              moves++;
            }
            // en passant to the left
            size_t en_passant_start = jump_row + multiplier;
            if (i == en_passant_start && j > 0 &&
                position.board[i][j - 1].color == opponent &&
                position.board[i][j - 1].type == 'P' &&
                position.en_passant_target.Equals(new_i, (int)j - 1) &&
                MoveOk(*check, i, j, (size_t)new_i, j - 1)) {
              int di =
                  (int)position.kings[position.board[i][j].color].row - (int)i;
              int dj = (int)position.kings[position.board[i][j].color].column -
                       (int)j - 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                      }
                      break;
                    }
                    iter_j++;
                  }
                }
              }
              if (good && std::abs(di) == std::abs(dj)) {
                di = di > 0 ? 1 : -1;
                dj = dj > 0 ? 1 : -1;
                int iter_i = (int)i;
                int iter_j = (int)j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[(size_t)iter_i][(size_t)iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[(size_t)iter_i][(size_t)iter_j]
                                 .color == opponent) {
                    if (position.board[(size_t)iter_i][(size_t)iter_j].type ==
                            'B' ||
                        position.board[(size_t)iter_i][(size_t)iter_j].type ==
                            'Q') {
                      good = false;
                    }
                    break;
                  }
                  iter_i += di;
                  iter_j += dj;
                }
              }
              if (good) {
                moves++;
              }
            }
            // en passant to the right
            if (i == en_passant_start && j < 7 &&
                position.board[i][j + 1].color == opponent &&
                position.board[i][j + 1].type == 'P' &&
                position.en_passant_target.Equals(new_i, (int)j + 1) &&
                MoveOk(*check, i, j, (size_t)new_i, j + 1)) {
              int di =
                  (int)position.kings[position.board[i][j].color].row - (int)i;
              int dj = (int)position.kings[position.board[i][j].color].column -
                       (int)j + 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                      }
                      break;
                    }
                    iter_j++;
                  }
                }
              }
              if (good && std::abs(di) == std::abs(dj)) {
                di = di > 0 ? 1 : -1;
                dj = dj > 0 ? 1 : -1;
                int iter_i = (int)i;
                int iter_j = (int)j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[(size_t)iter_i][(size_t)iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[(size_t)iter_i][(size_t)iter_j]
                                 .color == opponent) {
                    if (position.board[(size_t)iter_i][(size_t)iter_j].type ==
                            'B' ||
                        position.board[(size_t)iter_i][(size_t)iter_j].type ==
                            'Q') {
                      good = false;
                    }
                    break;
                  }
                  iter_i += di;
                  iter_j += dj;
                }
              }
              if (good) {
                moves++;
              }
            }
          }
          break;
        }
        case 'N':
          if (check->king_must_move) {
            break;
          }
          for (size_t k = 0; k < 8; k++) {
            new_i = (int)i + knight_moves[k][0];
            new_j = (int)j + knight_moves[k][1];
            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
                position.board[(size_t)new_i][(size_t)new_j].color !=
                    position.board[i][j].color) {
              if (MoveOk(*check, i, j, (size_t)new_i, (size_t)new_j)) {
                moves++;
              }
            }
          }
          break;
        case 'Q':
        case 'B':
          if (check->king_must_move) {
            break;
          }
          for (size_t k = 0; k < 4; k++) {
            new_i = (int)i + bishop_moves[k][0];
            new_j = (int)j + bishop_moves[k][1];

            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[(size_t)new_i][(size_t)new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(*check, i, j, (size_t)new_i, (size_t)new_j)) {
                moves++;
              }
              if (position.board[(size_t)new_i][(size_t)new_j].color ==
                  opponent) {
                break;
              }
              new_i += bishop_moves[k][0];
              new_j += bishop_moves[k][1];
            }
          }
          if (position.board[i][j].type == 'B') {
            break;
          }
        case 'R':
          if (check->king_must_move) {
            break;
          }
          for (size_t k = 0; k < 4; k++) {
            new_i = (int)i + rook_moves[k][0];
            new_j = (int)j + rook_moves[k][1];
            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[(size_t)new_i][(size_t)new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(*check, i, j, (size_t)new_i, (size_t)new_j)) {
                moves++;
              }
              if (position.board[(size_t)new_i][(size_t)new_j].color ==
                  opponent) {
                break;
              }
              new_i = new_i + rook_moves[k][0];
              new_j = new_j + rook_moves[k][1];
            }
          }
          break;
      }
    }
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


int char_to_int(char c) { return (int)c - int('0'); }

size_t char_to_size_t(char c) { return ((size_t)c - (size_t)'0'); }

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
  //base_position.previous_move = &position;
  Color opponent = position.white_to_move ? black : white;
  Position* new_position;
  if (position.outcomes == nullptr) {
    position.outcomes = new std::vector<Position*>;
  }
  Check check_info;
  GetCheckInfo(check_info, position);

  int new_i, new_j;
  for (size_t i = 0; i < 8; i++) {
    for (size_t j = 0; j < 8; j++) {
      if (position.board[i][j].color != (position.white_to_move ? white : black)) {
        continue;
      }
      int evaluation_multiplier = position.board[i][j].color == white ? 1 : -1;
      switch (position.board[i][j].type) {
        case 'P': {
          if (check_info.king_must_move) {
            break;
          }
          int multiplier;
          size_t promotion_row, starting_row, jump_row;
          if (position.board[i][j].color == white) {
            multiplier = -1;
            starting_row = 6;
            jump_row = 4;
            promotion_row = 1;
          } else {
            multiplier = 1;
            starting_row = 1;
            jump_row = 3;
            promotion_row = 6;
          }
          bool left_capture =
              j > 0 && position.board[i + multiplier][j - 1].color == opponent;
          bool right_capture =
              j < 7 && position.board[i + multiplier][j + 1].color == opponent;
          new_i = (int)i + multiplier;
          if (i == promotion_row) {
            // promotion
            for (size_t k = 0; k < 4; k++) {
              // one square
              if (position.board[(size_t)new_i][j].color == empty &&
                  MoveOk(check_info, i, j, (size_t)new_i, j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[i + multiplier][j].color =
                    new_position->board[i][j].color;
                new_position->board[(size_t)new_i][j].type =
                    promotion_pieces[k];
                new_position->board[i][j].Empty();
                new_position->evaluation +=
                    piece_values[promotion_pieces[k]] - 1;
                new_position->fifty_move_rule = 0;
                position.outcomes->emplace_back(new_position);
              }
              // captures to the left
              if (left_capture && MoveOk(check_info, i, j, (size_t)new_i, j - 1)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[i + multiplier][j - 1].color =
                    new_position->board[i][j].color;
                new_position->board[i + multiplier][j - 1].type =
                    promotion_pieces[k];
                new_position->board[i][j].Empty();
                new_position->fifty_move_rule = 0;
                new_position->evaluation +=
                    (-
                    multiplier *
                    piece_values[position.board[(size_t)new_i][j - 1].type]) + piece_values[promotion_pieces[k]] - 1;
                position.outcomes->emplace_back(new_position);
              }
              // captures to the right
              if (right_capture && MoveOk(check_info, i, j, (size_t)new_i, j + 1)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[i + multiplier][j + 1].color =
                    new_position->board[i][j].color;
                new_position->board[(size_t)new_i][j + 1].type =
                    promotion_pieces[k];
                new_position->board[i][j].Empty();
                new_position->fifty_move_rule = 0;
                new_position->evaluation +=
                    (-multiplier *
                        piece_values[position.board[(size_t)new_i][j + 1].type]) +
                    piece_values[promotion_pieces[k]] - 1;
                position.outcomes->emplace_back(new_position);
              }
            }
          } else {
            if (position.board[(size_t)new_i][j].color == empty) {
              // one square forward
              if (MoveOk(check_info, i, j, (size_t)new_i, j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[(size_t)new_i][j] = new_position->board[i][j];
                new_position->board[i][j].Empty();
                new_position->fifty_move_rule = 0;
                position.outcomes->emplace_back(new_position);
              }
              // two squares forward
              if (i == starting_row &&
                  position.board[jump_row][j].color == empty &&
                  MoveOk(check_info, i, j, jump_row, j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[jump_row][j] =
                    new_position->board[i][j];
                new_position->fifty_move_rule = 0;
                new_position->board[i][j].Empty();
                new_position->en_passant_target.Set((int)i + multiplier, (int)j);
                position.outcomes->emplace_back(new_position);
              }
            }
            // captures to the left
            if (left_capture && MoveOk(check_info, i, j, (size_t)new_i, j - 1)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i][j - 1] =
                  new_position->board[i][j];
              new_position->board[i][j].Empty();
              new_position->evaluation +=
                  (-multiplier *
                   piece_values[position.board[(size_t)new_i][j - 1].type]);
              new_position->fifty_move_rule = 0;

              position.outcomes->emplace_back(new_position);
            }
            // captures to the right
            if (j < 7 &&
                position.board[(size_t)new_i][j + 1].color == opponent &&
                MoveOk(check_info, i, j, (size_t)new_i, j + 1)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[i + multiplier][j + 1] =
                  new_position->board[i][j];
              new_position->board[i][j].Empty();
              new_position->evaluation +=
                  (-multiplier *
                   piece_values[position.board[(size_t)new_i][j + 1].type]);
              new_position->fifty_move_rule = 0;
              position.outcomes->emplace_back(new_position);
            }
            // en passant to the left
            size_t en_passant_start = position.white_to_move ? (size_t)3 : (size_t)4;
            if (i == en_passant_start && j > 0 &&
                position.board[i][j - 1].color == opponent &&
                position.board[i][j - 1].type == 'P' &&
                position.en_passant_target.Equals((int)i + multiplier,
                                                  (int)j - 1) &&
                MoveOk(check_info, i, j, (size_t)new_i, j - 1)) {
              int di = (int)position.kings[position.board[i][j].color].row - (int)i;
              int dj = (int)position.kings[position.board[i][j].color].column -
                       (int)j - 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  size_t iter_j =
                      position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                      }
                      break;
                    }
                    iter_j++;
                  }
                }
              }
              if (good && std::abs(di) == std::abs(dj)) {
                di = di > 0 ? 1 : -1;
                dj = dj > 0 ? 1 : -1;
                int iter_i = (int)i;
                int iter_j = (int)j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[(size_t)iter_i][(size_t)iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[(size_t)iter_i][(size_t)iter_j].color == opponent) {
                    if (position.board[(size_t)iter_i][(size_t)iter_j].type ==
                            'B' ||
                        position.board[(size_t)iter_i][(size_t)iter_j].type ==
                            'Q') {
                      good = false;
                    }
                    break;
                  }
                  iter_i += di;
                  iter_j += dj;
                }
              }
              if (good) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[(size_t)new_i][j - 1] =
                    new_position->board[i][j];
                new_position->board[i][j].Empty();
                new_position->board[i][j - 1].Empty();
                new_position->evaluation -= multiplier;
                new_position->fifty_move_rule = 0;
                position.outcomes->emplace_back(new_position);
              }
            }
            // en passant to the right
            if (i == en_passant_start && j < 7 &&
                position.board[i][j + 1].color == opponent &&
                position.board[i][j + 1].type == 'P' &&
                position.en_passant_target.Equals(new_i, (int)j + 1) &&
                MoveOk(check_info, i, j, (size_t)new_i, j + 1)) {
              int di = (int)position.kings[position.board[i][j].color].row - (int)i;
              int dj = (int)position.kings[position.board[i][j].color].column -
                       (int)j + 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j =
                      (int)position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][(size_t)iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][(size_t)iter_j].color == opponent) {
                      if (position.board[i][(size_t)iter_j].type == 'R' ||
                          position.board[i][(size_t)iter_j].type == 'Q') {
                        good = false;
                      }
                      break;
                    }
                    iter_j++;
                  }
                }
              }
              if (good && std::abs(di) == std::abs(dj)) {
                di = di > 0 ? 1 : -1;
                dj = dj > 0 ? 1 : -1;
                int iter_i = (int)i;
                int iter_j = (int)j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[(size_t)iter_i][(size_t)iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[(size_t)iter_i][(size_t)iter_j]
                                 .color ==
                             opponent) {
                    if (position.board[(size_t)iter_i][(size_t)iter_j].type ==
                            'B' ||
                        position.board[(size_t)iter_i][(size_t)iter_j].type ==
                            'Q') {
                      good = false;
                    }
                    break;
                  }
                  iter_i += di;
                  iter_j += dj;
                }
              }
              if (good) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[i + multiplier][j + 1] =
                    new_position->board[i][j];
                new_position->board[i][j].Empty();
                new_position->board[i][j + 1].Empty();
                new_position->evaluation -= multiplier;
                new_position->fifty_move_rule = 0;
                position.outcomes->emplace_back(new_position);
              }
            }
          }
          break;
        }
        case 'N':
          if (check_info.king_must_move) {
            break;
          }
          for (size_t k = 0; k < 8; k++) {
            new_i = (int)i + knight_moves[k][0];
            new_j = (int)j + knight_moves[k][1];
            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
                position.board[(size_t)new_i][(size_t)new_j].color !=
                    position.board[i][j].color &&
                MoveOk(check_info, i, j, (size_t)new_i, (size_t)new_j)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i][(size_t)new_j] = position.board[i][j];
              new_position->board[i][j].Empty();
              if (!position.board[(size_t)new_i][(size_t)new_j].IsEmpty()) {
                new_position->evaluation +=
                    (evaluation_multiplier *
                     piece_values[position.board[(size_t)new_i][(size_t)new_j]
                                      .type]);
                new_position->fifty_move_rule = 0;
              }
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
            new_i = (int)i + bishop_moves[k][0];
            new_j = (int)j + bishop_moves[k][1];

            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[(size_t)new_i][(size_t)new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(check_info, i, j, (size_t)new_i, (size_t)new_j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[(size_t)new_i][(size_t)new_j] = position.board[i][j];
                new_position->board[i][j].Empty();
                if (!position.board[(size_t)new_i][(size_t)new_j]
                                     .IsEmpty()) {
                  new_position->fifty_move_rule = 0;
                  new_position->evaluation +=
                      (evaluation_multiplier *
                       piece_values[position.board[(size_t)new_i][(size_t)new_j]
                                        .type]);
                }
                position.outcomes->emplace_back(new_position);
              }
              if (position.board[(size_t)new_i][(size_t)new_j].color == opponent) {
                break;
              }
              new_i += bishop_moves[k][0];
              new_j += bishop_moves[k][1];
            }
          }
          if (position.board[i][j].type == 'B') {
            break;
          }
        case 'R':
          if (check_info.king_must_move) {
            break;
          }
          for (size_t k = 0; k < 4; k++) {
            new_i = (int)i + rook_moves[k][0];
            new_j = (int)j + rook_moves[k][1];
            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[(size_t)new_i][(size_t)new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(check_info, i, j, (size_t)new_i, (size_t)new_j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[(size_t)new_i][(size_t)new_j] = position.board[i][j];
                new_position->board[i][j].Empty();
                  if (position.board[7][7] == "WR" &&
                      new_position->board[7][7] == "  ") {
                    new_position->castling.white_o_o = false;
                  } else if (position.board[7][0] == "WR" &&
                             new_position->board[7][0] == "  ") {
                    new_position->castling.white_o_o_o = false;
                  } else if (position.board[0][7] == "BR" &&
                             new_position->board[0][7] == "  ") {
                    new_position->castling.black_o_o = false;
                  } else if (position.board[0][0] == "WR" &&
                             new_position->board[0][0] == "  ") {
                    new_position->castling.black_o_o_o = false;
                  }
                  if (!position.board[(size_t)new_i][(size_t)new_j].IsEmpty()) {
                    new_position->fifty_move_rule = 0;
                    new_position->evaluation +=
                        (evaluation_multiplier *
                         piece_values[position.board[(size_t)new_i][(size_t)new_j].type]);
                  }
                  position.outcomes->emplace_back(new_position);
              }
              if (position.board[(size_t)new_i][(size_t)new_j].color == opponent) {
                break;
              }
              new_i += rook_moves[k][0];
              new_j += rook_moves[k][1];
            }
          }
          break;
        case 'K':
          for (size_t k = 0; k < 8; k++) {
            new_i = (int)i + king_moves[k][0];
            new_j = (int)j + king_moves[k][1];
            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
                position.board[(size_t)new_i][(size_t)new_j].color !=
                    position.board[i][j].color &&
                !InCheck(position, position.board[i][j].color, new_i, new_j)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[(size_t)new_i][(size_t)new_j] = position.board[i][j];
              new_position->board[i][j].Empty();
              new_position->kings[position.board[i][j].color] = {(size_t)new_i,
                                                       (size_t)new_j};
              //if (!InCheck(*new_position, position.to_move)) {
                if (position.white_to_move) {
                  new_position->castling.white_o_o =
                      new_position->castling.white_o_o_o = false;
                } else {
                  new_position->castling.black_o_o =
                      new_position->castling.black_o_o_o = false;
                }
                if (!position.board[(size_t)new_i][(size_t)new_j].IsEmpty()) {
                  new_position->fifty_move_rule = 0;
                  new_position->evaluation +=
                      (evaluation_multiplier *
                       piece_values[position.board[(size_t)new_i][(size_t)new_j].type]);
                }

                position.outcomes->emplace_back(new_position);
              //} else {
              //  delete new_position;
              //}
            }
          }
          if ((position.white_to_move
                  ? position.castling.white_o_o
                  : position.castling.black_o_o) &&
                        position.board[i][j + 1].color == empty &&
                        position.board[i][j + 2].color == empty &&
                        !check_info.in_check &&
              !InCheck(position, (position.white_to_move ? white : black), (int)i, (int)j + 1) &&
              !InCheck(position, position.board[i][j].color, (int)i, (int)j + 2)) {
            new_position = base_position.CreateDeepCopy();
            new_position->board[i][6].set(position.board[i][j].color, 'K');
            new_position->board[i][5].set(position.board[i][j].color, 'R');
            new_position->kings[position.board[i][j].color] = {i, 6};
            new_position->board[i][4].Empty();
            new_position->board[i][7].Empty();
            if (position.white_to_move) {
              new_position->castling.white_o_o =
                  new_position->castling.white_o_o_o = false;
            } else {
              new_position->castling.black_o_o =
                  new_position->castling.black_o_o_o = false;
            }
            position.outcomes->emplace_back(new_position);
          }
          if ((position.white_to_move
                  ? position.castling.white_o_o_o
                  : position.castling.black_o_o_o) &&
                        position.board[i][j - 1].color == empty &&
                        position.board[i][j - 2].color == empty &&
                        position.board[i][j - 3].color == empty &&
                        !check_info.in_check &&
              !InCheck(position, position.board[i][j].color, (int)i, (int)j - 1) &&
              !InCheck(position, position.board[i][j].color, (int)i, (int)j - 2)) {
            new_position = base_position.CreateDeepCopy();
            new_position->board[i][2].set(position.board[i][j].color, 'K');
            new_position->board[i][3].set(position.board[i][j].color, 'R');
            new_position->board[i][4].Empty();
            new_position->board[i][0].Empty();
            new_position->kings[position.white_to_move ? white : black] = {i, 2};
            if (position.white_to_move) {
              new_position->castling.white_o_o =
                  new_position->castling.white_o_o_o = false;
            } else {
              new_position->castling.black_o_o =
                  new_position->castling.black_o_o_o = false;
            }
            position.outcomes->emplace_back(new_position);
          }

          break;
      }
    }
  }
  /*if (position.outcomes->size() == 0) {
    delete position.outcomes;
    position.outcomes = nullptr;
  }*/
  // for (int i = 0; i < moves.size(); i++) {
  //     for (int j = 0; j < 8; j++) {
  //         for (int k = 0; k < 8; k++) {
  //             std::cout << moves[i].first[j][k];
  //         }
  //         std::cout << std::endl;
  //     }
  // }
  // return;
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
  std::vector<int> dest;
  std::vector<int> start;
  char piece;
  bool capture;
  bool checkmate;
  char promotion;
  Move()
      : check(false),
        dest({-1, -1}),
        start({-1, -1}),
        piece(' '),
        capture(false),
        checkmate(false),
        promotion(' ') {}
};

Board GetBoard(Move info, const Position& position) {
  Board board = position.board;
  Color to_move = position.white_to_move ? white : black;
  switch (info.piece) {
    case 'P': {
      int multiplier = to_move == white ? -1 : 1;
      if (info.capture) {
        if (board[(size_t)info.dest[0]][(size_t)info.dest[1]].color == empty) {
          // en passant
          board[(size_t)info.dest[0]][(size_t)info.dest[1]] =
              board[(size_t)info.dest[0] - multiplier][(size_t)info.start[1]];
          board[(size_t)info.dest[0] - multiplier][(size_t)info.start[1]].Empty();
          board[(size_t)info.dest[0] - multiplier][(size_t)info.dest[1]].Empty();
        } else {
          board[(size_t)info.dest[0]][(size_t)info.dest[1]] =
              board[(size_t)info.dest[0] - multiplier][(size_t)info.start[1]];
          board[(size_t)info.dest[0] - multiplier][(size_t)info.start[1]].Empty();
        }
      } else if (board[(size_t)info.dest[0] - multiplier][(size_t)info.dest[1]].type ==
                 'P') {
        board[(size_t)info.dest[0]][(size_t)info.dest[1]] =
            board[(size_t)info.dest[0] - multiplier][(size_t)info.dest[1]];
        board[(size_t)info.dest[0] - multiplier][(size_t)info.dest[1]].Empty();
      } else {
        board[(size_t)info.dest[0]][(size_t)info.dest[1]] =
            board[position.white_to_move ? (size_t)6 : (size_t)1][(size_t)info.dest[1]];
        board[position.white_to_move ? (size_t)6 : (size_t)1][(size_t)info.dest[1]].Empty();
      }
      if (info.promotion != ' ') {
        board[(size_t)info.dest[0]][(size_t)info.dest[1]].type = info.promotion;
      }
      return board;
    }
    case 'N':
      for (size_t k = 0; k < 8; k++) {
        int i = info.dest[0] + knight_moves[k][0];
        int j = info.dest[1] + knight_moves[k][1];
        if (i >= 0 && i <= 7 && j >= 0 && j <= 7 &&
            board[(size_t)i][(size_t)j].color == to_move &&
            board[(size_t)i][(size_t)j].type == 'N' &&
            (info.start[0] != -1 ? i == info.start[0] : true) &&
            (info.start[1] != -1 ? j == info.start[1] : true)) {
          board[(size_t)info.dest[0]][(size_t)info.dest[1]] =
              board[(size_t)i][(size_t)j];
          board[(size_t)i][(size_t)j].Empty();
          break;
        }
      }
      return board;
    case 'Q':
    case 'B': {
      Color opponent = to_move == white ? black : white;
      for (size_t k = 0; k < 4; k++) {
        int i = info.dest[0] + bishop_moves[k][0];
        int j = info.dest[1] + bishop_moves[k][1];
        while (i >= 0 && i <= 7 && j >= 0 && j <= 7) {
          if (board[(size_t)i][(size_t)j].color == opponent) {
            break;
          }
          if (board[(size_t)i][(size_t)j].type == info.piece &&
              (info.start[0] != -1 ? i == info.start[0] : true) &&
              (info.start[1] != -1 ? j == info.start[1] : true)) {
            board[(size_t)info.dest[0]][(size_t)info.dest[1]] =
                board[(size_t)i][(size_t)j];
            board[(size_t)i][(size_t)j].Empty();
            return board;
          }
          if (board[(size_t)i][(size_t)j].color == to_move) {
            break;
          }
          i += bishop_moves[k][0];
          j += bishop_moves[k][1];
        }
      }
      [[fallthrough]];
    }
    case 'R': {
      Color opponent = to_move == white ? black : white;
      for (size_t k = 0; k < 4; k++) {
        int i = info.dest[0] + rook_moves[k][0];
        int j = info.dest[1] + rook_moves[k][1];
        while (i >= 0 && i <= 7 && j >= 0 && j <= 7) {
          if (board[(size_t)i][(size_t)j].color == opponent) {
            break;
          }
          if (board[(size_t)i][(size_t)j].type == info.piece &&
              (info.start[0] != -1 ? i == info.start[0] : true) &&
              (info.start[1] != -1 ? j == info.start[1] : true)) {
            board[(size_t)info.dest[0]][(size_t)info.dest[1]] =
                board[(size_t)i][(size_t)j];
            board[(size_t)i][(size_t)j].Empty();
            return board;
          }
          if (board[(size_t)i][(size_t)j].color == to_move) {
            break;
          }
          i += rook_moves[k][0];
          j += rook_moves[k][1];
        }
      }
      return board;
    }
    case 'K': {
      int new_i, new_j;
      for (size_t k = 0; k < 8; k++) {
        new_i = info.dest[0] + king_moves[k][0];
        new_j = info.dest[1] + king_moves[k][1];
        if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
          if (position.board[(size_t)new_i][(size_t)new_j].type == 'K') {
            board[(size_t)info.dest[0]][(size_t)info.dest[1]] =
                board[(size_t)new_i][(size_t)new_j];
            board[(size_t)new_i][(size_t)new_j].Empty();
            return board;
          }
        }
      }
    }

  }
  return board;
}

Position* GetPosition(Position& position, const std::string& move) {
  Board new_board = position.board;
  if (move == "0-0" || move == "O-O") {
    if (position.white_to_move) {
      new_board[7][6].set(white, 'K');
      new_board[7][5].set(white, 'R');
      new_board[7][4].Empty();
      new_board[7][7].Empty();
    } else {
      new_board[0][6].set(black, 'K');
      new_board[0][5].set(black, 'R');
      new_board[0][4].Empty();
      new_board[0][7].Empty();
    }
    return FindPosition(new_board, position);
  }
  if (move == "O-O-O" || move == "0-0-0") {
    if (position.white_to_move) {
      new_board[7][2].set(white, 'K');
      new_board[7][3].set(white, 'R');
      new_board[7][4].Empty();
      new_board[7][0].Empty();
    } else {
      new_board[0][2].set(black, 'K');
      new_board[0][3].set(black, 'R');
      new_board[0][4].Empty();
      new_board[0][0].Empty();
    }
    return FindPosition(new_board, position);
  }
  Move info;
  int current = (int) move.length() - 1;
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
  info.dest[0] = 8 - char_to_int(move[(size_t)current]);
  current--;
  info.dest[1] = move[(size_t)current] - 'a';
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

std::string MakeString(const Position& position, const bool& chess_notation, const bool& white_on_bottom) {
  std::stringstream result;
  if (!chess_notation) {
    if (white_on_bottom) {
      result << "    0    1    2    3    4    5    6    7   \n";
    }
    else {
      result << "    7    6    5    4    3    2    1    0   \n";
    }
  }
  if (white_on_bottom) {
    for (size_t i = 0; i <= 7; i++) {
      result << "  +----+----+----+----+----+----+----+----+\n"
             << (chess_notation ? (8 - i) : i) << " |";
      for (size_t j = 0; j <= 7; j++) {
        char color;

        switch (position.board[i][j].color) {
          case white:
            color = 'W';
            break;
          case black:
            color = 'B';
            break;
          case empty:
            color = ' ';
            break;
          default:
            std::cout << std::endl << "Unknown color error in MakeString.";
            exit(1);
        }
        result << ' ' << color << position.board[i][j].type
               << " |";
      }
      result << '\n';
    }
  }
  else {
    for (int i = 7; i >= 0; i--) {
      result << "  +----+----+----+----+----+----+----+----+\n"
             << (chess_notation ? (8 - i) : i) << " |";
      for (int j = 7; j >= 0; j--) {
        //char piece;
        //bool white_piece = position.board[(size_t)i][(size_t)j].color == white;
        //switch (position.board[(size_t)i][(size_t)j].type) {
        //  case 'K':
        //    piece = white_piece ? '\u2654' : '\u265A';
        //    break;
        //  case 'Q':
        //    piece = white_piece ? '\u2655' : '\u265B';
        //    break;
        //  case 'R':
        //    piece = white_piece ? '\u2656' : '\u265C';
        //    break;
        //  default:
        //    piece = ' ';
        //}
        char color;
        
        switch (position.board[(size_t)i][(size_t)j].color) {
          case white:
            color = 'W';
            break;
          case black:
            color = 'B';
            break;
          case empty:
            color = ' ';
            break;
          default:
            std::cout << std::endl << "Unknown color error in MakeString.";
            exit(1);
        }
        result << ' ' << color << position.board[(size_t)i][(size_t)j].type
               << " |";
      }
      result << '\n';
    }
  }
  result << "  +----+----+----+----+----+----+----+----+";
  if (chess_notation) {
    if (white_on_bottom) {
    result << "\n    a    b    c    d    e    f    g    h    ";
    }
    else {
    result << "\n    h    g    f    e    d    c    b    a    ";
    }
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
int EvaluateMaterial(const Position& position) {
  int material = 0;
  for (size_t i = 0; i < 8; i++) {
    for (size_t j = 0; j < 8; j++) {
      if (position.board[i][j].color == empty ||
          position.board[i][j].type == 'K') {
        continue;
      }
      if (position.board[i][j].color == white) {
        material += piece_values[position.board[i][j].type];
      } else {
        material -= piece_values[position.board[i][j].type];
      }
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

  if (!position.outcomes) {
    new_generate_moves(position);
  }
  else {
    position.evaluation = EvaluateMaterial(position);
  }

  if (position.outcomes->size() == 0) {
    position.depth = 1000;
    if (InCheck(position, position.white_to_move ? white : black)) {
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
  double initial_evaluation = position.evaluation;
  if (depth == 0 &&
      position.outcomes->back()->evaluation == position.evaluation) {
     //initial_evaluation = position.evaluation;
     position.evaluation =
         round(position.evaluation) + EvaluateMobility(position);
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
    if (position->white_to_move && InCheck(*position, white)) {
      return -1;
    }
    if (!position->white_to_move && InCheck(*position, black)) {
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
      alpha = INT_MIN;
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
            alpha = INT_MIN;
            beta = INT_MAX;
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


std::string GetMove(Position& position1, Position& position2,
  const bool chess_notation) {
  //if (position1.castling.white_o_o != position2.castling.white_o_o) {
  //  return chess_notation ? "O-O" : "74767775";
  //}
  //if (position1.castling.white_o_o_o != position2.castling.white_o_o_o) {
  //  return chess_notation ? "O-O-O" : "74727073";
  //}
  //if (position1.castling.black_o_o != position2.castling.black_o_o) {
  //  return chess_notation ? "O-O" : "04060705";
  //}
  //if (position1.castling.black_o_o_o != position2.castling.black_o_o_o) {
  //  return chess_notation ? "O-O-O" : "04020003";
  //}


  
  std::vector<Point> differences;
  for (size_t i = 0; i < 8; i++) {
    for (size_t j = 0; j < 8; j++) {
      if (position1.board[i][j] != position2.board[i][j]) {
        differences.emplace_back(i, j);
      }
    }
  }
  if (differences.size() == 4) {
    if (position2.board[7][6].type == 'K' &&
        position1.board[7][6].color == empty) {
      return chess_notation ? "O-O" : "74767775";
    }
    if (position2.board[7][2].type == 'K' &&
        position1.board[7][2].color == empty) {
      return chess_notation ? "O-O" : "74727073";
    }
    if (position2.board[0][6].type == 'K' &&
        position1.board[0][6].color == empty) {
      return chess_notation ? "O-O" : "04060705";
    }
    if (position2.board[0][2].type == 'K' &&
        position1.board[0][2].color == empty) {
      return chess_notation ? "O-O-O" : "04020003";
    }
  }
  if (differences.size() == 3) {
    if (position2
            .board[(size_t)differences[0].row][(size_t)differences[0].column]
      .color == empty) {
      if (chess_notation) {
        return std::string({char('a'+ differences[2].column), 'x', char('a' + differences[1].column), char('0' + 8 - differences[0].row)});
      } else {
        std::stringstream output;
        output << differences[2].row << differences[2].column << differences[1].row << differences[1].column << differences[0].row << differences[1].column;
        return output.str();
      }
    } else {
      if (chess_notation) {
        return std::string({char('a' + differences[2].column), 'x',
                            char('a' + differences[0].column),
                            char('0' + 8 - differences[0].row)});
      } else {
        std::stringstream output;
        output << differences[2].row << differences[2].column
               << differences[0].row << differences[0].column
               << differences[1].row << differences[1].column;
        return output.str();
      }
    }
    /*else {
      return chess_notation ? 'a' + differences[1].column : '0'; 
    }*/
    
  }
  Point origin, dest;
  if (position2.board[differences[0].row][differences[0].column].color == empty) {
    origin = differences[0];
    dest = differences[1];
  } else {
    origin = differences[1];
    dest = differences[0];
  }
  if (!chess_notation) {
    std::stringstream output;
    output << origin.row << origin.column << dest.row << dest.column;
    return output.str();
  }
  Check check_info;
  if (position1.board[(size_t)origin.row][(size_t)origin.column].type != 'P') {
    GetCheckInfo(check_info, position1);
  }
  std::stringstream output;
  std::vector<Point> found;
  switch (position1.board[(size_t)origin.row][(size_t)origin.column].type) {
    case 'P':
      if (position1.board[(size_t)dest.row][(size_t)dest.column].color != empty) {
        // capture
        output << (char) ('a' + origin.column) << 'x' << static_cast<char>('a' + dest.column)
               << (8 - dest.row);
      } else {
        output << static_cast<char>('a' + dest.column) << (8 - dest.row);
      }
      if (position2.board[dest.row][dest.column].type != 'P') {
        // promotion
        output << '='
               << position2.board[(size_t)dest.row][(size_t)dest.column].type;
      }
      break;
    case 'N':
      output << 'N';
      for (size_t k = 0; k < 8; k++) {
        int new_i = (int)dest.row + knight_moves[k][0];
        int new_j = (int)dest.column + knight_moves[k][1];
        if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
            position1.board[(size_t)new_i][(size_t)new_j].color ==
                    position2.board[(size_t)dest.row][(size_t)dest.column]
                    .color &&
            position1.board[(size_t)new_i][(size_t)new_j].type == 'N' &&
            ((size_t)new_i !=
                origin.row || (size_t)new_j != origin.column) &&
            MoveOk(check_info, (size_t)new_i, (size_t)new_j, (size_t)dest.row,
                   (size_t)dest.column)) {
          found.emplace_back((size_t)new_i, (size_t)new_j);
        }
      }
      if (found.size() > 0) {
        if (std::none_of(found.begin(), found.end(), [origin](Point p) {
              return p.column == origin.column;
            })) {
          output << (char) ('a' + origin.column);
        } else if (std::none_of(found.begin(), found.end(), [origin](Point p) {
                     return p.row == origin.row;
                   })) {
          output << 8 - origin.row;
        } else {
          output << static_cast<char>('a' + origin.column) << origin.row;
        }
      }
      if (position1.board[(size_t)dest.row][(size_t)dest.column].color != empty) {
        output << 'x';
      }
      output << static_cast<char> ('a' + dest.column) << (8 - dest.row);
      break;
    case 'Q':
    case 'B':
      output << position1.board[(size_t)origin.row][(size_t)origin.column].type;
      for (size_t k = 0; k < 4; k++) {
        int new_i = (int)dest.row + bishop_moves[k][0];
        int new_j = (int)dest.column + bishop_moves[k][1];
        while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
          if (position1.board[(size_t)new_i][(size_t)new_j].color == (position2.white_to_move ? white : black)) {
            break;
          }
          if (position1.board[(size_t)new_i][(size_t)new_j].color ==
                  (position1.white_to_move ? white : black)&&
              position1.board[(size_t)new_i][(size_t)new_j].type ==
                  position1.board[origin.row][origin.column].type &&
              (new_i != (int)origin.row || new_j != (int)origin.column) &&
              MoveOk(check_info, (size_t)new_i, (size_t)new_j, (size_t)dest.row,
                     (size_t)dest.column)) {
            found.emplace_back((size_t)new_i, (size_t)new_j);
          }
          if (position1.board[(size_t)new_i][(size_t)new_j].color ==
              position2.board[(size_t)dest.row][(size_t)dest.column].color) {
            break;
          }
          new_i += bishop_moves[k][0];
          new_j += bishop_moves[k][1];
        }
      }
      if (position1.board[(size_t)origin.row][(size_t)origin.column].type ==
          'B') {
        if (found.size() > 0) {
          if (std::none_of(found.begin(), found.end(), [origin](Point p) {
                return p.column == origin.column;
              })) {
            output << (char)('a' + origin.column);
          } else if (std::none_of(
                         found.begin(), found.end(),
                         [origin](Point p) { return p.row == origin.row; })) {
            output << 8 - origin.row;
          } else {
            output << (char)('a' + origin.column) << origin.row;
          }
        }
        if (position1.board[(size_t)dest.row][(size_t)dest.column].color !=
            empty) {
          output << 'x';
        }
        output << (char)('a' + dest.column) << (8 - dest.row);
        break;
      }
    case 'R':
      if (position1.board[(size_t)origin.row][(size_t)origin.column].type ==
          'R') {
        output << 'R';
      }
      for (size_t k = 0; k < 4; k++) {
        int new_i = (int)dest.row + rook_moves[k][0];
        int new_j = (int)dest.column + rook_moves[k][1];
        while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
          if (position1.board[(size_t)new_i][(size_t)new_j].color ==
              (position2.white_to_move ? white : black)) {
            break;
          }
          if (position1.board[(size_t)new_i][(size_t)new_j].color ==
                  (position1.white_to_move ? white : black) &&
              position1.board[(size_t)new_i][(size_t)new_j].type ==
                  position1.board[origin.row][origin.column].type &&
              ((size_t)new_i != origin.row || (size_t)new_j != origin.column) &&
              MoveOk(check_info, (size_t)new_i, (size_t)new_j, (size_t)dest.row,
                     (size_t)dest.column)) {
            found.emplace_back(new_i, new_j);
          }
          if (position1.board[(size_t)new_i][(size_t)new_j].color ==
              position2.board[(size_t)dest.row][(size_t)dest.column].color) {
            break;
          }
          new_i += rook_moves[k][0];
          new_j += rook_moves[k][1];
        }
      }
      if (found.size() > 0) {
        if (std::none_of(found.begin(), found.end(), [origin](Point p) {
              return p.column == origin.column;
            })) {
          output << (char)('a' + origin.column);
        } else if (std::none_of(found.begin(), found.end(), [origin](Point p) {
                     return p.row == origin.row;
                   })) {
          output << 8 - origin.row;
        } else {
          output << (char)('a' + origin.column) << origin.row;
        }
      }
      if (position1.board[dest.row][dest.column].color != empty) {
        output << 'x';
      }
      output << (char)('a' + dest.column) << (8 - dest.row);
      break;
    case 'K':
      output << 'K';
      if (position1.board[dest.row][dest.column].color != empty) {
        output << 'x';
      }
      output << (char)('a' + dest.column) << (8 - dest.row);
      break;
      


  }
  if (std::abs(position2.evaluation) == (position2.white_to_move ? (mate - position2.number) : (-mate + position2.number))) {
    output << '#';
  } else if (InCheck(position2, position2.white_to_move ? white : black)) {
    output << '+';
  }
  return output.str();
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
      if (match[1] ==  "FEN") {
        std::stringstream elements_line(match[2]);
        std::vector<std::string> elements;
        for (int i = 0; i < 6; i++) {
          std::string s;
          elements_line >> s;
          elements.push_back(s);
        }
        size_t i = 0;
        size_t j = 0;
        for (size_t k = 0; k < elements[0].length(); k++) {
          if (elements[0][k] == '/') {
            i++;
            j = 0;
            continue;
          }
          if (i > 7 || j > 7) {
              std::cout << "FEN error";
              exit(1);
          }
          if (isdigit(elements[0][k])) {
            for (int l = char_to_int(elements[0][k]); l > 0; l--) {
              if (j > 7) {
                std::cout << "FEN error";
                exit(1);
              }
              position->board[i][j].Empty();
              j++;
            }
          } else {
            if (islower(elements[0][k])) {
              position->board[i][j].color = white;
            } else {
              position->board[i][j].color = white;
            }
            position->board[i][j].type = (char)toupper(elements[0][k]);
            j++;
          }

        }
        position->white_to_move = toupper(elements[1][0]) == 'W';
        if (elements[2][0] == 'K') {
          position->castling.white_o_o = true;
        }
        if (elements[2][0] == 'Q') {
          position->castling.white_o_o_o = true;
        }
        if (elements[2][0] == 'k') {
          position->castling.black_o_o = true;
        }
        if (elements[2][0] == 'q') {
          position->castling.black_o_o_o = true;
        }
        if (elements[3] != "-") {
          position->en_passant_target.row = elements[3][0] - 'a';
          position->en_passant_target.column = char_to_int(elements[3][1]);
        } else {
          position->en_passant_target.clear();
        }
        position->fifty_move_rule = (size_t)stoi(elements[4]);
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
      assert(GetPosition(*position, moves_match[i]));
      position = GetPosition(*position, moves_match[i]);
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
        std::stringstream elements_line(match[2]);
        std::vector<std::string> elements;
        for (int i = 0; i < 6; i++) {
          std::string s;
          elements_line >> s;
          elements.push_back(s);
        }
        size_t i = 0;
        size_t j = 0;
        for (size_t k = 0; k < elements[0].length(); k++) {
          if (elements[0][k] == '/') {
            i++;
            j = 0;
            continue;
          }
          if (i > 7 || j > 7) {
            std::cout << "FEN error";
            exit(1);
          }
          if (isdigit(elements[0][k])) {
            for (int l = char_to_int(elements[0][k]); l > 0; l--) {
              if (j > 7) {
                std::cout << "FEN error";
                exit(1);
              }
              new_position->board[i][j].Empty();
              j++;
            }
          } else {
            if (islower(elements[0][k])) {
              new_position->board[i][j].color = black;
            } else {
              new_position->board[i][j].color = white;
            }
            new_position->board[i][j].type = (char)toupper(elements[0][k]);
          }

          j++;
        }
        new_position->white_to_move = toupper(elements[1][0]) == 'W';
        if (elements[2][0] == 'K') {
          new_position->castling.white_o_o = true;
        }
        if (elements[2][0] == 'Q') {
          new_position->castling.white_o_o_o = true;
        }
        if (elements[2][0] == 'k') {
          new_position->castling.black_o_o = true;
        }
        if (elements[2][0] == 'q') {
          new_position->castling.black_o_o_o = true;
        }
        if (elements[3] != "-") {
          new_position->en_passant_target.row = elements[3][0] - 'a';
          new_position->en_passant_target.column = char_to_int(elements[3][1]);
        } else {
          new_position->en_passant_target.clear();
        }
        new_position->fifty_move_rule = (size_t)stoi(elements[4]);
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
      new_position = GetPosition(*new_position, moves_match[i]);
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


std::string ToLower(std::string input) {
  std::string output;
  for (size_t i = 0; i < input.length(); i++) {
    output += input[i];
  }
  return output;
}



int CountMaterial(const Position& position) {
  int count = 0;
  for (size_t i = 0; i < 8; i++) {
    for (size_t j = 0; j < 8; j++) {
      if (position.board[i][j].color != empty) {
        if (position.board[i][j].type == 'K') {
          count += 4;
        } else {
          count += piece_values[position.board[i][j].type];
        }
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
      4.0, round(log2(max_positions) / log2(CountMoves(position) / 2.0)));
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
  for (size_t i = 0; i < 8; i++) {
    for (size_t j = 0; j < 8; j++) {
      if (position->board[i][j].type == 'K') {
        position->kings[position->board[i][j].color].row = i;
        position->kings[position->board[i][j].color].column = j;
      }
    }
  }

  bool chess_notation;
  std::cout << "Chess notation? ";
  std::cin >> chess_notation;
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
  std::cout << MakeString(*position, chess_notation, white_on_bottom) << std::endl;
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
            GetMove(*position, *best_move, chess_notation);

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
          std::cout << MakeString(*best_move, chess_notation, white_on_bottom) << std::endl;
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
          std::cout << GetMove(*position, *line[0], chess_notation);
          //std::cout << "[got first move] ";
          for (size_t i = 1; i < line.size(); i++) {
            std::cout << ' ' << GetMove(*line[i - 1], *line[i], chess_notation);
            //std::cout << "[got next move] ";
          }
          std::cout/* << "[finished line]"*/ << std::endl;
          std::cout << "Moves: " << best_move->outcomes->size() << std::endl
                    << "Material: " << (float) CountMaterial(*best_move) / 2.0 << std::endl;
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
            std::cout << MakeString(*new_position, chess_notation, white_on_bottom) << std::endl;
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
    } else if (chess_notation) {
      new_position = GetPosition(*position, move);
    } else {
      new_board = position->board;
      if (move == "O-O" || move == "0-0") {
        if (position->white_to_move) {
          new_board[7][6].set(white, 'K');
          new_board[7][5].set(white, 'R');
          new_board[7][4].Empty();
          new_board[7][7].Empty();
        } else {
          new_board[0][6].set(black, 'K');
          new_board[0][5].set(black, 'R');
          new_board[0][4].Empty();
          new_board[0][7].Empty();
        }
      } else if (move == "O-O-O" || move == "0-0-0") {
        if (position->white_to_move) {
          new_board[7][2].set(white, 'K');
          new_board[7][3].set(white, 'R');
          new_board[7][4].Empty();
          new_board[7][0].Empty();
        } else {
          new_board[0][2].set(black, 'K');
          new_board[0][3].set(black, 'R');
          new_board[0][4].Empty();
          new_board[0][0].Empty();
        }
      } else {
        size_t si = char_to_size_t(move[0]);
        size_t sj = char_to_size_t(move[1]);
        size_t di = char_to_size_t(move[2]);
        size_t dj = char_to_size_t(move[3]);
        new_board[di][dj] = new_board[si][sj];
        new_board[si][sj].Empty();
        if (move.length() == 6) {
          if (move[5] == '=') {
            new_board[di][dj].type = (char)toupper(move[6]);
          } else {
            new_board[char_to_size_t(move[4])][char_to_size_t(move[5])].Empty();
          }
        }
        new_position = FindPosition(new_board, *position);
      }
    }
    if (new_position != nullptr) {
      // Position* played_position = new_position;
      stop_mutex.lock();
      *info->stop = true;
      stop_mutex.unlock();
      //printf("[main thread %d] acquire.\n", GetCurrentThreadId());
      waiting.acquire();
      UpdatePGN(new_position, GetMove(*position, *new_position, true));
      if (!new_position->outcomes) {
        new_generate_moves(*new_position);
      }



      //printf("[main thread %d] acquire complete.\n", GetCurrentThreadId());
      if (!mental_chess) {
        std::cout << MakeString(*new_position, chess_notation, white_on_bottom) << std::endl;
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