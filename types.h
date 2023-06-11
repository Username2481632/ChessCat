#pragma once

#include <array>
#include <map>
#include <set>
#include <vector>

// enum class PieceType : char { Empty, Rook, Knight, Bishop, Queen, King, Pawn
// }; enum class ColorType : char { White, Black }; struct Piece {
//   PieceType type = PieceType::Empty;
//   ColorType color = ColorType::White;
// };
enum class Color : char { White, Black, Empty };

struct Piece {
  Color color;
  char type;

  Piece(const Color& c, const char& t);

  bool IsEmpty() const;
  
  bool operator==(const Piece& other) const;
  bool operator!=(const Piece& other) const;

  bool Equals(const Color& c, const char& t) const;

  void set(const Color& c, const char& t);
  void Empty();

};


struct Castling {
  bool white_o_o : 1;
  bool white_o_o_o : 1;
  bool black_o_o : 1;
  bool black_o_o_o : 1;

  bool operator!=(const Castling other) const;
  bool operator==(const Castling other) const;
};

using Board = std::array<Piece, 64>;

// struct Point {
//   size_t row;
//   size_t column;
//   bool Equals(size_t r, size_t c) { return row == r && column == c; }
//   Point(size_t r, size_t c) : row(r), column(c) {}
//   //void clear() { row = column = -1;
//   //}
//   void Set(size_t r, size_t c) {
//     row = r;
//     column = c;
//   };
//   bool operator==(const Point& other) const {
//     return row == other.row && column == other.column;
//   }
//
//   bool operator<(const Point& other) const {
//     if (row < other.row) return true;
//     if (row > other.row) return false;
//     if (column < other.column) return true;
//     return false;
//   }
//   Point(){}
// };

struct Kings {
  size_t white_king;
  size_t black_king;
  size_t& operator[](const Color& color);
  Kings(size_t wk, size_t bk);
};

struct Check {
  bool in_check = false;
  bool king_must_move = false;
  std::map<size_t, std::set<size_t>> pinned;
  std::set<size_t> block;
};
