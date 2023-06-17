#include "constants.h"

std::map<char, double> piece_values = {
    {'P', 1.0}, {'N', 3.0}, {'B', 3.0}, {'R', 5.0}, {'Q', 9.0}};
const int knight_moves[8] = {-17, -15, -6, 10, 17, 15, 6, -10};
const int bishop_moves[4] = {-9, -7, 9, 7};
const int king_moves[8] = {-9, -8, -7, 1, 9, 8, 7, -1};
const int rook_moves[4] = {-8, 1, 8, -1};
const int max_positions = 40000000;

const Board starting_board = {
    {Piece(Color::Black, 'R'), Piece(Color::Black, 'N'),
     Piece(Color::Black, 'B'), Piece(Color::Black, 'Q'),
     Piece(Color::Black, 'K'), Piece(Color::Black, 'B'),
     Piece(Color::Black, 'N'), Piece(Color::Black, 'R'),
     Piece(Color::Black, 'P'), Piece(Color::Black, 'P'),
     Piece(Color::Black, 'P'), Piece(Color::Black, 'P'),
     Piece(Color::Black, 'P'), Piece(Color::Black, 'P'),
     Piece(Color::Black, 'P'), Piece(Color::Black, 'P'),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::Empty, ' '), Piece(Color::Empty, ' '),
     Piece(Color::White, 'P'), Piece(Color::White, 'P'),
     Piece(Color::White, 'P'), Piece(Color::White, 'P'),
     Piece(Color::White, 'P'), Piece(Color::White, 'P'),
     Piece(Color::White, 'P'), Piece(Color::White, 'P'),
     Piece(Color::White, 'R'), Piece(Color::White, 'N'),
     Piece(Color::White, 'B'), Piece(Color::White, 'Q'),
     Piece(Color::White, 'K'), Piece(Color::White, 'B'),
     Piece(Color::White, 'N'), Piece(Color::White, 'R')}};
