//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTERPROCESS_DETAIL_OS_THREAD_FUNCTIONS_HPP
#define BOOST_INTERPROCESS_DETAIL_OS_THREAD_FUNCTIONS_HPP

#include <boost/interprocess/detail/config_begin.hpp>
#include <boost/interprocess/detail/workaround.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/interprocess/detail/posix_time_types_wrk.hpp>
#include <cstddef>

#if defined(BOOST_INTERPROCESS_WINDOWS)
#  include <boost/interprocess/detail/win32_api.hpp>
#else
#  include <pthread.h>
#  include <unistd.h>
#  include <sched.h>
#  include <time.h>
#  ifdef BOOST_INTERPROCESS_BSD_DERIVATIVE
      //Some *BSD systems (OpenBSD & NetBSD) need sys/param.h before sys/sysctl.h, whereas
      //others (FreeBSD & Darwin) need sys/types.h
#     include <sys/types.h>
#     include <sys/param.h>
#     include <sys/sysctl.h>
#  endif
//According to the article "C/C++ tip: How to measure elapsed real time for benchmarking"
#  if defined(CLOCK_MONOTONIC_PRECISE)   //BSD
#     define BOOST_INTERPROCESS_CLOCK_MONOTONIC CLOCK_MONOTONIC_PRECISE
#  elif defined(CLOCK_MONOTONIC_RAW)     //Linux
#     define BOOST_INTERPROCESS_CLOCK_MONOTONIC CLOCK_MONOTONIC_RAW
#  elif defined(CLOCK_HIGHRES)           //Solaris
#     define BOOST_INTERPROCESS_CLOCK_MONOTONIC CLOCK_HIGHRES
#  elif defined(CLOCK_MONOTONIC)         //POSIX (AIX, BSD, Linux, Solaris)
#     define BOOST_INTERPROCESS_CLOCK_MONOTONIC CLOCK_MONOTONIC
#  elif !defined(CLOCK_MONOTONIC) && (defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__))
#     include <mach/mach_time.h>  // mach_absolute_time, mach_timebase_info_data_t
#     define BOOST_INTERPROCESS_MATCH_ABSOLUTE_TIME
#  else
#     error "No high resolution steady clock in your system, please provide a patch"
#  endif
#endif

