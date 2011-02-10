// copyright 2009, 2010 t. schneider tes@mit.edu
// 
// this file is part of libamac, a medium access control for
// acoustic networks. 
//
// see the readme file within this directory for information
// pertaining to usage and purpose of this script.
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

#include <iostream>
#include <cmath>

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include "goby/acomms/libdccl/dccl_constants.h"
#include "goby/util/logger.h"
#include "goby/acomms/acomms_helpers.h"

#include "mac_manager.h"

using goby::util::goby_time;
using goby::util::as;
using namespace goby::util::tcolor;

goby::acomms::MACManager::MACManager(std::ostream* log /* =0 */)
    : log_(log),
      timer_(io_),
      timer_is_running_(false),
      current_slot_(slot_order_.begin()),
      startup_done_(false)
{ }

goby::acomms::MACManager::~MACManager()
{ }

void goby::acomms::MACManager::do_work()
{    
    // let the io service execute ready handlers (in this case, is the timer up?)
    if(timer_is_running_) io_.poll();
}

void goby::acomms::MACManager::restart_timer()
{    
    // cancel any old timer jobs waiting
    timer_.cancel();
    timer_.expires_at(next_slot_t_);
    timer_.async_wait(boost::bind(&MACManager::send_poll, this, _1));
    timer_is_running_ = true;
}

void goby::acomms::MACManager::stop_timer()
{
    timer_is_running_ = false;
    timer_.cancel();
}

void goby::acomms::MACManager::startup(const protobuf::MACConfig& cfg)
{
    if(startup_done_)
    {
        if(log_) *log_ << warn << group("mm_out") << "startup() called but driver is already started." << std::endl;
        return;
    }
    
    // create a copy for us
    cfg_ = cfg;
    
    switch(cfg_.type())
    {
        case protobuf::MAC_AUTO_DECENTRALIZED:
        {
            if(log_) *log_ << group("mac")
                           << "Using the Decentralized Slotted TDMA MAC scheme with autodiscovery"
                           << std::endl;

            
            protobuf::Slot blank_slot;
            blank_slot.set_src(acomms::BROADCAST_ID);
            blank_slot.set_dest(acomms::QUERY_DESTINATION_ID);
            blank_slot.set_rate(cfg_.rate());
            blank_slot.set_type(protobuf::SLOT_DATA);
            blank_slot.set_slot_seconds(cfg_.slot_seconds());
            blank_slot.set_last_heard_time(as<std::string>(goby_time()));
            blank_it_ = add_slot(blank_slot);
            

            protobuf::Slot our_slot;
            our_slot.set_src(cfg_.modem_id());
            our_slot.set_dest(acomms::QUERY_DESTINATION_ID);
            our_slot.set_rate(cfg_.rate());
            our_slot.set_type(protobuf::SLOT_DATA);
            our_slot.set_slot_seconds(cfg_.slot_seconds());
            our_slot.set_last_heard_time(as<std::string>(goby_time()));

            add_slot(our_slot);
            
            slot_order_.sort();

            next_slot_t_ = next_cycle_time();
            position_blank();
            
            break;
        }
        
        case protobuf::MAC_POLLED:
        case protobuf::MAC_FIXED_DECENTRALIZED:
            for(int i = 0, n = cfg_.cycle_size(); i < n; ++i)
                add_slot(cfg_.cycle(i));
            
            if(log_ && cfg_.type() == protobuf::MAC_POLLED)
                *log_ << group("mac") << "Using the Centralized Polling MAC scheme" << std::endl;
            else if(log_ && cfg_.type() == protobuf::MAC_FIXED_DECENTRALIZED)
                *log_ << group("mac") << "Using the Decentralized (Fixed) Slotted TDMA MAC scheme" << std::endl;

            break;

        default:
            return;
    }

    if(log_) *log_ << group("mac")
                 << "the MAC TDMA first cycle begins at time: "
                 << next_slot_t_ << std::endl;
    
    
    if(!slot_order_.empty())
        restart_timer();

    startup_done_ = true;
}

void goby::acomms::MACManager::shutdown()
{
    stop_timer();
    
    slot_order_.clear();
    id2slot_.clear();
    current_slot_ = slot_order_.begin();
    startup_done_ = false;
}


