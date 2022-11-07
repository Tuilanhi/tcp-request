// your PA3 BoundedBuffer.cpp code here
#include "BoundedBuffer.h"

using namespace std;

BoundedBuffer::BoundedBuffer(int _cap) : cap(_cap)
{
    // modify as needed
}

BoundedBuffer::~BoundedBuffer()
{
    // modify as needed
}

void BoundedBuffer::push(char *msg, int size)
{
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    //      use one of the vector constructor's
    // 2. Wait until there is room in the queue (i.e., queue lengh is less than cap)
    //      waiting on slot available
    // 3. Then push the vector at the end of the queue
    // 4. Wake up threads that were waiting for push
    //      notifying data available

    // sizeof(msg) => sizeof(pointer)
    vector<char> v(msg, msg + size);
    // after a thread has passed through lock it
    unique_lock<std::mutex> lck(m);
    // check to see if the q.size() < cap
    // wait for slot available, return true if q.size() < cap and work as expected; if the condition has not met return false and wait.
    // [this] {return (int)q.size() < cap} => lambda function
    // it returns false, wait() unlocks the mutex and puts the thread in a waiting state
    slot_available.wait(lck, [this]
                        { return (int)q.size() < cap; });
    q.push(v);
    // unlock
    lck.unlock();
    // notify data available that the queue is no longer empty and ready for pop
    // Everytime you push, a notification will be sent to any thread waiting in the pop method because the queue is empty and let it know that the queue is not empty anymore
    // it the previous condition returns false, it will notify the pop() that queue size is greater than cap, and needs to be popped
    data_available.notify_one();
}

int BoundedBuffer::pop(char *msg, int size)
{
    // 1. Wait until the queue has at least 1 item
    //      waiting on data available
    // 2. Pop the front item of the queue. The popped item is a vector<char>
    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    //      use vector.data()
    // 4. Wake up threads that were waiting for pop
    //      notifying slot available
    // 5. Return the vector's length to the caller so that they know how many bytes were popped

    // lock the mutex
    unique_lock<std::mutex> lck(m);
    // return true once it has confirmed that the queue is not empty; return false and wait if it is not empty
    // it returns false, wait() unlocks the mutex and puts the thread in a waiting state
    // check the supplied condition with the mutex locked and returns immediately only if the condition returns true
    data_available.wait(lck, [this]
                        { return !q.empty(); });
    vector<char> v = q.front();
    q.pop();
    // unlock so the thread doesn't have to wait for the lock to be unlocked after immediately waking up
    lck.unlock();
    // perform copying v.data() to msg and assertion outside the lock to prevent hinder in performance
    assert((int)v.size() <= size);
    memcpy(msg, v.data(), v.size());
    // notify slot available for push since the queue has been emptied and ready for push
    // Everytime you pop, a notification will be sent to any thread waiting in the push method because the queue is not empty and let it know the queue is empty
    slot_available.notify_one();
    return v.size();
}

size_t BoundedBuffer::size()
{
    return q.size();
}