#ifndef COMPACTVECTOR_H
#define COMPACTVECTOR_H

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// A vector-compatible container optimized for the common zero/one element case.
// Its object footprint matches a three-pointer std::vector on 64-bit builds.
template <typename T>
class CompactVector
{
public:
    using value_type = T;
    using size_type = std::uint32_t;
    using difference_type = std::ptrdiff_t;
    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;
    using iterator = T *;
    using const_iterator = const T *;

    CompactVector() noexcept = default;

    CompactVector(const CompactVector &other)
    {
        assign(other.begin(), other.end());
    }

    CompactVector(CompactVector &&other) noexcept
    {
        moveFrom(std::move(other));
    }

    ~CompactVector()
    {
        release();
    }

    CompactVector &operator=(const CompactVector &other)
    {
        if (this != &other) assign(other.begin(), other.end());
        return *this;
    }

    CompactVector &operator=(CompactVector &&other) noexcept
    {
        if (this != &other) {
            release();
            moveFrom(std::move(other));
        }
        return *this;
    }

    CompactVector &operator=(const std::vector<T> &other)
    {
        assign(other.begin(), other.end());
        return *this;
    }

    CompactVector &operator=(std::vector<T> &&other)
    {
        assign(std::make_move_iterator(other.begin()),
               std::make_move_iterator(other.end()));
        other.clear();
        return *this;
    }

    iterator begin() noexcept { return data(); }
    const_iterator begin() const noexcept { return data(); }
    const_iterator cbegin() const noexcept { return data(); }
    iterator end() noexcept { return data() + m_size; }
    const_iterator end() const noexcept { return data() + m_size; }
    const_iterator cend() const noexcept { return data() + m_size; }

    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type capacity() const noexcept { return m_capacity; }

    reference operator[](size_type index) noexcept { return data()[index]; }
    const_reference operator[](size_type index) const noexcept { return data()[index]; }
    reference front() noexcept { return data()[0]; }
    const_reference front() const noexcept { return data()[0]; }
    reference back() noexcept { return data()[m_size - 1]; }
    const_reference back() const noexcept { return data()[m_size - 1]; }

    void clear() noexcept
    {
        std::destroy_n(data(), m_size);
        m_size = 0;
    }

    void reserve(size_type requested)
    {
        if (requested > m_capacity) reallocate(requested);
    }

    void shrink_to_fit()
    {
        const size_type requested = std::max<size_type>(1, m_size);
        if (requested < m_capacity) reallocate(requested);
    }

    void push_back(const T &value) { emplace_back(value); }
    void push_back(T &&value) { emplace_back(std::move(value)); }

    template <typename... Args>
    reference emplace_back(Args &&...args)
    {
        ensureAdditional(1);
        T *slot = data() + m_size;
        ::new (static_cast<void *>(slot)) T(std::forward<Args>(args)...);
        ++m_size;
        return *slot;
    }

    void pop_back()
    {
        --m_size;
        std::destroy_at(data() + m_size);
        compactIfExcessive();
    }

    iterator insert(const_iterator position, const T &value)
    {
        return insertImpl(position, value);
    }

    iterator insert(const_iterator position, T &&value)
    {
        return insertImpl(position, std::move(value));
    }

    iterator erase(const_iterator position)
    {
        const size_type index = static_cast<size_type>(position - cbegin());
        T *values = data();
        for (size_type i = index; i + 1 < m_size; ++i)
            values[i] = std::move(values[i + 1]);
        --m_size;
        std::destroy_at(values + m_size);
        compactIfExcessive();
        return data() + index;
    }

    template <typename InputIt>
    void assign(InputIt first, InputIt last)
    {
        clear();
        const auto distance = std::distance(first, last);
        if (distance < 0
            || static_cast<std::uint64_t>(distance) > std::numeric_limits<size_type>::max())
            throw std::length_error("CompactVector capacity exceeded");
        const size_type count = static_cast<size_type>(distance);
        const size_type compactCapacity = std::max<size_type>(1, count);
        if (compactCapacity != m_capacity) reallocate(compactCapacity);
        T *destination = data();
        for (; first != last; ++first, ++m_size)
            ::new (static_cast<void *>(destination + m_size)) T(*first);
    }

private:
    using Allocator = std::allocator<T>;
    using AllocatorTraits = std::allocator_traits<Allocator>;
    using InlineStorage = std::aligned_storage_t<sizeof(T), alignof(T)>;

