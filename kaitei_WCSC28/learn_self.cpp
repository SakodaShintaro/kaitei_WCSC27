#include"learn_self.hpp"
#include"position.hpp"
#include"searcher.hpp"
#include"eval_params.hpp"
#include"thread.hpp"
#include"position_for_learn.h"
#include<cmath>

//���񉻃X���b�h
static std::vector<std::thread> slave_threads;

//�~�j�o�b�`�T�C�Y
//�ǂꂭ�炢�������̂��悭�킩��Ȃ�
static int mini_batch_size;

//�w�K��
static double learn_rate;

static std::mutex MUTEX;

static std::atomic<bool> stop_learn;

static std::chrono::time_point<std::chrono::steady_clock> start_time;

static std::unique_ptr<EvalParams<double>> learning_parameters(new EvalParams<double>);

static std::ofstream learn_log;

static unsigned long long sum_learned_games = 0, sum_learned_pos = 0;

static std::atomic<unsigned long long> win_num(0), lose_num(0), consecutive_lose_num(0);

static double threshold, win_sum = 0.5, deep_coefficient;

static int teacher_depth;

void learnSelf() {
    std::cout << "start learnSelf()" << std::endl;

    start_time = std::chrono::steady_clock::now();

    //����őҋ@���Ă��T���p�X���b�h�͏I�����Ă����͂�
    threads.clear();

    //�]���֐��ǂݍ���
    eval_params->readFile();
    //�p�����[�^���w�K�p�̂��̂ɃR�s�[���Ă���
    learning_parameters->copy(eval_params);

    //�I�v�V�������w�K�����ɐݒ�
    shared_data.limit_msec = LLONG_MAX;
    usi_option.byoyomi_margin = 0LL;

    std::cout << "mini_batch_size = ";
    std::cin >> mini_batch_size;
    std::cout << "learn_rate = ";
    std::cin >> learn_rate;
    std::cout << "threshold(0.0~1.0) = ";
    std::cin >> threshold;
    std::cout << "���t�ǖʂ����T���[�� = ";
    std::cin >> teacher_depth;
    std::cout << "�[���T���̕��ɂ�����W��(0.0~1.0) = ";
    std::cin >> deep_coefficient;

    unsigned int thread_num;
    std::cout << "thread_num = ";
    std::cin >> thread_num;
    //�Œ�ł�1,�ō��ł��g���鐔�܂�
    thread_num = std::min(std::max(1u, thread_num), std::thread::hardware_concurrency());
    printf("�g�p����X���b�h�� = %u\n", thread_num);

    stop_learn = false;

    learn_log.open("learn_self.log");
    learn_log << "mini_batch_size " << mini_batch_size << " learn_rate " << learn_rate << " threshold " << threshold << std::endl;

    //�X���b�h�̍쐬
    slave_threads.resize(thread_num);
    for (unsigned int i = 0; i < thread_num; i++) {
        slave_threads[i] = std::thread(learnSelfSlave, i);
    }

    while (true) {
        std::string input;
        std::cin >> input;
        if (input == "stop") {
            stop_learn = true;
            break;
        }
    }
    for (unsigned int i = 0; i < thread_num; i++) {
        slave_threads[i].join();
        printf("%2d�X���b�h��join\n", i);
    }

    learn_log.close();
    std::cout << "finish learnSelf()" << std::endl;
}

