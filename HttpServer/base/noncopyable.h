#ifndef REACTOR_BASE_NONCOPYABLE_H
#define REACTOR_BASE_NONCOPYABLE_H

class noncopyable {
 public:
  noncopyable(const noncopyable&) = delete;  // 派生类的对应操作也是被 delete 的
  void operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;  // 外界不能访问，派生类中可以访问
  ~noncopyable() = default;
};

#endif  // REACTOR_BASE_NONCOPYABLE_H