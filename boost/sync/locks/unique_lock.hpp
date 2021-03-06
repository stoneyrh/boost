/*
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 *
 * (C) Copyright 2007 Anthony Williams
 * (C) Copyright 2011-2012 Vicente J. Botet Escriba
 * (C) Copyright 2013 Andrey Semashev
 */
/*!
 * \file   locks/unique_lock.hpp
 *
 * \brief  This header defines an exclusive lock guard.
 */

#ifndef BOOST_SYNC_LOCKS_UNIQUE_LOCK_HPP_INCLUDED_
#define BOOST_SYNC_LOCKS_UNIQUE_LOCK_HPP_INCLUDED_

#include <cstddef>
#include <boost/throw_exception.hpp>
#include <boost/move/core.hpp>
#include <boost/move/utility.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/utility/explicit_operator_bool.hpp>
#include <boost/sync/detail/config.hpp>
#include <boost/sync/detail/time_traits.hpp>
#include <boost/sync/locks/lock_options.hpp>
#include <boost/sync/locks/unique_lock_fwd.hpp>
#include <boost/sync/locks/shared_lock_fwd.hpp>
#include <boost/sync/locks/upgrade_lock_fwd.hpp>

#include <boost/sync/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {

namespace sync {

/*!
 * \brief A unique lock scope guard
 */
template< typename Mutex >
class unique_lock
{
public:
    typedef Mutex mutex_type;

private:
    mutex_type* m_mutex;
    bool m_is_locked;

private:
    explicit unique_lock(upgrade_lock< mutex_type >&);
    unique_lock& operator= (upgrade_lock< mutex_type >& other);

public:
    BOOST_MOVABLE_BUT_NOT_COPYABLE(unique_lock)

    unique_lock() BOOST_NOEXCEPT :
        m_mutex(NULL), m_is_locked(false)
    {
    }

    explicit unique_lock(mutex_type& m) :
        m_mutex(&m), m_is_locked(false)
    {
        lock();
    }
    unique_lock(mutex_type& m, adopt_lock_t) BOOST_NOEXCEPT :
        m_mutex(&m), m_is_locked(true)
    {
    }
    unique_lock(mutex_type& m, defer_lock_t) BOOST_NOEXCEPT :
        m_mutex(&m), m_is_locked(false)
    {
    }
    unique_lock(mutex_type& m, try_to_lock_t) :
        m_mutex(&m), m_is_locked(false)
    {
        try_lock();
    }

    template< typename Time >
    unique_lock(typename enable_if_c< detail::time_traits< Time >::is_supported, mutex_type& >::type m, Time const& t) :
        m_mutex(&m), m_is_locked(false)
    {
        timed_lock(t);
    }

    unique_lock(BOOST_RV_REF(unique_lock) that) BOOST_NOEXCEPT :
        m_mutex(that.m_mutex), m_is_locked(that.m_is_locked)
    {
        that.m_mutex = NULL;
        that.m_is_locked = false;
    }

    explicit unique_lock(BOOST_RV_REF(upgrade_lock< mutex_type >) that)
        m_mutex(that.m_mutex), is_locked(that.m_is_locked)
    {
        if (m_is_locked)
        {
            m_mutex->unlock_upgrade_and_lock();
        }
        that.release();
    }

    // Conversion from upgrade lock
    unique_lock(BOOST_RV_REF(upgrade_lock< mutex_type >) ul, try_to_lock_t) :
        m_mutex(NULL), m_is_locked(false)
    {
        if (ul.owns_lock())
        {
            if (ul.mutex()->try_unlock_upgrade_and_lock())
            {
                m_mutex = ul.release();
                m_is_locked = true;
            }
        }
        else
        {
            m_mutex = ul.release();
        }
    }

    // Conversion from shared locking
    unique_lock(BOOST_RV_REF(shared_lock< mutex_type >) sl, try_to_lock_t) :
        m_mutex(NULL), m_is_locked(false)
    {
        if (sl.owns_lock())
        {
            if (sl.mutex()->try_unlock_shared_and_lock())
            {
                m_mutex = sl.release();
                m_is_locked = true;
            }
        }
        else
        {
            m_mutex = sl.release();
        }
    }

    template< typename Time >
    unique_lock(BOOST_RV_REF(shared_lock< mutex_type >) sl, Time const& t, typename enable_if_c< detail::time_traits< Time >::is_supported, int >::type = 0) :
        m_mutex(NULL), m_is_locked(false)
    {
        if (sl.owns_lock())
        {
            if (sl.mutex()->timed_unlock_shared_and_lock(t))
            {
                m_mutex = sl.release();
                m_is_locked = true;
            }
        }
        else
        {
            m_mutex = sl.release();
        }
    }

    ~unique_lock()
    {
        if (m_is_locked)
        {
            m_mutex->unlock();
        }
    }

