#pragma once

#include"move.hpp"
#include<vector>

class Position;
class History;
extern const int MAX_MOVE_LIST_SIZE;

class MovePicker {
private:
	Move* begin() { return moves_; }
    Move* end() { return end_; }

	const Position& pos_;

	Depth depth_;
	Move tt_move_;

	//�w���萶���̒i�K
	int stage_;

	//���ɕԂ���A�������ꂽ�w����̖���
    Move *cur_, *end_;

    //�z����m�ۂ���p�̃|�C���^
    Move *moves_;

    const History& history_;

    Square to_;

public:
	//�ʏ�T������Ă΂��ۂ̃R���X�g���N�^
	MovePicker(const Position& pos, const Move ttMove, const Depth depth, const History& history);

    //�Î~�T������Ă΂��ۂ̃R���X�g���N�^
    MovePicker(const Position& pos, const Move ttMove, const Depth depth, const History& history, const Square to);

    ~MovePicker() {
        delete[] moves_;
    }
	Move nextMove();
	void generateNextStage();
    void scoreCapture();
    void scoreNonCapture();

    void printAllMoves() {
        for (auto itr = cur_; itr != end_; itr++) {
            itr->printWithScore();
        }
    }
};

extern int FromToHistory[SquareNum][SquareNum];
extern int PieceToHistory;