namespace boost {
namespace interprocess {
namespace ipcdetail{

#if (defined BOOST_INTERPROCESS_WINDOWS)

typedef unsigned long OS_process_id_t;
typedef unsigned long OS_thread_id_t;
typedef OS_thread_id_t OS_systemwide_thread_id_t;

//process
inline OS_process_id_t get_current_process_id()
{  return winapi::get_current_process_id();  }

inline OS_process_id_t get_invalid_process_id()
{  return OS_process_id_t(0);  }

//thread
inline OS_thread_id_t get_current_thread_id()
{  return winapi::get_current_thread_id();  }

inline OS_thread_id_t get_invalid_thread_id()
{  return OS_thread_id_t(0xffffffff);  }

inline bool equal_thread_id(OS_thread_id_t id1, OS_thread_id_t id2)
{  return id1 == id2;  }

//return the system tick in ns
inline unsigned long get_system_tick_ns()
{
   unsigned long curres;
   winapi::set_timer_resolution(10000, 0, &curres);
   //Windows API returns the value in hundreds of ns
   return (curres - 1ul)*100ul;
}

//return the system tick in us
inline unsigned long get_system_tick_us()
{
   unsigned long curres;
   winapi::set_timer_resolution(10000, 0, &curres);
   //Windows API returns the value in hundreds of ns
   return (curres - 1ul)/10ul + 1ul;
}

typedef unsigned __int64 OS_highres_count_t;

inline OS_highres_count_t get_system_tick_in_highres_counts()
{
   __int64 freq;
   unsigned long curres;
   winapi::set_timer_resolution(10000, 0, &curres);
   //Frequency in counts per second
   if(!winapi::query_performance_frequency(&freq)){
      //Tick resolution in ms
      return (curres-1)/10000u + 1;
   }
   else{
      //In femtoseconds
      __int64 count_fs    = __int64(1000000000000000LL - 1LL)/freq + 1LL;
      __int64 tick_counts = (__int64(curres)*100000000LL - 1LL)/count_fs + 1LL;
      return static_cast<OS_highres_count_t>(tick_counts);
   }
}

inline OS_highres_count_t get_current_system_highres_count()
{
   __int64 count;
   if(!winapi::query_performance_counter(&count)){
      count = winapi::get_tick_count();
   }
   return count;
}

inline void zero_highres_count(OS_highres_count_t &count)
{  count = 0;  }

inline bool is_highres_count_zero(const OS_highres_count_t &count)
{  return count == 0;  }

template <class Ostream>
inline Ostream &ostream_highres_count(Ostream &ostream, const OS_highres_count_t &count)
{
   ostream << count;
   return ostream;
}

inline OS_highres_count_t system_highres_count_subtract(const OS_highres_count_t &l, const OS_highres_count_t &r)
{  return l - r;  }

inline bool system_highres_count_less(const OS_highres_count_t &l, const OS_highres_count_t &r)
{  return l < r;  } 

inline void thread_sleep_tick()
{  winapi::sleep_tick();   }

inline void thread_yield()
{  winapi::sched_yield();  }

inline void thread_sleep(unsigned int ms)
{  winapi::Sleep(ms);  }

//systemwide thread
inline OS_systemwide_thread_id_t get_current_systemwide_thread_id()
{
   return get_current_thread_id();
}

inline void systemwide_thread_id_copy
   (const volatile OS_systemwide_thread_id_t &from, volatile OS_systemwide_thread_id_t &to)
{
   to = from;
}

inline bool equal_systemwide_thread_id(const OS_systemwide_thread_id_t &id1, const OS_systemwide_thread_id_t &id2)
{
   return equal_thread_id(id1, id2);
}

inline OS_systemwide_thread_id_t get_invalid_systemwide_thread_id()
{
   return get_invalid_thread_id();
}

inline long double get_current_process_creation_time()
{
   winapi::interprocess_filetime CreationTime, ExitTime, KernelTime, UserTime;

   get_process_times
      ( winapi::get_current_process(), &CreationTime, &ExitTime, &KernelTime, &UserTime);

   typedef long double ldouble_t;
   const ldouble_t resolution = (100.0l/1000000000.0l);
   return CreationTime.dwHighDateTime*(ldouble_t(1u<<31u)*2.0l*resolution) +
              CreationTime.dwLowDateTime*resolution;
}

inline unsigned int get_num_cores()
{
   winapi::system_info sysinfo;
   winapi::get_system_info( &sysinfo );
   //in Windows dw is long which is equal in bits to int
   return static_cast<unsigned>(sysinfo.dwNumberOfProcessors);
}

#else    //#if (defined BOOST_INTERPROCESS_WINDOWS)

typedef pthread_t OS_thread_id_t;
typedef pid_t     OS_process_id_t;

struct OS_systemwide_thread_id_t
{
   OS_systemwide_thread_id_t()
      :  pid(), tid()
   {}

   OS_systemwide_thread_id_t(pid_t p, pthread_t t)
      :  pid(p), tid(t)
   {}

   OS_systemwide_thread_id_t(const OS_systemwide_thread_id_t &x)
      :  pid(x.pid), tid(x.tid)
   {}

   OS_systemwide_thread_id_t(const volatile OS_systemwide_thread_id_t &x)
      :  pid(x.pid), tid(x.tid)
   {}

   OS_systemwide_thread_id_t & operator=(const OS_systemwide_thread_id_t &x)
   {  pid = x.pid;   tid = x.tid;   return *this;   }

   OS_systemwide_thread_id_t & operator=(const volatile OS_systemwide_thread_id_t &x)
   {  pid = x.pid;   tid = x.tid;   return *this;  }

   void operator=(const OS_systemwide_thread_id_t &x) volatile
   {  pid = x.pid;   tid = x.tid;   }

