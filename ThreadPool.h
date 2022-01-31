#pragma once

#include <thread>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>

using std::function;
using std::vector;
using std::thread;
using std::queue;
using std::mutex;
using std::condition_variable;
using std::future;

class ThreadPool
{
    public:
        ThreadPool(int n) { mThreads.resize(n); for(int i = 0;i < n;i++) mThreads[i] = thread(PoolWorker(this)); }
        template <typename T,typename... Args>
        auto fEnqueue(T f,Args&&... args)
        {
            using funcType = decltype(f(args...))();
            function<funcType> func = std::bind<void>(std::forward<T>(f), std::forward<Args>(args)...);
            auto taskPtr = std::make_shared<std::packaged_task<funcType>>(func);
            if(!mShutDown)
            {
                std::unique_lock<mutex> lock(mMutex);
                mJobs.emplace([taskPtr]() {(*taskPtr)(); });
                mCondition.notify_one();
            }
            return taskPtr->get_future();
        }
        void fShutDown() { mShutDown = true;}
    private:
        class PoolWorker
        {
            public:
                PoolWorker(ThreadPool* pool) : mPool(pool) {    }
                void operator()() 
                {
                    
                    while(!mPool->mShutDown)
                    {
                        function<void()> f;
                        {
                            std::unique_lock<mutex> lock(mPool->mMutex);
                            if(mPool->mJobs.empty())
                            {
                                if(mPool->mShutDown)
                                    break;
                                mPool->mCondition.wait(lock);
                            }
                            f = std::move(mPool->mJobs.front());
                            mPool->mJobs.pop();
                        }
                        f();
                    }
                }
            private:
                ThreadPool* mPool;
        };
        friend class PoolWorker;
        bool mShutDown = false;
        mutex mMutex;
        condition_variable mCondition;
        vector<thread> mThreads;
        queue<function<void()>> mJobs;
};