void goby::acomms::MACManager::send_poll(const boost::system::error_code& e)
{    
    // canceled the last timer
    if(e == boost::asio::error::operation_aborted) return;   
    
    const protobuf::Slot& s = (*current_slot_)->second;
    
    bool send_poll = true;    
    switch(cfg_.type())
    {
        case protobuf::MAC_FIXED_DECENTRALIZED:
        case protobuf::MAC_AUTO_DECENTRALIZED:
            send_poll = (s.src() == cfg_.modem_id());
            break;

        case protobuf::MAC_POLLED:
            // be quiet in the case where src = 0
            send_poll = (s.src() != BROADCAST_ID);
            break;

        default:
            break;
    }

    if(log_)
    {
        *log_ << group("mac") << "cycle order: [";
    
        BOOST_FOREACH(id2slot_it it, slot_order_)
        {
            if(it==(*current_slot_))
                *log_ << " " << green;
            
            switch(s.type())
            {
                case protobuf::SLOT_DATA: *log_ << "d"; break;
                case protobuf::SLOT_PING: *log_ << "p"; break;
                case protobuf::SLOT_REMUS_LBL: *log_ << "r"; break; 
            }

            *log_ << it->second.src() << "/" << it->second.dest() << "@" << it->second.rate() << " " << nocolor;
        }

        *log_ << " ]" << std::endl;

        *log_ << group("mac") << "starting slot: " << s << std::endl;
    }

    
    if(send_poll)
    {
        switch(s.type())
        {
            case protobuf::SLOT_DATA:
            {
                protobuf::ModemMsgBase m;
                m.set_src(s.src());
                m.set_dest(s.dest());
                m.set_rate(s.rate());
                signal_initiate_transmission(&m);
                break;
            }
            
            case protobuf::SLOT_REMUS_LBL:
            case protobuf::SLOT_PING:
            {
                protobuf::ModemRangingRequest m;
                m.mutable_base()->set_src(s.src());
                m.mutable_base()->set_dest(s.dest());

                if(s.type() == protobuf::SLOT_REMUS_LBL)
                    m.set_type(protobuf::REMUS_LBL_RANGING);
                else if(s.type() == protobuf::SLOT_PING)
                    m.set_type(protobuf::MODEM_TWO_WAY_PING);
                
                signal_initiate_ranging(&m);
                break;
            }            
            default:
                break;
        }
    }
    
    ++current_slot_;
    
    switch(cfg_.type())
    {
        case protobuf::MAC_AUTO_DECENTRALIZED:
            expire_ids();

            if (current_slot_ == slot_order_.end())
            {
                ++cycles_since_day_start_;
                if(log_) *log_ << group("mac") << "cycles since day start: "
                             << cycles_since_day_start_ << std::endl;    
                position_blank();
            }
            next_slot_t_ += boost::posix_time::seconds(cfg_.slot_seconds());
            break;
            
        case protobuf::MAC_FIXED_DECENTRALIZED:
        case protobuf::MAC_POLLED:
            if (current_slot_ == slot_order_.end()) current_slot_ = slot_order_.begin();
            next_slot_t_ += boost::posix_time::seconds(s.slot_seconds());
            break;

        default:
            break;
 
    }
    
    restart_timer();
}

boost::posix_time::ptime goby::acomms::MACManager::next_cycle_time()
{
    using namespace boost::gregorian;
    using namespace boost::posix_time;

    int since_day_start = goby_time().time_of_day().total_seconds();
    cycles_since_day_start_ = (floor(since_day_start/cycle_length()) + 1);
    
    if(log_) *log_ << group("mac") << "cycles since day start: "
                 << cycles_since_day_start_ << std::endl;
    
    unsigned secs_to_next = cycles_since_day_start_*cycle_length();

    
    // day start plus the next cycle starting from now
    return ptime(day_clock::universal_day(), seconds(secs_to_next));
}

