// Copyright 2009-2013 Toby Schneider (https://launchpad.net/~tes)
//                     Massachusetts Institute of Technology (2007-)
//                     Woods Hole Oceanographic Institution (2007-)
//                     Goby Developers Team (https://launchpad.net/~goby-dev)
// 
//
// This file is part of the Goby Underwater Autonomy Project Binaries
// ("The Goby Binaries").
//
// The Goby Binaries are free software: you can redistribute them and/or modify
// them under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// The Goby Binaries are distributed in the hope that they will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Goby.  If not, see <http://www.gnu.org/licenses/>.

#include <dlfcn.h>

#include <boost/signals2.hpp>
#include <boost/algorithm/string.hpp>

#include "goby/common/logger.h"
#include "goby/acomms/connect.h"
#include "goby/common/time.h"

#include "iFrontSeat.h"

using namespace goby::common::logger;
namespace gpb = goby::moos::protobuf;
using goby::common::goby_time;
using goby::glog;

iFrontSeatConfig iFrontSeat::cfg_;
iFrontSeat* iFrontSeat::inst_ = 0;
void* iFrontSeat::driver_library_handle_= 0;

int main(int argc, char * argv[])
{
   // load plugin driver from environmental variable IFRONTSEAT_DRIVER_LIBRARY
    char * driver_lib_path = getenv ("IFRONTSEAT_DRIVER_LIBRARY");
    if (driver_lib_path)
    {
        std::cerr << "Loading iFrontSeat driver library: " << driver_lib_path << std::endl;
        iFrontSeat::driver_library_handle_ = dlopen(driver_lib_path, RTLD_LAZY);
        if(!iFrontSeat::driver_library_handle_)
        {
            std::cerr << "Failed to open library: " << driver_lib_path << std::endl;
            exit(EXIT_FAILURE);
        }
    }        
    else
    {
        std::cerr << "Environmental variable IFRONTSEAT_DRIVER_LIBRARY must be set with name of the dynamic library containing the specific driver to use." << std::endl;
        exit(EXIT_FAILURE);
    }
    
    return goby::moos::run<iFrontSeat>(argc, argv);
}

iFrontSeat* iFrontSeat::get_instance()
{
    if(!inst_)
        inst_ = new iFrontSeat();
    return inst_;
}

FrontSeatInterfaceBase* load_driver(iFrontSeatConfig* cfg)
{
    typedef FrontSeatInterfaceBase* (*driver_load_func)(iFrontSeatConfig*);
    driver_load_func driver_load_ptr = (driver_load_func) dlsym(iFrontSeat::driver_library_handle_,
                                                                "frontseat_driver_load");

    if(!driver_load_ptr)
        glog.is(DIE) && glog << "Function frontseat_driver_load in library defined in IFRONTSEAT_DRIVER_LIBRARY does not exist." <<  std::endl;

    FrontSeatInterfaceBase* driver = (*driver_load_ptr)(cfg);
    
    if(!driver)
        glog.is(DIE) && glog << "Function frontseat_driver_load in library defined in IFRONTSEAT_DRIVER_LIBRARY returned a null pointer." <<  std::endl;

    return driver;
}


iFrontSeat::iFrontSeat()
    : GobyMOOSApp(&cfg_),
      frontseat_(load_driver(&cfg_)),
      translator_(this),
      next_status_time_(goby_time<double>() + cfg_.status_period())
{

    
    // commands
    subscribe(cfg_.moos_var().prefix() + cfg_.moos_var().command_request(),
              &iFrontSeat::handle_mail_command_request, this);
    goby::acomms::connect(&frontseat_->signal_command_response,
                          this, &iFrontSeat::handle_driver_command_response);
    
    // data
    subscribe(cfg_.moos_var().prefix() + cfg_.moos_var().data_to_frontseat(),
              &iFrontSeat::handle_mail_data_to_frontseat, this);
    goby::acomms::connect(&frontseat_->signal_data_from_frontseat,
                          this, &iFrontSeat::handle_driver_data_from_frontseat);
    
    // raw
    subscribe(cfg_.moos_var().prefix() + cfg_.moos_var().raw_out(),
              &iFrontSeat::handle_mail_raw_out, this);
    goby::acomms::connect(&frontseat_->signal_raw_from_frontseat,
                          this, &iFrontSeat::handle_driver_raw_in);

    goby::acomms::connect(&frontseat_->signal_raw_to_frontseat,
                          this, &iFrontSeat::handle_driver_raw_out);

    
    // IvP Helm State
    subscribe("IVPHELM_STATE", &iFrontSeat::handle_mail_helm_state, this);

}