void learnSelfSlave(int id) {
    //�����_���Ɏw���萔�������_���Ɍ��肷��
    std::random_device seed_gen;
    std::default_random_engine engine(seed_gen());
    std::uniform_int_distribution<unsigned int> dist(2, 20);

    //games[j]��first��Move�̕���(����), second�͌���(1�Ȃ��菟��,0�Ȃ��菟��)
    std::vector<std::pair<std::vector<Move>, double>> games(mini_batch_size);

    //�T���N���X
    //history���傫������(?) ���I�Ɋm�ۂ��Ȃ��ƃX�^�b�N�I�[�o�[�t���[�ɂȂ�
    std::unique_ptr<Searcher> searcher(new Searcher(Searcher::MAIN));

    //���z
    std::unique_ptr<EvalParams<double>> grad(new EvalParams<double>);

    for (unsigned long long i = 0; !stop_learn; i++) {
        games.clear();
        games.resize(mini_batch_size);

        double win_point = 0;

        //���������߂�
        for (int j = 0; j < mini_batch_size; j++) {
            Position pos;
            std::unique_ptr<PositionForLearn> pos_standard(new PositionForLearn);
            pos_standard->ptr.reset(new EvalParams<int>());
            pos_standard->ptr->readFile();

            if (j == 0) {
                std::cout << "eval_params->abs() = " << eval_params->abs() << ", ptr->abs() = " << pos_standard->ptr->abs() << std::endl;
            }

            games[j].first.clear();

            unsigned int random_turn = dist(engine);
            while (true) {
                //j�������̂Ƃ�pos�����
                Move best_move = ((pos.turn_number() % 2) == (j % 2) ? searcher->thinkForGenerateLearnData(pos, Depth(teacher_depth) * (int)PLY, random_turn)
                    : searcher->thinkForGenerateLearnData(*pos_standard, Depth(teacher_depth) * (int)PLY, random_turn));

                if (stop_learn) { //��~�V�O�i���̊m�F
                    return;
                }

                if (best_move == NULL_MOVE || (best_move.score != MIN_SCORE && best_move.score <= -4000)) { //NULL_MOVE�͓���������
                    if (pos.color() == BLACK) {
                        games[j].second = 0.0;
                    } else {
                        games[j].second = 1.0;
                    }
                    break;
                }

                if (pos.turn_number() % 2 == 1 && best_move.score != MIN_SCORE) {
                    best_move.score = -best_move.score;
                }

                games[j].first.push_back(best_move);
                if (!pos.isLegalMove(best_move)) {
                    pos.printForDebug();
                    best_move.printWithScore();
                }
                pos.doMove(best_move);
                pos_standard->doMove(best_move);

                Score dummy;
                if (pos.isRepeating(dummy)) { //�����
                    games[j].second = 0.5;
                    break;
                }

                if (pos.turn_number() >= 256) { //�������������H
                                                //�������]���l�Ɋ�Â������ʂƂ���
                    //games[j].second = sigmoid((double)best_move.score, 1.0 / 600);
                    ////�������ő�ł�0.25~0.75���炢�ɃL���b�v����
                    //games[j].second = std::max(0.25, std::min(0.75, games[j].second));
                    //�������������̕����ǂ�����
                    games[j].second = 0.5;
                    break;
                }
            }
            //�����ǖڂ�pos�����,�����łȂ��Ƃ���pos_standard�����
            if (j % 2 == 0) {
                win_point += games[j].second;
            } else {
                win_point += 1 - games[j].second;
            }

            //if ((j + 1) % 1 == 0) {
            //    printf("(%2d) %3d�ǐ����I�� : %3u��, result = %.2f, random_turn = %3u\n", id, j + 1, pos.turn_number(), games[j].second, random_turn);
            //}
        }
        win_point /= mini_batch_size;

        //�w�K����
        grad->clear();
        std::pair<double, double> sum_loss = { 0.0, 0.0 };
        unsigned int learn_pos_num = 0;
        for (int j = 0; j < mini_batch_size; j++) {
            auto game = games[j];
            const int finish_turn_num = static_cast<int>(game.first.size());

            Position pos;

            for (Move m : game.first) {
                if (m.score == MIN_SCORE) { //�����_�����[�u�Ƃ������ƂȂ̂Ŋw�K�͂��Ȃ�
                    if (!pos.isLegalMove(m)) {
                        pos.printForDebug();
                        m.printWithScore();
                    }

                    pos.doMove(m);
                    continue;
                }

                if (stop_learn) { //��~�V�O�i���̊m�F
                    return;
                }

                if (isMatedScore(m.score)) { //�l�݂̒l�������甲����
                    break;
                }

                //�󂢒T���̕]���l
                Move shallow_best = (searcher->thinkForGenerateLearnData(pos, Depth(0), 0));
                
                auto pv = searcher->pv();
                int move_num = 0;
                for (auto pv_move : pv) {
                    pos.doMove(pv_move);
                    move_num++;
                }

                Score shallow_score = pos.score();

                //�����̌v�Z
                auto loss = calcLoss(shallow_score, m.score, game.second);
                sum_loss.first += loss.first;
                sum_loss.second += loss.second;
                learn_pos_num++;

                //���z��ω��������
                double delta = calcGrad(shallow_score, m.score, game.second, pos.turn_number(), finish_turn_num);

                //���z���X�V
                grad->updateGradient(pos.makeEvalElements(), delta);

                for (int num = 0; num < move_num; num++) {
                    pos.undo();
                }

                if (!pos.isLegalMove(m)) {
                    pos.printForDebug();
                    m.printWithScore();
                }

                pos.doMove(m);
            }
        }

        MUTEX.lock();
        //�p�����[�^�X�V
        //double�̂��̂Ŋw�K���Ă����ʏ�g������(int)�ɃR�s�[����
        learning_parameters->updateParamsSGD(grad, learn_rate);
        eval_params->roundCopy(learning_parameters);

        //�����o��
        eval_params->writeFile("tmp.bin");

        //�����ɂ��Ďw���ړ����ς����
        win_sum = 0.8 * win_sum + 0.2 * win_point;

        sum_learned_games += mini_batch_size;

        auto now_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now_time - start_time);
        const int seconds = static_cast<int>(elapsed.count());
        assert(learn_pos_num > 0);
        sum_learned_pos += learn_pos_num;
        printf("(%2d) %5lld��I��(�v%9lld��, %9lld�ǖ�):%3d����%2d��%2d�b�o��, ���� = (%.5f, %.5f)\n", id, i + 1, sum_learned_games, sum_learned_pos, seconds / 3600, seconds % 3600 / 60, seconds % 60,
            sum_loss.first / learn_pos_num, sum_loss.second / learn_pos_num);
        learn_log << sum_loss.first / learn_pos_num << " " << sum_loss.second / learn_pos_num << std::endl;

        //���ȑ΋ǂ̌��ʂ��m�F����
        printf("(%2d) ����̏��� : %3.1f, ���Ϗ��� : %3.1f  ", id, win_point * 100.0, win_sum * 100);

        if (win_sum >= threshold) {
            //�����z�����Ȃ�t�@�C���ɏ�������
            eval_params->writeFile();
            win_sum = 0.3;
            consecutive_lose_num = 0;
            win_num++;
        } else {
            //�����z����
            std::cout << ++consecutive_lose_num << "�A�s ";
            lose_num++;
        }
        std::cout << win_num << "��" << lose_num << "�s" << std::endl;

        MUTEX.unlock();
    }
}

