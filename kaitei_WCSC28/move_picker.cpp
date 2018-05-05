#include"move_picker.hpp"
#include"position.hpp"
#include"history.hpp"
#include"eval_params.hpp"
#include<functional>
#include<algorithm>

extern int piece_value[];

enum Stage {
    MAIN_SEARCH_START,
    MAIN_SEARCH_TT,
	MAIN_SEARCH_CAPTURE,
    MAIN_SEARCH_KILLERS,
	MAIN_SEARCH_NON_CAPTURE,
	MAIN_SEARCH_END,

    ROOT_NODE,

    QSEARCH_START,
    QSEARCH_TT,
    QSEARCH_CAPTURE,
    QSEARCH_END,
};

MovePicker::MovePicker(const Position& pos, const Move ttMove, const Depth depth, const History& history, const Move killers[2])
	: pos_(pos), tt_move_(ttMove), depth_(depth), history_(history) {
	stage_ = MAIN_SEARCH_START - 1;
    moves_ = new Move[MAX_MOVE_LIST_SIZE];

    //�܂�������͐擪���w��
    cur_ = moves_;
    end_ = moves_;

    killer_moves_[0] = killers[0];
    killer_moves_[1] = killers[1];
}

MovePicker::MovePicker(const Position& pos, const Move ttMove, const Depth depth, const History& history)
    : pos_(pos), tt_move_(ttMove), depth_(depth), history_(history) {
    stage_ = QSEARCH_START - 1;
    moves_ = new Move[MAX_MOVE_LIST_SIZE];

    //�܂�������͐擪���w��
    cur_ = moves_;
    end_ = moves_;
}

Move MovePicker::nextMove() {
    //NULL_MOVE���Ԃ�����I���Ƃ�������
    while (true) {
        while (cur_ == end_) {
            //���̃J�e�S���̎�𐶐�
            generateNextStage();
        }

        switch (stage_) {
        case MAIN_SEARCH_START:
        case MAIN_SEARCH_TT:
            return *(cur_++);
        case MAIN_SEARCH_CAPTURE:
            //tt_move�Ƃ̏d���폜
            if (*cur_ != tt_move_) {
                return *(cur_++);
            }
            cur_++;
            break;
        case MAIN_SEARCH_KILLERS:
            return *(cur_++);
        case MAIN_SEARCH_NON_CAPTURE:
            //tt_move, killer_moves�Ƃ̏d���폜
            if (*cur_ != tt_move_ && *cur_ != killer_moves_[0] && *cur_ != killer_moves_[1]) {
                return *(cur_++);
            }
            cur_++;
            break;
        case MAIN_SEARCH_END:
            return *(cur_++);


            //�Î~�T��
        case QSEARCH_START:
        case QSEARCH_TT:
            return *(cur_++);
        case QSEARCH_CAPTURE:
            //tt_move�Ƃ̏d���폜
            if (*cur_ != tt_move_) {
                return *(cur_++);
            }
            cur_++;
            break;
        case QSEARCH_END:
            return *(cur_++);
        }
    }
}

