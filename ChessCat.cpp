#include <Windows.h>
#include <assert.h>
#include <process.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <semaphore>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ratio>
#include <cstdint>
#include <psapi.h>
#undef min
#undef max

std::map<char, int> piece_values = {{'P', 1}, {'N', 3}, {'B', 3}, {'R', 5},
                                    {'Q', 9}};
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
struct Piece {
  char color;
  char type;
  
  bool IsEmpty() const { return color == ' ';
  }
  bool operator==(const Piece& other) const {
    return color == other.color && type == other.type;
  }
  bool operator!=(const Piece& other) const {
    return color != other.color || type != other.type;
  }
  bool operator==(const char* p) const { return color == p[0] && type == p[1]; }
  void set(const char& c, const char& t) {
    color = c;
    type = t;
  }
  void empty() {
    color = ' ';
    type = ' ';
  }
  Piece(const char& c, const char& t) : color(c), type(t){};
};
struct Point {
  int row;
  int column;
  bool Equals(int r, int c) { return row == r && column == c; }
  Point(int r, int c) : row(r), column(c) {}
  void clear() { row = column = -1;
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
    return white_o_o == other.white_o_o && white_o_o_o == other.white_o_o_o &&
           black_o_o == other.black_o_o && black_o_o_o == other.black_o_o_o;
  };
};

using Board = std::vector<std::vector<Piece>>;
struct Kings {
  Point white_king;
  Point black_king;
  Point& operator[](const char color) {
    return color == 'W' ? white_king : black_king;
  };
  Kings(Point wk, Point bk) : white_king(wk), black_king(bk){};
};

// int g_copied_boards = 0;
//int boards_created = 0;
//int boards_destroyed = 0;

Board starting_board = {
    {Piece('B', 'R'), Piece('B', 'N'), Piece('B', 'B'), Piece('B', 'Q'),
     Piece('B', 'K'), Piece('B', 'B'), Piece('B', 'N'), Piece('B', 'R')},
    {Piece('B', 'P'), Piece('B', 'P'), Piece('B', 'P'), Piece('B', 'P'),
     Piece('B', 'P'), Piece('B', 'P'), Piece('B', 'P'), Piece('B', 'P')},
    {Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '),
     Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' ')},
    {Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '),
     Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' ')},
    {Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '),
     Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' ')},
    {Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '),
     Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' '), Piece(' ', ' ')},
    {Piece('W', 'P'), Piece('W', 'P'), Piece('W', 'P'), Piece('W', 'P'),
     Piece('W', 'P'), Piece('W', 'P'), Piece('W', 'P'), Piece('W', 'P')},
    {Piece('W', 'R'), Piece('W', 'N'), Piece('W', 'B'), Piece('W', 'Q'),
     Piece('W', 'K'), Piece('W', 'B'), Piece('W', 'N'), Piece('W', 'R')}};

struct Position {
  bool white_to_move;
  int number;
  double evaluation;
  Board board;
  Castling castling;
  //time_t time;
  char to_move;
  std::vector<Position*>* outcomes;
  int depth;
  //Position* best_move;
  Kings kings;
  Point en_passant_target;
  int fifty_move_rule;
  bool was_capture;
  //bool was_forced;

  Position(bool wtm, int n, double e, Board b,
           Castling c /*, time_t t*/, char tm,
           std::vector<Position*>* o /*,
           Position* pm*/, int d, /*Position* bm,*/Kings k, Point ept, int fmc, bool wc)
      : white_to_move(wtm),
        number(n),
        evaluation(e),
        board(b),
        castling(c),
        //time(t),
        to_move(tm),
        outcomes(o),
        //previous_move(pm),
        depth(d),
        //best_move(bm),
        kings(k),
        en_passant_target(ept),
        fifty_move_rule(fmc),
  was_capture(wc) {
    //boards_created++;
  }
  /*Position(const Board& b, const Castling& c, time_t ti, char to)
      : board(b), castling(c), time(ti), to_move(to) {}*/
  /*Position(const Board& b, const Castling& c)
      : Position(b, c, 0LL) {}*/
  // Possibilities possibilities;

  static Position* StartingPosition() {
    return new Position(true, 0, 0.0, starting_board, Castling(true, true, true, true), 'W', new std::vector<Position*>, 0, Kings(Point(7, 4), Point(0, 4)), Point(-1, -1), 0, false);
  }

  Position CreateShallowCopy() {
    return Position(!white_to_move, number + 1, evaluation, board, castling,
                    /*time, */to_move, nullptr /* outcomes *//*,
                    nullptr *//* previous_move */, depth - 1, /*
                    nullptr /* best_move */
                    /*, */ kings, Point(-1, -1), fifty_move_rule + 1, false);
  }
  ~Position() {
    // delete all outcomes pointers
    if (outcomes) {
      for (int i = 0; i < outcomes->size(); i++) {
        delete (*outcomes)[i];
        // boards_destroyed++;
      }
      delete outcomes;
      outcomes = nullptr;
    }

  };

