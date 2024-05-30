#include "threadpool.h"

template <typename T>
inline threadpool<T>::threadpool(connection_pool *connPool, int thread_number, int max_request)
{
    // 数量不对时 抛出异常
    if(thread_number <= 0 || max_request <=0){
        throw std::exception();
    }

    //线程id初始化 初始m_thread_number个线程的线程池
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    for (int i = 0; i < thread_number; i++)
    {
        //循环创建线程，并将工作线程按要求进行运行
        //传递给 pthread_create 函数的参数是 this 指针，它指向当前的 threadpool 对象
        if(pthread_create(m_threads+i , NULL , worker , this) != 0){
            delete [] m_threads;
            throw std::exception();
        }
        //将线程进行分离后，不用单独对工作线程进行回收
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
    
    this->m_connPool = connPool;    
}

template <typename T>
inline threadpool<T>::~threadpool()
{
    
}

//添加任务到请求队列
template <typename T>
inline bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    
    if(m_workqueue.size() > m_max_requests){
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();

    //使用信号
    m_queuestat.post();    
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool* pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        m_queuestat.wait();
        
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
        }

        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;
        }
        
        //从连接池中取出一个数据库连接
        request->mysql = m_connPool->GetConnection();

        //process(模板类中的方法,这里是http类)进行处理
        request->process();
    }
    
}
