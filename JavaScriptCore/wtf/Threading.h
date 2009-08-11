/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Note: The implementations of InterlockedIncrement and InterlockedDecrement are based
 * on atomic_increment and atomic_exchange_and_add from the Boost C++ Library. The license
 * is virtually identical to the Apple license above but is included here for completeness.
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 * 
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 * 
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef Threading_h
#define Threading_h

#include "Platform.h"

#if PLATFORM(WINCE)
#include <windows.h>
#endif

#include <wtf/Assertions.h>
#include <wtf/Locker.h>
#include <wtf/Noncopyable.h>

#if PLATFORM(WIN_OS) && !PLATFORM(WINCE)
#include <windows.h>
#elif PLATFORM(DARWIN)
#include <libkern/OSAtomic.h>
#elif defined ANDROID
#include "cutils/atomic.h"
#elif COMPILER(GCC)
#if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2))
#include <ext/atomicity.h>
#else
#include <bits/atomicity.h>
#endif
#endif

#if USE(PTHREADS)
#include <pthread.h>
#elif PLATFORM(GTK)
#include <wtf/GOwnPtr.h>
typedef struct _GMutex GMutex;
typedef struct _GCond GCond;
#endif

#if PLATFORM(QT)
#include <qglobal.h>
QT_BEGIN_NAMESPACE
class QMutex;
class QWaitCondition;
QT_END_NAMESPACE
#endif

#include <stdint.h>

// For portability, we do not use thread-safe statics natively supported by some compilers (e.g. gcc).
#define AtomicallyInitializedStatic(T, name) \
    WTF::lockAtomicallyInitializedStaticMutex(); \
    static T name; \
    WTF::unlockAtomicallyInitializedStaticMutex();

namespace WTF {

typedef uint32_t ThreadIdentifier;
typedef void* (*ThreadFunction)(void* argument);

// Returns 0 if thread creation failed.
// The thread name must be a literal since on some platforms it's passed in to the thread.
ThreadIdentifier createThread(ThreadFunction, void*, const char* threadName);

// Internal platform-specific createThread implementation.
ThreadIdentifier createThreadInternal(ThreadFunction, void*, const char* threadName);

// Called in the thread during initialization.
// Helpful for platforms where the thread name must be set from within the thread.
void setThreadNameInternal(const char* threadName);

ThreadIdentifier currentThread();
bool isMainThread();
int waitForThreadCompletion(ThreadIdentifier, void**);
void detachThread(ThreadIdentifier);

#if USE(PTHREADS)
typedef pthread_mutex_t PlatformMutex;
typedef pthread_rwlock_t PlatformReadWriteLock;
typedef pthread_cond_t PlatformCondition;
#elif PLATFORM(GTK)
typedef GOwnPtr<GMutex> PlatformMutex;
typedef void* PlatformReadWriteLock; // FIXME: Implement.
typedef GOwnPtr<GCond> PlatformCondition;
#elif PLATFORM(QT)
typedef QT_PREPEND_NAMESPACE(QMutex)* PlatformMutex;
typedef void* PlatformReadWriteLock; // FIXME: Implement.
typedef QT_PREPEND_NAMESPACE(QWaitCondition)* PlatformCondition;
#elif PLATFORM(WIN_OS)
struct PlatformMutex {
    CRITICAL_SECTION m_internalMutex;
    size_t m_recursionCount;
};
typedef void* PlatformReadWriteLock; // FIXME: Implement.
struct PlatformCondition {
    size_t m_waitersGone;
    size_t m_waitersBlocked;
    size_t m_waitersToUnblock; 
    HANDLE m_blockLock;
    HANDLE m_blockQueue;
    HANDLE m_unblockLock;

    bool timedWait(PlatformMutex&, DWORD durationMilliseconds);
    void signal(bool unblockAll);
};
#else
typedef void* PlatformMutex;
typedef void* PlatformReadWriteLock;
typedef void* PlatformCondition;
#endif
    
class Mutex : public Noncopyable {
public:
    Mutex();
    ~Mutex();

    void lock();
    bool tryLock();
    void unlock();

public:
    PlatformMutex& impl() { return m_mutex; }
private:
    PlatformMutex m_mutex;
};

typedef Locker<Mutex> MutexLocker;

class ReadWriteLock : public Noncopyable {
public:
    ReadWriteLock();
    ~ReadWriteLock();

    void readLock();
    bool tryReadLock();

    void writeLock();
    bool tryWriteLock();
    
