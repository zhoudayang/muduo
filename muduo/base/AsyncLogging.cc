#include <muduo/base/AsyncLogging.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>

using namespace muduo;

AsyncLogging::AsyncLogging(const string& basename,
                           size_t rollSize,
                           int flushInterval)
  : flushInterval_(flushInterval),
    running_(false),
    basename_(basename),
    rollSize_(rollSize),
    thread_(boost::bind(&AsyncLogging::threadFunc, this), "Logging"),
    latch_(1),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),
    nextBuffer_(new Buffer),
    buffers_()
{
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  buffers_.reserve(16);
}

void AsyncLogging::append(const char* logline, int len)
{
  muduo::MutexLockGuard lock(mutex_);
  if (currentBuffer_->avail() > len)
  {
    currentBuffer_->append(logline, len);
  }
  else
  {
    //将currentBuffer_加入等待写入文件的已经填满的缓冲
    buffers_.push_back(currentBuffer_.release());

    if (nextBuffer_)
    {
      //move nextBuffer_ to currentBuffer_
      currentBuffer_ = boost::ptr_container::move(nextBuffer_);
    }
    else// next Buffer is empty
    {
      currentBuffer_.reset(new Buffer); // Rarely happens
    }
    currentBuffer_->append(logline, len);
    //通知后端程序将缓冲中的log写入硬盘
    cond_.notify();
  }
}

void AsyncLogging::threadFunc()
{
  assert(running_ == true);
  latch_.countDown();
  LogFile output(basename_, rollSize_, false);
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();
  BufferVector buffersToWrite;
  buffersToWrite.reserve(16);
  while (running_)//loop forever 将前端缓冲区的内容写入文件
  {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      muduo::MutexLockGuard lock(mutex_);
      if (buffers_.empty())  // unusual usage!
      {
        //wait for log information
        cond_.waitForSeconds(flushInterval_);
      }
      //push currentBuiffer_ to end of the vector
      buffers_.push_back(currentBuffer_.release());
      //swap currentBuffer and newBuffer1
      currentBuffer_ = boost::ptr_container::move(newBuffer1);
      //swap buffer vector
      buffersToWrite.swap(buffers_);
      if (!nextBuffer_)// if nextBuffer is empty ,swap nextBuffer with newBuffer2
      {
        nextBuffer_ = boost::ptr_container::move(newBuffer2);
      }
    }

    assert(!buffersToWrite.empty());
    //超出后端处理能力，丢弃多余的日志buffer
    if (buffersToWrite.size() > 25)
    {
      char buf[256];
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size()-2);
      fputs(buf, stderr);
      output.append(buf, static_cast<int>(strlen(buf)));
      //删除bufferToWrite begin+2 -> end范围的buffer
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end());
    }

    for (size_t i = 0; i < buffersToWrite.size(); ++i)
    {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffersToWrite[i].data(), buffersToWrite[i].length());
    }

    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2);//resize buffersToWriteto size 2,because newBuffer1 and newBuffer2 may need two buffer size
    }

    if (!newBuffer1)//get buffer content for newBuffer1
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = buffersToWrite.pop_back();
      newBuffer1->reset();
    }

    if (!newBuffer2)//get buffer content for newBuffer2
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = buffersToWrite.pop_back();
      newBuffer2->reset();
    }

    buffersToWrite.clear();
    output.flush();
  }
  output.flush();
}

