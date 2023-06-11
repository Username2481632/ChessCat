#include "get_check_info.h"

#include "constants.h"

void GetCheckInfo(Check& output, Position& position,
                  Color side) {
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
      if (position.board[(size_t)new_i].type == 'N' &&
          position.board[(size_t)new_i].color == opponent) {
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
            // if (position.board[found.row][found.column].type == 'B' ||
            //     position.board[found.row][found.column].type == 'Q' /*need to
            //     account for pawns too*/) {
            //  (position.board[(size_t)new_i][(size_t)new_j].type != 'Q' ?
            //  (position.board[found.row][found.column].type !=
            //  position.board[(size_t)new_i][(size_t)new_j].type ||
            //  (position.board[found.row][found.column].type == 'Q' &&
            //  (position.board[(size_t)new_i][(size_t)new_j].type == 'B' ||
            //  position.board[(size_t)new_i][(size_t)new_j].type == 'R'))) :
            //  (position.board[found.row][found.column].type == 'B' ||
            //  position.board[found.row][found.column].type == 'R')
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
    if ((((new_i & 7) != ((int)i & 7)) && ((new_i >> 3) != ((int)i >> 3))) ||
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