void goby::acomms::MACManager::handle_modem_all_incoming(const protobuf::ModemMsgBase& m)
{
    unsigned id = m.src();
    
    if(cfg_.type() != protobuf::MAC_AUTO_DECENTRALIZED)
        return;
    

// if we haven't heard from this id before we have to reset (since the cycle changed
    bool new_id = !id2slot_.count(id);
    
    if(new_id)
    {
        if(log_) *log_ << group("mac") << "discovered id " << id << std::endl;
        
        protobuf::Slot new_slot;
        
        new_slot.set_src(id);
        new_slot.set_dest(acomms::QUERY_DESTINATION_ID);
        new_slot.set_rate(cfg_.rate());
        new_slot.set_type(protobuf::SLOT_DATA);
        new_slot.set_slot_seconds(cfg_.slot_seconds());
        new_slot.set_last_heard_time(as<std::string>(goby_time()));

        slot_order_.push_back(id2slot_.insert(std::make_pair(id, new_slot)));

        slot_order_.sort();

        process_cycle_size_change();
    }
    else
    {
        std::pair<id2slot_it, id2slot_it> p = id2slot_.equal_range(id);
        for(id2slot_it it = p.first; it != p.second; ++it)
            it->second.set_last_heard_time(as<std::string>(goby_time()));
    }
}

void goby::acomms::MACManager::expire_ids()
{
    bool reset = false;
    
    for(id2slot_it it = id2slot_.begin(), n = id2slot_.end(); it != n; ++it)
    {
        if(as<boost::posix_time::ptime>(it->second.last_heard_time()) <
           goby_time()-boost::posix_time::seconds(cycle_length()*cfg_.expire_cycles())
           && it->first != cfg_.modem_id()
           && it->first != BROADCAST_ID)
        {
            if(log_) *log_ << group("mac") << "removed id " << it->first
                           << " after not hearing for " << cfg_.expire_cycles()
                           << " cycles." << std::endl;

            id2slot_.erase(it);
            slot_order_.remove(it);
            reset = true;
        }
    }

    if(reset) process_cycle_size_change();
}

void goby::acomms::MACManager::process_cycle_size_change()
{
    next_slot_t_ = next_cycle_time();
    if(log_) *log_ << group("mac") << "the MAC TDMA next cycle begins at time: "
                 << next_slot_t_ << std::endl;
    
    
    if(cfg_.type() == protobuf::MAC_AUTO_DECENTRALIZED && slot_order_.size() > 1)
        position_blank();
  
    restart_timer();
}


unsigned goby::acomms::MACManager::cycle_sum()
{
    unsigned s = 0;
    BOOST_FOREACH(id2slot_it it, slot_order_)
        s += it->second.src();
    return s;
}

unsigned goby::acomms::MACManager::cycle_length()
{
    unsigned length = 0;
    BOOST_FOREACH(const id2slot_it& it, slot_order_)
        length += it->second.slot_seconds();

    
    return length;
}
 

void goby::acomms::MACManager::position_blank()
{
    unsigned blank_pos = cycle_length() - ((cycles_since_day_start_ % ENTROPY) == (cycle_sum() % ENTROPY)) - 1;
    
    slot_order_.remove(blank_it_);
    
    std::list<id2slot_it>::iterator id_it = slot_order_.begin();
    for(unsigned i = 0; i < blank_pos; ++i)
        ++id_it;

    slot_order_.insert(id_it, blank_it_);
    
    current_slot_ = slot_order_.begin();
}

std::map<int, goby::acomms::protobuf::Slot>::iterator goby::acomms::MACManager::add_slot(const protobuf::Slot& s)    
{
    std::map<int, protobuf::Slot>::iterator it =
        id2slot_.insert(std::pair<int, protobuf::Slot>(s.src(), s));

    slot_order_.push_back(it);
    current_slot_ = slot_order_.begin();
    
    if(log_) *log_ << group("mac") << "added new slot " << s << std::endl;
    process_cycle_size_change();
    
    return it;
}

void goby::acomms::MACManager::add_flex_groups(util::FlexOstream* tout)
{
    tout->add_group("mac", util::Colors::blue, "MAC related messages (goby_amac)");
}


bool goby::acomms::MACManager::remove_slot(const protobuf::Slot& s)    
{
    bool removed_a_slot = false;
    
    for(id2slot_it it = id2slot_.begin(), n = id2slot_.end(); it != n; ++it)
    {
        if(s == it->second)
        {
            if(log_) *log_ << group("mac") << "removed slot " << it->second << std::endl;
            slot_order_.remove(it);
            id2slot_.erase(it);
            removed_a_slot = true;
            break;
        }
    }

    process_cycle_size_change();
    
    if(slot_order_.empty())
        stop_timer();

    if(removed_a_slot)
        current_slot_ = slot_order_.begin();
        
    return removed_a_slot;
}