void MovePicker::generateNextStage() {
	switch (++stage_) {
        //�ʏ�T������Ă΂��Ƃ��͂�������
    case MAIN_SEARCH_START:
        if (pos_.isKingChecked()) {
            //���肪�������Ă�����S���������čŌ��NULL_MOVE�ǉ�
            pos_.generateEvasionMovesBB(end_);
            *(end_++) = NULL_MOVE;
        }
        break;
    case MAIN_SEARCH_TT:
        //tt_move��ݒ肷��
        //tt�ň�@�肪������͎̂d�����Ȃ��̂��Ȃ�
#if DEBUG
        if (tt_move_ != NULL_MOVE && !pos_.isLegalMove(tt_move_)) {
            pos_.print();
            Position tmp(pos_);
            tmp.printAllMoves();
            tt_move_.print();
            assert(false);
        }
#endif
        if (tt_move_ != NULL_MOVE && pos_.isLegalMove(tt_move_)) {
            *(end_++) = tt_move_;
        }
        break;
    case MAIN_SEARCH_CAPTURE:
        pos_.generateCaptureMovesBB(end_);
        scoreCapture();
        break;
    case MAIN_SEARCH_KILLERS:
        if (killer_moves_[0] != NULL_MOVE && pos_.isLegalMove(killer_moves_[0]) && killer_moves_[0] != tt_move_)
            *(end_++) = killer_moves_[0];
        if (killer_moves_[1] != NULL_MOVE && pos_.isLegalMove(killer_moves_[1]) && killer_moves_[1] != tt_move_)
            *(end_++) = killer_moves_[1];
        break;
	case MAIN_SEARCH_NON_CAPTURE:
		pos_.generateNonCaptureMovesBB(end_);
        scoreNonCapture();
        break;
	case MAIN_SEARCH_END:
        *(end_++) = NULL_MOVE;
        //�����MovePicker����NULL_MOVE���Ԃ�悤�ɂȂ�̂Ŏw���萶�����I���
		break;


        //�Î~�T������Ă΂��Ƃ��͂�������
    case QSEARCH_START:
        if (pos_.isKingChecked()) {
            //���肪�������Ă�����S���������čŌ��NULL_MOVE�ǉ�
            pos_.generateEvasionMovesBB(end_);
            *(end_++) = NULL_MOVE;
        }
        break;
    case QSEARCH_TT:
        //tt_move��ݒ肷��
        //tt�ň�@�肪������͎̂d�����Ȃ��̂��Ȃ�
#if DEBUG
        if (tt_move_ != NULL_MOVE && !pos_.isLegalMove(tt_move_)) {
            pos_.print();
            Position tmp(pos_);
            tmp.printAllMoves();
            tt_move_.print();
            assert(false);
        }
#endif
        if (tt_move_ != NULL_MOVE && pos_.isLegalMove(tt_move_) && tt_move_.capture() != EMPTY) {
            *(end_++) = tt_move_;
        }
        break;
    case QSEARCH_CAPTURE:
        if (depth_ == 0) {
            //�[�������傤��0�̂Ƃ�������������ׂĐ���
            pos_.generateCaptureMovesBB(end_);
        } else {
            //�[�������Ȃ���Ԃ��肾���𐶐�
            pos_.generateRecaptureMovesToBB(pos_.lastMove().to(), end_);
        }
        scoreCapture();
        break;
    case QSEARCH_END:
        *(end_++) = NULL_MOVE;
        //�����MovePicker����NULL_MOVE���Ԃ�悤�ɂȂ�̂Ŏw���萶�����I���
        break;
	}
}

void MovePicker::scoreCapture() {
    for (auto itr = cur_; itr != end_; ++itr) {
        if (pos_.isMoreControl(itr->to())) { //�G�Ȕ���
            itr->score = static_cast<Score>(piece_value[itr->capture()]);
            if (itr->score < 0) { //���̋��Capture����ƕ��̒l���t��
                itr->score *= -1;
            }
        } else { //�����
            itr->score = static_cast<Score>(-piece_value[itr->subject()]);
            if (itr->score > 0) { //���̋�ŋCapture����Ɛ��̒l�����Ă��܂�
                itr->score *= -1;
            }
        }
    }
    std::sort(cur_, end_, std::greater<Move>());
}

void MovePicker::scoreNonCapture() {
    for (auto itr = cur_; itr != end_; ++itr) {
        itr->score = Score(0);
        if (itr->isPromote()) {
            if (pieceToColor(itr->subject()) == BLACK) {
                itr->score += piece_value[promote(itr->subject())] - piece_value[itr->subject()];
            } else {
                itr->score -= piece_value[promote(itr->subject())] - piece_value[itr->subject()];
            }
        }
        itr->score += history_[*itr];

        //������KP�̒l��p����̂͂ǂ��Ȃ񂩂�
        //����ς肿����Ǝキ�Ȃ��Ă�H(�v����)
        //if (kind(itr->subject()) != KING && !itr->isDrop()) {
        //    itr->score += eval_params->kp[SquareToNum[pos_.kingSquare(BLACK)]][pieceState(itr->subject(), itr->to())]
        //        - eval_params->kp[SquareToNum[pos_.kingSquare(BLACK)]][pieceState(itr->subject(), itr->from())];
        //    itr->score += eval_params->kp[SquareToNum[InvSquare[pos_.kingSquare(WHITE)]]][invPieceState(pieceState(itr->subject(), itr->to()))]
        //        - eval_params->kp[SquareToNum[InvSquare[pos_.kingSquare(WHITE)]]][invPieceState(pieceState(itr->subject(), itr->from()))];
        //}
    }
    std::sort(cur_, end_, std::greater<Move>());
}