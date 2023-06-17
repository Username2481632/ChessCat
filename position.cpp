#include "position.h"

#include "constants.h"

Position::Position(bool wtm, int n, double e, Board b,
                   Castling c /*, time_t t*/, std::vector<Position*>* o,
                   Position* pm, int d,
                   /*Position* bm,*/ Kings k, int ept, size_t fmc, bool wc)
    : white_to_move(wtm),
      number(n),
      evaluation(e),
      board(b),
      castling(c),
      // time(t),
      outcomes(o),
      depth(d),
      previous_move(pm),
      // best_move(bm),
      kings(k),
      en_passant_target(ept),
      fifty_move_rule(fmc),was_capture(wc) {
  // boards_created++;
}

// static
Position* Position::StartingPosition() {
    return new Position(true, 0, 0.0, starting_board,
                        Castling(true, true, true, true), nullptr, nullptr, -1,
                        Kings(60, 4), -1, 0, false);
}

Position Position::GenerateMovesCopy() {
    return Position(!white_to_move, number + 1, round(evaluation), board,
                    castling, nullptr /* outcomes */, this /* previous_move */,
                    -1 /*depth*/, /*
nullptr /* best_move */
                    /*, */ kings, -1, fifty_move_rule + 1, false);
}

Position::~Position() {
    // delete all outcomes pointers
    if (outcomes) {
      for (size_t i = 0; i < outcomes->size(); i++) {
        delete (*outcomes)[i];
        // boards_destroyed++;
      }
      delete outcomes;
      outcomes = nullptr;
    }
}

Position* Position::CreateDeepCopy() {
    Position* new_position =
        new Position(white_to_move, number, evaluation, board, castling,
                     nullptr /* outcomes */, previous_move /* previous_move */,
                     depth /*,
nullptr */ /* best_move */, kings, en_passant_target, fifty_move_rule, was_capture);
    return new_position;
}

Position* Position::RealDeepCopy() const {  // previous move is not a deep copy
    Position* new_position =
        new Position(white_to_move, number, evaluation, board,
                     castling /*, time*/, nullptr /* outcomes */,
                     previous_move /* previous_move */, depth /*,
nullptr */ /* best_move */, kings, en_passant_target, fifty_move_rule, was_capture);

    if (outcomes) {
      new_position->outcomes = new std::vector<Position*>;
      for (size_t i = 0; i < outcomes->size(); i++) {
        new_position->outcomes->emplace_back((*outcomes)[i]->RealDeepCopy());
        //(*new_position->outcomes)[i]->previous_move = new_position;
      }
    }

    return new_position;
}