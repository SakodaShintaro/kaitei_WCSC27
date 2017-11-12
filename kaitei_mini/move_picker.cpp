#include"move_picker.hpp"
#include"position.hpp"
#include"history.hpp"
#include<functional>
#include<algorithm>

enum Stage {
    MAIN_SEARCH_START,
	MAIN_SEARCH_CAPTURE,
	MAIN_SEARCH_NON_CAPTURE,
	MAIN_SEARCH_END,

    QSEARCH_START,
    QSEARCH_CAPTURE,
    QSEARCH_END,
};

MovePicker::MovePicker(const Position& pos, const Move ttMove, const Depth depth, const History& history) 
	: pos_(pos), tt_move_(ttMove), depth_(depth), history_(history) {
	stage_ = MAIN_SEARCH_START;
    moves_ = new Move[MAX_MOVE_LIST_SIZE];

    //�܂�������͐擪���w��
    cur_ = moves_;
    end_ = moves_;
    if (pos.isKingChecked(pos.color())) {
        //���肪�������Ă�����S���������čŌ��NULL_MOVE�ǉ�
        pos.generateEvasionMoves(end_);
        *(end_++) = NULL_MOVE;
    }
}

MovePicker::MovePicker(const Position& pos, const Move ttMove, const Depth depth, const History& history, const Square to)
    : pos_(pos), tt_move_(ttMove), depth_(depth), history_(history), to_(to) {
    stage_ = QSEARCH_START;
    moves_ = new Move[MAX_MOVE_LIST_SIZE];

    //�܂�������͐擪���w��
    cur_ = moves_;
    end_ = moves_;
    if (pos.isKingChecked(pos.color())) {
        //���肪�������Ă�����S���������čŌ��NULL_MOVE�ǉ�
        pos.generateEvasionMoves(end_);
        *(end_++) = NULL_MOVE;
    }
}

Move MovePicker::nextMove() {
    //NULL_MOVE���Ԃ�����I���Ƃ�������

	while (cur_ == end_) {
		//���̃J�e�S���̎�𐶐�
		generateNextStage();
	}

    return *(cur_++);
}

void MovePicker::generateNextStage() {
	switch (stage_++) {
        //�ʏ�T������Ă΂��Ƃ��͂�������
    case MAIN_SEARCH_START:
        //tt_move��ݒ肷��
        if (tt_move_ != NULL_MOVE && pos_.isLegalMove(tt_move_)) {
            *(end_++) = tt_move_;
        }
        break;
    case MAIN_SEARCH_CAPTURE:
		pos_.generateCaptureMoves(end_);
        scoreCapture();
	    break;
	case MAIN_SEARCH_NON_CAPTURE:
		pos_.generateNonCaptureMoves(end_);
        scoreNonCapture();
        break;
	case MAIN_SEARCH_END:
        *(end_++) = NULL_MOVE;
        //�����MovePicker����NULL_MOVE���Ԃ�悤�ɂȂ�̂Ŏw���萶�����I���
		break;


        //�Î~�T������Ă΂��Ƃ��͂�������
    case QSEARCH_START:
        //tt_move��ݒ肷��
        if (tt_move_ != NULL_MOVE && pos_.isLegalMove(tt_move_)) {
            *(end_++) = tt_move_;
        }
        break;
    case QSEARCH_CAPTURE:
        if (depth_ == 0) {
            //�[�������傤��0�̂Ƃ�������������ׂĐ���
            pos_.generateCaptureMoves(end_);
        } else {
            //�[�������Ȃ���Ԃ��肾���𐶐�
            pos_.generateRecaptureMovesTo(to_, end_);
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
        itr->score = static_cast<Score>(kind(itr->capture()));
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