    union Storage {
        InlineStorage inlineValue;
        T *heap;
        constexpr Storage() noexcept : inlineValue{} {}
        ~Storage() {}
    } m_storage;

    size_type m_size = 0;
    size_type m_capacity = 1;

    bool isInline() const noexcept { return m_capacity == 1; }

    pointer data() noexcept
    {
        return isInline() ? reinterpret_cast<T *>(&m_storage.inlineValue) : m_storage.heap;
    }

    const_pointer data() const noexcept
    {
        return isInline() ? reinterpret_cast<const T *>(&m_storage.inlineValue) : m_storage.heap;
    }

    void ensureAdditional(size_type additional)
    {
        if (additional <= m_capacity - m_size) return;
        if (m_size > std::numeric_limits<size_type>::max() - additional)
            throw std::length_error("CompactVector capacity exceeded");
        const size_type required = m_size + additional;
        const size_type doubled = m_capacity <= std::numeric_limits<size_type>::max() / 2
                                      ? m_capacity * 2
                                      : std::numeric_limits<size_type>::max();
        reallocate(std::max(required, doubled));
    }

    void compactIfExcessive()
    {
        if (m_capacity >= 8 && m_size <= m_capacity / 4)
            reallocate(std::max<size_type>(1, m_size));
    }

    void reallocate(size_type requested)
    {
        Allocator allocator;
        T *oldData = data();
        const bool oldInline = isInline();
        T *newData = requested == 1 ? reinterpret_cast<T *>(&m_storage.inlineValue)
                                    : AllocatorTraits::allocate(allocator, requested);

        if (requested == 1 && !oldInline) {
            T *oldHeap = m_storage.heap;
            if (m_size != 0) {
                T temporary(std::move(oldHeap[0]));
                std::destroy_n(oldHeap, m_size);
                AllocatorTraits::deallocate(allocator, oldHeap, m_capacity);
                m_storage.inlineValue = InlineStorage{};
                ::new (static_cast<void *>(newData)) T(std::move(temporary));
            } else {
                AllocatorTraits::deallocate(allocator, oldHeap, m_capacity);
                m_storage.inlineValue = InlineStorage{};
            }
            m_capacity = 1;
            return;
        }

        if (requested > 1) {
            for (size_type i = 0; i < m_size; ++i)
                ::new (static_cast<void *>(newData + i)) T(std::move_if_noexcept(oldData[i]));
            std::destroy_n(oldData, m_size);
            if (!oldInline)
                AllocatorTraits::deallocate(allocator, oldData, m_capacity);
            m_storage.heap = newData;
        }
        m_capacity = requested;
    }

    template <typename U>
    iterator insertImpl(const_iterator position, U &&value)
    {
        const size_type index = static_cast<size_type>(position - cbegin());
        if (index == m_size) {
            emplace_back(std::forward<U>(value));
            return data() + index;
        }
        ensureAdditional(1);
        T *values = data();
        ::new (static_cast<void *>(values + m_size)) T(std::move(values[m_size - 1]));
        for (size_type i = m_size - 1; i > index; --i)
            values[i] = std::move(values[i - 1]);
        values[index] = std::forward<U>(value);
        ++m_size;
        return values + index;
    }

    void release() noexcept
    {
        clear();
        if (!isInline()) {
            Allocator allocator;
            AllocatorTraits::deallocate(allocator, m_storage.heap, m_capacity);
        }
        m_storage.inlineValue = InlineStorage{};
        m_capacity = 1;
    }

    void moveFrom(CompactVector &&other) noexcept
    {
        if (other.isInline()) {
            if (other.m_size != 0) {
                ::new (static_cast<void *>(data())) T(std::move(other.front()));
                m_size = 1;
                other.clear();
            }
            return;
        }
        m_storage.heap = other.m_storage.heap;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_storage.inlineValue = InlineStorage{};
        other.m_size = 0;
        other.m_capacity = 1;
    }
};

#endif
