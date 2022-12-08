#ifndef THREADPOOL_H
#define THRAEDPOOL_H

#include <pthread.h>
#include <list>
#include "locker.h"
#include <cstdio>

// 线程池类，定义成模板类是为了代码的复用
// 模板参数T 就是 任务类
template <typename T>
class threadpool
{
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T *request);

private:
    static void *worker(void *arg);
    void run();

private:
    // 线程的数量
    int m_thread_number;

    // 线程池数组
    pthread_t *m_threads;

    // 请求队列中最多允许的，等待处理的请求数量
    int m_max_requests;

    // 请求队列
    std::list<T *> m_workqueue;

    // 互斥锁
    locker m_queuelocker;

    // 信号量用来判断是否有任务需要处理
    sem m_queuestat; // 调用默认构造函数

    // 是否结束线程
    bool m_stop;
};

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : 
        m_thread_number(thread_number), m_max_requests(max_requests),
        m_stop(false), m_threads(NULL)
{
    if ((thread_number <= 0) || max_requests <= 0)
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if (!m_threads) // NULL
    {
        throw std::exception();
    }

    for (int i = 0; i < thread_number; i++)
    {
        printf("create the %dth thread\n", i);

        // worker 必须是静态函数，静态函数不能使用 成员变量 和 成员函数
        // 所以 传递 threadpool 对象的 指针作为 参数
        if (pthread_create(m_threads + i, NULL, worker, (void*)this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }

        if (pthread_detach(m_threads[i]) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *request)
{
    // 给队列加锁
    m_queuelocker.lock();

    // 超过队列最大请求个数
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);

    m_queuelocker.unlock();
    
    // 增加信号量，用于唤醒阻塞的线程
    m_queuestat.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{
    // 静态函数不能访问 成员变量 和 成员函数
    // 只能通过传参的方式 传递参数
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    // 一个线程的运行，就是不断地循环
    // 实际上还是要阻塞，通过信号量来实现
    while (!m_stop)
    {
        // 看是否有信号量，没有就阻塞
        m_queuestat.wait();

        // 被唤醒
        // 检查 工作队列是否为空
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (!request)
        {
            continue;
        }

        // 处理请求
        // 由请求自身定义自己怎么处理
        request->process();
    }
}
#endif