    unique_lock& operator= (BOOST_RV_REF(unique_lock) that) BOOST_NOEXCEPT
    {
        swap((unique_lock&)that);
        return *this;
    }

    unique_lock& operator= (BOOST_RV_REF(upgrade_lock< mutex_type >) other)
    {
        unique_lock temp(boost::move(other));
        swap(temp);
        return *this;
    }

    void lock()
    {
        if (!m_mutex)
        {
            boost::throw_exception(boost::lock_error(system::errc::operation_not_permitted, "boost unique_lock has no mutex"));
        }
        if (m_is_locked)
        {
            boost::throw_exception(boost::lock_error(system::errc::resource_deadlock_would_occur, "boost unique_lock already owns the mutex"));
        }

        m_mutex->lock();
        m_is_locked = true;
    }

    bool try_lock()
    {
        if (!m_mutex)
        {
            boost::throw_exception(boost::lock_error(system::errc::operation_not_permitted, "boost unique_lock has no mutex"));
        }
        if (m_is_locked)
        {
            boost::throw_exception(boost::lock_error(system::errc::resource_deadlock_would_occur, "boost unique_lock already owns the mutex"));
        }

        m_is_locked = m_mutex->try_lock();

        return m_is_locked;
    }

    template< typename Time >
    typename enable_if_c< detail::time_traits< Time >::is_supported, bool >::type timed_lock(Time const& time)
    {
        if (!m_mutex)
        {
            boost::throw_exception(boost::lock_error(system::errc::operation_not_permitted, "boost unique_lock has no mutex"));
        }
        if (m_is_locked)
        {
            boost::throw_exception(boost::lock_error(system::errc::resource_deadlock_would_occur, "boost unique_lock already owns the mutex"));
        }

        m_is_locked = m->timed_lock(time);

        return m_is_locked;
    }

    template< typename Duration >
    typename detail::enable_if_tag< Duration, detail::time_duration_tag, bool >::type try_lock_for(Duration const& rel_time)
    {
        if (!m_mutex)
        {
            boost::throw_exception(boost::lock_error(system::errc::operation_not_permitted, "boost unique_lock has no mutex"));
        }
        if (m_is_locked)
        {
            boost::throw_exception(boost::lock_error(system::errc::resource_deadlock_would_occur, "boost unique_lock already owns the mutex"));
        }

        m_is_locked = m_mutex->try_lock_for(rel_time);

        return m_is_locked;
    }

    template< typename TimePoint >
    typename detail::enable_if_tag< Duration, detail::time_point_tag, bool >::type try_lock_until(TimePoint const& abs_time)
    {
        if (!m_mutex)
        {
            boost::throw_exception(boost::lock_error(system::errc::operation_not_permitted, "boost unique_lock has no mutex"));
        }
        if (m_is_locked)
        {
            boost::throw_exception(boost::lock_error(system::errc::resource_deadlock_would_occur, "boost unique_lock owns already the mutex"));
        }

        m_is_locked = m_mutex->try_lock_until(abs_time);

        return m_is_locked;
    }

    void unlock()
    {
        if (!m_mutex)
        {
            boost::throw_exception(boost::lock_error(system::errc::operation_not_permitted, "boost unique_lock has no mutex"));
        }
        if (m_is_locked)
        {
            boost::throw_exception(boost::lock_error(system::errc::operation_not_permitted, "boost unique_lock doesn't own the mutex"));
        }

        m_mutex->unlock();
        m_is_locked = false;
    }

    BOOST_EXPLICIT_OPERATOR_BOOL()

    bool operator! () const BOOST_NOEXCEPT
    {
        return !m_is_locked;
    }

    bool owns_lock() const BOOST_NOEXCEPT
    {
        return m_is_locked;
    }

    mutex_type* mutex() const BOOST_NOEXCEPT
    {
        return m_mutex;
    }

    mutex_type* release() BOOST_NOEXCEPT
    {
        mutex_type* const res = m_mutex;
        m_mutex = NULL;
        m_is_locked = false;
        return res;
    }

    void swap(unique_lock& that) BOOST_NOEXCEPT
    {
        mutex_type* const p = m_mutex;
        m_mutex = that.m_mutex;
        that.m_mutex = p;
        const bool f = m_is_locked;
        m_is_locked = that.m_is_locked;
        that.m_is_locked = f;
    }
};

template< typename Mutex >
inline void swap(unique_lock< Mutex >& lhs, unique_lock< Mutex >& rhs) BOOST_NOEXCEPT
{
    lhs.swap(rhs);
}

} // namespace sync

} // namespace boost

#include <boost/sync/detail/footer.hpp>

#endif // BOOST_SYNC_LOCKS_UNIQUE_LOCK_HPP_INCLUDED_
