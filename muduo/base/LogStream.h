#ifndef MUDUO_BASE_LOGSTREAM_H
#define MUDUO_BASE_LOGSTREAM_H

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <assert.h>
#include <string.h> // memcpy
#ifndef MUDUO_STD_STRING
#include <string>
#endif
#include <boost/noncopyable.hpp>

namespace muduo
{

namespace detail
{

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000*1000;


//buffer with fixed size 
template<int SIZE>
class FixedBuffer : boost::noncopyable
{
 public:
  FixedBuffer()
  //使用char数组的头指针初始化cur_ ,cur_是一个指向当前位置的指针
    : cur_(data_)
  {
   //设置函数指针
    setCookie(cookieStart);
  }

  ~FixedBuffer()
  {
   // 设置函数指针
    setCookie(cookieEnd);
  }

  void append(const char* /*restrict*/ buf, size_t len)
  {
    // FIXME: append partially
    if (implicit_cast<size_t>(avail()) > len)
    {
      memcpy(cur_, buf, len);
      cur_ += len;
    }
  }
  //return data_
  const char* data() const { return data_; }
  //return length of the data 
  int length() const { return static_cast<int>(cur_ - data_); }

  // write to data_ directly
  char* current() { return cur_; }
  // return available capacity
  int avail() const { return static_cast<int>(end() - cur_); }
  // push cur_ forward len position
  void add(size_t len) { cur_ += len; }
  //reset cur_ to head 
  void reset() { cur_ = data_; }
  //bzero() 会将内存块（字符串）的前n个字节清零
  void bzero() { ::bzero(data_, sizeof data_); }

  // for used by GDB
  const char* debugString();
  // set fuction pointer 
  void setCookie(void (*cookie)()) { cookie_ = cookie; }
  // for used by unit test
  string toString() const { return string(data_, length()); }
  // convert char * to StringPiece
  StringPiece toStringPiece() const { return StringPiece(data_, length()); }

 private:
 //return end pointer of the data_
  const char* end() const { return data_ + sizeof data_; }
  // Must be outline function for cookies.
  static void cookieStart();
  static void cookieEnd();

  void (*cookie_)();
  char data_[SIZE];
  char* cur_;
};

}

class LogStream : boost::noncopyable
{
  typedef LogStream self;
 public:
  //buffer with small size 
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;
  // buffer with << function and bool parameter
  self& operator<<(bool v)
  {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  self& operator<<(short);
  self& operator<<(unsigned short);
  self& operator<<(int);
  self& operator<<(unsigned int);
  self& operator<<(long);
  self& operator<<(unsigned long);
  self& operator<<(long long);
  self& operator<<(unsigned long long);

  self& operator<<(const void*);

  self& operator<<(float v)
  {
    *this << static_cast<double>(v);
    return *this;
  }
  self& operator<<(double);
  // self& operator<<(long double);

// append v to buffer 
  self& operator<<(char v)
  {
    buffer_.append(&v, 1);
    return *this;
  }

  // self& operator<<(signed char);
  // self& operator<<(unsigned char);
  // if str is nullptr ,appen null six times, else append str 
  self& operator<<(const char* str)
  {
    if (str)
    {
      buffer_.append(str, strlen(str));
    }
    else
    {
      buffer_.append("(null)", 6);
    }
    return *this;
  }
  // use const char * << function
  self& operator<<(const unsigned char* str)
  {
    return operator<<(reinterpret_cast<const char*>(str));
  }
  // append v to buffer 
  self& operator<<(const string& v)
  {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }

#ifndef MUDUO_STD_STRING
  self& operator<<(const std::string& v)
  {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }
#endif

  self& operator<<(const StringPiece& v)
  {
    buffer_.append(v.data(), v.size());
    return *this;
  }

  self& operator<<(const Buffer& v)
  {
    *this << v.toStringPiece();
    return *this;
  }

  void append(const char* data, int len) { buffer_.append(data, len); }
  const Buffer& buffer() const { return buffer_; }
  void resetBuffer() { buffer_.reset(); }

 private:
  void staticCheck();

  template<typename T>
  void formatInteger(T);
  //fixed buffer 
  Buffer buffer_;
 // 最大数字长度
  static const int kMaxNumericSize = 32;
};

//用于存储数字转换成的字符串
class Fmt // : boost::noncopyable
{
 public:
  template<typename T>
  Fmt(const char* fmt, T val);
  //return buf_
  const char* data() const { return buf_; }
  //return length of the buf_ (the same mean is the length of number)
  int length() const { return length_; }

 private:
  char buf_[32];
  int length_;
};
// append fmt.data() to buffer 
inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
{
  s.append(fmt.data(), fmt.length());
  return s;
}

}
#endif  // MUDUO_BASE_LOGSTREAM_H

