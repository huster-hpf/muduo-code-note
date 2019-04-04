// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

/*

ThreadLocal可以叫做Thread的局部变量，通过ThreadLocal创建的对象每个线程都拥有其副本被各自线程独有，
操作其对象不会造成线程不安全。
这是通过封装Linux操作系统提供的线程特定数据实现，封装也比较简单，不涉及技巧。

*/

#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include <muduo/base/Mutex.h>  // MCHECK
#include <muduo/base/noncopyable.h>

#include <pthread.h>

namespace muduo
{

template<typename T>
class ThreadLocal : noncopyable
{
 public:
	//一旦某个线程创建了一个key，比如key[1]，
	//那么其他线程也有自己的key[1]，
	//它们通过各自的key[1]访问到的实际数据（堆上内存分配的空间）是不同的，
	//pthread_key_delete 只是删除key，
	//实际数据空间的释放需要在pthread_key_create 中注册一个回调函数destructor去delete T*。
  ThreadLocal()
  {
    MCHECK(pthread_key_create(&pkey_, &ThreadLocal::destructor));
	 /*
    创建线程键，此键由当前进程全部线程共有。
    可以通过此键索引线程特定数据。
    */
  }

  ~ThreadLocal()				
  {
    MCHECK(pthread_key_delete(pkey_));		//取消键与线程特定数据的关联
  }

  T& value()					//获取键值
  {	
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));
    if (!perThreadValue)
    {
      T* newObj = new T();
      MCHECK(pthread_setspecific(pkey_, newObj));
      perThreadValue = newObj;
    }
    return *perThreadValue;
  }

 private:

  static void destructor(void *x)		//析构函数，x必须强制转换，告诉编译器内存组织关系
  {
    T* obj = static_cast<T*>(x);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];		//判断传入对象是否有定义
    T_must_be_complete_type dummy; (void) dummy;
    delete obj;
  }

 private:
  pthread_key_t pkey_;		//线程键
};

}  // namespace muduo

#endif  // MUDUO_BASE_THREADLOCAL_H