    void unlock();

private:
    PlatformReadWriteLock m_readWriteLock;
};

class ThreadCondition : public Noncopyable {
public:
    ThreadCondition();
    ~ThreadCondition();
    
    void wait(Mutex& mutex);
    // Returns true if the condition was signaled before absoluteTime, false if the absoluteTime was reached or is in the past.
    // The absoluteTime is in seconds, starting on January 1, 1970. The time is assumed to use the same time zone as WTF::currentTime().
    bool timedWait(Mutex&, double absoluteTime);
    void signal();
    void broadcast();
    
private:
    PlatformCondition m_condition;
};

#if PLATFORM(WIN_OS)
#define WTF_USE_LOCKFREE_THREADSAFESHARED 1

#if COMPILER(MINGW) || COMPILER(MSVC7) || PLATFORM(WINCE)
inline void atomicIncrement(int* addend) { InterlockedIncrement(reinterpret_cast<long*>(addend)); }
inline int atomicDecrement(int* addend) { return InterlockedDecrement(reinterpret_cast<long*>(addend)); }
#else
inline void atomicIncrement(int volatile* addend) { InterlockedIncrement(reinterpret_cast<long volatile*>(addend)); }
inline int atomicDecrement(int volatile* addend) { return InterlockedDecrement(reinterpret_cast<long volatile*>(addend)); }
#endif

#elif PLATFORM(DARWIN)
#define WTF_USE_LOCKFREE_THREADSAFESHARED 1

inline void atomicIncrement(int volatile* addend) { OSAtomicIncrement32Barrier(const_cast<int*>(addend)); }
inline int atomicDecrement(int volatile* addend) { return OSAtomicDecrement32Barrier(const_cast<int*>(addend)); }

#elif defined ANDROID

inline void atomicIncrement(int volatile* addend) { android_atomic_inc(addend); }
inline int atomicDecrement(int volatile* addend) { return android_atomic_dec(addend); }

#elif COMPILER(GCC)
#define WTF_USE_LOCKFREE_THREADSAFESHARED 1

inline void atomicIncrement(int volatile* addend) { __gnu_cxx::__atomic_add(addend, 1); }
inline int atomicDecrement(int volatile* addend) { return __gnu_cxx::__exchange_and_add(addend, -1) - 1; }

#endif

class ThreadSafeSharedBase : public Noncopyable {
public:
    ThreadSafeSharedBase(int initialRefCount = 1)
        : m_refCount(initialRefCount)
    {
    }

    void ref()
    {
#if USE(LOCKFREE_THREADSAFESHARED)
        atomicIncrement(&m_refCount);
#else
        MutexLocker locker(m_mutex);
        ++m_refCount;
#endif
    }

    bool hasOneRef()
    {
        return refCount() == 1;
    }

    int refCount() const
    {
#if !USE(LOCKFREE_THREADSAFESHARED)
        MutexLocker locker(m_mutex);
#endif
        return static_cast<int const volatile &>(m_refCount);
    }

protected:
    // Returns whether the pointer should be freed or not.
    bool derefBase()
    {
#if USE(LOCKFREE_THREADSAFESHARED)
        if (atomicDecrement(&m_refCount) <= 0)
            return true;
#else
        int refCount;
        {
            MutexLocker locker(m_mutex);
            --m_refCount;
            refCount = m_refCount;
        }
        if (refCount <= 0)
            return true;
#endif
        return false;
    }

private:
    template<class T>
    friend class CrossThreadRefCounted;

    int m_refCount;
#if !USE(LOCKFREE_THREADSAFESHARED)
    mutable Mutex m_mutex;
#endif
};

template<class T> class ThreadSafeShared : public ThreadSafeSharedBase {
public:
    ThreadSafeShared(int initialRefCount = 1)
        : ThreadSafeSharedBase(initialRefCount)
    {
    }

    void deref()
    {
        if (derefBase())
            delete static_cast<T*>(this);
    }
};

// This function must be called from the main thread. It is safe to call it repeatedly.
// Darwin is an exception to this rule: it is OK to call it from any thread, the only requirement is that the calls are not reentrant.
void initializeThreading();

void lockAtomicallyInitializedStaticMutex();
void unlockAtomicallyInitializedStaticMutex();

} // namespace WTF

using WTF::Mutex;
using WTF::MutexLocker;
using WTF::ThreadCondition;
using WTF::ThreadIdentifier;
using WTF::ThreadSafeShared;

using WTF::createThread;
using WTF::currentThread;
using WTF::isMainThread;
using WTF::detachThread;
using WTF::waitForThreadCompletion;

#endif // Threading_h