double calcGrad(Score shallow_score, Score deep_score, double result, int curr_turn_num, int finish_turn_num) {
    double shallow_win_rate = sigmoid((double)shallow_score, 1.0 / 600);
    double deep_win_rate = sigmoid((double)deep_score, 1.0 / 600);
    return deep_coefficient * (shallow_win_rate - deep_win_rate) + (1 - deep_coefficient) * (shallow_win_rate - result);
}

std::pair<double, double> calcLoss(Score shallow_score, Score deep_score, double result) {
    std::pair<double, double> loss;
    double shallow_win_rate = sigmoid((double)shallow_score, 1.0 / 600);
    double deep_win_rate = sigmoid((double)deep_score, 1.0 / 600);

    constexpr double epsilon = 0.000001;
    loss.first = (-deep_win_rate * std::log(shallow_win_rate + epsilon) - (1.0 - deep_win_rate) * std::log(1.0 - shallow_win_rate + epsilon));
    loss.second = (-result * std::log(shallow_win_rate + epsilon) - (1.0 - result) * std::log(1.0 - shallow_win_rate + epsilon));

    if (std::isnan(loss.first) || std::isnan(loss.second)) {
        printf("shallow_score = %d, deep_score = %d, result = %f\n", shallow_score, deep_score, result);
        assert(false);
    }

    return loss;
}