void iFrontSeat::loop()
{
    frontseat_->do_work();

    if(cfg_.status_period() &&
       goby_time<double>() > next_status_time_)
    {
        std::string serialized;
        serialize_for_moos(&serialized, frontseat_->status());
        glog.is(DEBUG1) && glog << "Status: " << serialized << std::endl;
        publish(cfg_.moos_var().prefix() + cfg_.moos_var().status(), serialized);
        next_status_time_ += cfg_.status_period();
    }

    if(cfg_.exit_on_error() &&
       (frontseat_->state() == gpb::INTERFACE_FS_ERROR ||
        frontseat_->state() == gpb::INTERFACE_HELM_ERROR))
    {
        glog.is(DIE) && glog << "Error state detected and `exit_on_error` == true, so quitting. Bye!" << std::endl;
    }   
    
}


void iFrontSeat::handle_mail_command_request(const CMOOSMsg& msg)
{
    if(frontseat_->state() != gpb::INTERFACE_COMMAND)
    {
        glog.is(DEBUG1) && glog << "Not sending command because the interface is not in the command state" << std::endl;
    }
    else
    {
        gpb::CommandRequest command;
        parse_for_moos(msg.GetString(), &command);
        frontseat_->send_command_to_frontseat(command);
    }
}

void iFrontSeat::handle_mail_data_to_frontseat(const CMOOSMsg& msg)
{
    if(frontseat_->state() != gpb::INTERFACE_COMMAND &&
       frontseat_->state() != gpb::INTERFACE_LISTEN)
    {
        glog.is(DEBUG1) && glog << "Not sending data because the interface is not in the command or listen state" << std::endl;
    }
    else
    {
        gpb::FrontSeatInterfaceData data;
        parse_for_moos(msg.GetString(), &data);
        frontseat_->send_data_to_frontseat(data);
    }
}

void iFrontSeat::handle_mail_raw_out(const CMOOSMsg& msg)
{
    // no recursively sending our own messages
    if(msg.GetSource() == GetAppName())
        return;
    
    
    if(frontseat_->state() != gpb::INTERFACE_COMMAND &&
       frontseat_->state() != gpb::INTERFACE_LISTEN)
    {
        glog.is(DEBUG1) && glog << "Not sending raw because the interface is not in the command or listen state" << std::endl;
    }
    else
    {
        gpb::FrontSeatRaw raw;
        parse_for_moos(msg.GetString(), &raw);
        frontseat_->send_raw_to_frontseat(raw);
    }
    
}

void iFrontSeat::handle_mail_helm_state(const CMOOSMsg& msg)
{
    std::string sval = msg.GetString();
    boost::trim(sval);
    if(boost::iequals(sval, "drive"))
        frontseat_->set_helm_state(gpb::HELM_DRIVE);
    else if(boost::iequals(sval, "park"))
        frontseat_->set_helm_state(gpb::HELM_PARK);
    else
        frontseat_->set_helm_state(gpb::HELM_NOT_RUNNING);
}

void iFrontSeat::handle_driver_command_response(const goby::moos::protobuf::CommandResponse& response)
{
    std::string serialized;
    serialize_for_moos(&serialized, response);
    publish(cfg_.moos_var().prefix() + cfg_.moos_var().command_response(), serialized);
}

void iFrontSeat::handle_driver_data_from_frontseat(const goby::moos::protobuf::FrontSeatInterfaceData& data)
{
    std::string serialized;
    serialize_for_moos(&serialized, data);
    publish(cfg_.moos_var().prefix() + cfg_.moos_var().data_from_frontseat(), serialized);
}

void iFrontSeat::handle_driver_raw_in(const goby::moos::protobuf::FrontSeatRaw& data)
{
    std::string serialized;
    serialize_for_moos(&serialized, data);
    publish(cfg_.moos_var().prefix() + cfg_.moos_var().raw_in(), serialized);
}

void iFrontSeat::handle_driver_raw_out(const goby::moos::protobuf::FrontSeatRaw& data)
{
    std::string serialized;
    serialize_for_moos(&serialized, data);
    publish(cfg_.moos_var().prefix() + cfg_.moos_var().raw_out(), serialized);
}
