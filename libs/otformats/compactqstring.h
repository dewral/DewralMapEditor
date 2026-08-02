#ifndef COMPACTQSTRING_H
#define COMPACTQSTRING_H

#include <QString>

#include <memory>
#include <utility>

class CompactQString
{
public:
    CompactQString() = default;
    CompactQString(const QString &value) { set(value); }
    CompactQString(QString &&value) { set(std::move(value)); }

    CompactQString(const CompactQString &other)
    {
        if (other.m_value) m_value = std::make_unique<QString>(*other.m_value);
    }

    CompactQString(CompactQString &&) noexcept = default;

    CompactQString &operator=(const CompactQString &other)
    {
        if (this != &other) set(other.value());
        return *this;
    }

    CompactQString &operator=(CompactQString &&) noexcept = default;
    CompactQString &operator=(const QString &value)
    {
        set(value);
        return *this;
    }

    CompactQString &operator=(QString &&value)
    {
        set(std::move(value));
        return *this;
    }

    bool isEmpty() const noexcept { return !m_value || m_value->isEmpty(); }
    qsizetype capacity() const noexcept { return m_value ? m_value->capacity() : 0; }
    void clear() noexcept { m_value.reset(); }

    const QString &value() const noexcept
    {
        static const QString empty;
        return m_value ? *m_value : empty;
    }

    operator const QString &() const noexcept { return value(); }

    friend bool operator==(const CompactQString &left, const CompactQString &right)
    {
        return left.value() == right.value();
    }

    friend bool operator!=(const CompactQString &left, const CompactQString &right)
    {
        return !(left == right);
    }

    friend bool operator==(const CompactQString &left, const QString &right)
    {
        return left.value() == right;
    }

    friend bool operator==(const QString &left, const CompactQString &right)
    {
        return left == right.value();
    }

    friend bool operator!=(const CompactQString &left, const QString &right)
    {
        return !(left == right);
    }

    friend bool operator!=(const QString &left, const CompactQString &right)
    {
        return !(left == right);
    }

private:
    std::unique_ptr<QString> m_value;

    void set(const QString &value)
    {
        if (value.isEmpty()) {
            clear();
        } else if (m_value) {
            *m_value = value;
        } else {
            m_value = std::make_unique<QString>(value);
        }
    }

    void set(QString &&value)
    {
        if (value.isEmpty()) {
            clear();
        } else if (m_value) {
            *m_value = std::move(value);
        } else {
            m_value = std::make_unique<QString>(std::move(value));
        }
    }
};

static_assert(sizeof(CompactQString) == sizeof(void *),
              "CompactQString must only store an optional pointer");

#endif
