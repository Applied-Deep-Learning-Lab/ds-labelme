/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <queue>
#include <mutex>

/// Simple concurrent Queue class using an stl queue.
/// Nothing is special here compare to stl queue except
/// it has only simple operations and it is thread safe.
template <typename T>
class ConcurrentQueue
{
public:
    void push(const T &elm);
    T pop();
    bool is_empty();

private:
    std::queue<T> queue_;
    std::mutex mutex_;
};

template <typename T>
void ConcurrentQueue<T>::push(const T &elm)
{
    mutex_.lock();
    queue_.push(elm);
    mutex_.unlock();
}

template <typename T>
T ConcurrentQueue<T>::pop()
{
    mutex_.lock();
    T elm = queue_.front();
    queue_.pop();
    mutex_.unlock();
    return elm;
}

template <typename T>
bool ConcurrentQueue<T>::is_empty()
{
    mutex_.lock();
    bool res = queue_.empty();
    mutex_.unlock();
    return res;
}