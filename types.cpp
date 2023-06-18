#include "types.h"

bool Castling::operator!=(const Castling other) const {
  return white_o_o != other.white_o_o || white_o_o_o != other.white_o_o_o ||
         black_o_o != other.black_o_o || black_o_o_o != other.black_o_o_o;
};
bool Castling::operator==(const Castling other) const {
  return white_o_o == other.white_o_o && white_o_o_o == other.white_o_o_o &&
         black_o_o == other.black_o_o && black_o_o_o == other.black_o_o_o;
};

unsigned char& Kings::operator[](const Color& color) {
  return color == Color::White ? white_king : black_king;
};

Kings::Kings(unsigned char wk, unsigned char bk) : white_king(wk), black_king(bk){};

Piece::Piece(const Color& c, const char& t) : color(c), type(t){};

bool Piece::IsEmpty() const { return color == Color::Empty; }

bool Piece::operator==(const Piece& other) const { 
  return color == other.color && type == other.type; 
}
bool Piece::operator!=(const Piece& other) const {
  return color != other.color || type != other.type;
}

bool Piece::Equals(const Color& c, const char& t) const {
  return color == c && type == t;
}

void Piece::set(const Color& c, const char& t) {
  color = c;
  type = t;
}

void Piece::Empty() {
  color = Color::Empty;
  type = ' ';
}

