#pragma once
#include <cstdint>
#include <functional>
#include <cassert>
#include <map>
#include <unordered_set>

namespace moon
{
    template<typename ExpirePolicy>
    class base_timer
    {
        using expire_policy_type = ExpirePolicy;
    public:
        base_timer() = default;

        base_timer(const base_timer&) = delete;
        base_timer& operator=(const base_timer&) = delete;

        void update(int64_t now)
        {
            if (stop_)
            {
                return;
            }

            do
            {
                expire_policy_type v;
                {
                    std::lock_guard lock{ lock_ };
                    if (auto iter = timers_.begin(); iter == timers_.end() || iter->first > now)
                    {
                        break;
                    }
                    else
                    {
                        v = std::move(iter->second);
                        timers_.erase(iter);
                    }
                }
                v();
            } while (true);
            return;
        }

        void pause()
        {
            stop_ = true;
        }

        void resume()
        {
            stop_ = false;
        }

        uint32_t make_timerid()
        {
            return ++timerid_;
        }

        template<typename... Args>
        uint32_t add(time_t expiretime, Args&&... args)
        {
            std::lock_guard lock{ lock_ };
            auto id = ++timerid_;
            timers_.emplace(expiretime, expire_policy_type{ id, std::forward<Args>(args)... });
            return id;
        }

        size_t size() const
        {
            return timers_.size();
        }
    private:
        bool stop_ = false;
        std::atomic<uint32_t> timerid_ = 0;
        std::mutex lock_;
        std::multimap<int64_t, expire_policy_type> timers_;
    };

    class default_expire_policy
    {
    public:
        using handler_type = std::function<void()>;

        default_expire_policy(uint32_t, handler_type handler)
            :handler_(std::move(handler))
        {
        }

        void  operator()()
        {
            handler_();
        }
    private:
        handler_type handler_;
    };

    using timer = base_timer<default_expire_policy>;
}