   pid_t       pid;
   pthread_t   tid;
};

inline void systemwide_thread_id_copy
   (const volatile OS_systemwide_thread_id_t &from, volatile OS_systemwide_thread_id_t &to)
{
   to.pid = from.pid;
   to.tid = from.tid;
}

//process
inline OS_process_id_t get_current_process_id()
{  return ::getpid();  }

inline OS_process_id_t get_invalid_process_id()
{  return pid_t(0);  }

//thread
inline OS_thread_id_t get_current_thread_id()
{  return ::pthread_self();  }

inline OS_thread_id_t get_invalid_thread_id()
{
   static pthread_t invalid_id;
   return invalid_id;
}

inline bool equal_thread_id(OS_thread_id_t id1, OS_thread_id_t id2)
{  return 0 != pthread_equal(id1, id2);  }

inline void thread_yield()
{  ::sched_yield();  }

#ifndef BOOST_INTERPROCESS_MATCH_ABSOLUTE_TIME
typedef struct timespec OS_highres_count_t;
#else
typedef unsigned long long OS_highres_count_t;
#endif

inline unsigned long get_system_tick_ns()
{
   #ifdef _SC_CLK_TCK
   long hz =::sysconf(_SC_CLK_TCK); // ticks per sec
   if(hz <= 0){   //Try a typical value on error
      hz = 100;
   }
   return 999999999ul/static_cast<unsigned long>(hz)+1ul;
   #else
      #error "Can't obtain system tick value for your system, please provide a patch"
   #endif
}

inline OS_highres_count_t get_system_tick_in_highres_counts()
{
   #ifndef BOOST_INTERPROCESS_MATCH_ABSOLUTE_TIME
   struct timespec ts;
   ts.tv_sec = 0;
   ts.tv_nsec = get_system_tick_ns();
   return ts;
   #else
   mach_timebase_info_data_t info;
   mach_timebase_info(&info);
            //ns
   return static_cast<OS_highres_count_t>
   (  
      static_cast<double>(get_system_tick_ns()) 
         / (static_cast<double>(info.numer) / info.denom)
   );
   #endif
}

//return system ticks in us
inline unsigned long get_system_tick_us()
{
   return (get_system_tick_ns()-1)/1000ul + 1ul;
}

inline OS_highres_count_t get_current_system_highres_count()
{
   #if defined(BOOST_INTERPROCESS_CLOCK_MONOTONIC)
      struct timespec count;
      ::clock_gettime(BOOST_INTERPROCESS_CLOCK_MONOTONIC, &count);
      return count;
   #elif defined(BOOST_INTERPROCESS_MATCH_ABSOLUTE_TIME)
      return ::mach_absolute_time();
   #endif
}

#ifndef BOOST_INTERPROCESS_MATCH_ABSOLUTE_TIME

inline void zero_highres_count(OS_highres_count_t &count)
{  count.tv_sec = 0; count.tv_nsec = 0;  }

inline bool is_highres_count_zero(const OS_highres_count_t &count)
{  return count.tv_sec == 0 && count.tv_nsec == 0;  }

template <class Ostream>
inline Ostream &ostream_highres_count(Ostream &ostream, const OS_highres_count_t &count)
{
   ostream << count.tv_sec << "s:" << count.tv_nsec << "ns";
   return ostream;
}

inline OS_highres_count_t system_highres_count_subtract(const OS_highres_count_t &l, const OS_highres_count_t &r)
{
   OS_highres_count_t res;

   if (l.tv_nsec < r.tv_nsec){
      res.tv_nsec = 1000000000 + l.tv_nsec - r.tv_nsec;        
      res.tv_sec  = l.tv_sec - 1 - r.tv_sec;
   }
   else{
      res.tv_nsec = l.tv_nsec - r.tv_nsec;        
      res.tv_sec  = l.tv_sec - r.tv_sec;
   }

   return res;
}

inline bool system_highres_count_less(const OS_highres_count_t &l, const OS_highres_count_t &r)
{  return l.tv_sec < r.tv_sec || (l.tv_sec == r.tv_sec && l.tv_nsec < r.tv_nsec);  } 

#else

inline void zero_highres_count(OS_highres_count_t &count)
{  count = 0;  }

inline bool is_highres_count_zero(const OS_highres_count_t &count)
{  return count == 0;  }

template <class Ostream>
inline Ostream &ostream_highres_count(Ostream &ostream, const OS_highres_count_t &count)
{
   ostream << count ;
   return ostream;
}

inline OS_highres_count_t system_highres_count_subtract(const OS_highres_count_t &l, const OS_highres_count_t &r)
{  return l - r;  }

inline bool system_highres_count_less(const OS_highres_count_t &l, const OS_highres_count_t &r)
{  return l < r;  } 

#endif

inline void thread_sleep_tick()
{
   struct timespec rqt;
   //Sleep for the half of the tick time
   rqt.tv_sec  = 0;
   rqt.tv_nsec = get_system_tick_ns()/2;
   ::nanosleep(&rqt, 0);
}

inline void thread_sleep(unsigned int ms)
{
   struct timespec rqt;
   rqt.tv_sec = ms/1000u;
   rqt.tv_nsec = (ms%1000u)*1000000u;
   ::nanosleep(&rqt, 0);
}

//systemwide thread
inline OS_systemwide_thread_id_t get_current_systemwide_thread_id()
{
   return OS_systemwide_thread_id_t(::getpid(), ::pthread_self());
}

inline bool equal_systemwide_thread_id(const OS_systemwide_thread_id_t &id1, const OS_systemwide_thread_id_t &id2)
{
   return (0 != pthread_equal(id1.tid, id2.tid)) && (id1.pid == id2.pid);
}

inline OS_systemwide_thread_id_t get_invalid_systemwide_thread_id()
{
   return OS_systemwide_thread_id_t(get_invalid_process_id(), get_invalid_thread_id());
}

inline long double get_current_process_creation_time()
{ return 0.0L; }

inline unsigned int get_num_cores()
{
   #ifdef _SC_NPROCESSORS_ONLN
      long cores = ::sysconf(_SC_NPROCESSORS_ONLN);
      // sysconf returns -1 if the name is invalid, the option does not exist or
      // does not have a definite limit.
      // if sysconf returns some other negative number, we have no idea
      // what is going on. Default to something safe.
      if(cores <= 0){
         return 1;
      }
      //Check for overflow (unlikely)
      else if(static_cast<unsigned long>(cores) >=
              static_cast<unsigned long>(static_cast<unsigned int>(-1))){
         return static_cast<unsigned int>(-1);
      }
      else{
         return static_cast<unsigned int>(cores);
      }
   #elif defined(BOOST_INTERPROCESS_BSD_DERIVATIVE) && defined(HW_NCPU)
      int request[2] = { CTL_HW, HW_NCPU };
      int num_cores;
      std::size_t result_len = sizeof(num_cores);
      if ( (::sysctl (request, 2, &num_cores, &result_len, 0, 0) < 0) || (num_cores <= 0) ){
         //Return a safe value
         return 1;
      }
      else{
         return static_cast<unsigned int>(num_cores);
      }
   #endif
}

#endif   //#if (defined BOOST_INTERPROCESS_WINDOWS)

typedef char pid_str_t[sizeof(OS_process_id_t)*3+1];

inline void get_pid_str(pid_str_t &pid_str, OS_process_id_t pid)
{
   bufferstream bstream(pid_str, sizeof(pid_str));
   bstream << pid << std::ends;
}

inline void get_pid_str(pid_str_t &pid_str)
{  get_pid_str(pid_str, get_current_process_id());  }

}  //namespace ipcdetail{
}  //namespace interprocess {
}  //namespace boost {

#include <boost/interprocess/detail/config_end.hpp>

#endif   //BOOST_INTERPROCESS_DETAIL_OS_THREAD_FUNCTIONS_HPP