  Position* CreateDeepCopy() {
    Position* new_position = new Position(white_to_move, number, evaluation, board, castling, /*time, */to_move,
        nullptr /* outcomes *//*, nullptr *//* previous_move */, depth /*,
        nullptr *//* best_move */, kings, en_passant_target, fifty_move_rule, was_capture);
    return new_position;
  }
  Position* RealDeepCopy() const {
    Position* new_position = new Position(
        white_to_move, number, evaluation, board, castling /*, time*/,
        to_move,
        nullptr /* outcomes *//*, nullptr *//* previous_move */, depth /*,
        nullptr *//* best_move */, kings, en_passant_target, fifty_move_rule, was_capture);

    if (outcomes) {
      new_position->outcomes = new std::vector<Position*>;
      for (int i = 0; i < outcomes->size(); i++) {
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

int count_moves(const Position& position, int i, int j) {
  int moves_count = 0;
  int new_i, new_j;
  char opponent = position.board[i][j].color == 'W' ? 'B' : 'W';
  switch (position.board[i][j].type) {
    case 'P':
      // captures
      if (j > 0 && position.board[i][j - 1].color == opponent) {
        moves_count++;
      }
      if (j < 7 && position.board[i][j + 1].color == opponent) {
        moves_count++;
      }
      // moving forward
      if (position.board[i][j].color == 'W') {
        // one square forward
        if (position.board[i - 1][j].color == ' ') {
          moves_count++;

          // two squares forward
          if (i == 6 && position.board[4][j].color == ' ') {
            moves_count++;
          }
        }
      } else if (position.board[i + 1][j].color == ' ') {
        // one square forward
        moves_count++;

        // two squares forward
        if (i == 1 && position.board[3][j].color == ' ') {
          moves_count++;
        }
      }
      break;
    case 'N':
      for (int k = 0; k <= 7; k++) {
        new_i = i + knight_moves[k][0];
        if (new_i >= 0 && new_i <= 7) {
          new_j = j + knight_moves[k][1];
          if (new_j >= 0 && new_j <= 7 &&
              position.board[new_i][new_j].color !=
                  position.board[i][j].color) {
            moves_count++;
          }
        }
      }
      break;
    case 'Q':
    case 'B':
      for (int k = 0; k < 4; k++) {
        int max_times = std::min(bishop_moves[k][0] == 1 ? (7 - i) : i,
                                 bishop_moves[k][1] == 1 ? (7 - j) : j);
        for (int t = 0; t < max_times; t++) {
          new_i = i + bishop_moves[k][0];
          new_j = j + bishop_moves[k][1];
          if (position.board[new_i][new_j].color !=
              position.board[i][j].color) {
            moves_count++;
          } else {
            break;  // ran into own piece
          }
          if (position.board[new_i][new_j].color == opponent) {
            break;  // captured opponent
          }
        }
      }
      if (position.board[i][j].type == 'B') {
        break;
      }
    case 'R':
      // move left
      for (new_j = j - 1; new_j >= 0; new_j--) {
        if (position.board[i][new_j].color == position.board[i][j].color) {
          break;  // ran into own piece
        }
        moves_count++;
        if (position.board[i][new_j].color == opponent) {
          break;  // captured opponent and can't go further
        }
      }

      // move up
      for (new_i = i - 1; new_i >= 0; new_i--) {
        if (position.board[new_i][j].color == position.board[i][j].color) {
          break;
        }
        moves_count++;
        if (position.board[new_i][j].color == opponent) {
          break;
        }
      }

      // move right
      for (new_j = j + 1; new_j <= 7; new_j++) {
        if (position.board[i][new_j].color == position.board[i][j].color) {
          break;
        }
        moves_count++;
        if (position.board[i][new_j].color == opponent) {
          break;
        }
      }

      // move down
      for (new_i = i + 1; new_i <= 7; new_i++) {
        if (position.board[new_i][j].color == position.board[i][j].color) {
          break;
        }
        moves_count++;
        if (position.board[new_i][j].color == opponent) {
          break;
        }
      }
      break;
    case 'K':
      for (int k = 0; k < 8; k++) {
        new_i = i + king_moves[k][0];
        if (new_i >= 0 && new_i <= 7) {
          new_j = j + king_moves[k][1];
          if (new_j >= 0 && new_j <= 7 &&
              position.board[i + king_moves[k][0]][j + king_moves[k][1]]
                      .color != position.board[i][j].color) {
            moves_count++;
          }
        }
      }
  }
  return moves_count;
}



struct Check {
  bool in_check = false;
  bool king_must_move = false;
  std::map<Point, std::set<Point>> pinned;
  std::set<Point> block;
};


void GetCheckInfo(Check& output, Position& position, char side = ' ') {
  if (side == ' ') {
    side = position.to_move;
  }
  int new_i, new_j;
  /*if (!king_present(position)) {
    int i=1;
  }*/
  char opponent;
  Point found(-1, -1);
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
  int i = position.kings[side].row;
  int j = position.kings[side].column;
  // pawn check
  if (side == 'W') {
    opponent = 'B';
    new_i = i -1;
  } else {
    opponent = 'W';
    new_i = i + 1;
  }
  // right
  new_j = j + 1;
  if (new_i >= 0 && new_i <= 7 && j < 7 && position.board[new_i][j + 1].color == opponent &&
        position.board[new_i][j + 1].type == 'P') {
    output.in_check = true;
    output.block.insert(Point(new_i, new_j));
  }
  // left
  new_j = j - 1;
  if (new_i >= 0 && new_i <= 7 && j > 0 && position.board[new_i][new_j].color == opponent &&
        position.board[new_i][new_j].type == 'P') {
    if (output.in_check) {
      output.king_must_move = true;
    } else {
      output.in_check = true;
      output.block.insert(Point(new_i, new_j));
    }
  }

  // knight check
  for (int k = 0; k < 8; k++) {
    new_i = i + knight_moves[k][0];
    if (new_i >= 0 && new_i <= 7) {
      new_j = j + knight_moves[k][1];
      if (new_j >= 0 && new_j <= 7 &&
          position.board[new_i][new_j].color == opponent &&
          position.board[new_i][new_j].type == 'N') {
        if (output.in_check) {
          output.king_must_move = true;
        } else {
          output.in_check = true;
          output.block.insert(Point(new_i, new_j));
        }
      }
    }
  }
  // bishop check
  for (int k = 0; k < 4; k++) {
    found.row = -1;
    found.column = -1;
    done.clear();
    new_i = i + bishop_moves[k][0];
    new_j = j + bishop_moves[k][1];

    while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
      done.insert(Point(new_i, new_j));
      if (position.board[new_i][new_j].color == side) {
        if (found.row != -1) {
          break;
        }
        found.row = new_i;
        found.column = new_j;

      } else if (position.board[new_i][new_j].color == opponent) {
        if (position.board[new_i][new_j].type == 'B' ||
            position.board[new_i][new_j].type == 'Q') {
          if (found.row != -1) {
            //if (position.board[found.row][found.column].type == 'B' ||
            //    position.board[found.row][found.column].type == 'Q' /*need to account for pawns too*/) {
              // (position.board[new_i][new_j].type != 'Q' ? (position.board[found.row][found.column].type != position.board[new_i][new_j].type || (position.board[found.row][found.column].type == 'Q' && (position.board[new_i][new_j].type == 'B' || position.board[new_i][new_j].type == 'R'))) : (position.board[found.row][found.column].type == 'B' || position.board[found.row][found.column].type == 'R')
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


  for (int k = 0; k < 4; k++) {
    found.row = -1;
    found.column = -1;
    done.clear();
    new_i = i + rook_moves[k][0];
    new_j = j + rook_moves[k][1];

    while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
      done.insert(Point(new_i, new_j));
      if (position.board[new_i][new_j].color == position.board[i][j].color) {
        if (found.row != -1) {
          break;
        }
        found.row = new_i;
        found.column = new_j;

      } else if (position.board[new_i][new_j].color == opponent) {
        if (position.board[new_i][new_j].type == 'R' ||
            position.board[new_i][new_j].type == 'Q') {
          if (found.row != -1) {
            if (position.board[found.row][found.column].type == 'R' ||
                position.board[found.row][found.column].type == 'Q') {
              // (position.board[new_i][new_j].type != 'Q' ?
              // (position.board[found.row][found.column].type !=
              // position.board[new_i][new_j].type ||
              // (position.board[found.row][found.column].type == 'Q' &&
              // (position.board[new_i][new_j].type == 'B' ||
              // position.board[new_i][new_j].type == 'R'))) :
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

bool MoveOk(Check check, int i, int j, int new_i, int new_j) {
  Point origin = Point(i, j);
  Point dest = Point(new_i, new_j);
  return ((check.in_check ? check.block.count(dest)
          : true) && (check.pinned.contains(origin)
              ? check.pinned[origin].contains(dest)
              : true));
}
bool InCheck(Position& position, char side, int ki = -1, int kj = -1) {
  int new_i, new_j;
  /*if (!king_present(position)) {
    int i=1;
  }*/
  char opponent = side == 'W' ? 'B' : 'W';
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
  int i = ki == -1 ? position.kings[side].row : ki;
  int j = kj == -1 ? position.kings[side].column : kj;
  // pawn check
  new_i = i + (side == 'W' ? -1 : 1);
  if (new_i >= 0 && new_i <= 7 &&
      ((j < 7 && position.board[new_i][j + 1].color == opponent &&
        position.board[new_i][j + 1].type == 'P') ||
       (j > 0 && position.board[new_i][j - 1].color == opponent &&
        position.board[new_i][j - 1].type == 'P'))) {
    return true;
  }

  // knight check
  for (int k = 0; k < 8; k++) {
    new_i = i + knight_moves[k][0];
    if (new_i >= 0 && new_i <= 7) {
      new_j = j + knight_moves[k][1];
      if (new_j >= 0 && new_j <= 7 &&
          position.board[new_i][new_j].color == opponent &&
          position.board[new_i][new_j].type == 'N') {
        return true;
      }
    }
  }
  // bishop check
  for (int k = 0; k < 4; k++) {
    new_i = i + bishop_moves[k][0];
    new_j = j + bishop_moves[k][1];

    while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
      if (position.board[new_i][new_j].color ==
          side && position.board[new_i][new_j].type != 'K') {
        break;
      }

      if (position.board[new_i][new_j].color == opponent) {
        if (position.board[new_i][new_j].type == 'B' ||
            position.board[new_i][new_j].type == 'Q') {
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
  for (new_j = j - 1; new_j >= 0; new_j--) {
    if (position.board[i][new_j].color == opponent) {
      if ((position.board[i][new_j].type == 'R' ||
           position.board[i][new_j].type == 'Q')) {
        return true;
      }
      break;
    }
    if (position.board[i][new_j].color == side &&
        position.board[i][new_j].type != 'K') {
      break;
    }
  }

  // up
  for (new_i = i - 1; new_i >= 0; new_i--) {
    if (position.board[new_i][j].color == opponent) {
      if ((position.board[new_i][j].type == 'R' ||
           position.board[new_i][j].type == 'Q')) {
        return true;
      }
      break;
    }
    if (position.board[new_i][j].color == side &&
        position.board[new_i][j].type != 'K') {
      break;
    }
  }

  // right
  for (new_j = j + 1; new_j <= 7; new_j++) {
    if (position.board[i][new_j].color == opponent) {
      if ((position.board[i][new_j].type == 'R' ||
           position.board[i][new_j].type == 'Q')) {
        return true;
      }
      break;
    }
    if (position.board[i][new_j].color == side &&
        position.board[i][new_j].type != 'K') {
      break;
    }
  }

  // down
  for (new_i = i + 1; new_i <= 7; new_i++) {
    if (position.board[new_i][j].color == opponent) {
      if ((position.board[new_i][j].type == 'R' ||
           position.board[new_i][j].type == 'Q')) {
        return true;
      }
      break;
    }
    if (position.board[new_i][j].color == side &&
        position.board[new_i][j].type != 'K') {
      break;
    }
  }
  // king check
  for (int k = 0; k < 8; k++) {
    new_i = i + king_moves[k][0];
    new_j = j + king_moves[k][1];
    if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
        position.board[new_i][new_j].type == 'K' &&
        position.board[new_i][new_j].color == opponent) {
      return true;
    }

  }
  return false;
}


const int king_value = 1000;


double EvaluateMobility(Position& position) {
  char opponent;
  Check white_check;
  Check black_check;
  GetCheckInfo(white_check, position);
  GetCheckInfo(black_check, position);
  bool blocks = true;
  bool pins = true;
  int new_i, new_j;
  double mobility_score = 0;
  Check* check;

  for (int i = 0; i <= 7; i++) {
    for (int j = 0; j <= 7; j++) {
      if (position.board[i][j].color == ' ') {
        continue;
      }
      int moves = 0;
      if (position.board[i][j].type == 'K') {
        for (int k = 0; k < 8; k++) {
          new_i = i + king_moves[k][0];
          new_j = j + king_moves[k][1];
          if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 && !InCheck(position, position.board[i][j].color, new_i, new_j)) {
            moves++;
          }
        }
        mobility_score += (double)moves / (double)king_value;
        continue;
      }
      if (position.board[i][j].color == 'W') {
        check = &white_check;
      } else {
        check = &black_check;
      }
      // counting moves
      //int addition = position.board[i][j].color == 'W' ? 1 : -1;
      opponent = position.board[i][j].color == 'W' ? 'B' : 'W';
      switch (position.board[i][j].type) {
        case 'P': {
          if (check->king_must_move) {
            break;
          }
          int multiplier, promotion_row, starting_row;
          if (position.board[i][j].color == 'W') {
            multiplier = -1;
            starting_row = 6;
            promotion_row = 1;
          } else {
            multiplier = 1;
            starting_row = 1;
            promotion_row = 6;
          }
          bool left_capture =
              j > 0 && position.board[i + multiplier][j - 1].color == opponent;
          bool right_capture =
              j < 7 && position.board[i + multiplier][j + 1].color == opponent;
          new_i = i + multiplier;
          if (i == promotion_row) {
            // promotion
            // one square

            if (position.board[new_i][j].color == ' ') {
              if (MoveOk(*check, i, j, new_i, j)) {
                moves += 4;
              }
            }
            // captures to the left
            if (left_capture && MoveOk(*check, i, j, new_i, j - 1)) {
              moves += 4;
            }
            // captures to the right
            if (right_capture && MoveOk(*check, i, j, new_i, j + 1)) {
              moves += 4;
            }
          } else if (position.board[new_i][j].color == ' ') {
            // one square forward
            if (MoveOk(*check, i, j, new_i, j)) {
              moves++;
            }
            // two squares forward
            if (i == starting_row &&
                position.board[i + multiplier * 2][j].color == ' ' &&
                MoveOk(*check, i, j, i + multiplier * 2, j)) {
              moves++;
            }
          } else {
            if (left_capture && MoveOk(*check, i, j, new_i, j - 1)) {
              moves++;
            }
            // captures to the right
            if (left_capture && MoveOk(*check, i, j, new_i, j + 1)) {
              moves++;
            }
            // en passant to the left
            int en_passant_start = (int)(3.5 + (0.5 * multiplier));
            if (i == en_passant_start && j > 0 &&
                position.board[i][j - 1].color == opponent &&
                position.board[i][j - 1].type == 'P' &&
                position.en_passant_target.Equals(new_i, j - 1) &&
                MoveOk(*check, i, j, new_i, j - 1)) {
              int di = position.kings[position.board[i][j].color].row - i;
              int dj =
                  position.kings[position.board[i][j].color].column - j - 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j =
                      position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
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
                int iter_i = i;
                int iter_j = j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[iter_i][iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[iter_i][iter_j].color == opponent) {
                    if (position.board[iter_i][iter_j].type == 'B' ||
                        position.board[iter_i][iter_j].type == 'Q') {
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
                position.en_passant_target.Equals(new_i, j + 1) &&
                MoveOk(*check, i, j, new_i, j + 1)) {
              int di = position.kings[position.board[i][j].color].row - i;
              int dj =
                  position.kings[position.board[i][j].color].column - j + 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j =
                      position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
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
                int iter_i = i;
                int iter_j = j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[iter_i][iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[iter_i][iter_j].color == opponent) {
                    if (position.board[iter_i][iter_j].type == 'B' ||
                        position.board[iter_i][iter_j].type == 'Q') {
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
          for (int k = 0; k < 8; k++) {
            new_i = i + knight_moves[k][0];
            new_j = j + knight_moves[k][1];
            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
                position.board[new_i][new_j].color !=
                    position.board[i][j].color) {
              if (MoveOk(*check, i, j, new_i, new_j)) {
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
          for (int k = 0; k < 4; k++) {
            new_i = i + bishop_moves[k][0];
            new_j = j + bishop_moves[k][1];

            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[new_i][new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(*check, i, j, new_i, new_j)) {
                moves++;
              }
              if (position.board[new_i][new_j].color == opponent) {
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
          for (int k = 0; k < 4; k++) {
            new_i = i + rook_moves[k][0];
            new_j = j + rook_moves[k][1];
            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[new_i][new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(*check, i, j, new_i, new_j)) {
                moves++;
              }
              if (position.board[new_i][new_j].color == opponent) {
                break;
              }
              new_i = new_i + rook_moves[k][0];
              new_j = new_j + rook_moves[k][1];
            }
          }
          break;

      }

      if (position.board[i][j].color == 'W') {
        double p_m = mobility_score;
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


void evaluate_position(Position& position) {
  int evaluation = 0;

  char opponent;
  Check white_check;
  Check black_check;
  GetCheckInfo(white_check, position);
  GetCheckInfo(black_check, position);
  bool blocks = true;
  bool pins = true;
  int new_i, new_j;
  double mobility_score = 0;
  Check* check;


  for (int i = 0; i <= 7; i++) {
    for (int j = 0; j <= 7; j++) {
      if (position.board[i][j].color == ' ' ||
          position.board[i][j].type == 'K') {
        continue;
      }
      if (position.board[i][j].color == 'W') {
        check = &white_check;
      } else {
        check = &black_check;
      }
      // counting moves
      int addition = position.board[i][j].color == 'W' ? 1 : -1;
      opponent = position.board[i][j].color == 'W' ? 'B' : 'W';
      double moves = 0;
      switch (position.board[i][j].type) {
        case 'P': {
          if (check->king_must_move) {
            break;
          }
          int multiplier, promotion_row, starting_row;
          if (position.board[i][j].color == 'W') {
            multiplier = -1;
            starting_row = 6;
            promotion_row = 1;
          } else {
            multiplier = 1;
            starting_row = 1;
            promotion_row = 6;
          }
          bool left_capture =
              j > 0 && position.board[i + multiplier][j - 1].color == opponent;
          bool right_capture =
              j < 7 && position.board[i + multiplier][j + 1].color == opponent;
          new_i = i + multiplier;
          if (i == promotion_row) {
            // promotion
            // one square
            
            if (position.board[new_i][j].color == ' ') {
              if (MoveOk(*check, i, j, new_i, j)) {
                moves += 4;
              }
            }
            // captures to the left
            if (left_capture && MoveOk(*check, i, j, new_i, j - 1)) {
              moves += 4;
            }
            // captures to the right
            if (right_capture && MoveOk(*check, i, j, new_i, j + 1)) {
              moves += 4;
            }
          } else if (position.board[new_i][j].color == ' ') {
            // one square forward
            if (MoveOk(*check, i, j, new_i, j)) {
              moves++;
            }
            // two squares forward
            if (i == starting_row &&
                position.board[i + multiplier * 2][j].color == ' ' && MoveOk(*check, i, j, i + multiplier * 2, j)) {
              moves++;
            }
          } else {
            if (left_capture && MoveOk(*check, i, j, new_i, j - 1)) {
              moves++;
            }
            // captures to the right
            if (left_capture && MoveOk(*check, i, j, new_i, j + 1)) {
              moves++;
            }
            // en passant to the left
            int en_passant_start = (int)(3.5 + (0.5 * multiplier));
            if (i == en_passant_start && j > 0 &&
                position.board[i][j - 1].color == opponent &&
                position.board[i][j - 1].type == 'P' &&
                position.en_passant_target.Equals(new_i, j - 1) &&
                MoveOk(*check, i, j, new_i, j - 1)) {
              int di = position.kings[position.board[i][j].color].row - i;
              int dj = position.kings[position.board[i][j].color].column - j - 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j = position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][iter_j].color == position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color ==
                      opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                        position.board[i][iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j = position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][iter_j].color == position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
                        good = false;
                      }
                      break;
                    }
                    iter_j++;
                  }
                }
              }
              if (good && std::abs(di) ==
                  std::abs(dj)) {
                di = di > 0 ? 1 : -1;
                dj = dj > 0 ? 1 : -1;
                int iter_i = i;
                int iter_j = j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 && iter_j <= 7) {
                  if (position.board[iter_i][iter_j].color == position.board[i][j].color) {
                    break;

                  } else if (position.board[iter_i][iter_j].color == opponent) {
                    if (position.board[iter_i][iter_j].type == 'B' ||
                        position.board[iter_i][iter_j].type == 'Q') {
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
                position.board[i][j + 1].type == 'P' && position.en_passant_target.Equals(new_i, j + 1) &&
                MoveOk(*check, i, j, new_i, j + 1)) {
              int di = position.kings[position.board[i][j].color].row - i;
              int dj = position.kings[position.board[i][j].color].column - j + 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j = position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][iter_j].color == position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j = position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][iter_j].color == position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
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
                int iter_i = i;
                int iter_j = j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[iter_i][iter_j].color == position.board[i][j].color) {
                    break;

                  } else if (position.board[iter_i][iter_j].color == opponent) {
                    if (position.board[iter_i][iter_j].type == 'B' ||
                        position.board[iter_i][iter_j].type == 'Q') {
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
          for (int k = 0; k < 8; k++) {
            new_i = i + knight_moves[k][0];
            new_j = j + knight_moves[k][1];
            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
                position.board[new_i][new_j].color != position.board[i][j].color) {
              if (MoveOk(*check, i, j, new_i, new_j)) {
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
          for (int k = 0; k < 4; k++) {
            new_i = i + bishop_moves[k][0];
            new_j = j + bishop_moves[k][1];

            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[new_i][new_j].color == position.board[i][j].color) {
                break;
              }
              if (MoveOk(*check, i, j, new_i, new_j)) {
                moves++;
              }
              if (position.board[new_i][new_j].color == opponent) {
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
          for (int k = 0; k < 4; k++) {
            new_i = i + rook_moves[k][0];
            new_j = j + rook_moves[k][1];
            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[new_i][new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(*check, i, j, new_i, new_j)) {
                moves++;
              }
              if (position.board[new_i][new_j].color == opponent) {
                break;
              }
              new_i = new_i + rook_moves[k][0];
              new_j = new_j + rook_moves[k][1];
            }
          }
          break;
        /*case 'K':
          for (int k = 0; k < 8; k++) {
            new_i = i + king_moves[k][0];
            new_j = j + king_moves[k][1];
            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
                position.board[new_i][new_j].color != position.board[i][j].color) {
              if (!InCheck(position, position.board[i][j].color, new_i,
                           new_j)) {
                moves++;
              }
            }
          }
          if (position.board[i][j].color == 'W'
                  ? position.castling.white_o_o
                  : position.castling.black_o_o &&
                        position.board[i][j + 1].color == ' ' &&
                        position.board[i][j + 2].color == ' ' &&
                        !check.in_check &&
                        !InCheck(position, position.board[i][j].color, i, j + 1) &&
                        !InCheck(position, position.board[i][j].color, i, j + 2)) {
            moves++;
          }
          if (position.board[i][j].color == 'W'
                  ? position.castling.white_o_o_o
                  : position.castling.black_o_o_o &&
                        position.board[i][j - 1].color == ' ' &&
                        position.board[i][j - 2].color == ' ' &&
                        position.board[i][j - 3].color == ' ' &&
                        !check.in_check &&
                        !InCheck(position, position.board[i][j].color, i, j - 1) &&
                        !InCheck(position, position.board[i][j].color, i, j - 2)) {
            moves++;
          }
          break;*/
      }

      if (position.board[i][j].color == 'W') {
        mobility_score += ((double) moves / piece_values[position.board[i][j].type]);
        // piece values
        evaluation += piece_values[position.board[i][j].type];
      } else {
        mobility_score -= ((double) moves / piece_values[position.board[i][j].type]);
        // piece values
        evaluation -= piece_values[position.board[i][j].type];
      }
    }
  }
  //if ((mobility_score / 100.0) > 1 || (mobility_score / 100.0) < -1) {
  //  int h = 4;
  //}

  position.evaluation = evaluation + (mobility_score / 1000.0);
  //int a;
}

bool was_capture(const Board& board1, const Board& board2) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (board1[i][j] != board2[i][j] && board1[i][j].color != ' ' &&
          board2[i][j].color != ' ') {
        return true;
      }
    }
  }
  return false;
}

// using Possibilities = std::map<Pair, double>;

// struct Minimax {
//   double evaluation;
//   // Castling castling;
//   HistoryType move_history;
//   // Possibilities possibilities;
// };

const char promotion_pieces[4] = {'Q', 'R', 'B', 'N'};

// void CopyBoard(std::string dest[8][8], const std::string src[8][8]) {
//     for (int i = 0; i < 8; i++)
//         for (int j = 0; j < 8; j++)
//             dest[i][j] = src[i][j];
// }

// void CopyCastling(bool src[4], bool dst[4]) {
//     for (int i = 0; i < 4; i++) {
//         dst[i] = src[i];
//     }
// }

// void CopyHistory(HistoryType& dst, HistoryType& src) {
//     for (int i = 0; i < src.size(); i++) {
//         dst[i] = src[i];
//     }
// }

int char_to_int(char c) { return (int)c - int('0'); }

const std::set<char> pieces = {'K', 'Q', 'R', 'B', 'N'};

// Board GetBoard(char piece, int dest[2],
//                          int start[2] /*might be unknown*/, bool capture,
//                          bool check, const Position& position) {
//  int new_i, new_j;
//  char other_color = position.to_move == 'W' ? 'B' : 'W';
//  Board new_board = position.board;
//  std::string object = position.to_move + piece;
//  switch (piece) {
//    case 'P': {
//      int multiplier = position.to_move == 'W' ? -1 : 1;
//      if (capture) {
//        if (start[0] != -1) {
//          new_board[dest[0] - multiplier][start[0]] = "  ";
//          new_board[dest[0]][dest[1]] = object;
//        } else {
//          if (new_board[dest[0] - multiplier][dest[1] - 1] == object) {
//            new_board[dest[0] - multiplier][dest[1] - 1] = "  ";
//            new_board[dest[0]][dest[1]] = piece + position.to_move;
//          } else if (new_board[dest[0] - multiplier][dest[1] + 1] == object) {
//            new_board[dest[0] - multiplier][dest[1] + 1] = "  ";
//            new_board[dest[0]][dest[1]] = piece + position.to_move;
//          } else if (new_board[dest[0]][dest[1] + 1] == object) {
//            //new // en passant
//          }
//        }
//      } else {
//        if (new_board[dest[0] - multiplier][dest[1]] == object) {
//          new_board[dest[0] - multiplier][dest[1]] = "  ";
//          new_board[dest[0]][dest[1]] = object;
//        } else if (new_board[dest[0] - (multiplier * 2)][dest[1]] ==
//                             object) {
//          new_board[dest[0] - (multiplier * 2)][dest[1]] = "  ";
//          new_board[dest[0]][dest[1]] = object;
//        }
//      }
//      return new_board;
//      break;
//    }
//    case 'N':
//      for (int k = 0; k < 8; k++) {
//        if (new_board[dest[0] + knight_moves[k][0]][dest[1] +
//        knight_moves[k][1]]
//                 [0] == position.to_move &&
//            new_board[dest[0] + knight_moves[k][0]][dest[1] +
//            knight_moves[k][1]]
//                 [1] == 'N' &&
//            (start[0] != -1 ? dest[0] + knight_moves[k][0] == start[0]
//                            : true) &&
//            (start[1] != -1 ? dest[1] + knight_moves[k][1] == start[1]
//                            : true)) {
//          new_board[dest[0] + knight_moves[k][0]][dest[1] +
//          knight_moves[k][1]] = "  "; new_board[dest[0]][dest[1]] = object;
//          return new_board;
//        }
//      }
//      break;
//    case 'B':
//      for (int k = 0; k < 4; k++) {
//        new_i = dest[0] + bishop_moves[k][0];
//        new_j = dest[1] + bishop_moves[k][1];
//        while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
//          if (board[new_i][new_j][0] == other_color) {
//            break;
//          }
//          if (board[new_i][new_j][1] == 'B' &&
//              board[new_i][new_j][0] == color &&
//              (start[0] != -1 ? new_i == start[0] : true) &&
//              (start[1] != -1 ? new_j == start[1] : true)) {
//            return {new_i, new_j};
//          }
//          new_i += bishop_moves[k][0];
//          new_j += bishop_moves[k][1];
//        }
//      }
//      break;
//  }
//}
const int mate = 10000;

// bool king_present(const Position& position) {
//   for (int i = 0; i < 8; i++) {
//     for (int j = 0; j < 8; j++) {
//       if (position.board[i][j][0] == position.to_move &&
//           position.board[i][j][1] == 'K') {
//         return true;
//       }
//     }
//   }
//   return false;
// }



bool board_exists(const Position& position, const Position& new_position) {
  if (position.outcomes == nullptr) {
    return false;
  }
  for (int i = 0; i < position.outcomes->size(); i++) {
    if ((*(*position.outcomes)[i]).board == new_position.board) {
      return true;
    }
  }
  return false;
}

//void generate_moves(Position& position) {
//  Position base_position = position.CreateShallowCopy();
//  base_position.previous_move = &position;
//  char opponent = position.to_move == 'W' ? 'B' : 'W';
//  base_position.to_move = opponent;
//  Position* new_position;
//  if (position.outcomes == nullptr) {
//    position.outcomes = new std::vector<Position*>;
//  }
//
//  int new_i, new_j;
//  for (int i = 0; i < 8; i++) {
//    for (int j = 0; j < 8; j++) {
//      if (position.board[i][j].color != position.to_move) {
//        continue;
//      }
//      switch (position.board[i][j].type) {
//        case 'P': {
//          int multiplier, promotion_row, starting_row;
//          if (position.to_move == 'W') {
//            multiplier = -1;
//            starting_row = 6;
//            promotion_row = 1;
//          } else {
//            multiplier = 1;
//            starting_row = 1;
//            promotion_row = 6;
//          }
//          bool left_capture =
//              j > 0 && position.board[i + multiplier][j - 1].color == opponent;
//          bool right_capture =
//              j < 7 && position.board[i + multiplier][j + 1].color == opponent;
//
//          if (i == promotion_row) {
//            // promotion
//            for (int k = 0; k < 4; k++) {
//              // one square
//              if (position.board[i + multiplier][j].color == ' ') {
//                new_position = base_position.CreateDeepCopy();
//                new_position->board[i + multiplier][j].color =
//                    new_position->board[i][j].color;
//                new_position->board[i + multiplier][j].type =
//                    promotion_pieces[k];
//                new_position->board[i][j].empty();
//                if (!InCheck(*new_position, position.to_move) &&
//                    !board_exists(position, *new_position)) {
//                  position.outcomes->emplace_back(new_position);
//                } else {
//                  delete new_position;
//                }
//              }
//              // captures to the left
//              if (left_capture) {
//                new_position = base_position.CreateDeepCopy();
//                new_position->board[i + multiplier][j - 1].color =
//                    new_position->board[i][j].color;
//                new_position->board[i + multiplier][j - 1].type =
//                    promotion_pieces[k];
//                new_position->board[i][j].empty();
//                if (!InCheck(*new_position, position.to_move) &&
//                    !board_exists(position, *new_position)) {
//                  position.outcomes->emplace_back(new_position);
//                } else {
//                  delete new_position;
//                }
//              }
//              // captures to the right
//              if (right_capture) {
//                new_position = base_position.CreateDeepCopy();
//                new_position->board[i + multiplier][j + 1].color =
//                    new_position->board[i][j].color;
//                new_position->board[i + multiplier][j + 1].type =
//                    promotion_pieces[k];
//                new_position->board[i][j].empty();
//                if (!InCheck(*new_position, position.to_move) &&
//                    !board_exists(position, *new_position)) {
//                  position.outcomes->emplace_back(new_position);
//                } else {
//                  delete new_position;
//                }
//              }
//            }
//          } else {
//            if (position.board[i + multiplier][j].color == ' ') {
//              // one square forward
//              new_position = base_position.CreateDeepCopy();
//              new_position->board[i + multiplier][j] =
//                  new_position->board[i][j];
//              new_position->board[i][j].empty();
//              if (!InCheck(*new_position, position.to_move) &&
//                  !board_exists(position, *new_position)) {
//                position.outcomes->emplace_back(new_position);
//              } else {
//                delete new_position;
//              }
//              // two squares forward
//              if (i == starting_row &&
//                  position.board[i + multiplier * 2][j].color == ' ') {
//                new_position = base_position.CreateDeepCopy();
//                new_position->board[i + (multiplier * 2)][j] =
//                    new_position->board[i][j];
//                new_position->board[i][j].empty();
//                if (!InCheck(*new_position, position.to_move) &&
//                    !board_exists(position, *new_position)) {
//                  position.outcomes->emplace_back(new_position);
//                } else {
//                  delete new_position;
//                }
//              }
//            }
//            // captures to the left
//            if (left_capture) {
//              new_position = base_position.CreateDeepCopy();
//              new_position->board[i + multiplier][j - 1] =
//                  new_position->board[i][j];
//              new_position->board[i][j].empty();
//              if (!InCheck(*new_position, position.to_move) &&
//                  !board_exists(position, *new_position)) {
//                position.outcomes->emplace_back(new_position);
//              } else {
//                delete new_position;
//              }
//            }
//            // captures to the right
//            if (j < 7 &&
//                position.board[i + multiplier][j + 1].color == opponent) {
//              new_position = base_position.CreateDeepCopy();
//              new_position->board[i + multiplier][j + 1] =
//                  new_position->board[i][j];
//              new_position->board[i][j].empty();
//              if (!InCheck(*new_position, position.to_move) &&
//                  !board_exists(position, *new_position)) {
//                position.outcomes->emplace_back(new_position);
//              } else {
//                delete new_position;
//              }
//            }
//            // en passant to the left
//            int en_passant_start = (int)(3.5 + (0.5 * multiplier));
//            if (i == en_passant_start && j > 0 &&
//                position.board[i][j - 1].color == opponent &&
//                position.board[i][j - 1].type == 'P' &&
//                position.board[i + multiplier][j - 1].color == ' ' &&
//                position.board[promotion_row][j - 1].color == ' ' &&
//                position.previous_move &&
//                position.previous_move->board[promotion_row][j - 1].color ==
//                    opponent &&
//                position.previous_move->board[promotion_row][j - 1].type ==
//                    'P') {
//              new_position = base_position.CreateDeepCopy();
//              new_position->board[promotion_row - multiplier][j - 1] =
//                  new_position->board[i][j];
//              new_position->board[i][j].empty();
//              new_position->board[i][j - 1].empty();
//              if (!InCheck(*new_position, position.to_move) &&
//                  !board_exists(position, *new_position)) {
//                position.outcomes->emplace_back(new_position);
//              } else {
//                delete new_position;
//              }
//            }
//            // en passant to the right
//            if (i == en_passant_start && j < 7 &&
//                position.board[i][j + 1].color == opponent &&
//                position.board[i][j + 1].type == 'P' &&
//                position.board[i + multiplier][j + 1].color == ' ' &&
//                position.board[promotion_row][j + 1].color == ' ' &&
//                position.previous_move &&
//                position.previous_move->board[promotion_row][j + 1].color ==
//                    opponent &&
//                position.previous_move->board[promotion_row][j + 1].type ==
//                    'P') {
//              new_position = base_position.CreateDeepCopy();
//              new_position->board[i + multiplier][j + 1] =
//                  new_position->board[i][j];
//              new_position->board[i][j].empty();
//              new_position->board[i][j + 1].color =
//                  new_position->board[i][j + 1].type = ' ';
//              if (!InCheck(*new_position, position.to_move) &&
//                  !board_exists(position, *new_position)) {
//                position.outcomes->emplace_back(new_position);
//              } else {
//                delete new_position;
//              }
//            }
//          }
//          
//          break;
//        }
//        case 'N':
//          for (int k = 0; k < 8; k++) {
//            new_i = i + knight_moves[k][0];
//            new_j = j + knight_moves[k][1];
//            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
//                position.board[new_i][new_j].color != position.to_move) {
//              new_position = base_position.CreateDeepCopy();
//              new_position->board[new_i][new_j] = position.board[i][j];
//              new_position->board[i][j].empty();
//              if (!InCheck(*new_position, position.to_move) &&
//                  !board_exists(position, *new_position)) {
//                position.outcomes->emplace_back(new_position);
//              } else {
//                delete new_position;
//              }
//            }
//          }
//          break;
//        case 'Q':
//        case 'B':
//          for (int k = 0; k < 4; k++) {
//            new_i = i + bishop_moves[k][0];
//            new_j = j + bishop_moves[k][1];
//
//            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
//              if (position.board[new_i][new_j].color == position.to_move) {
//                break;
//              }
//              new_position = base_position.CreateDeepCopy();
//              new_position->board[new_i][new_j] = position.board[i][j];
//              new_position->board[i][j].empty();
//              if (!InCheck(*new_position, position.to_move) &&
//                  !board_exists(position, *new_position)) {
//                position.outcomes->emplace_back(new_position);
//              } else {
//                delete new_position;
//              }
//              if (position.board[new_i][new_j].color == opponent) {
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
//          for (int k = 0; k < 4; k++) {
//            new_i = i + rook_moves[k][0];
//            new_j = j + rook_moves[k][1];
//            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
//              if (position.board[new_i][new_j].color == position.to_move) {
//                break;
//              }
//              new_position = base_position.CreateDeepCopy();
//              new_position->board[new_i][new_j] = position.board[i][j];
//              new_position->board[i][j].empty();
//              if (!InCheck(*new_position, position.to_move) &&
//                  !board_exists(position, *new_position)) {
//                if (position.board[7][7] == "WR" &&
//                    new_position->board[7][7] == "  ") {
//                  new_position->castling.white_o_o = false;
//                } else if (position.board[7][0] == "WR" &&
//                           new_position->board[7][0] == "  ") {
//                  new_position->castling.white_o_o_o = false;
//                } else if (position.board[0][7] == "BR" &&
//                           new_position->board[0][7] == "  ") {
//                  new_position->castling.black_o_o = false;
//                } else if (position.board[0][0] == "WR" &&
//                           new_position->board[0][0] == "  ") {
//                  new_position->castling.black_o_o_o = false;
//                }
//
//                position.outcomes->emplace_back(new_position);
//              } else {
//                delete new_position;
//              }
//              if (position.board[new_i][new_j].color == opponent) {
//                break;
//              }
//              new_i = new_i + rook_moves[k][0];
//              new_j = new_j + rook_moves[k][1];
//            }
//          }
//          break;
//        case 'K':
//          for (int k = 0; k < 8; k++) {
//            new_i = i + king_moves[k][0];
//            new_j = j + king_moves[k][1];
//            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
//                position.board[new_i][new_j].color != position.to_move) {
//              new_position = base_position.CreateDeepCopy();
//              new_position->board[new_i][new_j] = position.board[i][j];
//              new_position->board[i][j].empty();
//              new_position->kings[position.to_move] = {new_i, new_j};
//              if (!InCheck(*new_position, position.to_move) &&
//                  !board_exists(position, *new_position)) {
//                if (position.to_move == 'W') {
//                  new_position->castling.white_o_o =
//                      new_position->castling.white_o_o_o = false;
//                } else {
//                  new_position->castling.black_o_o =
//                      new_position->castling.black_o_o_o = false;
//                }
//                position.outcomes->emplace_back(new_position);
//              } else {
//                delete new_position;
//              }
//            }
//          }
//          if ((position.to_move == 'W'
//                  ? position.castling.white_o_o
//                  : position.castling.black_o_o) &&
//              position.board[i][j + 1].color == ' ' &&
//              position.board[i][j + 2].color == ' ' &&
//              !InCheck(position, position.to_move) &&
//              !InCheck(position, position.to_move, i, j + 1) &&
//              !InCheck(position, position.to_move, i, j + 2)) {
//            new_position = base_position.CreateDeepCopy();
//            new_position->board[i][6].set(position.to_move, 'K');
//            new_position->board[i][5].set(position.to_move, 'R');
//            new_position->kings[position.to_move] = {i, 6};
//            new_position->board[i][4].empty();
//            new_position->board[i][7].empty();
//            if (position.to_move == 'W') {
//              new_position->castling.white_o_o = new_position->castling.white_o_o_o = false;
//            } else {
//              new_position->castling.black_o_o = new_position->castling.black_o_o_o = false;
//            }
//            if (!board_exists(position, *new_position)) {
//              position.outcomes->emplace_back(new_position);
//            } else {
//              delete new_position;
//            }
//          }
//          if ((position.to_move == 'W'
//                  ? position.castling.white_o_o_o
//                  : position.castling.black_o_o_o) &&
//              position.board[i][j - 1].color == ' ' &&
//              position.board[i][j - 2].color == ' ' &&
//              position.board[i][j - 3].color == ' ' &&
//              !InCheck(position, position.to_move) &&
//              !InCheck(position, position.to_move, i, j - 1) &&
//              !InCheck(position, position.to_move, i, j - 2)) {
//            new_position = base_position.CreateDeepCopy();
//            new_position->board[i][2].set(position.to_move, 'K');
//            new_position->board[i][3].set(position.to_move, 'R');
//            new_position->board[i][4].empty();
//            new_position->board[i][0].empty();
//            new_position->kings[position.to_move] = {i, 2};
//            if (position.to_move == 'W') {
//              new_position->castling.white_o_o =
//                  new_position->castling.white_o_o_o = false;
//            } else {
//              new_position->castling.black_o_o =
//                  new_position->castling.black_o_o_o = false;
//            }
//            if (!board_exists(position, *new_position)) {
//              position.outcomes->emplace_back(new_position);
//            } else {
//              delete new_position;
//            }
//          }
//
//          break;
//      }
//    }
//  }
//  /*if (position.outcomes->size() == 0) {
//    delete position.outcomes;
//    position.outcomes = nullptr;
//  }*/
//  // for (int i = 0; i < moves.size(); i++) {
//  //     for (int j = 0; j < 8; j++) {
//  //         for (int k = 0; k < 8; k++) {
//  //             std::cout << moves[i].first[j][k];
//  //         }
//  //         std::cout << std::endl;
//  //     }
//  // }
//  // return;
//}
//







void new_generate_moves(Position& position) {
  Position base_position = position.CreateShallowCopy();
  //base_position.previous_move = &position;
  char opponent = position.white_to_move ? 'B' : 'W';
  base_position.to_move = opponent;
  Position* new_position;
  if (position.outcomes == nullptr) {
    position.outcomes = new std::vector<Position*>;
  }
  Check check_info;
  GetCheckInfo(check_info, position);

  int new_i, new_j;
  //for (int i = 0; i <= 7; i++) {
  //  for (int j = 0; j <= 7; j++) {
  //    if (position.board[i][j].color == ' '/* ||
  //        position.board[i][j].type == 'K'*/) {
  //      continue;
  //    }
  //    //// counting moves
  //    //int addition = position.board[i][j].color == 'W' ? 1 : -1;
  //    //opponent = position.board[i][j].color == 'W' ? 'B' : 'W';
  //    //double moves = 0;
  //    switch (position.board[i][j].type) {

  //        case 'K':
  //          for (int k = 0; k < 8; k++) {
  //            new_i = i + king_moves[k][0];
  //            new_j = j + king_moves[k][1];
  //            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
  //                position.board[new_i][new_j].color !=
  //          position.board[i][j].color) { if (!InCheck(position,
  //          position.board[i][j].color, new_i, new_j)) { moves++;
  //              }
  //            }
  //          }
  //          if (position.board[i][j].color == 'W'
  //                  ? position.castling.white_o_o
  //                  : position.castling.black_o_o &&
  //                        position.board[i][j + 1].color == ' ' &&
  //                        position.board[i][j + 2].color == ' ' &&
  //                        !check.in_check &&
  //                        !InCheck(position, position.board[i][j].color, i, j +
  //          1) && !InCheck(position, position.board[i][j].color, i, j + 2)) {
  //            moves++;
  //          }
  //          if (position.board[i][j].color == 'W'
  //                  ? position.castling.white_o_o_o
  //                  : position.castling.black_o_o_o &&
  //                        position.board[i][j - 1].color == ' ' &&
  //                        position.board[i][j - 2].color == ' ' &&
  //                        position.board[i][j - 3].color == ' ' &&
  //                        !check.in_check &&
  //                        !InCheck(position, position.board[i][j].color, i, j -
  //          1) && !InCheck(position, position.board[i][j].color, i, j - 2)) {
  //            moves++;
  //          }
  //          break;
  //    //}
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (position.board[i][j].color != position.to_move) {
        continue;
      }
      int evaluation_multiplier = position.board[i][j].color == 'W' ? 1 : -1;
      switch (position.board[i][j].type) {
        case 'P': {
          if (check_info.king_must_move) {
            break;
          }
          int multiplier, promotion_row, starting_row;
          if (position.board[i][j].color == 'W') {
            multiplier = -1;
            starting_row = 6;
            promotion_row = 1;
          } else {
            multiplier = 1;
            starting_row = 1;
            promotion_row = 6;
          }
          bool left_capture =
              j > 0 && position.board[i + multiplier][j - 1].color == opponent;
          bool right_capture =
              j < 7 && position.board[i + multiplier][j + 1].color == opponent;
          new_i = i + multiplier;
          if (i == promotion_row) {
            // promotion
            for (int k = 0; k < 4; k++) {
              // one square
              if (position.board[new_i][j].color == ' ' &&
                  MoveOk(check_info, i, j, new_i, j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[i + multiplier][j].color =
                    new_position->board[i][j].color;
                new_position->board[i + multiplier][j].type =
                    promotion_pieces[k];
                new_position->board[i][j].empty();
                new_position->evaluation +=
                    piece_values[promotion_pieces[k]] - 1;
                new_position->fifty_move_rule = 0;
                position.outcomes->emplace_back(new_position);
              }
              // captures to the left
              if (left_capture && MoveOk(check_info, i, j, new_i, j - 1)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[i + multiplier][j - 1].color =
                    new_position->board[i][j].color;
                new_position->board[i + multiplier][j - 1].type =
                    promotion_pieces[k];
                new_position->board[i][j].empty();
                new_position->was_capture = true;
                new_position->fifty_move_rule = 0;
                new_position->evaluation +=
                    (-
                    multiplier *
                    piece_values[position.board[new_i][j - 1].type]) + piece_values[promotion_pieces[k]] - 1;
                position.outcomes->emplace_back(new_position);
              }
              // captures to the right
              if (right_capture && MoveOk(check_info, i, j, new_i, j + 1)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[i + multiplier][j + 1].color =
                    new_position->board[i][j].color;
                new_position->board[i + multiplier][j + 1].type =
                    promotion_pieces[k];
                new_position->board[i][j].empty();
                new_position->was_capture = true;
                new_position->fifty_move_rule = 0;
                new_position->evaluation +=
                    (-multiplier *
                        piece_values[position.board[new_i][j + 1].type]) +
                    piece_values[promotion_pieces[k]] - 1;
                position.outcomes->emplace_back(new_position);
              }
            }
          } else {
            if (position.board[new_i][j].color == ' ') {
              // one square forward
              if (MoveOk(check_info, i, j, new_i, j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[new_i][j] = new_position->board[i][j];
                new_position->board[i][j].empty();
                new_position->fifty_move_rule = 0;
                position.outcomes->emplace_back(new_position);
              }
              // two squares forward
              if (i == starting_row &&
                  position.board[i + multiplier * 2][j].color == ' ' &&
                  MoveOk(check_info, i, j, i + (multiplier * 2), j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[i + (multiplier * 2)][j] =
                    new_position->board[i][j];
                new_position->fifty_move_rule = 0;
                new_position->board[i][j].empty();

                position.outcomes->emplace_back(new_position);
              }
            }
            // captures to the left
            if (left_capture && MoveOk(check_info, i, j, new_i, j - 1)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[i + multiplier][j - 1] =
                  new_position->board[i][j];
              new_position->board[i][j].empty();
              new_position->was_capture = true;
              new_position->evaluation +=
                  (-multiplier *
                   piece_values[position.board[new_i][j - 1].type]);
              new_position->fifty_move_rule = 0;

              position.outcomes->emplace_back(new_position);
            }
            // captures to the right
            if (j < 7 &&
                position.board[i + multiplier][j + 1].color == opponent &&
                MoveOk(check_info, i, j, new_i, j + 1)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[i + multiplier][j + 1] =
                  new_position->board[i][j];
              new_position->board[i][j].empty();
              new_position->was_capture = true;
              new_position->evaluation +=
                  (-multiplier *
                   piece_values[position.board[new_i][j + 1].type]);
              new_position->fifty_move_rule = 0;
              position.outcomes->emplace_back(new_position);
            }
            // en passant to the left
            int en_passant_start = position.white_to_move ? 3 : 4;
            if (i == en_passant_start && j > 0 &&
                position.board[i][j - 1].color == opponent &&
                position.board[i][j - 1].type == 'P' &&
                position.en_passant_target.Equals(i + multiplier, j - 1) &&
                MoveOk(check_info, i, j, new_i, j - 1)) {
              int di = position.kings[position.board[i][j].color].row - i;
              int dj =
                  position.kings[position.board[i][j].color].column - j - 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j =
                      position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
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
                int iter_i = i;
                int iter_j = j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[iter_i][iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[iter_i][iter_j].color == opponent) {
                    if (position.board[iter_i][iter_j].type == 'B' ||
                        position.board[iter_i][iter_j].type == 'Q') {
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
                new_position->board[promotion_row - multiplier][j - 1] =
                    new_position->board[i][j];
                new_position->board[i][j].empty();
                new_position->board[i][j - 1].empty();
                new_position->was_capture = true;
                new_position->evaluation -= multiplier;
                new_position->fifty_move_rule = 0;
                position.outcomes->emplace_back(new_position);
              }
            }
            // en passant to the right
            if (i == en_passant_start && j < 7 &&
                position.board[i][j + 1].color == opponent &&
                position.board[i][j + 1].type == 'P' &&
                position.en_passant_target.Equals(new_i, j + 1) &&
                MoveOk(check_info, i, j, new_i, j + 1)) {
              int di = position.kings[position.board[i][j].color].row - i;
              int dj =
                  position.kings[position.board[i][j].color].column - j + 1;
              bool good = true;
              if (position.kings[position.board[i][j].color].row == i) {
                if (position.kings[position.board[i][j].color].column > j) {
                  int iter_j =
                      position.kings[position.board[i][j].color].column;
                  while (iter_j >= 0) {
                    if (position.board[i][iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
                        good = false;
                        break;
                      }
                    }
                    iter_j--;
                  }
                } else {
                  int iter_j =
                      position.kings[position.board[i][j].color].column;
                  while (iter_j <= 7) {
                    if (position.board[i][iter_j].color ==
                        position.board[i][j].color) {
                      break;
                    }
                    if (position.board[i][iter_j].color == opponent) {
                      if (position.board[i][iter_j].type == 'R' ||
                          position.board[i][iter_j].type == 'Q') {
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
                int iter_i = i;
                int iter_j = j;

                while (iter_i >= 0 && iter_i <= 7 && iter_j >= 0 &&
                       iter_j <= 7) {
                  if (position.board[iter_i][iter_j].color ==
                      position.board[i][j].color) {
                    break;

                  } else if (position.board[iter_i][iter_j].color == opponent) {
                    if (position.board[iter_i][iter_j].type == 'B' ||
                        position.board[iter_i][iter_j].type == 'Q') {
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
                new_position->board[i][j].empty();
                new_position->board[i][j + 1].color =
                    new_position->board[i][j + 1].type = ' ';
                new_position->was_capture = true;
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
          for (int k = 0; k < 8; k++) {
            new_i = i + knight_moves[k][0];
            new_j = j + knight_moves[k][1];
            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
                position.board[new_i][new_j].color !=
                    position.board[i][j].color && MoveOk(check_info, i, j, new_i, new_j)) {
              new_position = base_position.CreateDeepCopy();
              new_position->board[new_i][new_j] = position.board[i][j];
              new_position->board[i][j].empty();
              if (!position.board[new_i][new_j].IsEmpty()) {
                new_position->was_capture = true;
                new_position->evaluation +=
                    (evaluation_multiplier *
                     piece_values[position.board[new_i][new_j].type]);
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
          for (int k = 0; k < 4; k++) {
            new_i = i + bishop_moves[k][0];
            new_j = j + bishop_moves[k][1];

            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[new_i][new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(check_info, i, j, new_i, new_j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[new_i][new_j] = position.board[i][j];
                new_position->board[i][j].empty();
                if (!position.board[new_i][new_j]
                                     .IsEmpty()) {
                  new_position->was_capture = true;
                  new_position->fifty_move_rule = 0;
                  new_position->evaluation +=
                      (evaluation_multiplier *
                       piece_values[position.board[new_i][new_j].type]);
                }
                position.outcomes->emplace_back(new_position);
              }
              if (position.board[new_i][new_j].color == opponent) {
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
          for (int k = 0; k < 4; k++) {
            new_i = i + rook_moves[k][0];
            new_j = j + rook_moves[k][1];
            while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
              if (position.board[new_i][new_j].color ==
                  position.board[i][j].color) {
                break;
              }
              if (MoveOk(check_info, i, j, new_i, new_j)) {
                new_position = base_position.CreateDeepCopy();
                new_position->board[new_i][new_j] = position.board[i][j];
                new_position->board[i][j].empty();
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
                  if (!position.board[new_i][new_j].IsEmpty()) {
                    new_position->was_capture = true;
                    new_position->fifty_move_rule = 0;
                    new_position->evaluation +=
                        (evaluation_multiplier *
                         piece_values[position.board[new_i][new_j].type]);
                  }
                  position.outcomes->emplace_back(new_position);
              }
              if (position.board[new_i][new_j].color == opponent) {
                break;
              }
              new_i = new_i + rook_moves[k][0];
              new_j = new_j + rook_moves[k][1];
            }
          }
          break;
        case 'K':
          for (int k = 0; k < 8; k++) {
            new_i = i + king_moves[k][0];
            new_j = j + king_moves[k][1];
            if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
                position.board[new_i][new_j].color != position.to_move && !InCheck(position, position.to_move, new_i, new_j))  {
              new_position = base_position.CreateDeepCopy();
              new_position->board[new_i][new_j] = position.board[i][j];
              new_position->board[i][j].empty();
              new_position->kings[position.to_move] = {new_i, new_j};
              //if (!InCheck(*new_position, position.to_move)) {
                if (position.to_move == 'W') {
                  new_position->castling.white_o_o =
                      new_position->castling.white_o_o_o = false;
                } else {
                  new_position->castling.black_o_o =
                      new_position->castling.black_o_o_o = false;
                }
                if (!position.board[new_i][new_j].IsEmpty()) {
                  new_position->was_capture = true;
                  new_position->fifty_move_rule = 0;
                  new_position->evaluation +=
                      (evaluation_multiplier *
                       piece_values[position.board[new_i][new_j].type]);
                }

                position.outcomes->emplace_back(new_position);
              //} else {
              //  delete new_position;
              //}
            }
          }
          if ((position.to_move == 'W'
                  ? position.castling.white_o_o
                  : position.castling.black_o_o) &&
                        position.board[i][j + 1].color == ' ' &&
                        position.board[i][j + 2].color == ' ' &&
                        !check_info.in_check &&
                        !InCheck(position, position.to_move, i, j + 1) &&
                        !InCheck(position, position.to_move, i, j + 2)) {
            new_position = base_position.CreateDeepCopy();
            new_position->board[i][6].set(position.to_move, 'K');
            new_position->board[i][5].set(position.to_move, 'R');
            new_position->kings[position.to_move] = {i, 6};
            new_position->board[i][4].empty();
            new_position->board[i][7].empty();
            if (position.to_move == 'W') {
              new_position->castling.white_o_o =
                  new_position->castling.white_o_o_o = false;
            } else {
              new_position->castling.black_o_o =
                  new_position->castling.black_o_o_o = false;
            }
            position.outcomes->emplace_back(new_position);
          }
          if ((position.to_move == 'W'
                  ? position.castling.white_o_o_o
                  : position.castling.black_o_o_o) &&
                        position.board[i][j - 1].color == ' ' &&
                        position.board[i][j - 2].color == ' ' &&
                        position.board[i][j - 3].color == ' ' &&
                        !check_info.in_check &&
                        !InCheck(position, position.to_move, i, j - 1) &&
                        !InCheck(position, position.to_move, i, j - 2)) {
            new_position = base_position.CreateDeepCopy();
            new_position->board[i][2].set(position.to_move, 'K');
            new_position->board[i][3].set(position.to_move, 'R');
            new_position->board[i][4].empty();
            new_position->board[i][0].empty();
            new_position->kings[position.to_move] = {i, 2};
            if (position.to_move == 'W') {
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
    for (int i = 0; i < position.outcomes->size(); i++) {
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
        start({-1, -1}),
        dest({-1, -1}),
        capture(false),
        checkmate(false),
        promotion(' '),
        piece(' ') {}
};

Board GetBoard(Move info, const Position& position) {
  Board board = position.board;
  switch (info.piece) {
    case 'P': {
      int multiplier = position.to_move == 'W' ? -1 : 1;
      if (info.capture) {
        if (board[info.dest[0]][info.dest[1]].color == ' ') {
          // en passant
          board[info.dest[0]][info.dest[1]] =
              board[info.dest[0] - multiplier][info.start[1]];
          board[info.dest[0] - multiplier][info.start[1]].empty();
          board[info.dest[0] - multiplier][info.dest[1]].empty();
        } else {
          board[info.dest[0]][info.dest[1]] =
              board[info.dest[0] - multiplier][info.start[1]];
          board[info.dest[0] - multiplier][info.start[1]].empty();
        }
      } else if (board[info.dest[0] - multiplier][info.dest[1]].type == 'P') {
        board[info.dest[0]][info.dest[1]] =
            board[info.dest[0] - multiplier][info.dest[1]];
        board[info.dest[0] - multiplier][info.dest[1]].empty();
      } else {
        board[info.dest[0]][info.dest[1]] =
            board[info.dest[0] - multiplier * 2][info.dest[1]];
        board[info.dest[0] - multiplier * 2][info.dest[1]].empty();
      }
      if (info.promotion != ' ') {
        board[info.dest[0]][info.dest[1]].type = info.promotion;
      }
      return board;
    }
    case 'N':
      for (int k = 0; k < 8; k++) {
        int i = info.dest[0] + knight_moves[k][0];
        int j = info.dest[1] + knight_moves[k][1];
        if (i >= 0 && i <= 7 && j >= 0 && j <= 7 && board[i][j].color == position.to_move && board[i][j].type == 'N' &&
            (info.start[0] != -1 ? i == info.start[0] : true) &&
            (info.start[1] != -1 ? j == info.start[1] : true)) {
          board[info.dest[0]][info.dest[1]] = board[i][j];
          board[i][j].empty();
          break;
        }
      }
      return board;
    case 'Q':
    case 'B': {
      char opponent = position.to_move == 'W' ? 'B' : 'W';
      for (int k = 0; k < 4; k++) {
        int i = info.dest[0] + bishop_moves[k][0];
        int j = info.dest[1] + bishop_moves[k][1];
        while (i >= 0 && i <= 7 && j >= 0 && j <= 7) {
          if (board[i][j].color == opponent) {
            break;
          }
          if (board[i][j].type == info.piece &&
              (info.start[0] != -1 ? i == info.start[0] : true) &&
              (info.start[1] != -1 ? j == info.start[1] : true)) {
            board[info.dest[0]][info.dest[1]] = board[i][j];
            board[i][j].empty();
            return board;
          }
          if (board[i][j].color == position.to_move) {
            break;
          }
          i += bishop_moves[k][0];
          j += bishop_moves[k][1];
        }
      }
    }
    case 'R': {
      char opponent = position.to_move == 'W' ? 'B' : 'W';
      for (int k = 0; k < 4; k++) {
        int i = info.dest[0] + rook_moves[k][0];
        int j = info.dest[1] + rook_moves[k][1];
        while (i >= 0 && i <= 7 && j >= 0 && j <= 7) {
          if (board[i][j].color == opponent) {
            break;
          }
          if (board[i][j].type == info.piece &&
              (info.start[0] != -1 ? i == info.start[0] : true) &&
              (info.start[1] != -1 ? j == info.start[1] : true)) {
            board[info.dest[0]][info.dest[1]] = board[i][j];
            board[i][j].empty();
            return board;
          }
          if (board[i][j].color == position.to_move) {
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
      for (int k = 0; k < 8; k++) {
        new_i = info.dest[0] + king_moves[k][0];
        new_j = info.dest[1] + king_moves[k][1];
        if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
          if (position.board[new_i][new_j].type == 'K') {
            board[info.dest[0]][info.dest[1]] = board[new_i][new_j];
            board[new_i][new_j].empty();
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
    if (position.to_move == 'W') {
      new_board[7][6].set('W', 'K');
      new_board[7][5].set('W', 'R');
      new_board[7][4].empty();
      new_board[7][7].empty();
    } else {
      new_board[0][6].set('B', 'K');
      new_board[0][5].set('B', 'R');
      new_board[0][4].empty();
      new_board[0][7].empty();
    }
    return FindPosition(new_board, position);
  }
  if (move == "O-O-O" || move == "0-0-0") {
    if (position.to_move == 'W') {
      new_board[7][2].set('W', 'K');
      new_board[7][3].set('W', 'R');
      new_board[7][4].empty();
      new_board[7][0].empty();
    } else {
      new_board[0][2].set('B', 'K');
      new_board[0][3].set('B', 'R');
      new_board[0][4].empty();
      new_board[0][0].empty();
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
  if (pieces.count(move[current])) {
    info.promotion = move[current];
    current -= 2;
  }
  if (move[current] == '+') {
    info.check = true;
    current--;
  }
  if (move[current] == '#') {
    info.checkmate = true;
    current--;
  }
  info.dest[0] = 8 - char_to_int(move[current]);
  current--;
  info.dest[1] = move[current] - 'a';
  current--;
  if (current == -1) {
    return FindPosition(GetBoard(info, position), position);
  }
  if (move[current] == 'x') {
    info.capture = true;
    current--;
    if (current == -1) {
      return FindPosition(GetBoard(info, position), position);
    }
  }
  if (isdigit(move[current])) {
    info.start[0] = 8 - char_to_int(move[current]);
    current--;
    if (current == -1) {
      return FindPosition(GetBoard(info, position), position);
    }
  }
  if (current != 0 || info.piece == 'P') {
    info.start[1] = move[current] - 'a';
  }
  return FindPosition(GetBoard(info, position), position);
}

// void SetPosition(std::string& move, Position& position) {
//   char piece;
//   int dest[2];
//   int start[2] = {-1, -1};
//   std::vector<int> starting;
//   Board new_board = position.board;
//   bool check;
//   bool capture;
//   if (move == "O-O") {
//     if (position.to_move == 'W') {
//       new_board[7][6] = "WK";
//       new_board[7][5] = "WR";
//       new_board[7][4] = "  ";
//       new_board[7][7] = "  ";
//       position = *FindPosition(new_board, position);
//
//     } else {
//       new_board[0][6] = "BK";
//       new_board[0][5] = "BR";
//       new_board[0][4] = "  ";
//       new_board[0][7] = "  ";
//       position = *FindPosition(new_board, position);
//     }
//     return;
//   }
//   if (move == "O-O-O" || move == "0-0-0") {
//     if (position.to_move == 'W') {
//       new_board[7][2] = "WK";
//       new_board[7][3] = "WR";
//       new_board[7][4] = "  ";
//       new_board[7][0] = "  ";
//       position = *FindPosition(new_board, position);
//     } else {
//       new_board[0][2] = "BK";
//       new_board[0][3] = "BR";
//       new_board[0][4] = "  ";
//       new_board[0][0] = "  ";
//       position = *FindPosition(new_board, position);
//     }
//     /*
//     new_position = FindPosition(new_board, *position);
//       if (new_position == nullptr) {
//         position = new_position;
//       } else {
//         std::cout << "Invalid move." << std::endl;
//         continue;
//       }
//     */
//     return;
//   }
//   if (pieces.count(move[0])) {
//     piece = move[0];
//   } else {
//     piece = 'P';
//   }
//   size_t current = move.length() - 1;
//   if (move[current] == '+') {
//     check = true;
//     current--;
//   } else {
//     check = false;
//   }
//   dest[0] = 8 - char_to_int(move[current]);
//   current--;
//   dest[1] = move[current] - 'a';
//   current--;
//   /*
//   if (current == -1) {
//     new_board = GetBoard(piece, dest, start, false, check, position);
//     position = *FindPosition(new_board, position);
//     return;
//   }
//   if (move[current] == 'x') {
//     capture = true;
//     current--;
//     if (current == -1) {
//       starting = getPiece(piece, dest, board, start, capture, check, color);
//       output.first[dest[0]][dest[1]] =
//       output.first[starting[0]][starting[1]];
//       output.first[starting[0]][starting[1]] = "  ";
//       return output;
//     }
//   } else {
//     capture = false;
//   }
//   if (isdigit(move[current])) {
//     start[1] = 8 - char_to_int(move[current]);
//     current--;
//   }
//   if (islower(move[current])) {
//     start[0] = move[current] - 'a';
//     current--;
//   }
//   starting = getPiece(piece, dest, board, start, capture, check, color);
//   output.first[dest[0]][dest[1]] = output.first[starting[0]][starting[1]];
//   output.first[starting[0]][starting[1]] = "  ";
//   return output;
// }
// void SaveBoard(const Board& board, const Castling& castling) {
//   // write board and castling to file
//   std::ofstream file;
//   file.open("board.txt");
//   for (int i = 0; i < 8; i++) {
//     for (int j = 0; j < 8; j++) {
//       file << board[i][j] << " ";
//     }
//     file << std::endl;
//   }
//   for (int i = 0; i < 4; i++) {
//     file << castling[i] << " ";
//   }
//   file.close();*/
// }
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
  for (int i = 7; i >= 0; i--) {
    result << "  +----+----+----+----+----+----+----+----+\n"
           << (chess_notation ? (8 - i) : i) << " |";
    for (int j = 7; j >= 0; j--) {
      result << ' ' << position.board[i][j].color << position.board[i][j].type
             << " |";
    }
    result << '\n';
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





//void SavePosition(Position& position) {
//  std::ofstream file;
//  file.open("position.txt");
//  if (!file.is_open()) {
//    return;
//  }
//  // board castling to_move move history kings
//  /* Position* first_move = &position;
//   while (first_move->previous_move != nullptr) {
//    first_move = first_move->previous_move;
//  }
//  while (first_move != &position) {
//    file << MakeString(*first_move, false) << "\n"
//         << first_move->castling[0] << first_move->castling[1]
//         << first_move->castling[2] << first_move->castling[3] << "\n\n";
//    first_move = first_move->outcomes[0];
//  } // move_history
//  file << "---";*/
//  file << MakeString(position, false) << "\n"
//       << position.castling.white_o_o << position.castling.white_o_o_o << position.castling.black_o_o
//       << position.castling.black_o_o_o << "\n"
//       << position.to_move;
//  file.close();
//
//  // MakeString(position);
//}
void ReadPosition(Position& position) {
  std::ifstream file;
  file.open("position.txt");
  if (!file.is_open()) {
    return;
  }
  file.ignore(88);
  for (int i = 0; i < 8; i++) {
    file.ignore(4);
    for (int j = 0; j < 8; j++) {
      char piece[5];
      file.read(piece, sizeof(piece));
      position.board[i][j].color = piece[0];
      position.board[i][j].type = piece[1];
    }
    // skip newline
    file.ignore(44);
  }

  char castle;
  file.read(&castle, sizeof(castle));
  position.castling.white_o_o = castle == '1';
  file.read(&castle, sizeof(castle));
  position.castling.white_o_o_o = castle == '1';
  file.read(&castle, sizeof(castle));
  position.castling.black_o_o = castle == '1';
  file.read(&castle, sizeof(castle));
  position.castling.black_o_o_o = castle == '1';

  file.ignore(1);
  char to_move[1];
  file.read(to_move, sizeof(to_move));
  position.to_move = to_move[0];
  position.white_to_move = position.to_move == 'W';
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (position.board[i][j].type == 'K') {
        position.kings[position.board[i][j].color].row = i;
        position.kings[position.board[i][j].color].column = j;
        break;
      }
    }
  }
  file.close();
}

// void ReadBoard(Board& board, Castling& castling) {
//   // write board and castling to file
//   std::ifstream file;
//   file.open("board.txt");
//   if (!file.is_open()) return;
//   for (int i = 0; i < 8; i++) {
//     for (int j = 0; j < 8; j++) {
//       char piece[3];
//       file.read(piece, sizeof(piece));
//       board[i][j] = piece[0];
//       board[i][j] += piece[1];
//     }
//     // skip newline
//     file.ignore(1);
//   }
//   for (int i = 0; i < 4; i++) {
//     std::string piece;
//     std::getline(file, piece, ' ');
//     castling[i] = piece == "1";
//   }
//   file.close();
// }


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
     for (int i = 1; i < a.size() && i < b.size(); i++) {
       Position* a_outcome = a[i];
       Position* b_outcome = b[i];
       if (a_outcome->evaluation != b_outcome->evaluation) {
         return a_outcome->evaluation < b_outcome->evaluation;
       }
     }
     // If outcomes have same evaluation, recursively compare outcomes of
     // outcomes
     for (int i = 0; i < a.size() && i < b.size(); i++) {
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
  for (int i = 0; i < position->outcomes->size(); i++) {
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
const uint64_t max_bytes = (1 << 30) * (uint64_t)15;
//const int deepening_depth = 2;

void minimax(Position& position, int depth, double alpha, double beta,
             bool* stop/*, bool selective_deepening*/) {
  if (*stop || position.depth >= depth/* && !selective_deepening)*/) {
     return;
  }
  //bool deepened = false;

  /*if (depth > 3 || position.depth < 0) {
     if (deleting != 0) {
       max_depth_extension = -3;
     } else {
       PROCESS_MEMORY_COUNTERS_EX pmc;
       GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc,
                            sizeof(pmc));
       if (engine_to_move) {
         if (pmc.WorkingSetSize > max_bytes) {
          *stop = true;
         } else if (pmc.WorkingSetSize > critical_bytes) {
          max_depth_extension = 1;
         } else if (pmc.WorkingSetSize > safe_bytes) {
          max_depth_extension = -1;
         } else {
          max_depth_extension = -3;
         }
       } else {
         if (pmc.WorkingSetSize > max_bytes) {
          *stop = true;
         } else {
          max_depth_extension = -3;
         }
       }

     }
  }*/

  bool white_to_move = position.to_move == 'W';
  if (depth <= 0/* && !selective_deepening*/) {
     // if (!position.was_capture || depth < max_depth_extension) {
     position.evaluation = round(position.evaluation);
     position.evaluation += EvaluateMobility(position);
     position.depth = 0;
     return/* false*/;
     //}
  }

  if (!position.outcomes) {
     /*if (selective_deepening) {
       if (!position.was_capture) {
         return false;
       }
       depth = deepening_depth;
       selective_deepening = false;
       deepened = true;
     }*/
     
     new_generate_moves(position);
  }

  if (position.outcomes->size() == 0) {
     if (InCheck(position, position.to_move)) {
       position.evaluation =
           white_to_move ? (double)-mate + (double)position.number
                         : (double)mate - (double)position.number;  // checkmate
     } else {
       position.evaluation = 0;  // draw by stalemate
     }
     return;
  }
  if (position.fifty_move_rule >= 100) { // fifty move rule
     position.evaluation = 0;
     return;
  }
  if (depth >= min_depth) {
     PROCESS_MEMORY_COUNTERS_EX pmc;
     GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc,
                          sizeof(pmc));
     if (deleting == 0 && pmc.WorkingSetSize > max_bytes) {
       *stop = true;
     }
  }
  std::sort(position.outcomes->begin(), position.outcomes->end(),
               position.white_to_move ? GreaterOutcome : LessOutcome);
  double position_material = round(position.evaluation);
  //double initial_evaluation = (*position.outcomes)[0]->evaluation;
  double best_potential = (*position.outcomes)[0]->evaluation;
  //if (position.evaluation != (*position.outcomes)[0]->evaluation) {
  //   int h = 2;
  //}
  //int multiplier = initial_evaluation > 0 ? -1 : 1;
  position.evaluation = white_to_move ? INT_MIN : INT_MAX;


  // time_t s = time(NULL);
  /*std::sort(position.outcomes->begin(),
            position.outcomes->end(),
            position.white_to_move ? GreaterOutcome : LessOutcome);*/
  //std::cout << time(NULL) - s;
  for (int c = 0;
       c < position.outcomes
               ->size() /* && (selective_deepening ? (abs(initial_evaluation -
                           (*position.outcomes)[c]->evaluation) < 3) : true)*/
       ;
       c++) {
     // if (depth == original_depth) {
     //   output.possibilities[possible_moves[c]] = position_value.evaluation;
     // }
     if (position.to_move == 'W') {
       if (position.outcomes->size() <= 2) {
         minimax(*(*position.outcomes)[c], depth, alpha, beta, stop/*,
               selective_deepening*/);
       } else if (round((*position.outcomes)[c]->evaluation) >= position_material + 1) {
         minimax(*(*position.outcomes)[c], depth, alpha, beta, stop/*,
               selective_deepening*/);
       } else if (round((*position.outcomes)[c]->evaluation) <= position_material - 1) {
         minimax(*(*position.outcomes)[c], depth - 2, alpha, beta, stop/*,
               selective_deepening*/);
       } else {
         minimax(*(*position.outcomes)[c], depth - 1, alpha, beta, stop/*,
               selective_deepening*/);
       }
       /*if (position_value.evaluation > mate) {
           position_value.evaluation--;
       }*/

       if ((*position.outcomes)[c]->evaluation > position.evaluation) {
         position.evaluation = (*position.outcomes)[c]->evaluation;
         // position.best_move = (*position.outcomes)[c];
         /*output.castling = position_value.castling;*/
       }
       if ((*position.outcomes)[c]->evaluation > alpha) {
         alpha = (*position.outcomes)[c]->evaluation;
       }
     } else {
       /*if (position_value.evaluation < -mate) {
           position_value.evaluation++;
       }*/
       if (position.outcomes->size() <= 2) {
         minimax(*(*position.outcomes)[c], depth, alpha, beta, stop/*,
               selective_deepening*/);
       } else if (round((*position.outcomes)[c]->evaluation) <= position_material - 1) {
         minimax(*(*position.outcomes)[c], depth, alpha, beta, stop/*,
               selective_deepening*/);
       } else if (round((*position.outcomes)[c]->evaluation) >= position_material + 1) {
         minimax(*(*position.outcomes)[c], depth - 2, alpha, beta, stop/*,
               selective_deepening*/);
       } else {
         minimax(*(*position.outcomes)[c], depth - 1, alpha, beta, stop/*,
               selective_deepening*/);
       }
       if (/*position.evaluation == INT_MAX || LessOutcome(*/ (
               *position.outcomes)[c]
               ->evaluation < position.evaluation) {
         position.evaluation = (*position.outcomes)[c]->evaluation;
         // position.best_move = (*position.outcomes)[c];
         //  output.castling = position_value.castling;
         //  output.move_history = position_value.move_history;
       }
       if ((*position.outcomes)[c]->evaluation < beta) {
         beta = (*position.outcomes)[c]->evaluation;
       }
     }
     if (beta <= alpha) {
       // for (int j = c + 1; j < position.outcomes->size(); j++) {
       //   (*position.outcomes)[j]->evaluation = white_to_move ? INT_MIN :
       //   INT_MAX;
       // }
       break;
     }
  }
  if (!*stop) {
    if (depth <= 0) {
      position.depth = 1;
    } else {
      position.depth = depth;
    }
  }
  //Position* best_move = GetBestMove(&position);
  //assert(best_move->evaluation == position.evaluation);
 
}

int thread_count = 0;


//void Premoves(void* arg) {
//  thread_count++;
//  // std::cout << "Premoves thread starting: " << GetCurrentThreadId();
//  Position* position = (Position*)arg;
//  while (!stop && position->depth < 5) {
//    minimax(*position, position->depth + 1, INT_MIN, INT_MAX);
//  }
//  // std::cout << "Premoves thread ending";
//  thread_count--;
//  _endthread();
//}





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
  for (int i = 0; i < arg->position->outcomes->size(); i++) {
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
  while (position->outcomes) {
    position = GetBestMove(position);
    line.emplace_back(position);
  }
  return line;
}

void calculate_moves(void* varg) {
  //double minimum_time = 3;
  //int64_t max_memory = 13;
  //int64_t max_bytes = 1073741824 * max_memory;
  //int max_depth = /*floor(log2(max_bytes / sizeof(Position)) / log2(32));*/ 6;
  //std::chrono::duration<double> time_span = duration_cast<std::chrono::duration<double>::zero>;

  ThreadInfo* input = (ThreadInfo*)varg;

  while (true) {
    //printf("[calculate_moves %d] loop.  outcomes=%p, size=%d\n",
    //       GetCurrentThreadId(), input->position->outcomes,
    //       input->position->outcomes ? input->position->outcomes->size() : 0);
     
     
     
    
    // if (!input->position->outcomes) {
    //  generate_moves(*input->position);
    //}
    
    //max_depth =
    //    (int)floor(log2((((double)max_bytes / 32.0) *
    //                    input->position->outcomes->size()) / sizeof(Position)) /
    //               log2(32));
    if (!*input->stop) {

      if (input->position->to_move == input->side) {
        //printf("[calculate_moves] minimax start");
        while (input->position->depth < min_depth && !*input->stop) {
          minimax(*input->position, input->position->depth + 1, INT_MIN, INT_MAX, input->stop);
        }
        /*bool deepened = true;
        PROCESS_MEMORY_COUNTERS_EX pmc;
        GetProcessMemoryInfo(GetCurrentProcess(),
                             (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
        while ((deleting != 0 || pmc.WorkingSetSize < max_bytes) && deepened) {
          deepened = minimax(*input->position, min_depth + 2, INT_MIN, INT_MAX, input->stop);
          GetProcessMemoryInfo(GetCurrentProcess(),
                               (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
        }*/
        //std::vector<Position*> line = GetLine(input->position);
       /* while ((line.back()->was_capture || input->position->evaluation != line.back()->evaluation) && (deleting != 0 || pmc.WorkingSetSize < max_bytes)) {
          minimax(*input->position, min_depth + 2, INT_MIN, INT_MAX, input->stop,
                  true);
          line = GetLine(input->position);
          GetProcessMemoryInfo(GetCurrentProcess(),
                               (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
        }*/
        done = true;
        *input->stop = true;
        done_cv.notify_one();

      } else {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        GetProcessMemoryInfo(GetCurrentProcess(),
                             (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
        while (!*input->stop && (deleting != 0 || pmc.WorkingSetSize < max_bytes)) {
          minimax(*input->position,
                  input->position->depth + 1,
                  INT_MIN, INT_MAX, input->stop);
          GetProcessMemoryInfo(GetCurrentProcess(),
                               (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
        }
        *input->stop = true;
      }

    }
    if (*input->stop) {
      waiting.release();
      //printf("[calculate_moves %d] block.\n", GetCurrentThreadId());
      std::unique_lock lk(stop_cv_mutex);
      stop_cv.wait(lk, [input] { return !*input->stop; });
      if (!input->position->outcomes) {
        new_generate_moves(*input->position);
      }
      if (input->position->to_move ==
                  input->side && /*(input->position->depth >= min_depth ||*/
              input->position->outcomes->size() == 1 ||
          input->position->outcomes->size() == 0 /*)*/) {
        done = true;
        *input->stop = true;
        done_cv.notify_one();
      }
    }

    

    for (int i = 0; i < input->trash->size(); i++) {
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

// void DeepDelete(Position& position) {
//   /*if (position.outcomes != nullptr) {
//     for (int i = 0; i < position.outcomes->size(); i++) {
//       DeepDelete(*(*position.outcomes)[i]);
//       delete (*position.outcomes)[i];
//       (*position.outcomes)[i];
//     }
//     delete position.outcomes;
//     position.outcomes;
//   }*/
//   delete &position;
// }

int CheckGameOver(Position* position) {
  if (position->outcomes->size() == 0) {
    if (position->white_to_move && InCheck(*position, 'W')) {
      return 1;
    }
    if (!position->white_to_move && InCheck(*position, 'B')) {
      return -1;
    }
    return 0;
  }
  return 2;
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
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (position1.board[i][j] != position2.board[i][j]) {
        differences.emplace_back(i, j);
      }
    }
  }
  if (differences.size() == 4) {
    if (position2.board[7][6].type == 'K' &&
        position1.board[7][6].color == ' ') {
      return chess_notation ? "O-O" : "74767775";
    }
    if (position2.board[7][2].type == 'K' &&
        position1.board[7][2].color == ' ') {
      return chess_notation ? "O-O" : "74727073";
    }
    if (position2.board[0][6].type == 'K' &&
        position1.board[0][6].color == ' ') {
      return chess_notation ? "O-O" : "04060705";
    }
    if (position2.board[0][2].type == 'K' &&
        position1.board[0][2].color == ' ') {
      return chess_notation ? "O-O-O" : "04020003";
    }
  }
  if (differences.size() == 3) {
    if (position2.board[differences[0].row][differences[0].column]
      .color == ' ') {
      if (chess_notation) {
        return std::string({char('a'+ differences[2].column), 'x', char('a' + differences[1].column), char(8 - differences[0].row)});
      } else {
        std::stringstream output;
        output << differences[2].row << differences[2].column << differences[1].row << differences[1].column << differences[0].row << differences[1].column;
        return output.str();
      }
    } else {
      if (chess_notation) {
        return std::string({char('a' + differences[2].column), 'x',
                            char('a' + differences[0].column),
                            char(8 - differences[0].row)});
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
  if (position2.board[differences[0].row][differences[0].column].color == ' ') {
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
  if (position1.board[origin.row][origin.column].type != 'P') {
    GetCheckInfo(check_info, position1);
  }
  std::stringstream output;
  std::vector<Point> found;
  switch (position1.board[origin.row][origin.column].type) {
    case 'P':
      if (position1.board[dest.row][dest.column].color != ' ') {
        // capture
        output << (char) ('a' + origin.column) << 'x' << static_cast<char>('a' + dest.column)
               << (8 - dest.row);
      } else {
        output << static_cast<char>('a' + dest.column) << (8 - dest.row);
      }
      if (position2.board[dest.row][dest.column].type != 'P') {
        // promotion
        output << '=' << position2.board[dest.row][dest.column].type;
      }
      break;
    case 'N':
      output << 'N';
      for (int k = 0; k < 8; k++) {
        int new_i = dest.row + knight_moves[k][0];
        int new_j = dest.column + knight_moves[k][1];
        if (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7 &&
            position1.board[new_i][new_j].color == position1.to_move &&
            position1.board[new_i][new_j].type == 'N' && (new_i !=
                origin.row ||
            new_j != origin.column) &&
            MoveOk(check_info, new_i, new_j, dest.row, dest.column)) {
          found.emplace_back(new_i, new_j);
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
      if (position1.board[dest.row][dest.column].color != ' ') {
        output << 'x';
      }
      output << static_cast<char> ('a' + dest.column) << (8 - dest.row);
      break;
    case 'Q':
    case 'B':
      output << position1.board[origin.row][origin.column].type;
      for (int k = 0; k < 4; k++) {
        int new_i = dest.row + bishop_moves[k][0];
        int new_j = dest.column + bishop_moves[k][1];
        while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
          if (position1.board[new_i][new_j].color == position2.to_move) {
            break;
          }
          if (position1.board[new_i][new_j].color == position1.to_move &&
              position1.board[new_i][new_j].type ==
                  position1.board[origin.row][origin.column].type &&
              (new_i != origin.row || new_j != origin.column) &&
              MoveOk(check_info, new_i, new_j, dest.row, dest.column)) {
            found.emplace_back(new_i, new_j);
          }
          if (position1.board[new_i][new_j].color == position1.to_move) {
            break;
          }
          new_i += bishop_moves[k][0];
          new_j += bishop_moves[k][1];
        }
      }
      if (position1.board[origin.row][origin.column].type == 'B') {
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
        if (position1.board[dest.row][dest.column].color != ' ') {
          output << 'x';
        }
        output << (char)('a' + dest.column) << (8 - dest.row);
        break;
      }
    case 'R':
      if (position1.board[origin.row][origin.column].type == 'R') {
        output << 'R';
      }
      for (int k = 0; k < 4; k++) {
        int new_i = dest.row + rook_moves[k][0];
        int new_j = dest.column + rook_moves[k][1];
        while (new_i >= 0 && new_i <= 7 && new_j >= 0 && new_j <= 7) {
          if (position1.board[new_i][new_j].color == position2.to_move) {
            break;
          }
          if (position1.board[new_i][new_j].color == position1.to_move &&
              position1.board[new_i][new_j].type ==
                  position1.board[origin.row][origin.column].type &&
              (new_i != origin.row || new_j != origin.column) &&
              MoveOk(check_info, new_i, new_j, dest.row, dest.column)) {
            found.emplace_back(new_i, new_j);
          }
          if (position1.board[new_i][new_j].color == position1.to_move) {
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
      if (position1.board[dest.row][dest.column].color != ' ') {
        output << 'x';
      }
      output << (char)('a' + dest.column) << (8 - dest.row);
      break;
    case 'K':
      output << 'K';
      if (position1.board[dest.row][dest.column].color != ' ') {
        output << 'x';
      }
      output << (char)('a' + dest.column) << (8 - dest.row);
      break;
      


  }
  if (std::abs(position2.evaluation) == (position2.white_to_move ? (mate - position2.number) : (-mate + position2.number))) {
    output << '#';
  } else if (InCheck(position2, position2.to_move)) {
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
        int i = 0;
        int j = 0;
        for (int k = 0; k < elements[0].length(); k++) {
          if (elements[0][k] == '/') {
            i++;
            j = 0;
            continue;
          }
          if (i < 0 || i > 7 || j < 0 || j > 7) {
              std::cout << "FEN error";
              exit(1);
          }
          if (isdigit(elements[0][k])) {
            for (int l = char_to_int(elements[0][k]); l > 0; l--) {
              if (j > 7) {
                std::cout << "FEN error";
                exit(1);
              }
              position->board[i][j].empty();
              j++;
            }
          } else {
            if (islower(elements[0][k])) {
              position->board[i][j].color = 'B';
            } else {
              position->board[i][j].color = 'W';
            }
            position->board[i][j].type = toupper(elements[0][k]);
          }

          j++;
        }
        position->to_move = toupper(elements[1][0]);
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
        position->fifty_move_rule = stoi(elements[4]);
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
    for (int i = 1; (i < moves_match.size() && moves_match[i].matched); i++) {
      position_number++;
      new_generate_moves(*position);
      position = GetPosition(*position, moves_match[i]);
      //moves_match[i];
    }
    moves_line_copy = moves_match.suffix().str();
  }
  position->number = position_number;
  position->depth = 0;

  //getline(file, line);
  //int i = 0;
  //while (i < line.length() - 4) {
  //  i += 3;
  //  std::string m;
  //  m += line[i];
  //  i++;
  //  m += line[i];
  //  new_generate_moves(*position);
  //  position = GetPosition(*position, m);
  //  i++;
  //  m = "";
  //  m += line[i];
  //  i++;
  //  m += line[i];
  //  new_generate_moves(*position);
  //  position = GetPosition(*position, m);
  //}

  


  //int brackets = 0;
  //char c;
  //do {
  //  file.read(&c, sizeof(c));
  //  if (c == '[') {
  //    brackets++;
  //    std::string id = ""; // read until a whitespace
  //    switch (id) { case "FEN":

  //    }
  //  } else if (c == ']') {
  //    brackets--;
  //  }
  //} while (brackets != 0 || c == ']');

  //for (int i = 0; i < 8; i++) {
  //  file.ignore(4);
  //  for (int j = 0; j < 8; j++) {
  //    char piece[5];
  //    file.read(piece, sizeof(piece));
  //    position.board[i][j].color = piece[0];
  //    position.board[i][j].type = piece[1];
  //  }
  //  // skip newline
  //  file.ignore(44);
  //}

  //char castle;
  //file.read(&castle, sizeof(castle));
  //position.castling.white_o_o = castle == '1';
  //file.read(&castle, sizeof(castle));
  //position.castling.white_o_o_o = castle == '1';
  //file.read(&castle, sizeof(castle));
  //position.castling.black_o_o = castle == '1';
  //file.read(&castle, sizeof(castle));
  //position.castling.black_o_o_o = castle == '1';

  //file.ignore(1);
  //char to_move[1];
  //file.read(to_move, sizeof(to_move));
  //position.to_move = to_move[0];
  //position.white_to_move = position.to_move == 'W';
  //for (int i = 0; i < 8; i++) {
  //  for (int j = 0; j < 8; j++) {
  //    if (position.board[i][j].type == 'K') {
  //      position.kings[position.board[i][j].color].row = i;
  //      position.kings[position.board[i][j].color].column = j;
  //      break;
  //    }
  //  }
  //}
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
  int last_digit = (int) output_string.find_last_not_of('0') + 1;
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
    for (int i = 1; (i < moves_match.size() && moves_match[i].matched); i++) {
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
  for (int i = 0; i < lines.size(); i++) {
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
        int i = 0;
        int j = 0;
        for (int k = 0; k < elements[0].length(); k++) {
          if (elements[0][k] == '/') {
            i++;
            j = 0;
            continue;
          }
          if (i < 0 || i > 7 || j < 0 || j > 7) {
            std::cout << "FEN error";
            exit(1);
          }
          if (isdigit(elements[0][k])) {
            for (int l = char_to_int(elements[0][k]); l > 0; l--) {
              if (j > 7) {
                std::cout << "FEN error";
                exit(1);
              }
              new_position->board[i][j].empty();
              j++;
            }
          } else {
            if (islower(elements[0][k])) {
              new_position->board[i][j].color = 'B';
            } else {
              new_position->board[i][j].color = 'W';
            }
            new_position->board[i][j].type = toupper(elements[0][k]);
          }

          j++;
        }
        new_position->to_move = toupper(elements[1][0]);
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
        new_position->fifty_move_rule = stoi(elements[4]);
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
    for (int i = 1; (i < moves_match.size() && moves_match[i].matched); i++) {
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
  for (int i = 0; i < input.length(); i++) {
    output += input[i];
  }
  return output;
}


int CountMaterial(const Position& position) {
  int count = 0;
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (position.board[i][j].color != ' ') {
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


int main() {
  thread_count++;

  //int max_threads = 6; // 12

  VBoard v_board = {{"BR", "BN", "BB", "BQ", "BK", "BB", "BN", "BR"},
                    {"BP", "BP", "BP", "BP", "BP", "BP", "BP", "BP"},
                    {"  ", "  ", "  ", "  ", "  ", "  ", "  ", "  "},
                    {"  ", "  ", "  ", "  ", "  ", "  ", "  ", "  "},
                    {"  ", "  ", "  ", "  ", "  ", "  ", "  ", "  "},
                    {"  ", "  ", "  ", "  ", "  ", "  ", "  ", "  "},
                    {"WP", "WP", "WP", "WP", "WP", "WP", "WP", "WP"},
                    {"WR", "WN", "WB", "WQ", "WK", "WB", "WN", "WR"}};
  Board board;
  for (int i = 0; i < 8; i++) {
    std::vector<Piece> row;
    for (int j = 0; j < 8; j++) {
      row.emplace_back(v_board[i][j][0], v_board[i][j][1]);
    }
    board.emplace_back(row);
  }
  int max_positions = 2500000;

  //int rounding = 1e7;
  bool white_O_O = true;
  bool white_O_O_O = true;
  bool black_O_O = true;
  bool black_O_O_O = true;
  int number = 0;
  Kings kings = Kings(Point(7, 4), Point(0, 4));
  Castling castling = {white_O_O, white_O_O_O, black_O_O, black_O_O_O};
  char to_move = 'W';
  //Position* previous_move = NULL;
  std::vector<Position*>* outcomes = nullptr;
  Position* position =
      new Position(to_move == 'W', number, 0.0, board, castling/*, 0LL*/, to_move,
                   outcomes/*, previous_move*/, 0, /*nullptr,*/ kings, Point(-1, -1), 0, false);

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
    evaluate_position(*position);
  }
  else {
    std::ofstream file;
    file.open("PGN_position.txt");
    file << "[Event \"?\"]\n[Site \"?\"]\n[Date \"????.??.??\"]\n[Round \"?\"]\n[White \"?\"]\n[Black \"?\"]\n[Result \"*\"]\n\n*";
  }
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
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


  //new_generate_moves(*position);
  char color;
  bool game_over = false;
  std::cout << "What color am I playing? ";
  std::cin >> color;
  color = toupper(color);
  while (color != 'W' && color != 'B') {
    std::cout << "What color am I playing (enter \"W\" or \"B\")? ";
    std::cin >> color;
    color = toupper(color);
  }

  char bottom_board;
  std::cout << "What color is on the bottom of the board? ";
  std::cin >> bottom_board;
  bool white_on_bottom = bottom_board == 'W' || bottom_board == 'w';


  ThreadInfo* info = new ThreadInfo(position, new bool(false), new TrashType, color);
  std::string move;
  Board new_board;
  bool engine_on = true;
  Position* new_position = nullptr;
  int game_status;
  std::string lower_move;
  //new_generate_moves(*position);
  SeekResult result;
  std::cout << MakeString(*position, chess_notation, white_on_bottom) << std::endl;
  if (color == position->to_move) {
    //std::cout << "My move: ";
    min_depth = (int)std::max(
        4.0, floor(log2(max_positions) / log2(CountMaterial(*position) / 2.0)));
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
    /*if (position->outcomes == nullptr) {
      generate_moves(*position);
    }*/
    //SavePosition(*position);
    if (color == position->to_move && engine_on) {
      //printf("[main thread %d] block.\n", GetCurrentThreadId());
      std::cout << "My move: ";

      {
        std::unique_lock lk(done_cv_mtx);
        done_cv.wait(lk, [] { return done; });
      }
      //stop_mutex.lock();
      //*info->stop = true;
      //stop_mutex.unlock();
      // making_move.lock();
      //stop_mutex.lock();
      //*info->stop = true;
      //stop_mutex.unlock();
      waiting.acquire();
      //working.lock();
      game_status = CheckGameOver(position);
      Position* best_move;

      if (game_status != 2) {
        std::cout << std::endl;
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
          default:
            std::cout << "unexpected end of game.";
            exit(1);
        }
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

        /*while (position->time < minimum_time || position->depth < min_depth) {
          minimax(*position, position->depth + 1, INT_MIN, INT_MAX);
          if (position->outcomes->size() == 1 ||
              position->outcomes->size() == 0) {
            break;
          }
        }*/
        // delete position;
        // Sleep(1000);
        // return 0;
        /*for (int i = 0; i < position.outcomes->size(); i++) {
          for (int j = 0; j < 8; j++) {
            for (int k = 0; k < 8; k++) {
              std::cout << (*position.outcomes)[i]->board[j][k] << std::endl;
            }
          }
        }*/

        // position = best_move;
        game_status = CheckGameOver(best_move);

        // SavePosition(*best_move);

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
          std::cout << Convert(best_move->evaluation) << std::endl;
          std::cout << "Line: ";
          std::vector<Position*> line = GetLine(position);
          std::cout << GetMove(*position, *line[0], chess_notation);
          for (int i = 1; i < line.size(); i++) {
            std::cout << ' ' << GetMove(*line[i - 1], *line[i], chess_notation);
          }
          std::cout << std::endl;
          std::cout << "Moves: " << best_move->outcomes->size() << std::endl
                    << "Material: " << (float) CountMaterial(*best_move) / 2.0 << std::endl;
        }
        position = best_move;
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

    //move = GetMove(*position, (*(*position->outcomes)[0]), chess_notation);
    //std::cout << move << std::endl;
    //  stop = true;
    lower_move = ToLower(move);
    if (lower_move == "disable") {
      engine_on = false;
      continue;
    } else if (lower_move == "enable") {
      engine_on = true;
      continue;
    } else if (move.length() == 1) {
      std::cout << "Invalid move." << std::endl;
    } else if (move[0] == '+' || move[0] == '-') {
      if (move.length() == 1) {
        std::cout << "Invalid move." << std::endl;
      } else {
        result = SeekPosition(
            position, (move[0] == '+' ? 1 : -1) * stoi(move.substr(1, (move.length() - 1))));
      }
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
      //if (new_position->to_move == color) {
      //      std::cout << "My move: ";
      //}
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
    }    else if (chess_notation) {
      new_position = GetPosition(*position, move);
    } else {
      new_board = position->board;
      if (move == "O-O" || move == "0-0") {
        if (position->to_move == 'W') {
          new_board[7][6].set('W', 'K');
          new_board[7][5].set('W', 'R');
          new_board[7][4].empty();
          new_board[7][7].empty();
        } else {
          new_board[0][6].set('B', 'K');
          new_board[0][5].set('B', 'R');
          new_board[0][4].empty();
          new_board[0][7].empty();
        }
      } else if (move == "O-O-O" || move == "0-0-0") {
        if (position->to_move == 'W') {
          new_board[7][2].set('W', 'K');
          new_board[7][3].set('W', 'R');
          new_board[7][4].empty();
          new_board[7][0].empty();
        } else {
          new_board[0][2].set('B', 'K');
          new_board[0][3].set('B', 'R');
          new_board[0][4].empty();
          new_board[0][0].empty();
        }
      } else {
        int si = char_to_int(move[0]);
        int sj = char_to_int(move[1]);
        int di = char_to_int(move[2]);
        int dj = char_to_int(move[3]);
        new_board[di][dj] = new_board[si][sj];
        new_board[si][sj].empty();
        if (move.length() == 6) {
          if (move[5] == '=') {
            new_board[di][dj].type = toupper(move[6]);
          } else {
            new_board[char_to_int(move[4])][char_to_int(move[5])].empty();
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



      //printf("[main thread %d] acquire complete.\n", GetCurrentThreadId());
      if (!mental_chess) {
        std::cout << MakeString(*new_position, chess_notation, white_on_bottom) << std::endl;
        std::cout << Convert(new_position->evaluation) << std::endl;
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

          min_depth = (int) std::max(4.0, floor(log2(max_positions) /
                             log2(CountMaterial(*position) / 2.0)));
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