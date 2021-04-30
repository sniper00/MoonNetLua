#pragma once
#include <cstdint>
#include <vector>
#include <cassert>
#include <memory>
#include <type_traits>
#include <string>
#include <cstring>
#include <iostream>
#include <charconv>

namespace moon
{
    template<typename ValueType>
    class buffer_iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using self_type = buffer_iterator;
        using value_type = ValueType;
        using pointer = value_type*;
        using reference = value_type&;
        using difference_type = std::ptrdiff_t;

        explicit buffer_iterator(pointer p)
            :_Ptr(p)
        {
        }

        reference operator*() const
        {
            return  *_Ptr;
        }

        buffer_iterator& operator++()
        {
            ++_Ptr;
            return *this;
        }

        buffer_iterator operator++(int)
        {	// postincrement
            buffer_iterator _Tmp = *this;
            ++* this;
            return (_Tmp);
        }

        buffer_iterator& operator--()
        {
            --_Ptr;
            return *this;
        }

        buffer_iterator operator--(int)
        {	// postdecrement
            buffer_iterator _Tmp = *this;
            --* this;
            return (_Tmp);
        }

        buffer_iterator& operator+=(const difference_type _Off)
        {	// increment by integer
            _Ptr += _Off;
            return (*this);
        }

        buffer_iterator operator+(const difference_type _Off) const
        {	// return this + integer
            buffer_iterator _Tmp = *this;
            return (_Tmp += _Off);
        }

        buffer_iterator& operator-=(const difference_type _Off)
        {	// decrement by integer
            return (*this += -_Off);
        }

        buffer_iterator operator-(const difference_type _Off) const
        {	// return this - integer
            buffer_iterator _Tmp = *this;
            return (_Tmp -= _Off);
        }

        reference operator[](const difference_type _Off) const
        {	// subscript
            return (*(*this + _Off));
        }

        difference_type operator-(const buffer_iterator& _Right) const
        {	// return difference of iterators
            return (_Ptr - _Right._Ptr);
        }

        bool operator!=(const buffer_iterator& other) const
        {
            return _Ptr != other._Ptr;
        }

        bool operator==(const buffer_iterator& _Right) const
        {	// test for iterator equality
            return (_Ptr == _Right._Ptr);
        }

        bool operator<(const buffer_iterator& _Right) const
        {	// test if this < _Right
            return (_Ptr < _Right._Ptr);
        }

        bool operator>(const buffer_iterator& _Right) const
        {	// test if this > _Right
            return (_Right < *this);
        }

        bool operator<=(const buffer_iterator& _Right) const
        {	// test if this <= _Right
            return (!(_Right < *this));
        }

