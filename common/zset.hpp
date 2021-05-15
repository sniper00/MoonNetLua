#pragma once
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>
#include <limits>

namespace moon
{
    class zset
    {
        struct context
        {
            int64_t key = 0;
            int64_t score = 0;
            int64_t timestamp = 0;
            uint32_t rank = 0;
        };

        struct compare
        {
            bool operator()(context* a, context* b) const
            {
                if (a->score == b->score)
                {
                    if (a->timestamp == b->timestamp)
                    {
                        return a->key < b->key;
                    }
                    return a->timestamp < b->timestamp;
                }
                return a->score > b->score;
            }
        };
    public:
        using container_type = std::set<context*, compare>;
        using iterator = typename container_type::iterator;
        using const_iterator = typename container_type::const_iterator;
        using pointer = typename container_type::pointer;
        using const_pointer = typename container_type::const_pointer;

        static constexpr size_t JUMP_STEP = 100;

        zset(size_t max_count = std::numeric_limits<size_t>::max())
            :max_count_(max_count)
        {
        }

        void update(int64_t key, int64_t score, int64_t timestamp)
        {
            if (max_count_ == 0)
            {
                return;
            }

            auto iter = index_.find(key);
            if (index_.size() == max_count_ && iter == index_.end() && (*(order_.rbegin()))->score > score)
            {
                return;
            }

            if (iter == index_.end())
            {
                iter = index_.emplace(key, context{ key, score, timestamp }).first;
            }
            else
            {
                order_.erase(&iter->second);
            }

            iter->second.score = score;
            iter->second.timestamp = timestamp;

            order_.emplace(&iter->second);

            if (order_.size() > max_count_)
            {
                auto v = (*order_.rbegin());
                order_.erase(v);
                index_.erase(v->key);
            }

            ordered_ = false;
        }

        uint32_t rank(int64_t key)
        {
            prepare();
            auto iter = index_.find(key);
            if (iter != index_.end())
            {
                return iter->second.rank;
            }
            return 0;
        }

        void prepare()
        {
            if (!ordered_)
            {
                jump_.clear();
                uint32_t n = 0;
                for (auto iter = order_.begin(); iter != order_.end(); ++iter)
                {
                    if (n % JUMP_STEP == 0)
                    {
                        jump_.emplace_back(iter);
                    }
                    (*iter)->rank = ++n;
                }
                ordered_ = true;
            }
        }

        int64_t score(int64_t key)
        {
            auto iter = index_.find(key);
            if (iter != index_.end())
            {
                return iter->second.score;
            }
            return 0;
        }

        const_iterator start(uint32_t nrank) const
        {
            size_t idx = nrank / JUMP_STEP;
            if (idx >= jump_.size())
            {
                return  order_.end();
            }

            for (auto iter = jump_[idx]; iter != order_.end(); ++iter)
            {
                if ((*iter)->rank == nrank)
                {
                    return iter;
                }
            }
            return  order_.end();
        }

        const_iterator begin() const
        {
            return order_.begin();
        }

        const_iterator end() const
        {
            return  order_.end();
        }

        size_t size() const
        {
            return order_.size();
        }

        bool has(int64_t key) const
        {
            return (index_.find(key) != index_.end());
        }

        void clear()
        {
            jump_.clear();
            order_.clear();
            index_.clear();
        }

        size_t erase(int64_t key)
        {
            auto iter = index_.find(key);
            if (iter != index_.end())
            {
                order_.erase(&iter->second);
                ordered_ = false;
                index_.erase(iter);
                return 1;
            }
            return 0;
        }
    private:
        bool ordered_ = true;
        size_t max_count_;
        std::vector<const_iterator> jump_;
        std::set<context*, compare> order_;
        std::unordered_map<int64_t, context> index_;
    };
}


