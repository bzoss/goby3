// Copyright 2010-2020:
//   GobySoft, LLC (2013-)
//   Massachusetts Institute of Technology (2007-2014)
//   Community contributors (see AUTHORS file)
// File authors:
//   Toby Schneider <toby@gobysoft.org>
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

#ifndef GOBY_UTIL_LINEBASEDCOMMS_TCP_SERVER_H
#define GOBY_UTIL_LINEBASEDCOMMS_TCP_SERVER_H

#include <iostream> // for ios_base::fai...
#include <map>      // for map
#include <memory>   // for shared_ptr
#include <string>   // for string, basic...

#include <boost/asio/basic_socket.hpp>          // for basic_socket<...
#include <boost/asio/basic_socket_acceptor.hpp> // for basic_socket_...
#include <boost/asio/ip/basic_endpoint.hpp>     // for operator<<
#include <boost/asio/ip/tcp.hpp>                // for tcp::socket, tcp
#include <boost/asio/read_until.hpp>
#include <boost/bind.hpp>                              // for bind_t, list_...
#include <boost/lexical_cast/bad_lexical_cast.hpp>     // for bad_lexical_cast
#include <boost/smart_ptr/enable_shared_from_this.hpp> // for enable_shared...
#include <boost/system/error_code.hpp>                 // for error_code

#include "goby/util/as.h" // for as
#include "goby/util/asio_compat.h"
#include "goby/util/protobuf/linebasedcomms.pb.h" // for Datagram

//#include "connection.h" // for LineBasedConn...
#include "interface.h" // for LineBasedInte...

namespace goby
{
namespace util
{
class TCPConnection;

/// provides a basic TCP server for line by line text based communications to a one or more remote TCP clients
class TCPServer : public LineBasedInterface
{
  public:
    /// \brief create a TCP server
    ///
    /// \param port port of the server (use 50000+ to avoid problems with special system ports)
    /// \param delimiter string used to split lines
    TCPServer(unsigned port, const std::string& delimiter = "\r\n")
        : LineBasedInterface(delimiter),
          acceptor_(io_context(), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    {
    }
    ~TCPServer() override = default;

    typedef std::string Endpoint;
    void close(const Endpoint& endpoint)
    {
        io_.post(boost::bind(&TCPServer::do_close, this, endpoint));
    }

    /// \brief string representation of the local endpoint (e.g. 192.168.1.105:54230
    std::string local_endpoint() override
    {
        return goby::util::as<std::string>(acceptor_.local_endpoint());
    }

    const std::map<Endpoint, std::shared_ptr<TCPConnection>>& connections();

    friend class TCPConnection;
    //    friend class LineBasedConnection<boost::asio::ip::tcp::socket>;

  private:
    void do_start() override
    {
        start_accept();
        set_active(true);
    }

    void do_close() override { do_close(""); }
    void do_close(const Endpoint& endpt);

  private:
    void start_accept();
    void handle_accept(const std::shared_ptr<TCPConnection>& new_connection,
                       const boost::system::error_code& error);

  private:
    std::string server_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<TCPConnection> new_connection_;
    std::map<Endpoint, std::shared_ptr<TCPConnection>> connections_;
};

class TCPConnection : public boost::enable_shared_from_this<TCPConnection>
//                      public LineBasedConnection<boost::asio::ip::tcp::socket>
{
  public:
    static std::shared_ptr<TCPConnection> create(LineBasedInterface* interface);

    //    boost::asio::ip::tcp::socket& socket() override { return socket_; }

    void start()
    {
        //#ifdef USE_BOOST_IO_SERVICE
        //        socket_.get_io_service().post(boost::bind(&TCPConnection::read_start, this));
        //#else
        //boost::asio::post(socket_.get_executor(), boost::bind(&TCPConnection::read_start, this));
        //#endif
    }

    void write(const protobuf::Datagram& msg)
    {
#ifdef USE_BOOST_IO_SERVICE
        socket_.get_io_service().post(boost::bind(&TCPConnection::socket_write, this, msg));
#else
        boost::asio::post(socket_.get_executor(),
                          boost::bind(&TCPConnection::socket_write, this, msg));
#endif
    }

    void close()
    {
        // #ifdef USE_BOOST_IO_SERVICE
        //         socket_.get_io_service().post(boost::bind(&TCPConnection::socket_close, this, error));
        // #else
        //         boost::asio::post(socket_.get_executor(),
        //                           boost::bind(&TCPConnection::socket_close, this, error));
        // #endif
    }

    std::string local_endpoint() { return goby::util::as<std::string>(socket_.local_endpoint()); }
    std::string remote_endpoint() { return goby::util::as<std::string>(socket_.remote_endpoint()); }

  private:
    void socket_write(const protobuf::Datagram& line);
    //    void socket_close(const boost::system::error_code& error) override;

    TCPConnection(LineBasedInterface* interface)
        : //LineBasedConnection<boost::asio::ip::tcp::socket>(interface),
          socket_(interface->io_context())
    {
    }

  private:
    boost::asio::ip::tcp::socket socket_;
};
} // namespace util

} // namespace goby

#endif
