#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>

//对信号量的封装
//就是封装的 PV操作
class sem
{
private:
    /* data */
    sem_t m_sem;
public:

    sem();
    sem(int num);
    ~sem();
    //封装的信号减一 P
    bool wait();
    /* 
    封装的信号加一 V
        分为两种情况 
        如果有其他进行因为等待SV而挂起，则唤醒；
        若没有，则将SV值加一
        */
    bool post();
};


sem::sem(/* args */)
{
    if(sem_init(&m_sem,0,0) != 0){
        throw std::exception();
    }
}

inline sem::sem(int num)
{
    if(sem_init(&m_sem,0,num) != 0){
        throw std::exception();
    }
}

sem::~sem()
{
    sem_destroy(&m_sem);
}

inline bool sem::wait()
{
    return sem_wait(&m_sem) == 0;
}

inline bool sem::post()
{
    return sem_post(&m_sem) == 0;
}

class locker
{
private:
    /* data */
    pthread_mutex_t m_mutex;
public:
    locker(/* args */);
    ~locker();
    bool lock();
    bool unlock();
    pthread_mutex_t *get();
};

locker::locker(/* args */)
{
    if(pthread_mutex_init(&m_mutex,NULL) != 0){
        throw std::exception();
    }
}

locker::~locker()
{
    if(pthread_mutex_destroy(&m_mutex) != 0){
        throw std::exception();
    }
}

inline bool locker::lock()
{
    return pthread_mutex_lock(&m_mutex) == 0;
}

inline bool locker::unlock()
{
    return pthread_mutex_unlock(&m_mutex) == 0;
}

inline pthread_mutex_t *locker::get()
{
    return &m_mutex;
}

//对条件变量封装
class cond
{
private:
    /* data */
    pthread_cond_t m_cond;
public:
    cond(/* args */);
    ~cond();
    //用于等待目标条件变量
    bool wait(pthread_mutex_t *m_mutex);

    //
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t);

    //
    bool signal();

    //以广播的方式唤醒所有等待目标条件变量的线程
    bool broadcast();
};

cond::cond(/* args */)
{
    if(pthread_cond_init(&m_cond,NULL) != 0){
        throw std::exception();
    }
}

cond::~cond()
{
    if(pthread_cond_destroy(&m_cond) != 0){
        throw std::exception();
    }
}

inline bool cond::wait(pthread_mutex_t *m_mutex)
{
    int ret = 0;
    ret = pthread_cond_wait(&m_cond,m_mutex);
    return ret == 0;
}

inline bool cond::timewait(pthread_mutex_t *m_mutex, struct timespec t)
{
    int ret = 0;
    ret = pthread_cond_timedwait(&m_cond,m_mutex,&t);
    return ret;
}

inline bool cond::signal()
{
    return pthread_cond_signal(&m_cond) == 0;
}

inline bool cond::broadcast()
{
    return pthread_cond_broadcast(&m_cond) == 0;
}

#endif 