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
#include <filesystem>
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

#include "types.h"
#include "position.h"
#include "constants.h"
#include "get_check_info.h"
#include "utils.h"
#undef min
#undef max

std::binary_semaphore computed{0};
std::mutex position_mutex;

// int g_copied_boards = 0;
 //int boards_created = 0;
 //int boards_destroyed = 0;


using Pair = std::pair<Board, Castling>;
using HistoryType = std::vector<Position>;
using MovesType = std::vector<Pair>;
// using Possibilities = std::vector<Position>;



bool MoveOk(Check check, size_t i, size_t new_i) {
  return ((!check.in_check || check.block.contains(new_i)) && (!check.pinned.contains(i)
              ||check.pinned[i].contains(new_i)));
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
        if (position.board[(size_t)new_i].color == side &&
            position.board[(size_t)new_i].type != 'K') {
        break;
        } else if (position.board[(size_t)new_i].color == opponent) {
          if (position.board[(size_t)new_i].type == 'B' ||
              position.board[(size_t)new_i].type == 'Q') {
          return true;
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
      new_i = (int)i + rook_moves[k];
      if ((((new_i & 7) != ((int)i & 7)) && ((new_i >> 3) != ((int)i >> 3))) ||
          new_i > 63 || new_i < 0) {
        continue;
      }
      while (true) {
        if (position.board[(size_t)new_i].color == side &&
            position.board[(size_t)new_i].type != 'K') {
          break;
        } else if (position.board[(size_t)new_i].color == opponent) {
          if (position.board[(size_t)new_i].type == 'R' ||
              position.board[(size_t)new_i].type == 'Q') {
          return true;
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
    // king check
    for (size_t k = 0; k < 8; k++) {
      new_i = (int)i + king_moves[k];
      if (new_i >= 0 && new_i <= 63 && (((int)i & 7) - (new_i & 7)) <= 1 &&
          ((int)i & 7) - (new_i & 7) >= -1 &&
          position.board[(size_t)new_i].type == 'K' &&
          position.board[(size_t)new_i].color == opponent) {
        return true;
      }
    }
    return false;
}



const int king_value = 1000;



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
        [[fallthrough]];
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
  //Check white_check;
  //Check black_check;
  //GetCheckInfo(white_check, position);
  //GetCheckInfo(black_check, position);
  int new_i;
  size_t moves = 0;
  //Check& check_info = white_check;
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
      //if (position.board[i].color == Color::White) {
      //  check_info = white_check;
      //} else {
      //  check_info = black_check;
      //}
      // counting moves
      // int addition = position.board[i][j].color == 'W' ? 1 : -1;
      moves += CountPieceMoves(position, i, Check());
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





//unsigned int nodes = 0;


void new_generate_moves(Position& position) {
  //nodes++;
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
  for (size_t i = 0; i < 64; i++) {
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
          assert(((i & 7) == 7) || (i + multiplier + 1) <= 63);
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
              new_position->was_capture = true;
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
              new_position->was_capture = true;
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
              new_position->was_capture = true;
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
              new_position->was_capture = true;
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
              new_position->was_capture = true;
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
              new_position->was_capture = true;
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
                new_position->was_capture = true;
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
          [[fallthrough]];
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
                new_position->was_capture = true;
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
            new_position->kings[position.board[i].color] = (unsigned char)new_i;
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
              new_position->was_capture = true;
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
          new_position->kings[position.board[i].color] = (unsigned char)i + 2U;
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
          new_position->kings[position.white_to_move ? Color::White : Color::Black] = (unsigned char)i - 2U;
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
  //nodes += position.outcomes->size();
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
      int64_t multiplier = (to_move == Color::White) ? (-8) : 8;
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
          board[(size_t)info.dest] =
              board[(size_t)(((int)info.dest - multiplier) & (-8)) + (size_t)info.start[1]];
          board[(size_t)(((int)info.dest - multiplier) & (-8)) + (size_t)info.start[1]].Empty();
        }
      } else if (board[info.dest - multiplier].type ==
                 'P') {
        board[info.dest] =
            board[info.dest - multiplier];
        board[info.dest - multiplier].Empty();
      } else {
        board[info.dest] = board[info.dest - (multiplier * 2)];
        board[info.dest - (multiplier << 1)].Empty();
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
          if (board[(size_t)i].color ==
              (position.white_to_move ? Color::Black : Color::White)) {
            break;
          }
          if ((board[(size_t)i].type == info.piece) &&
              board[(size_t)i].color == to_move &&
              (info.start[0] == -1 || ((i >> 3) == info.start[0])) &&
              (info.start[1] == -1 || ((i & 7) == info.start[1]))&&
              MoveOk(check_info, (size_t)i, info.dest)) {
            found = (size_t)i;
            count++;
          }
          if (board[(size_t)i].color ==
              (position.white_to_move ? Color::White : Color::Black)) {
            break;
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
          if (board[(size_t)i].color ==
              (position.white_to_move ? Color::Black : Color::White)) {
            break;
          }
          if ((board[(size_t)i].type == info.piece) &&
              board[(size_t)i].color == to_move &&
              (info.start[0] == -1 || ((i >> 3) == info.start[0])) &&
              (info.start[1] == -1 || ((i & 7) == info.start[1]))&&
              MoveOk(check_info, (size_t)i, info.dest)) {
            found = (size_t)i;
            count++;
          }
          if (board[(size_t)i].color ==
              (position.white_to_move ? Color::White : Color::Black)) {
            break;
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
  int64_t current = (int64_t)move.length() - 1;
  if (pieces.count(toupper(move[0]))) {
    info.piece = toupper(move[0]);
  } else {
    info.piece = 'P';
  }
  if (move[(size_t)current] == '+') {
    info.check = true;
    current--;
  }
  if (move[(size_t)current] == '#') {
    info.checkmate = true;
    current--;
  }
  if (pieces.count(move[(size_t)current])) {
    info.promotion = move[(size_t)current];
    current -= 2;
  }
  info.dest = 8UL * (8UL - char_to_size_t(move[(size_t)current]));
  current--;
  info.dest += ((size_t)move[(size_t)current] - (size_t)'a');
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
        if ((i & 7) == 0) {
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
uint64_t max_bytes = (1 << 30) * (uint64_t)16;
uint64_t min_free = (2U << 30);
//const int deepening_depth = 2;

//int reasonability_limit = 8;
//using AlphaBeta = std::vector<std::pair<Position*, std::array<int, 2>>>;

bool IsQuiet(Position* position) {
  return !position->was_capture && !position->outcomes;
}

void minimax(Position& position, int depth, double alpha, double beta,
             bool* stop/*, bool reasonable_extension*//*, bool selective_deepening*//*, int initial_material*//*, AlphaBeta alpha_beta*/, bool quiescence_search) {
  if (*stop || position.depth >= depth) {
     return;
  }
  double initial_alpha = alpha;
  double initial_beta = beta;
  //alpha_beta.emplace_back(& position, std::array<int, 2>{INT_MIN, INT_MAX});
  if (position.fifty_move_rule >= 6) {
     int occurrences = 1;
     Position* previous_move = &position;
     for (size_t i = 1; i < position.fifty_move_rule; i += 2) {
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
  //double initial_evaluation = position.evaluation;
  if (!position.outcomes) {

    new_generate_moves(position);
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
  position.depth = depth;
  if (depth == 0 && !quiescence_search &&
      std::none_of(position.outcomes->begin(), position.outcomes->end(),
                   [](Position* p) { return p->outcomes; })) {
    return;
  }
  if (position.outcomes->size() <= 3 && quiescence_search) {
    depth++;
  }
  //if (depth == 0 && (*position.outcomes)[0]->outcomes) {
  //     depth = 1;
  //}
  

  if (depth >= (min_depth - 1)) {

     MEMORYSTATUSEX statex{};
    statex.dwLength = sizeof(statex);

    GlobalMemoryStatusEx(&statex);
     if (deleting == 0 && statex.ullAvailPhys < min_free) {
       *stop = true;
     }
  }

  std::sort(position.outcomes->begin(), position.outcomes->end(),
            position.white_to_move ? GreaterOutcome : LessOutcome);
  if (depth == 0 &&
      std::any_of(position.outcomes->begin(), position.outcomes->end(), IsQuiet)) {
     //initial_evaluation = position.evaluation;
     if (position.white_to_move) {
       if (position.evaluation > alpha) {
         alpha = position.evaluation;
       }
       /*for (int i = 0; i < alpha_beta.size(); i++) {
         if (position.evaluation > alpha_beta[i].second[0]) {
          alpha_beta[i].second[0] = position.evaluation;
         }
       }*/
     } else {
       if (position.evaluation < beta) {
         beta = position.evaluation;
       }
       //for (int i = 0; i < alpha_beta.size(); i++) {
       //  if (position.evaluation < alpha_beta[i].second[1]) {
       //   alpha_beta[i].second[1] = position.evaluation;
       //  }
       //}
     }
     if (beta <= alpha) {
       //for (int i = alpha_beta.size() -2; i >= 0; i--) {
       //  if (alpha_beta[i].second[0] < alpha_beta[i].second[1]) {
       //   alpha_beta[i].first->depth = -1;
       //   alpha_beta.erase(alpha_beta.begin() + i);
       //  }  
       //}
       position.depth = -1;
       return;
     }

  } else {
     position.evaluation = position.white_to_move ? std::numeric_limits<double>::lowest() : INT_MAX;
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
       if (!(*position.outcomes)[c]->was_capture &&
           !(*position.outcomes)[c]->outcomes) {
         continue;

       } else {
         //assert(round(initial_evaluation) !=
         //       round((*position.outcomes)[c]->evaluation));
         minimax(*(*position.outcomes)[c], depth, alpha, beta, stop/*,
                 reasonable_extension *//*, initial_material*//*, alpha_beta*/, quiescence_search);
       }
     } else {
       minimax(*(*position.outcomes)[c], depth - 1, alpha, beta, stop/*, alpha_beta*/, quiescence_search);
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
       /*for (int i = 0; i < alpha_beta.size(); i++) {
         if (position.evaluation > alpha_beta[i].second[0]) {
          alpha_beta[i].second[0] = position.evaluation;
         }
       }*/
     } else {
       if ((*position.outcomes)[c]->evaluation < position.evaluation) {
         position.evaluation = (*position.outcomes)[c]->evaluation;
       }
       if ((*position.outcomes)[c]->evaluation < beta) {
         beta = (*position.outcomes)[c]->evaluation;
       }
       /*for (int i = 0; i < alpha_beta.size(); i++) {
         if (position.evaluation < alpha_beta[i].second[1]) {
          alpha_beta[i].second[1] = position.evaluation;
         }
       }*/
     }
     if (beta <= alpha) {
       /*for (int i = alpha_beta.size() - 2; i >= 0; i--) {
         if (alpha_beta[i].second[0] < alpha_beta[i].second[1]) {
          alpha_beta[i].first->depth = -1;
          alpha_beta.erase(alpha_beta.begin() + i);
         }
       }*/
       position.depth = -1;
       return;
     }
  }
  if (position.evaluation <= initial_alpha ||
      position.evaluation >= initial_beta || *stop) {
     position.depth = -1;
  }

}
//void test_minimax(Position& position, int depth, double alpha, double beta,
//             bool* stop /*, bool reasonable_extension*/
//             /*, bool selective_deepening*/ /*, int initial_material*/) {
//  if (*stop) {
//      return;
//  }
//  if (position.fifty_move_rule >= 6) {
//      int occurrences = 1;
//      Position* previous_move = &position;
//      for (size_t i = 0; i < position.fifty_move_rule; i += 2) {
//       previous_move = previous_move->previous_move->previous_move;
//       if (previous_move->board == position.board &&
//           previous_move->castling == position.castling &&
//           previous_move->en_passant_target == position.en_passant_target) {
//         occurrences++;
//         if (occurrences == 3) {
//          position.evaluation = 0.0;
//          position.depth = 1000;
//          return;
//         }
//       }
//      }
//  }
//  double initial_evaluation = position.evaluation;
//  if (!position.outcomes) {
//      new_generate_moves(position);
//  }
//
//  if (position.outcomes->size() == 0) {
//      position.depth = 1000;
//      if (InCheck(position,
//                  position.white_to_move ? Color::White : Color::Black)) {
//       position.evaluation =
//           position.white_to_move
//               ? (double)-mate + (double)position.number
//               : (double)mate - (double)position.number;  // checkmate
//      } else {
//       position.evaluation = 0;  // draw by stalemate
//      }
//      return;
//  }
//  if (position.fifty_move_rule >= 100) {  // fifty move rule
//      position.evaluation = 0;
//      position.depth = 1000;
//      return;
//  }
//  if (position.outcomes->size() <= 3) {
//      depth++;
//  }
//  //if (depth == 0 && (*position.outcomes)[0]->outcomes &&
//  //   (*(*position.outcomes)[0]->outcomes)[0]->outcomes) {
//  //    depth = 1;
//  //}
//
//  if (depth >= (min_depth - 1)) {
//      PROCESS_MEMORY_COUNTERS_EX pmc{};
//      GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc,
//                           sizeof(pmc));
//      if (deleting == 0 && pmc.WorkingSetSize > max_bytes) {
//       *stop = true;
//      }
//  }
//
//  std::sort(position.outcomes->begin(), position.outcomes->end(),
//            position.white_to_move ? GreaterOutcome : LessOutcome);
//  if (depth == 0 &&
//      position.outcomes->back()->evaluation == initial_evaluation) {
//      // initial_evaluation = position.evaluation;
//      if (position.white_to_move) {
//       if (position.evaluation > alpha) {
//         alpha = position.evaluation;
//       }
//      } else {
//       if (position.evaluation < beta) {
//         beta = position.evaluation;
//       }
//      }
//      if (beta <= alpha) {
//       position.depth = -1;
//       return;
//      }
//
//  } else {
//      position.evaluation = position.white_to_move ? INT_MIN : INT_MAX;
//  }
//  // if (depth == 0) {
//  //   for (size_t i = 0; i < position.outcomes->size(); i++) {
//  //      if ((*position.outcomes)[i]->evaluation != initial_evaluation &&
//  //      !(*position.outcomes)[i]->outcomes) {
//  //        new_generate_moves(*(*position.outcomes)[i]);
//  //        if ((*position.outcomes)[i]->outcomes->size() > 0) {
//  //         (*position.outcomes)[i]->evaluation =
//  //             (*std::max_element(
//  //                  (*position.outcomes)[i]->outcomes->begin(),
//  //                  (*position.outcomes)[i]->outcomes->end(),
//  //                  ((*position.outcomes)[i]->white_to_move ? GreaterOutcome
//  //                                                          : LessOutcome)))
//  //                 ->evaluation;
//
//  //        if ((*position.outcomes)[i]->evaluation == initial_evaluation) {
//  //          (*position.outcomes)[i]->evaluation += 0.0000000001;
//  //        }
//  //       }
//  //     }
//
//  //   }
//  //  std::sort(position.outcomes->begin(), position.outcomes->end(),
//  //            position.white_to_move ? GreaterOutcome : LessOutcome);
//  //}
//  for (size_t c = 0; c < position.outcomes->size(); c++) {
//      if (depth == 0) {
//       if (!(*position.outcomes)[c]->was_capture &&
//           !(*position.outcomes)[c]->outcomes) {
//         continue;
//
//       } else {
//         // assert(round(initial_evaluation) !=
//         //        round((*position.outcomes)[c]->evaluation));
//         test_minimax(*(*position.outcomes)[c], depth, alpha, beta, stop /*,
//                  reasonable_extension */
//                 /*, initial_material*/);
//       }
//      } else {
//       test_minimax(*(*position.outcomes)[c], depth - 1, alpha, beta, stop);
//      }
//
//      if (position.white_to_move) {
//       if ((*position.outcomes)[c]->evaluation > position.evaluation) {
//         position.evaluation = (*position.outcomes)[c]->evaluation;
//         // position.best_move = (*position.outcomes)[c];
//         /*output.castling = position_value.castling;*/
//       }
//       if ((*position.outcomes)[c]->evaluation > alpha) {
//         alpha = (*position.outcomes)[c]->evaluation;
//       }
//      } else {
//       if ((*position.outcomes)[c]->evaluation < position.evaluation) {
//         position.evaluation = (*position.outcomes)[c]->evaluation;
//       }
//       if ((*position.outcomes)[c]->evaluation < beta) {
//         beta = (*position.outcomes)[c]->evaluation;
//       }
//      }
//      if (beta <= alpha) {
//       position.depth = -1;
//       return;
//      }
//  }
//  if (!*stop) {
//      position.depth = depth;
//  } else {
//      position.depth = -1;
//  }
//}

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
// std::mutex working;
std::condition_variable stop_cv;
std::mutex stop_cv_mutex;
std::binary_semaphore waiting{0};

struct ThreadInfo {
  Position* position;
  bool* stop;
  TrashType* trash;
  bool* engine_on;
  bool* engine_white;
  bool* adaptive;
  ThreadInfo(Position* p, bool* s, TrashType* t, bool* eo, bool* ew, bool* a)
      : position(p), stop(s), trash(t), engine_on(eo), engine_white(ew), adaptive(a) {}
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
  if (line.size() == 0) {
    int w = 4;
  }
  return line;
}

int CheckGameOver(Position* position) {
  if (!position->outcomes) {
    new_generate_moves(*position);
  }
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

const double adaptive_goal = -4;

void SearchPosition(Position* position, int minimum_depth, bool* stop, bool quiescence_search) {
  int depth = position->depth + 1;
  double alpha, beta;
  double alpha_aspiration_window = aspiration_window;
  double beta_aspiration_window = aspiration_window;
  double approximate_evaluation = position->evaluation;
  if (!position->outcomes) {
    new_generate_moves(*position);
  }
  while (depth <= minimum_depth && !*stop && CheckGameOver(position) == 2 &&
         (position->evaluation < ((double)mate / 2.0)) &&
         (position->evaluation > ((double)-mate / 2.0))) {
    if (depth > 2) {
      alpha = position->evaluation - alpha_aspiration_window;
      beta = position->evaluation + beta_aspiration_window;
    } else {
      alpha = std::numeric_limits<double>::lowest();
      beta = INT_MAX;
    }
    minimax(*position, depth, alpha, beta,
            stop/*, AlphaBeta()*/, quiescence_search);
    if (position->evaluation <= alpha) {
      alpha_aspiration_window *= 4; // unexpected advantage for black
    } else if (position->evaluation >= beta) {
      beta_aspiration_window *= 4; // unexpected advantage for white
    } else {
      assert(position->depth != -1 || *stop);
      //Position* bm = GetBestMove(position);
      alpha_aspiration_window = beta_aspiration_window = aspiration_window;
      approximate_evaluation = position->evaluation;
      position->depth = depth;
      depth++;
    }
  }
 // assert(*stop || GetBestMove(position)->depth != -1);
}
uint64_t max_positions = (max_bytes << 1) / (3 * sizeof(Position));
//const uint64_t leeway_bytes = (2U << 30);

const uint64_t min_bytes = (3U << 30);  // 3221225472

int FindMinDepth(Position& position, bool adaptive) {
  if (!position.outcomes) {
    new_generate_moves(position);
  }
  double branching_factor = (double)CountMoves(position) / 2.0;
 for (size_t i = 0; i < position.outcomes->size(); i++) {
    if ((*position.outcomes)[i]->was_capture) {
      branching_factor++;
    }
  }
  MEMORYSTATUSEX statex{};
  statex.dwLength = sizeof(statex);

  GlobalMemoryStatusEx(&statex);
  PROCESS_MEMORY_COUNTERS_EX pmc{};
  GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc,
                       sizeof(pmc));

  uint64_t available = (statex.ullAvailPhys + pmc.WorkingSetSize)/* / 4*/;
  if (available < min_bytes) {
    std::cout << std::endl << "Not enough RAM available." << std::endl;
    exit(1);
  }
  min_free = (uint64_t)((double)available * 2.0 / branching_factor);
  max_bytes = available - min_free;
  max_positions = (max_bytes << 1) / (3 * sizeof(Position));
  if (adaptive) {
    int adaptive_eval = (int) round(position.evaluation) + 5;
    //if (position.depth == -1) {
    //  SearchPosition(&position, 1, new bool(false), false);
    //}
    if (position.white_to_move ? (adaptive_eval > adaptive_goal) : (adaptive_eval < -adaptive_goal)) {
      max_positions /= (uint64_t)pow(3, abs(adaptive_eval));
    }
  }
  if (!adaptive) {
    return (int)std::max(4.0,
                         round(log2(max_positions) / (log2(branching_factor))));
  } else {
    return (int)std::max(1.0,
                         round(log2(max_positions) / (log2(branching_factor))));
  }
}
bool AdaptiveGoalReached(Position& position) {
  return (position.white_to_move ? (round(position.evaluation) <= adaptive_goal)
                                 : (round(position.evaluation) >= -adaptive_goal));
}

std::string games_folder_path;

void calculate_moves(void* varg) {
  //double minimum_time = 3;
  //int64_t max_memory = 13;
  //int64_t max_bytes = 1073741824 * max_memory;
  //int max_depth = /*floor(log2(max_bytes / sizeof(Position)) / log2(32));*/ 6;
  //std::chrono::duration<double> time_span = duration_cast<std::chrono::duration<double>::zero>;

  ThreadInfo* input = (ThreadInfo*)varg;

  while (true) {
    if (!*input->stop) {

      if (input->position->white_to_move == *input->engine_white && *input->engine_on) {
         //assert(input->position->depth > -1);
         if (AdaptiveGoalReached(*input->position) || !*input->adaptive) {
          SearchPosition(input->position, min_depth, input->stop, true);

         } else {
          SearchPosition(input->position, min_depth, input->stop, false);

         }
        // double alpha, beta;
        // double alpha_aspiration_window = aspiration_window;
        // double beta_aspiration_window = aspiration_window;
        // double approximate_evaluation = input->position->evaluation;
        //while (input->position->depth < min_depth && !*input->stop) {
        //  if (input->position->depth > 2) {
        //    alpha = input->position->evaluation - aspiration_window;
        //    beta = input->position->evaluation + aspiration_window;
        //  } else {
        //    alpha = std::numeric_limits<double>::lowest();
        //    beta = DBL_MAX;
        //  }
        //  minimax(*input->position, input->position->depth + 1, alpha, beta, input->stop);
        //  if (input->position->evaluation <= alpha) {
        //    alpha_aspiration_window *= 4; // unexpected advantage for black
        //  } else if (input->position->evaluation >= beta) {
        //    beta_aspiration_window *= 4; // unexpected advantage for white
        //  } else {
        //    alpha_aspiration_window = beta_aspiration_window =
        //        aspiration_window;
        //    approximate_evaluation = input->position->evaluation;
        //  }
        //}

        done = true;
        *input->stop = true;
        done_cv.notify_one();

      } else {
        while (!*input->stop && !input->adaptive && std::any_of(input->position->outcomes->begin(), input->position->outcomes->end(), [input](Position* position){
          return (position->evaluation < (double)mate / 2.0) && (position->evaluation > (double)-mate / 2.0) && (!*input->adaptive || AdaptiveGoalReached(*position));
                            })) {
          std::sort(input->position->outcomes->begin(), input->position->outcomes->end(),
                    input->position->white_to_move ? GreaterOutcome : LessOutcome);
          for (size_t i = 0;
               i < input->position->outcomes->size() && !*input->stop; i++) {
            //assert(!input->adaptive);
            if (!*input->adaptive || AdaptiveGoalReached(*input->position)) {
              SearchPosition(
                  (*input->position->outcomes)[i],
                  std::max(FindMinDepth(*(*input->position->outcomes)[i],
                                        *input->adaptive),
                           (*input->position->outcomes)[i]->depth + 1),
                  input->stop, true);
            }  // do position->depth + 1 while
                                                 // position->depth < min_depth
          }
        }
        *input->stop = true;
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
      if (input->position->white_to_move  ==
              *input->engine_white && /*(input->position->depth >= min_depth ||*/
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
const std::string game_folder_name = "games";

std::string SlotToPath(std::string slot) {
  return (std::filesystem::path(games_folder_path) / std::filesystem::path(slot))
             .string() +
         "_PGN_position.txt";
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
        if (new_i >= 0 && new_i <= 63 && ((new_i & 7) - ((int)dest & 7)) <= 2 &&
            (new_i & 7) - ((int)dest & 7) >=
                    -2 && 
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
      [[fallthrough]];
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
  if (position2.evaluation == (position1.white_to_move ? (mate - position2.number) : (-mate + position2.number))) {
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
  position.fifty_move_rule = (unsigned char)stoi(elements[4]);
}


Position* ReadPGN(Position* position, std::string file_name) {
  std::ifstream file;
  file.open(file_name);
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

  std::smatch moves_match;
  std::string moves_line_copy = moves_line;
  int position_number = 0;
  while (std::regex_search(moves_line_copy, moves_match, PGN_moves_regex)) {
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

bool GetEngineColorInput() {
  std::string response;
  std::cout << "What color am I playing? ";
  std::getline(std::cin, response);
  while (response != "W" && response != "w" && response != "b" &&
         response != "B") {
    std::cout << "What color am I playing (please enter \"w\" or \"b\")? ";
    std::getline(std::cin, response);
  }
  return ((response == "W") || (response == "w"));
}

void UpdatePGN(Position* new_position, std::string move_string,
               std::string slot) {
  std::ifstream file;
  file.open(SlotToPath(slot));
  if (!file.is_open()) {
    std::cout << "UpdatePGN could not open file." << std::endl;
    exit(1);
  }
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
  std::string new_line = "";
  while (std::regex_search(line_copy, moves_match, moves_regex) &&
         position_number <= new_position->number - 2) {
    if (position_number <= new_position->number - 3) {
      new_line += moves_match[0];
      position_number += 2;
    } else {
      //if (!new_position->white_to_move) {
      new_line += std::to_string((int)((double)new_position->number / 2.0)) +
                  ". " + moves_match[1].str() + " ";
      //} else {
        //new_line += moves_match[1];
      //}
      position_number++;
    }
    //for (size_t i = 1; (i < moves_match.size() && moves_match[i].matched); i++) {
    //  position_number++;
    //  if (position_number == new_position->number - 1) {
    //    if (i == 1) {
    //      line.erase(line.length() -
    //                 (line_copy.length() - moves_match[i].length() - 2 -
    //                  std::to_string((int) floor((double)new_position->number / 2.0))
    //                      .length()));
    //      line += " *";
    //    } else {
    //      line.erase(line.length() - (moves_match.suffix().str().length() + 1));
    //      line += " *";
    //    }
    //  }
    //}
    line_copy = moves_match.suffix().str();
  }
  if (!new_position->white_to_move) {
    //if (new_line.length() > 5) { // there are already moves there so a space must be added
    //  new_line += " ";
    //}
    new_line += std::to_string((int)((double)new_position->number / 2.0 + 0.5)) +
        ". " + move_string;
  } else {
    new_line += move_string;
  }
  int game_status = CheckGameOver(new_position);
  switch (game_status) {
    case 2:
      new_line += " *";
      break;
    case -1:
      new_line += " 0-1";
      break;
    case 0:
      new_line += " 0-0";
      break;
    case 1:
      new_line += " 1-0";
      break;
  }

  file.close();
  lines.emplace_back(new_line);
  std::ofstream output_file;
  output_file.open(SlotToPath(slot));
  for (size_t i = 0; i < lines.size(); i++) {
    output_file << lines[i] << (i != (lines.size() - 1) ? "\n" : "");
  }
  output_file.close();
}

using SeekResult = std::pair<bool, Position*>;

SeekResult SeekPosition(Position* position, int moves, std::string slot) {
  int new_position_number = position->number + moves;
  if (new_position_number < 0) {
    return std::pair(false, position);
  }
  std::ifstream file;
  file.open(SlotToPath(slot));
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
      assert(MoveToPosition(*new_position, moves_match[i]));
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
        std::cout << "Draw by stalemate." << std::endl;
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
  std::cout << "Game over.\n";
}


//void UpdateMinDepth(Position& position) {
//  if (!position.outcomes) {
//    new_generate_moves(position);
//  }
//  double branching_factor = (double)CountMoves(position) / 2.0;
//  double material = round(position.evaluation);
//  for (size_t i = 0; i < position.outcomes->size(); i++) {
//    if (EvaluateMaterial(*(*position.outcomes)[i]) != material) {
//      branching_factor++;
//    }
//  }
//
//  min_depth = (int)std::max(
//      4.0, round(log2(max_positions) / (log2(branching_factor))));
//}


//std::string GetInput(std::vector<std::string> acceptable_responses,
//                     std::string question) {
//  std::string response;
//  std::cout << question << '?';
//  std::getline(std::cin, response);
//  while (std::none_of(acceptable_responses.begin(), acceptable_responses.end(),
//                      [response](std::string r) { return r == response; })) {
//    std::cout << question << " (please enter \"" << acceptable_responses[0]
//              << "\" or \"" << acceptable_responses
//  }
//}

const std::regex file_regex(R"regex((.+)_PGN_position.txt)regex");


using Slots = std::set<std::string>;
Slots GetSlots() {
  std::set<std::string> slots;
  if (!std::filesystem::exists(games_folder_path)) {
    return slots;
  }
  std::smatch file_name_match;
  
  for (std::filesystem::directory_entry file :
       std::filesystem::directory_iterator(games_folder_path)) {
    std::string x = file.path().filename().string();
    if (std::regex_search(x, file_name_match, file_regex)) {
      slots.insert(file_name_match[1].str());
    } else {
      std::cout << " [WARNING: UNKNOWN FILE IN GAMES] ";
    }
  }
  //std::sort(slots.begin(), slots.end());
  return slots;
}

void LogSlots(Slots slots) {
  if (slots.size() > 0) {
    std::cout << "Existing slots:\n";
    size_t count = 0;
    for (std::string n : slots) {
      count++;
      std::cout << "  \"" << n << "\"\n";
    }
  }
}



bool RenameSlot(std::string& current_slot) { // return whether the renaming was successfull
  std::string response;
  Slots slots = GetSlots();
  if (slots.size() == 0) {
    std::cout << "No slots exist.\n*Then what is being used right now?*"
              << std::endl;
    exit(1);
  }
  LogSlots(slots);
  std::cout << "Pick a slot to rename: ";
  std::getline(std::cin, response);
  while (!slots.contains(response)) {
    std::cout << "Pick a slot to rename (please enter a valid slot): ";
    std::getline(std::cin, response);
  }
  std::string to_rename = response;
  std::cout << "Pick a new name: ";
  std::getline(std::cin, response);
  std::string new_name = response;
  if (std::rename(SlotToPath(to_rename).c_str(),
                  SlotToPath(new_name).c_str())) {
    return false;
  }
  if (to_rename == current_slot) {
    current_slot = new_name;
  }
  return true;
}

void DuplicateSlot() {
  std::string response;
  std::set<std::string> slots = GetSlots();
  if (slots.size() == 0) {
    std::cout << "No slots exist.\n\n";
    return;
  }
  LogSlots(slots);
  std::cout << "Select slot to duplicate: ";
  std::getline(std::cin, response);
  while (!slots.contains(response)) {
    std::cout << "Select slot to duplicate (please enter a valid slot): ";
    std::getline(std::cin, response);
  }
  std::string to_duplicate = response;
  std::cout << "Name the duplicated file: ";
  std::getline(std::cin, response);
  std::string new_name = response;
  if (slots.contains(response)) {

    if (GetUserInput("\"" + new_name +
                         "\" already exists. Do you want to overwrite it? ",
                     "\"" + new_name +
                         "\" already exists. Do you want to overwrite it "
                         "(please enter \"yes\" or \"no\")? ",
                     {"yes", "no"}) == "no") {
      return;
    }
  }
  std::ifstream infile(SlotToPath(to_duplicate));
  std::ofstream outfile(SlotToPath(new_name));

  char c;
  while (infile.get(c)) {
    outfile.put(c);
  }

  infile.close();
  outfile.close();
  std::cout << "Slot duplicated successfully." << std::endl;
}






bool DeleteSlot(std::string current_slot) {
  std::set<std::string> slots = GetSlots();
  if (slots.size() == 0) {
      std::cout << "No slots exist." << std::endl;
      exit(1);
  }
  LogSlots(slots);
  std::string to_delete = GetUserInput(
      "Pick a slot to delete: ",
      "Pick a slot to delete (please enter a valid slot): ", slots);
  if (GetUserInput("Are you sure you want to delete \"" + to_delete + "\"? ",
                   "Are you sure you want to delete \"" + to_delete +
                       "\" (please enter \"yes\" or \"no\")?",
                   {"yes", "no"}) == "no") {
      return false;
  }
  remove(SlotToPath(to_delete).c_str());
  if (to_delete == current_slot) {
      if (GetUserInput("Current slot deleted.\nRestart? ",
                       "Restart (please enter \"1\" or \"0\")? ",
                       {"1", "0"}) == "0") {
      exit(0);
      } else {
      return true;
      }
  }
  return false;
}

using PGNTags = std::map<std::string, std::string>;

PGNTags GetPGNTags(std::string slot) {
  std::string file_name = SlotToPath(slot);
  std::ifstream file;
  file.open(file_name);
  PGNTags tags;
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
      tags[match[1]] = match[2];
      } else {
      std::cout << "Invalid line: " << line;
      }
  } while (line[0] == '[');
  return tags;
}

void ShowMaterialEvaluation(Position& position) {
  std::cout << "Material evaluation: " << EvaluateMaterial(position)
            << std::endl;
}
void ShowEngineEvaluation(Position& position) {
  std::cout << "Evaluation on depth "
            << ((position.depth == -1) ? "?" : std::to_string(position.depth))
            << ": " << Convert(position.evaluation) << std::endl;
}
void ShowEngineLine(Position& position) {
  std::cout << "Line: ";
  std::vector<Position*> line = GetLine(&position);
  // std::cout << "[got line] ";
  if (line.size() == 0) {
      std::cout << "No line predicted.\n";
      return;
  }
  std::cout << GetMove(position, *line[0]);
  // std::cout << "[got first move] ";
  for (size_t i = 1; i < line.size(); i++) {
      std::cout << ' ' << GetMove(*line[i - 1], *line[i]);
  }
  std::cout /* << "[finished line]"*/ << std::endl;
}
void ShowMoveCount(Position& position) {
  std::cout << "Moves: " << position.outcomes->size() << std::endl;
}
void ShowMaterialCount(Position& position) {
  std::cout << "Material: " << CountMaterial(position) / 2.0 << std::endl;
}

void DisplayStats(Position& position, Tools tools) {
  for (size_t i = 0; i < tools.size(); i++) {
      if (tools[i].on) {
      tools[i].display(position);
      }
  }
}


int main() {
  thread_count++;
  // while (true) {
  Position* position = Position::StartingPosition();
  std::cout << std::setprecision(7);
std::string data_path = 
  AppDataPath() + (char)std::filesystem::path::preferred_separator + "ChessCat";
if (!std::filesystem::exists(data_path)) {
      std::filesystem::create_directory(data_path);
  }
  games_folder_path = data_path +
                      (char)std::filesystem::path::preferred_separator +
                      game_folder_name;
  bool confirmation = false;
  std::string slot;
  while (!confirmation) {
      std::string response;
      std::cout << "Resume? ";
      std::getline(std::cin, response);
      while (response != "1" && response != "0") {
      std::cout << "Resume (please enter 1 or 0)? ";
      std::getline(std::cin, response);
      }
      std::set<std::string> numbers = GetSlots();
      if (response == "1") {
      if (numbers.size() == 0) {
        confirmation = GetUserInput("No saved games exist.\nStart new game? ",
                                    "No saved games exist.\nStart new game "
                                    "(please enter \"1\" or \"0\")? ",
                                    {"1", "0"}) == "1";
      } else {
        LogSlots(numbers);
        std::cout << "Choose a slot to load from: ";
        std::getline(std::cin, response);
        while (!numbers.contains(response)) {
          std::cout << "This slot does not exist. Please choose a valid slot "
                       "to load from: ";
          std::getline(std::cin, response);
        }
        slot = response;
        position = ReadPGN(position, SlotToPath(slot));
        break;
      }
      } else {
      confirmation = true;
      }
      if (confirmation) {
      if (numbers.size() == 0) {
        std::cout << "No slots exist.\n";
      }
      LogSlots(numbers);
      std::cout << "Choose a slot to save the game in: ";
      std::getline(std::cin, response);
      std::string target_slot = response;
      if (numbers.contains(response)) {
        if (GetUserInput("This slot is already occupied. Do you want "
                         "to overwrite it? ",
                         "This slot is already occupied. Do you want "
                         "to overwrite it (please enter \"yes\" or \"no\")? ",
                         {"yes", "no"}) == "no") {
          confirmation = false;
          continue;
        }
      }
      slot = target_slot;

      if (!std::filesystem::exists(games_folder_path)) {
        std::filesystem::create_directory(games_folder_path);
      }
      std::ofstream file;
      file.open(SlotToPath(slot));
      file << "[Event \"?\"]\n[Site \"?\"]\n[Date \"????.??.??\"]\n[Round "
              "\"?\"]\n[White \"?\"]\n[Black \"?\"]\n[Result \"*\"]\n\n*";
      }
  }

  // std::cout << InCheck(*position, position->to_move);

  // standard notation: type the coordinates of the starting and end point
  // (such as 6444 for e4). if you type one more point, that point becomes an
  // empty square (for en passant). For castling, type the king move and the
  // rook move right after each other. For promotions, add =[insert piece
  // type] after the pawn move (such as 1404=Q) piece types are Q, B, R, N.
  bool game_over = false;
  bool engine_color_initialized = false;
  Tools tools = {Tool(std::string("ShowMaterialEvaluation"), &ShowMaterialEvaluation, false),
                 Tool(std::string("ShowEngineEvaluation"),
                      &ShowEngineEvaluation, true),
      Tool(std::string("ShowEngineLine"), &ShowEngineLine,
           true),
      Tool(std::string("ShowMoveCount"), &ShowMoveCount, false),
      Tool(std::string("ShowMaterialCount"), &ShowMaterialCount,
           false)};
    bool engine_on =
        GetUserInput(
            "Start with engine enabled? ",
            "Start with engine enabled (please enter \"1\" or \"0\")? ",
            {"1", "0"}) == "1";
    bool engine_white = true;
    bool* adaptive = new bool(false);
    if (engine_on) {
      engine_color_initialized = true;
      engine_white = GetEngineColorInput();
      adaptive =
          new bool(GetUserInput("Adaptive? ", "Adaptive (please enter \"1\" or \"0\")? ",
                       {"1", "0"}) == "1");
      if (*adaptive) {
        std::cout << "Note: Adaptive mode decreases engine strength to make "
                     "the game more fun. Do not use adaptive mode if you want "
                     "to engine to perform at its peak.\n";
      }
    }
    bool white_on_bottom =
        GetUserInput("What color is on the bottom of the board? ",
                     "What color is on the bottom of the board (please enter "
                     "\"w\" or \"b\")? ",
                     {"w", "b"}) == "w";


    ThreadInfo* info =
        new ThreadInfo(position, new bool(false), new TrashType, new bool(engine_on), new bool(engine_white), adaptive);
    std::string move;
    Board new_board = position->board;
    Position* new_position = nullptr;


    int game_status;
    std::string lower_move;
    // new_generate_moves(*position);
    SeekResult result;
    std::cout << MakeString(*position, white_on_bottom) << std::endl;
    min_depth = FindMinDepth(*position, *adaptive);
    if (engine_white != position->white_to_move) {
      // minimax(*position, 1, INT_MIN, INT_MAX, info->stop, false); // TODO:
      // delete this line
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
    SearchPosition(position, (*adaptive ? 1 : 2), info->stop, (!*adaptive || AdaptiveGoalReached(*position)));
    DisplayStats(*position, tools);
    t_c_mutex.lock();
    thread_count++;
    if (thread_count >= max_threads) {
      threads_maxed = true;
    }
    t_c_mutex.unlock();
    _beginthread(calculate_moves, 0, info);
    //std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    while (true) {
      if (!engine_color_initialized && engine_on) {
        engine_white = GetEngineColorInput();
        *info->engine_white = engine_white;
        *info->engine_on = true;
      }
      if (CountMaterial(*position) < adaptive_off_threshold) {
        *adaptive = false;
      }
      if (engine_white == position->white_to_move && engine_on) {
        // printf("[main thread %d] block.\n", GetCurrentThreadId());
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
          if (*adaptive && !AdaptiveGoalReached(*position) && position->outcomes->size() >= 2) {// && (position->white_to_move ? (position->previous_move->evaluation < position->evaluation) : (position->previous_move->evaluation > position->evaluation))) {
            best_move = (*position->outcomes)[1];
          }
          if (!best_move->outcomes) {
            new_generate_moves(*best_move);
          }
          assert(best_move->evaluation == position->evaluation);
          std::string move_string = GetMove(*position, *best_move);

          game_status = CheckGameOver(best_move);

          UpdatePGN(best_move, move_string, slot);

          info->trash->emplace_back(position, best_move);
          info->position = best_move;
          //double e = best_move->evaluation;
          //int d = best_move->depth;
          //best_move->depth = -1;
          //minimax(*best_move, d, std::numeric_limits<double>::lowest(), INT_MAX,
          //        new bool(false));
          //  assert(best_move->evaluation == e);
          
          // doesn't work with adaptive
          if (!best_move->outcomes) {
            new_generate_moves(*best_move);
          }
          std::cout << move_string
                    << std::endl;
          std::cout << MakeString(*best_move, white_on_bottom) << std::endl;
          DisplayStats(*best_move, tools);

          position = best_move;
          if (game_status != 2) {
            game_over = true;
            LogGameEnd(*position, game_status);
          }
        }

        done = false;
        *info->stop = false;
        stop_cv.notify_one();
      }
      std::cout << "Your move: ";
      std::getline(std::cin, move);

      lower_move = ToLower(move);
      if (lower_move == "help") {
        std::cout
            << "Disable             Turn the engine off.\nEnable           "
               "   Turn the "
               "engine on.\n+N                  Seek N moves forward from "
               "current "
               "position (using PGN file).\n-N                  Seek N "
               "moves back "
               "from current position (using PGN file).\nFlipBoard         "
               "  Flip "
               "the chess board being displayed.\nDeleteSlot          Delete a "
               "slot.\nChangeColor          Change the color the engine is "
               "playing "
               "for.\nRenameSlot          Rename a slot.\nViewCurrentSlot     "
               "View the current slot being used.\nDuplicateSlot       "
               "Duplicate "
               "a slot.\nRenameSlot          Rename a slot.\nRestart           "
               "  Restart the entire program.\nChangeGameTags      Change the "
               "tags stored about the game.\nAddGameTag          Add a tag to "
               "describe the game.\nDeleteGameTag       Delete a tag from the "
               "stored game info.\nToMove              View which side is to "
               "move.\nSetAdaptive         Set adaptive on or off.\nIsAdaptive "
               "         View whether adaptive mode is on.\nManageTools        "
               " Choose which tools should be displayed.\nViewSlots           "
               "View the current slots.\nGamesPath           View the path to "
               "the folder in which games are stored.\n"
            << std::endl;
        continue;
      } else if (lower_move == "gamespath") {
        std::cout << games_folder_path << std::endl;
        continue;
      } else if (lower_move == "viewslots") {
        Slots slots = GetSlots();
        LogSlots(slots);
        continue;
      } else if (lower_move == "managetools") {
        std::cout << "Available tools:\n";
        size_t indent = 10;
        for (size_t i = 0; i < tools.size(); i++) {
          if (tools[i].name.length() + 5 > indent) {
            indent = tools[i].name.length() + 5;
          }
        }
        Slots tool_set;
        std::cout << std::string(indent + 1, '=') << std::endl;
        for (size_t i = 0; i < tools.size(); i++) {
          tool_set.insert(ToLower(tools[i].name));
          std::cout << tools[i].name
                    << std::string((indent - tools[i].name.length()), ' ')
                    << tools[i].on << std::endl << std::string(indent + 1, '=') << std::endl;
        }

        std::string selected_tool = GetUserInput(
            "Select a tool to toggle: ",
            "Select a tool to toggle (please enter a valid tool): ", tool_set);
        size_t index = 0;
        for (;; index++) {
          assert(index < tools.size());
          if (ToLower(tools[index].name) == selected_tool) {
            break;
          }
        }
        tools[index].on =
            GetUserInput(
            "Do you want this tool on? ",
            "Do you want this tool on (please enter \"1\" or \"0\")? ",
                {"1", "0"}) == "1";
        continue;
      }
/*
        show_engine_evaluation =
            GetUserInput("Do you want to display the engine's evaluation? ",
                         "Do you want to display the engine's evaluation (please "
                         "enter \"1\" or \"0\")? ",
                         {"1", "0"}) == "1";
        continue;
      } else if (lower_move == "showmovecount") {
        show_move_count =
            GetUserInput("Do you want to display the move count? ",
                         "Do you want to display the move count (please "
                         "enter \"1\" or \"0\")? ",
                         {"1", "0"}) == "1";
        continue;
      } else if (lower_move == "showmaterialeval") {
        show_material_evaluation = GetUserInput("Do you want to display the material evaluation? ",
                                 "Do you want to display the material evaluation (please "
                                 "enter \"1\" or \"0\")? ",
                                 {"1", "0"}) == "1";
        continue;
      } */else if (lower_move == "isadaptive") {
        std::cout << "Adaptive mode is " << (*adaptive ? "on" : "off") << ".\n";
        continue;
      } else if (lower_move == "changeadaptive") {
        *adaptive = GetUserInput("Adaptive? ",
                                 "Adaptive (please enter \"1\" or \"0\")? ",
                                 {"1", "0"}) == "1";
        if (CountMaterial(*position) < adaptive_off_threshold) {
          *adaptive = false;
        }
      } else if (lower_move == "tomove") {
        std::cout << "It is " << (position->white_to_move ? "white" : "black")
                  << " to move.\n";
      } else if (lower_move == "addgametag") {
        std::cout << "This function is not done." << std::endl;
        continue;
      } else if (lower_move == "changegametags") {
        PGNTags tags = GetPGNTags(slot);
        Slots tag_keys;
        for (std::pair<std::string, std::string> pair : tags) {
          tag_keys.insert(pair.first);
          std::cout << pair.first << ": \"" << pair.second << "\"" << std::endl;
        }
        std::string to_modify =
            GetUserInput("Select a tag to modify: ",
                         "Please select a valid tag to modify: ", tag_keys);
        std::string new_value;
        std::cout << "Enter a new value: ";
        std::getline(std::cin, new_value);
        if (GetUserInput("Are you sure you want to change the value of tag \"" +
                             to_modify + "\" to \"" + new_value + "\"? ",
                         "Are you sure you want to change the value of tag \"" +
                             to_modify + "\" to \"" + new_value +
                             "\" (please enter \"1\" or \"0\")? ",
                         {"1", "0"}) ==
            "1") {
          tags[to_modify] = new_value;
        }
        continue;

      } else if (lower_move == "deletegametag") {
        std::cout << "This function is not done." << std::endl;
        continue;
      } else if (lower_move == "restart") {
        std::cout << "This function has not been completed yet." << std::endl;
        continue;
      } else if (lower_move == "renameslot") {
        RenameSlot(slot);
        continue;
      } else if (lower_move == "duplicateslot") {
        DuplicateSlot();
        continue;
      } else if (lower_move == "changecolor") {
        engine_white =
            GetUserInput(
                "What color am I playing? ",
                "What color am I playing (please enter \"w\" or \"b\")? ",
                {"w", "b"}) == "w";
        *info->engine_white = engine_white;
        continue;
      } else if (lower_move == "deleteslot") {
        if (DeleteSlot(slot)) {
          std::cout << "Restarting has not been implemented yet. The program will now exit.\n";
          return 0;
        };
        continue;
      } else if (lower_move == "flipboard") {
        white_on_bottom = !white_on_bottom;
        std::cout << MakeString(*position, white_on_bottom) << std::endl;
        continue;
      } else if (lower_move == "disable") {
        engine_on = false;
        continue;
      } else if (lower_move == "enable") {
        engine_on = true;
        continue;
      } else if (move.length() == 1) {
        new_position = nullptr;
      } else if (move[0] == '+' || move[0] == '-' && move.length() > 1 &&  std::all_of(move.begin() + 1, move.end(), isdigit)) {
        result = SeekPosition(position,
                              (move[0] == '+' ? 1 : -1) *
                                  stoi(move.substr(1, (move.length() - 1))),
                              slot);
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

        std::cout << MakeString(*new_position, white_on_bottom) << std::endl;
        std::cout << Convert(new_position->evaluation) << std::endl;
        game_over = false;
        info->trash->emplace_back(position, nullptr);
        position = new_position;
        info->position = position;
        min_depth = FindMinDepth(*position, *adaptive);
        done = false;
        *info->stop = false;
        stop_cv.notify_one();
        continue;
      } else if (game_over) {
        new_position = nullptr;
      } else if (!std::regex_match(lower_move, move_input_regex)) {
        new_position = nullptr;
      } else {
        new_position = MoveToPosition(*position, lower_move);
      }
      if (new_position != nullptr) {
        stop_mutex.lock();
        *info->stop = true;
        stop_mutex.unlock();
        waiting.acquire();
        UpdatePGN(new_position, GetMove(*position, *new_position), slot);
        if (!new_position->outcomes) {
          new_generate_moves(*new_position);
        }

        std::cout << MakeString(*new_position, white_on_bottom) << std::endl;
        DisplayStats(*new_position, tools);


        info->trash->emplace_back(position, new_position);
        position = new_position;
        info->position = position;
        if (!new_position->outcomes) {
          new_generate_moves(*new_position);
        }

        min_depth = FindMinDepth(*position, *adaptive);
        done = false;
        *info->stop = false;
        stop_cv.notify_one();
      } else {
        std::cout << "Invalid move. Type 'help' to view commands." << std::endl;
      }
    }
  //}

  return 0;
}