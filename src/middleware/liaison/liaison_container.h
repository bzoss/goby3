// Copyright 2009-2018 Toby Schneider (http://gobysoft.org/index.wt/people/toby)
//                     GobySoft, LLC (2013-)
//                     Massachusetts Institute of Technology (2007-2014)
//                     Community contributors (see AUTHORS file)
//
//
// This file is part of the Goby Underwater Autonomy Project Libraries
// ("The Goby Libraries").
//
// The Goby Libraries are free software: you can redistribute them and/or modify
// them under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2.1 of the License, or
// (at your option) any later version.
//
// The Goby Libraries are distributed in the hope that they will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Goby.  If not, see <http://www.gnu.org/licenses/>.

#ifndef LIAISONCONTAINER20130128H
#define LIAISONCONTAINER20130128H

#include <Wt/WContainerWidget>
#include <Wt/WText>
#include <Wt/WColor>

#include "goby/middleware/protobuf/liaison_config.pb.h"
#include "goby/middleware/group.h"
#include "goby/middleware/multi-thread-application.h"


namespace goby
{
    namespace common
    {
            
        enum 
        {
            LIAISON_INTERNAL_PUBLISH_SOCKET = 1,
            LIAISON_INTERNAL_SUBSCRIBE_SOCKET = 2
//            LIAISON_INTERNAL_COMMANDER_SUBSCRIBE_SOCKET = 3,
//            LIAISON_INTERNAL_COMMANDER_PUBLISH_SOCKET = 4,
//            LIAISON_INTERNAL_SCOPE_SUBSCRIBE_SOCKET = 5,
//            LIAISON_INTERNAL_SCOPE_PUBLISH_SOCKET = 6,  
        };

        const Wt::WColor goby_blue(28,159,203);
        const Wt::WColor goby_orange(227,96,52);
        
        inline std::string liaison_internal_publish_socket_name() { return "liaison_internal_publish_socket"; }
        inline std::string liaison_internal_subscribe_socket_name() { return "liaison_internal_subscribe_socket"; }
        
        class LiaisonContainer : public Wt::WContainerWidget
        {
        public:
            LiaisonContainer()
            {
                setStyleClass("fill");
                /* addWidget(new Wt::WText("<hr/>")); */
                /* addWidget(name_); */
                /* addWidget(new Wt::WText("<hr/>")); */
            }
            
            virtual ~LiaisonContainer()
            { }  
            
            void set_name(const Wt::WString& name)
            {
                name_.setText(name);
            }

            const Wt::WString& name() { return name_.text(); }
            
            virtual void focus() { }
            virtual void unfocus() { }
            virtual void cleanup() { }            

            
          private:
            Wt::WText name_;
        };        

        template<typename Derived, typename GobyThread>
            class LiaisonContainerWithComms : public LiaisonContainer
        {
        public:
        LiaisonContainerWithComms(const protobuf::LiaisonConfig& cfg)
            {
                static std::atomic<int> index(0);
                index_ = index++;
                
                // copy configuration 
                auto thread_lambda = [this, cfg]()
                    {
                        {
                            std::lock_guard<std::mutex> l(goby_thread_mutex);        
                            goby_thread_ = std::make_unique<GobyThread>(static_cast<Derived*>(this), cfg, index_);
                        }
                        
                        try { goby_thread_->run(thread_alive_); }
                        catch(...) { thread_exception_ = std::current_exception(); }

                        {
                            std::lock_guard<std::mutex> l(goby_thread_mutex);        
                            goby_thread_.reset();
                        }
                    };

                thread_ = std::unique_ptr<std::thread>(new std::thread(thread_lambda));

                // wait for thread to be created
                while(goby_thread() == nullptr)
                    usleep(1000);
            }
            
            virtual ~LiaisonContainerWithComms()
            {
                thread_alive_ = false;
                thread_->join();
                
                if(thread_exception_)
                {
                    goby::glog.is_warn() && goby::glog << "Comms thread had an uncaught exception" << std::endl;
                    std::rethrow_exception(thread_exception_);
                }
                
            }

            void post_to_wt(std::function<void()> func)
            {
                std::lock_guard<std::mutex> l(comms_to_wt_mutex);
                comms_to_wt_queue.push(func);
            }

            void process_from_wt()
            {
                std::lock_guard<std::mutex> l(wt_to_comms_mutex);                
                while(!wt_to_comms_queue.empty())
                {
                    wt_to_comms_queue.front()();
                    wt_to_comms_queue.pop();
                }
            }
            
            
        protected:
            GobyThread* goby_thread() 
            {
                std::lock_guard<std::mutex> l(goby_thread_mutex);
                return goby_thread_.get();
            }

            void post_to_comms(std::function<void()> func)
            {
                std::lock_guard<std::mutex> l(wt_to_comms_mutex);                
                wt_to_comms_queue.push(func);
            }
            
            void process_from_comms()
            {
                std::lock_guard<std::mutex> l(comms_to_wt_mutex);                
                while(!comms_to_wt_queue.empty())
                {
                    comms_to_wt_queue.front()();
                    comms_to_wt_queue.pop();
                }
            }

            
          private:

            // for comms
            std::mutex comms_to_wt_mutex;
            std::queue<std::function<void ()>> comms_to_wt_queue;
            std::mutex wt_to_comms_mutex;
            std::queue<std::function<void ()>> wt_to_comms_queue;

            // only protects the unique_ptr, not the underlying thread
            std::mutex goby_thread_mutex;
            std::unique_ptr<GobyThread> goby_thread_ { nullptr };

            int index_;
            std::unique_ptr<std::thread> thread_;
            std::atomic<bool> thread_alive_ {true};
            std::exception_ptr thread_exception_;

            
        };        

        
    }
}
#endif
