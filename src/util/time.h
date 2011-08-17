// copyright 2010 t. schneider tes@mit.edu
// 
// this file is a collection of time conversion utilities used in goby
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.

#ifndef Time20100713H
#define Time20100713H

#include <ctime>

#include <boost/date_time.hpp>
#include <boost/static_assert.hpp>

#include "goby/util/primitive_types.h"
#include "goby/util/as.h"

/// All objects related to the Goby Underwater Autonomy Project
namespace goby
{
    /// Utility objects for performing functions such as logging, non-acoustic communication (ethernet / serial), time, scientific, string manipulation, etc.
    namespace util
    {
        ///\name Time
        //@{        
        
        /// convert from boost date_time ptime to the number of seconds (including fractional) since 1/1/1970 0:00 UTC ("UNIX Time")
        double ptime2unix_double(boost::posix_time::ptime given_time);
    
        /// convert to boost date_time ptime from the number of seconds (including fractional) since 1/1/1970 0:00 UTC ("UNIX Time"): good to the microsecond
        boost::posix_time::ptime unix_double2ptime(double given_time);

        /// convert from boost date_time ptime to the number of microseconds since 1/1/1970 0:00 UTC ("UNIX Time")
        uint64 ptime2unix_microsec(boost::posix_time::ptime given_time);
    
        /// convert to boost date_time ptime from the number of microseconds since 1/1/1970 0:00 UTC ("UNIX Time"): good to the microsecond
        boost::posix_time::ptime unix_microsec2ptime(uint64 given_time);

        
        /// \brief specialization of `as` for double (assumed seconds since UNIX 1970-01-01 00:00:00 UTC) to ptime
        template<typename To, typename From>
            typename boost::enable_if<boost::mpl::and_< boost::is_same<To, double>, boost::is_same<From, boost::posix_time::ptime> >, To>::type
            as(const From& from)
        { return goby::util::ptime2unix_double(from); }

        /// \brief specialization of `as` for ptime to double (seconds since UNIX 1970-01-01 00:00:00 UTC)
        template<typename To, typename From>
            typename boost::enable_if<boost::mpl::and_< boost::is_same<To, boost::posix_time::ptime>, boost::is_same<From, double> >, To>::type
            as(const From& from)
        { return goby::util::unix_double2ptime(from); }


       /// \brief specialization of `as` for uint64 (assumed microseconds since UNIX 1970-01-01 00:00:00 UTC) to ptime
        template<typename To, typename From>
            typename boost::enable_if<boost::mpl::and_< boost::is_same<To, uint64>, boost::is_same<From, boost::posix_time::ptime> >, To>::type
            as(const From& from)
        { return goby::util::ptime2unix_microsec(from); }

        /// \brief specialization of `as` for ptime to uint64 (microseconds since UNIX 1970-01-01 00:00:00 UTC)
        template<typename To, typename From>
            typename boost::enable_if<boost::mpl::and_< boost::is_same<To, boost::posix_time::ptime>, boost::is_same<From, uint64> >, To>::type
            as(const From& from)
        { return goby::util::unix_microsec2ptime(from); }

        template<typename ReturnType>
            ReturnType goby_time()
        { BOOST_STATIC_ASSERT(sizeof(ReturnType) == 0); }

        template<>
            inline boost::posix_time::ptime goby_time<boost::posix_time::ptime>()
        { return boost::posix_time::microsec_clock::universal_time(); }

        inline boost::posix_time::ptime goby_time()
        { return goby_time<boost::posix_time::ptime>(); }
        
        template<> inline double goby_time<double>()
        { return as<double>(goby_time<boost::posix_time::ptime>()); }

        template<> inline uint64 goby_time<uint64>()
        { return as<uint64>(goby_time<boost::posix_time::ptime>()); }
        
        
        /// Simple string representation of goby_time()
        inline std::string goby_time_as_string(const boost::posix_time::ptime& t = goby_time())
        { return as<std::string>(t); }
        
        /// ISO string representation of goby_time()
        inline std::string goby_file_timestamp()
        {
            using namespace boost::posix_time;
            return to_iso_string(second_clock::universal_time());
        }
    
        /// convert to ptime from time_t from time.h (whole seconds since UNIX)
        inline boost::posix_time::ptime time_t2ptime(std::time_t t)
        { return boost::posix_time::from_time_t(t); }
    
        /// convert from ptime to time_t from time.h (whole seconds since UNIX)
        inline std::time_t ptime2time_t(boost::posix_time::ptime t)
        {
            std::tm out = boost::posix_time::to_tm(t);
            return mktime(&out);
        }
        
        /// time duration to double number of seconds: good to the microsecond
        inline double time_duration2double(boost::posix_time::time_duration time_of_day)
        {
            using namespace boost::posix_time;
            return (double(time_of_day.total_seconds()) + double(time_of_day.fractional_seconds()) / double(time_duration::ticks_per_second()));
        }


        
        //@}

    }
}
#endif
