// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BLOCKINGQUEUE_H
#define MUDUO_BASE_BLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <deque>
#include <assert.h>


/*
	无界阻塞队列
	muduo库的BlcokingQueue实际上用的生产这消费者模型。
	我们知道生产者消费者模型一般有两种实现方式，
	可以利用信号量也可以利用条件变量实现，muduo库采用条件变量实现。
	BlockingQueue比较简单，它是线程安全的，我们在外部调用它时无需加锁。
*/



namespace muduo
{

template<typename T>
class BlockingQueue : noncopyable
{
 public:
  BlockingQueue()
    : mutex_(),
      notEmpty_(mutex_),
      queue_()
  {
  }

  void put(const T& x)           //往阻塞队列放任务
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(x);
    notEmpty_.notify(); // wait morphing saves us
    // http://www.domaigne.com/blog/computing/condvars-signal-with-mutex-locked-or-not/
  }

  void put(T&& x)				//往阻塞队列放任务
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(std::move(x));
    notEmpty_.notify();
  }

  T take()                   	//从阻塞队列中取任务
  {
    MutexLockGuard lock(mutex_);     //取任务时也要保证线程安全
    // always use a while-loop, due to spurious wakeup
    while (queue_.empty())
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());  	//确保队列不为空
    T front(std::move(queue_.front()));		//取出队头
    queue_.pop_front();       	//弹出队头
    return std::move(front);    //移动构造函数，更快
  }

  size_t size() const			//返回队列大小
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

 private:
  mutable MutexLock mutex_;		//互斥锁，mutable修饰，可变的
  Condition         notEmpty_ GUARDED_BY(mutex_);    //GUARDED_BY是啥意思呢？
  std::deque<T>     queue_ GUARDED_BY(mutex_);		 //GUARDED_BY是啥意思呢？
};

}  // namespace muduo

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
