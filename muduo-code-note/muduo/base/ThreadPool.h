// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADPOOL_H
#define MUDUO_BASE_THREADPOOL_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Types.h>

#include <deque>
#include <vector>


/*
	我们知道线程池本质就是一个生产者消费者模型，它维护一个线程队列和任务队列。
	一旦任务队列（生产者）当中有任务，相当于生产者生产了东西，就唤醒线程队列中的线程（消费者）来执行这些任务。
	那么，这些线程就相当于消费者线程。
	muduo库的线程数目属于启动时配置，当线程池启动时，线程数目就已经固定下来。
*/




namespace muduo
{

class ThreadPool : noncopyable
{
 public:
  typedef std::function<void ()> Task;

  explicit ThreadPool(const string& nameArg = string("ThreadPool"));
  ~ThreadPool();

  // Must be called before start().
  void setMaxQueueSize(int maxSize) { maxQueueSize_ = maxSize; }     //设置线程池最大线程数目
  void setThreadInitCallback(const Task& cb)       //设置线程执行前的回调函数
  { threadInitCallback_ = cb; }

  void start(int numThreads);
  void stop();

  const string& name() const			//返回线程池名字
  { return name_; }

  size_t queueSize() const;				//返回队列大小

  // Could block if maxQueueSize > 0
  void run(Task f);

 private:
  bool isFull() const REQUIRES(mutex_);   //判断线程是否已满
  void runInThread();					//线程池的线程运行函数
  Task take();							//取任务函数

  mutable MutexLock mutex_;
  Condition notEmpty_ GUARDED_BY(mutex_);   //是否为空condition
  Condition notFull_ GUARDED_BY(mutex_);	//是否已满condition
  string name_;
  Task threadInitCallback_;					//线程执行前的回调函数
  std::vector<std::unique_ptr<muduo::Thread>> threads_;		//线程池   线程数组（容器）
  std::deque<Task> queue_ GUARDED_BY(mutex_);		//任务队列
  size_t maxQueueSize_;					//因为deque是通过push_back增加线程数目的，
										//所以通过外界max_queuesize存储最多线程数目
  bool running_;				//线程池运行状态
};

}  // namespace muduo

#endif  // MUDUO_BASE_THREADPOOL_H