        bool operator>=(const buffer_iterator& _Right) const
        {	// test if this >= _Right
            return (!(*this < _Right));
        }
    private:
        pointer _Ptr;
    };

    template<class Alloc>
    class base_buffer
    {
    public:
        using allocator_type = Alloc;
        using value_type = typename allocator_type::value_type;
        using iterator = buffer_iterator<value_type>;
        using const_iterator = buffer_iterator<const value_type>;
        using pointer = typename iterator::pointer;
        using const_pointer = typename const_iterator::pointer;

        //buffer default size
        constexpr static size_t   DEFAULT_CAPACITY = 128;

        enum class seek_origin
        {
            Begin,
            Current,
            End
        };

        base_buffer(size_t capacity = DEFAULT_CAPACITY, uint32_t headreserved = 0, const allocator_type& al = allocator_type())
            :flag_(0)
            , headreserved_(headreserved)
            , capacity_(0)
            , readpos_(0)
            , writepos_(0)
            , allocator_(al)
        {
            capacity = (capacity > 0 ? capacity : DEFAULT_CAPACITY);
            prepare(capacity + headreserved);
            readpos_ = writepos_ = headreserved_;
        }

        base_buffer(const base_buffer&) = delete;

        base_buffer& operator=(const base_buffer&) = delete;

        base_buffer(base_buffer&& other) noexcept
            : flag_(other.flag_)
            , headreserved_(other.headreserved_)
            , capacity_(other.capacity_)
            , readpos_(other.readpos_)
            , writepos_(other.writepos_)
            , data_(other.data_)
            , allocator_(std::move(other.allocator_))
        {
            other.headreserved_ = 0;
            other.data_ = nullptr;
            other.clear();
        }

        base_buffer& operator=(base_buffer&& other) noexcept
        {
            flag_ = other.flag_;
            headreserved_ = other.headreserved_;
            capacity_ = other.capacity_;
            readpos_ = other.readpos_;
            writepos_ = other.writepos_;
            data_ = other.data_;
            allocator_ = std::move(other.allocator_);
            other.headreserved_ = 0;
            other.data_ = nullptr;
            other.clear();
            return *this;
        }

        ~base_buffer()
        {
            allocator_.deallocate(data_, capacity_);
        }

        void init(size_t capacity = DEFAULT_CAPACITY, uint32_t headreserved = 0)
        {
            readpos_ = headreserved;
            writepos_ = headreserved;
            headreserved_ = headreserved;
            prepare(capacity);
            flag_ = 0;
        }

        template<typename T>
        void write_back(const T* Indata, size_t count)
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Indata || 0 == count)
                return;
            size_t n = sizeof(T) * count;

            prepare(n);

            auto* buff = reinterpret_cast<T*>(std::addressof(*end()));
            memcpy(buff, Indata, n);
            writepos_ += n;
        }

        template<typename T>
        bool write_front(const T* Indata, size_t count) noexcept
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Indata || 0 == count)
                return false;

            size_t n = sizeof(T) * count;

            if (n > readpos_)
            {
                return false;
            }

            readpos_ -= n;
            auto* buff = reinterpret_cast<T*>(std::addressof(*begin()));
            memcpy(buff, Indata, n);
            return true;
        }

        template<typename T, typename =  std::enable_if_t<std::is_arithmetic_v<T>>>
        bool write_chars(T value)
        {
            prepare(32);
            auto* b = data() + size();
            auto* e = b + 32;
            auto res = std::to_chars(b, e, value);
            if (res.ec != std::errc())
            {
                return false;
            }
            commit(res.ptr - b);
            return true;
        }

        template<typename T>
        bool read(T* Outdata, size_t count) noexcept
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Outdata || 0 == count)
                return false;

            size_t n = sizeof(T) * count;

            if (readpos_ + n > writepos_)
            {
                return false;
            }

            auto* buff = std::addressof(*begin());
            memcpy(Outdata, buff, n);
            readpos_ += n;
            return true;
        }

        void consume(std::size_t n)
        {
            seek(static_cast<int>(n));
        }

        size_t seek(int offset, seek_origin s = seek_origin::Current)  noexcept
        {
            switch (s)
            {
            case seek_origin::Begin:
                readpos_ = offset;
                break;
            case seek_origin::Current:
                readpos_ += offset;
                assert(readpos_ <= writepos_);
                if (readpos_ > writepos_)
                {
                    readpos_ = writepos_;
                }
                break;
            default:
                assert(false);
                break;
            }
            return readpos_;
        }

        void clear() noexcept
        {
            assert(nullptr != data_);
            flag_ = 0;
            writepos_ = readpos_ = headreserved_;
        }

        template<typename ValueType>
        void set_flag(ValueType v) noexcept
        {
            flag_ |= static_cast<uint32_t>(v);
        }

        template<typename ValueType>
        bool has_flag(ValueType v) const noexcept
        {
            return ((flag_ & static_cast<uint32_t>(v)) != 0);
        }

        template<typename ValueType>
        void clear_flag(ValueType v) noexcept
        {
            flag_ &= ~static_cast<uint32_t>(v);
        }

        void commit(std::size_t n) noexcept
        {
            writepos_ += n;
            assert(writepos_ <= capacity_);
            if (writepos_ >= capacity_)
            {
                writepos_ = capacity_;
            }
        }

        void revert(size_t n) noexcept
        {
            assert(writepos_ >= (readpos_+n));
            if (writepos_ >= n)
            {
                writepos_ -= n;
            }
        }

        const_iterator begin() const noexcept
        {
            return const_iterator{ data_ + readpos_ };
        }

        iterator begin() noexcept
        {
            return iterator{ data_ + readpos_ };
        }

        const_iterator end() const noexcept
        {
            return const_iterator{ data_ + writepos_ };
        }

        iterator end() noexcept
        {
            return iterator{ data_ + writepos_ };
        }

        pointer data() noexcept
        {
            return  std::addressof(*begin());
        }

        const_pointer data() const noexcept
        {
            return  std::addressof(*begin());
        }

        //readable size
        size_t size() const noexcept
        {
            return writepos_ - readpos_;
        }

        size_t capacity() const noexcept
        {
            return capacity_;
        }

        size_t reserved() const noexcept
        {
            return headreserved_;
        }

        void prepare(size_t need)
        {
            if (writeablesize() >= need)
            {
                return;
            }

            if (writeablesize() + readpos_ < need + headreserved_)
            {
                auto required_size = writepos_ + need;
                required_size = next_pow2(required_size);
                auto tmp = allocator_.allocate(required_size);
                if (nullptr != data_)
                {
                    std::memcpy(tmp, data_, writepos_);
                    allocator_.deallocate(data_, capacity_);
                }
                data_ = tmp;
                capacity_ = required_size;
            }
            else
            {
                size_t readable = size();
                if (readable != 0)
                {
                    assert(readpos_ >= headreserved_);
                    std::memcpy(data_ + headreserved_, data_ + readpos_, readable);
                }
                readpos_ = headreserved_;
                writepos_ = readpos_ + readable;
            }
        }

        size_t writeablesize() const noexcept
        {
            assert(capacity_ >= writepos_);
            return capacity_ - writepos_;
        }

    private:
        size_t next_pow2(size_t x)
        {
            if (!(x & (x - 1)))
            {
                return x;
            }
            x |= x >> 1;
            x |= x >> 2;
            x |= x >> 4;
            x |= x >> 8;
            x |= x >> 16;
            return x + 1;
        }
    private:
        uint32_t flag_;

        uint32_t headreserved_;

        size_t capacity_;
        //read position
        size_t readpos_;
        //write position
        size_t writepos_;

        value_type* data_ = nullptr;

        allocator_type allocator_;
    };
};

#ifdef MOON_ENABLE_MIMALLOC
#include "mimalloc.h"
namespace moon
{
    using buffer = base_buffer<mi_stl_allocator<char>>;
}
#else
namespace moon
{
    using buffer = base_buffer<std::allocator<char>>;
}
#endif



