// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include <muduo/base/Atomic.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Types.h>

#include <functional>
#include <memory>
#include <pthread.h>

namespace muduo
{

class Thread : noncopyable
{
 public:
  typedef std::function<void ()> ThreadFunc;

  explicit Thread(ThreadFunc, const string& name = string());    
  // FIXME: make it movable in C++11
  ~Thread();

  void start();         //启动线程
  int join(); 			// return pthread_join()，等待线程结束，实现线程同步

  bool started() const { return started_; }    //线程结束与否
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return tid_; }    //返回线程真实ID，由于pthread_t的id号可能是一样的，所以使用tid_
  const string& name() const { return name_; }    //返回线程名

  static int numCreated() { return numCreated_.get(); }      //返回线程数

 private:
  void setDefaultName();

  bool       started_;      //线程开启标志
  bool       joined_;       //等待线程结束标志，实现线程同步
  pthread_t  pthreadId_;    //进程中独立线程id（可能相同）
  pid_t      tid_;          //线程真实ID
  ThreadFunc func_;			//线程回调函数
  string     name_;        	//线程名
  CountDownLatch latch_;    //用于多线程等待满足条件后同时工作

  static AtomicInt32 numCreated_;    //原子计数器，记录线程个数
};

}  // namespace muduo
#endif  // MUDUO_BASE_THREAD_H
