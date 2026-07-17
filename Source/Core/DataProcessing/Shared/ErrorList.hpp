#pragma once

#include <Core/Containers/FlatSet.hpp>
#include <Core/Defines.hpp>

#include <type_traits>

namespace Hyperion::DataProcessing {

template <class TErrorType>
class ErrorList
{
public:
    using ErrorType = TErrorType;

    ErrorList()
        : m_errorSuppressionDepth(0)
    {
    }

    ErrorList(const ErrorList& other)
        : m_errors(other.m_errors),
          m_errorSuppressionDepth(other.m_errorSuppressionDepth)
    {
    }

    ErrorList& operator=(const ErrorList& other)
    {
        m_errors = other.m_errors;
        m_errorSuppressionDepth = other.m_errorSuppressionDepth;
        return *this;
    }

    ErrorList(ErrorList&& other) noexcept = default;
    ErrorList& operator=(ErrorList&& other) noexcept = default;

    ~ErrorList() = default;

    size_t Size() const { return m_errors.Size(); }

    ErrorType& operator[](size_t index) { return m_errors[index]; }
    const ErrorType& operator[](size_t index) const { return m_errors[index]; }

    void AddError(const ErrorType& error)
    {
        if (ErrorsSuppressed())
            return;

        m_errors.Insert(error);
    }

    void ClearErrors() { m_errors.Clear(); }

    void Concatenate(const ErrorList& other) { m_errors.Merge(other.m_errors); }

    bool ErrorsSuppressed() const { return m_errorSuppressionDepth > 0; }

    void SuppressErrors(bool suppress)
    {
        if (suppress)
        {
            m_errorSuppressionDepth++;
        }
        else
        {
            if (m_errorSuppressionDepth <= 0)
                return;

            m_errorSuppressionDepth--;
        }
    }

    bool HasFatalErrors() const
    {
        for (auto& error : m_errors)
        {
            using LevelType = std::decay_t<decltype(error.GetLevel())>;
            if (error.GetLevel() == LevelType(0))
                return true;
        }

        return false;
    }

    template <class F>
    bool HasError(F&& pred) const
    {
        for (auto& error : m_errors)
        {
            if (pred(error))
                return true;
        }

        return false;
    }

private:
    FlatSet<ErrorType> m_errors;
    uint32 m_errorSuppressionDepth;
};

} // namespace Hyperion::DataProcessing
