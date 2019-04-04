// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Thread.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/Exception.h>
#include <muduo/base/Logging.h>

#include <type_traits>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

namespace muduo
{
namespace detail
{

pid_t gettid()              //在linux系统中，每个进程都有一个pid，
							//类型pid_t，由getpid()取得。Linux下的POSIX线程也有一个id，
							//类型pthread_t，由pthread_self()取得，该线程由线程库维护，
							//其id空间是各个进程独立的（即不同线程中可能拥有相同的id)。
							//有时我们需要知道线程的真实id，就不能使用pid和pthread id，
							//需要使用该线程真实的id，称为tid。
							//(不是我们平常所用的pthread_t tid; tid=pthread_create()的那个tid)。
							
							//linux系统中有一个系统调用可以实现得到线程的真实id，
							//我们可以实现一个函数，返回该系统调用的返回值，
							//return syscall(SYS_gettid)，但是频繁系统调用会造成性能降低，
							//muduo库就使用了全局变量current_tid来缓存它，
							//我们只需要调用一次，以后直接获取该缓存就可以了。
{
  return static_cast<pid_t>(::syscall(SYS_gettid));
}

void afterFork()       			 //fork之后打扫战场，子进程中执行
{
  muduo::CurrentThread::t_cachedTid = 0;            //先清零tid
  muduo::CurrentThread::t_threadName = "main";    	//为什么要赋值为0和main，
													//因为fork可能在主线程中调用，
													//也可能在子线程中调用。fork得到一个新进程，
  CurrentThread::tid();                    			//此处再缓存tid               
													//新进程只有一个执行序列，只有一个线程
  // no need to call pthread_atfork(NULL, NULL, &afterFork);   //实际上服务器要么多进程，要么多线程。
																//如果都用，甚至可能死锁
}

class ThreadNameInitializer    //线程初始化
{
 public:
  ThreadNameInitializer()
  {
    muduo::CurrentThread::t_threadName = "main";
    CurrentThread::tid();
    pthread_atfork(NULL, NULL, &afterFork);      	//如果我们调用了fork函数，
													//调用成功后子进程会调用afterfork()
  }
};

ThreadNameInitializer init;

struct ThreadData     			//线程数据类，观察者模式
{
  typedef muduo::Thread::ThreadFunc ThreadFunc;
  ThreadFunc func_;
  string name_;
  pid_t* tid_;
  CountDownLatch* latch_;

  ThreadData(ThreadFunc func,
             const string& name,
             pid_t* tid,
             CountDownLatch* latch)
    : func_(std::move(func)),
      name_(name),
      tid_(tid),
      latch_(latch)
  { }

  void runInThread()        			//线程运行
  {
    *tid_ = muduo::CurrentThread::tid();
    tid_ = NULL;
    latch_->countDown();
    latch_ = NULL;

    muduo::CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_.c_str();
    ::prctl(PR_SET_NAME, muduo::CurrentThread::t_threadName);
    try
    {
      func_();							//运行线程运行函数,可能抛出的异常
      muduo::CurrentThread::t_threadName = "finished";
    }
    catch (const Exception& ex)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
      abort();
    }
    catch (const std::exception& ex)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      abort();
    }
    catch (...)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
      throw; // rethrow 					 //再次抛出
    }
  }
};

void* startThread(void* obj)				//线程启动
{
  ThreadData* data = static_cast<ThreadData*>(obj);   	//派生类指针转化成基类指针，
														//obj是派生类的this指针
  data->runInThread();
  delete data;
  return NULL;
}

}  // namespace detail

void CurrentThread::cacheTid()			////在这里第一次会缓存tid，并不会每次都systemcall，提高了效率
{
  if (t_cachedTid == 0)
  {
    t_cachedTid = detail::gettid();
    t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
  }
}

bool CurrentThread::isMainThread()      //是否是主线程
{
  return tid() == ::getpid();			//用于获得线程组的主线程的pid
}

void CurrentThread::sleepUsec(int64_t usec)			//休眠
{
  struct timespec ts = { 0, 0 };
  ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
  ::nanosleep(&ts, NULL);
}

AtomicInt32 Thread::numCreated_;

Thread::Thread(ThreadFunc func, const string& n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(0),
    func_(std::move(func)),
    name_(n),
    latch_(1)
{
  setDefaultName();
}

Thread::~Thread()						//这个析构函数是线程安全的。
										//析构时确认thread没有join，才会执行析构.

										//即线程的析构不会等待线程结束
										//如果thread对象的生命周期长于线程,
										//那么可以通过join等待线程结束。
										//否则thread对象析构时会自动detach线程，防止资源泄露
{
  if (started_ && !joined_)				//如果没有join，就detach，如果用过了，就不用了。
  {
    pthread_detach(pthreadId_);
  }
}

void Thread::setDefaultName()			//相当于给没有名字的线程起个名字 "Thread %d"
{
  int num = numCreated_.incrementAndGet();
  if (name_.empty())
  {
    char buf[32];
    snprintf(buf, sizeof buf, "Thread%d", num);
    name_ = buf;
  }
}

void Thread::start()			//线程启动
{
  assert(!started_);
  started_ = true;
  // FIXME: move(func_)
  detail::ThreadData* data = new detail::ThreadData(func_, name_, &tid_, &latch_);
  if (pthread_create(&pthreadId_, NULL, &detail::startThread, data))
  {
    started_ = false;
    delete data; // or no delete?
    LOG_SYSFATAL << "Failed in pthread_create";
  }
  else
  {
    latch_.wait();
    assert(tid_ > 0);
  }
}

int Thread::join()				//等待线程结束
{
  assert(started_);
  assert(!joined_);
  joined_ = true;
  return pthread_join(pthreadId_, NULL);
}

}  // namespace muduo
