#include "asio.hpp"
#include "common/buffer.hpp"

namespace moon
{
    using buffer_t = buffer;
    class streambuf
    {
    public:
        typedef asio::const_buffer const_buffers_type;
        typedef asio::mutable_buffer mutable_buffers_type;

        streambuf(buffer_t* buf, std::size_t maxsize = (std::numeric_limits<std::size_t>::max)())
            :buffer_(buf)
            , max_size_(maxsize)
        {

        }

        streambuf(const streambuf&) = delete;

        streambuf& operator=(const streambuf&) = delete;

        streambuf(streambuf&& other) noexcept
            :buffer_(other.buffer_)
            , max_size_(other.max_size_)
        {
            other.buffer_ = nullptr;
            other.max_size_ = 0;
        }

        streambuf& operator=(streambuf&& other) noexcept
        {
            buffer_ = other.buffer_;
            max_size_ = other.max_size_;
            other.buffer_ = nullptr;
            other.max_size_ = 0;
            return *this;
        }

        std::size_t size() const noexcept
        {
            if (nullptr == buffer_) return 0;
            return buffer_->size();
        }

        std::size_t max_size() const noexcept
        {
            return max_size_;
        }

        std::size_t capacity() const noexcept
        {
            if (nullptr == buffer_) return 0;
            return buffer_->capacity() - buffer_->reserved();
        }

        const_buffers_type data() const noexcept
        {
            if (nullptr == buffer_) return asio::buffer(asio::const_buffer(nullptr,0));
            return asio::const_buffer(buffer_->data(), buffer_->size());
        }

        mutable_buffers_type prepare(std::size_t n)
        {
            if (nullptr == buffer_) return asio::buffer(asio::mutable_buffer(nullptr, 0));
            buffer_->prepare(n);
            return asio::buffer(buffer_->data() + buffer_->size(), n);
        }

        void commit(std::size_t n)
        {
            if (nullptr == buffer_) return;
            buffer_->commit(n);
        }

        void consume(std::size_t n)
        {
            if (nullptr == buffer_) return;
            buffer_->consume(n);
        }
    private:
        buffer_t* buffer_;
        std::size_t max_size_;
    };
}