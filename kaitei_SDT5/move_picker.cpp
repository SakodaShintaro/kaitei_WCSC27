#include"move_picker.hpp"
#include"position.hpp"
#include"history.hpp"
#include<functional>
#include<algorithm>

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
        if (pos_.isKingChecked(pos_.color())) {
            //���肪�������Ă�����S���������čŌ��NULL_MOVE�ǉ�
            pos_.generateEvasionMovesBB(end_);
            *(end_++) = NULL_MOVE;
        }
        break;
    case MAIN_SEARCH_TT:
        //tt_move��ݒ肷��
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
        if (pos_.isKingChecked(pos_.color())) {
            //���肪�������Ă�����S���������čŌ��NULL_MOVE�ǉ�
            pos_.generateEvasionMovesBB(end_);
            *(end_++) = NULL_MOVE;
        }
        break;
    case QSEARCH_TT:
        //tt_move��ݒ肷��
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
        if (pos_.isMoreControl(itr->to()))
            itr->score = static_cast<Score>(kind(itr->capture()));
        else
            itr->score = static_cast<Score>(-kind(itr->subject()));
    }
    std::sort(cur_, end_, std::greater<Move>());
}

void MovePicker::scoreNonCapture() {
    for (auto itr = cur_; itr != end_; ++itr) {
        itr->score = Score(0);
        if (itr->isPromote())
            itr->score += 100;
        itr->score += history_[*itr];
    }
    std::sort(cur_, end_, std::greater<Move>());
}