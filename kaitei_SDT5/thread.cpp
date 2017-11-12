#include"thread.hpp"

#include"position.hpp"
#include"types.hpp"
#include"move_picker.hpp"
#include"shared_data.hpp"
#include"usi_options.hpp"

ThreadPool threads;
SharedData shared_data;

void Thread::idleLoop() {
    while (!exit_) {
        std::unique_lock<std::mutex> lock(mutex_);

        //exit_��searching_��true�ɂȂ�܂ŃX���[�v����
        sleep_condition_.wait(lock, [this]() {
            return exit_ || searching_;
        });

        lock.unlock();
        if (exit_)
            break;

        //�����ŒT���J�n
        searcher_.think();

        //�T���𔲂����̂Ńt���O��false��
        mutex_.lock();
        searching_ = false;
        sleep_condition_.notify_one();
        mutex_.unlock();
    }
}

ThreadPool::ThreadPool() {
    //���C���X���b�h�����
    this->emplace_back(new MainThread(0));
}