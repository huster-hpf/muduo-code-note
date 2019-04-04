// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)


/*

__thread是GCC内置的线程局部存储设施，存取效率可以和全局变量相比。
__thread变量每一个线程有一份独立实体，各个线程的值互不干扰。
可以用来修饰那些“带有全局性且值可能变，但是又不值得用全局锁保护”的变量。


__thread使用规则：只能修饰POD类型(类似整型指针的标量，
不带自定义的构造、拷贝、赋值、析构的类型，
二进制内容可以任意复制memset,memcpy,且内容可以复原)，
不能修饰class类型，因为无法自动调用构造函数和析构函数，
可以用于修饰全局变量，函数内的静态变量，
不能修饰函数的局部变量或者class的普通成员变量，
且__thread变量值只能初始化为编译器常量。

*/


#ifndef MUDUO_BASE_THREADLOCALSINGLETON_H
#define MUDUO_BASE_THREADLOCALSINGLETON_H

#include <muduo/base/noncopyable.h>

#include <assert.h>
#include <pthread.h>

namespace muduo
{

template<typename T>
class ThreadLocalSingleton : noncopyable
{
 public:
  ThreadLocalSingleton() = delete;
  ~ThreadLocalSingleton() = delete;

  static T& instance()			//返回单例对象，不需要按照线程安全方式实现，
								//本身就是__thread类型线程安全
  {
    if (!t_value_)				//如果指针为空创建
    {
      t_value_ = new T();
      deleter_.set(t_value_);	//把t_value_指针暴露给deleter，为了垃圾回收
    }
    return *t_value_;
  }

  static T* pointer()			//返回指针
  {
    return t_value_;
  }

 private:
  static void destructor(void* obj)		////通过RAII手法封装pthread_key_create和pthread_key_delete，供类调用
  {
    assert(obj == t_value_);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;
    delete t_value_;
    t_value_ = 0;
  }

  class Deleter
  {
   public:
    Deleter()
    {
      pthread_key_create(&pkey_, &ThreadLocalSingleton::destructor);
    }

    ~Deleter()
    {
      pthread_key_delete(pkey_);
    }

    void set(T* newObj)
    {
      assert(pthread_getspecific(pkey_) == NULL);
      pthread_setspecific(pkey_, newObj);
    }

    pthread_key_t pkey_;
  };

  static __thread T* t_value_;		//__thread关键字保证线程局部属性，只能修饰POD类型
  static Deleter deleter_;			//用来销毁T*指针所指对象
};

template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;	//定义，然后默认调用Deleter构造函数，创建键。
}  // namespace muduo
#endif  // MUDUO_BASE_THREADLOCALSINGLETON_H
