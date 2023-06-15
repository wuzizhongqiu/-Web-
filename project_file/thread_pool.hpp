#pragma onece

#include "task.hpp"

#include <iostream>
#include <queue>
#include <pthread.h>

#define NUM 10

class ThreadPool {
private:
    int _num;
    static int _cnt;
    bool _stop;
    std::queue<Task> _task_queue;
    pthread_mutex_t _lock;
    pthread_cond_t _cond;

    static ThreadPool* single_instance;

private: //构建单例
    ThreadPool(int num = NUM)
        : _num(num)
        , _stop(false)
    {
        pthread_mutex_init(&_lock, nullptr);
        pthread_cond_init(&_cond, nullptr); 
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

public:
    ~ThreadPool()
    {
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_cond);
    }

public:
    static ThreadPool* GetInstance() {
        static pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
        if(single_instance == nullptr) { //双检测锁可以让新线程不再去申请锁的资源，直接去调用之前创建的单例
            pthread_mutex_lock(&mutex_);
            if(single_instance == nullptr) { //这次检测可以保证线程的安全并减少消耗
                single_instance = new ThreadPool();
                single_instance->InitThreadPool();
            }
            pthread_mutex_unlock(&mutex_);
        }
        return single_instance;
    }

    static void* ThreadRoutine(void* args) {
        pthread_detach(pthread_self());
        ThreadPool* tp = (ThreadPool*)args;
        while(true) {
            tp->Lock();
            while(tp->TaskQueueIsEmpty()) {
                tp->ThreadWait(); //当我醒来的时候，一定是占用着互斥锁的
            }
            Task t;
            tp->PopTask(t);
            if(--_cnt == 0) { //如果线程池用完了，就再开一个线程池（这个地方想了真的好久啊，总算是解决了）
                single_instance = nullptr;
                _cnt = NUM;
            }
            tp->UnlocK();
            t.ProcessOn();
        }
    }

    bool InitThreadPool() {
        for(int i = 0; i < _num; i++) {
            pthread_t tid;
            if(pthread_create(&tid, nullptr, ThreadRoutine, this) != 0) { //创建失败
                LOG(FATAL, "create thread pool error!");
                return false;
            }
        }
        LOG(INFO, "create thread pool sucess!");
        return true;
    }   

    void PushTask(const Task& task) { //输入任务
        Lock();
        _task_queue.push(task);
        UnlocK();
        ThreadWakeUp(); //唤醒
    }

    void PopTask(Task& task) { //拿出任务
        task = _task_queue.front();
        _task_queue.pop();
    }


    void ThreadWait() {
        pthread_cond_wait(&_cond, &_lock);
    }

    void ThreadWakeUp() {
        pthread_cond_signal(&_cond);
    }

    bool TaskQueueIsEmpty() {
        return _task_queue.size() == 0 ? true : false;
    }

    bool IsStop() {
        return _stop;
    }

public:
    void Lock() {
        pthread_mutex_lock(&_lock);
    }

    void UnlocK() {
        pthread_mutex_unlock(&_lock);
    }
};

ThreadPool* ThreadPool::single_instance = nullptr;
int ThreadPool::_cnt = NUM;