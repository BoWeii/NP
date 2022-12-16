//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <cstdlib>
#include <iostream>
#include <memory>
#include <fstream>
#include <utility>
#include <boost/asio.hpp>
#include <vector>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <unordered_map>
#include <stdlib.h>
#include <sys/wait.h>
#include <boost/asio/signal_set.hpp>
#include <vector>

using boost::asio::ip::tcp;
using namespace std;

#define REPLY_LENGTH 8
#define REQEUST_LENGTH 9
#define REQUEST_GRANTED 90
#define REQUEST_REJECTED 91

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;

#define ALL_IP -1
struct SOCKS4_REPLY
{
    BYTE vn;
    BYTE cd;
    WORD dst_port;
    DWORD dst_ip;
};

static DWORD ip_to_dword(string ip)
{
    struct sockaddr_in sa;
    inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr));
    return sa.sin_addr.s_addr;
}
static WORD int_to_port(int port)
{
    return ((port & 0xff00) >> 8) | ((port & 0xff) << 8);
}
class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket, boost::asio::io_context &io_context)
        : io_context_(io_context),
          client_socket_(std::move(socket)),
          server_socket_(io_context),
          resolver_(boost::asio::make_strand(io_context))
    {
    }

    void start()
    {
        do_parse_request();
    }

private:
    void do_parse_request()
    {
        auto self(shared_from_this());
        client_socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                       [this, self](boost::system::error_code ec, std::size_t length)
                                       {
                                           if (!ec)
                                           {
                                               if (length < REQEUST_LENGTH)
                                               {
                                                   cerr << "[do_parse_request X] the length is not less than 9\n";
                                               }

                                               BYTE vn = data_[0];

                                               if (vn != 4)
                                               {
                                                   cerr << "[do_parse_request X] the vn is not equal to 4\n";
                                               }

                                               cd_ = data_[1];

                                               // swap DSTPORT value from big-endian to little-endian
                                               data_[2] ^= data_[3];
                                               data_[3] ^= data_[2];
                                               data_[2] ^= data_[3];

                                               WORD dst_port = *((WORD *)&data_[2]);
                                               char port_array[0x8] = {0};

                                               DWORD dst_IP = *((DWORD *)&data_[4]);
                                               char *user_id = &data_[8];

                                               sprintf(port_array, "%d", dst_port);

                                               if ((dst_IP & 0x00ffffff) == 0)
                                               {
                                                   // SOCKS 4A : DSTIP should be 0.0.0.x with nonzero x

                                                   int domain_name_start = 8 + strlen(user_id) + 1; // 1 for null byte
                                                   if (domain_name_start > (int)length)
                                                   {
                                                       cerr << "[do_parse_request x] wrong user_id\n";
                                                       return;
                                                   }

                                                   char *domain_name = &data_[domain_name_start];
                                                   do_resolve(domain_name, port_array);
                                               }
                                               else
                                               {
                                                   // SOCKS 4

                                                   // translate dst_ip to string "xxx.xxx.xxx.xxx"
                                                   char dstip_arr[0x10] = {0};
                                                   sprintf(dstip_arr, "%d.%d.%d.%d",
                                                           (dst_IP)&0xff,
                                                           (dst_IP >> 0x8) & 0xff,
                                                           (dst_IP >> 0x10) & 0xff,
                                                           (dst_IP >> 0x18) & 0xff);
                                                   do_resolve(dstip_arr, port_array);
                                               }
                                           }
                                       });
    }
    int check_firewall()
    {
        string file_name = "./socks.conf";
        ifstream file(file_name);

        if (file.fail())
        {
            cerr << "[firewall x] fail to open the " << file_name << endl;
            return -1;
        }

        if (file.is_open())
        {
            string line;
            while (getline(file, line))
            {
                vector<string> params;
                vector<string> ips;
                boost::algorithm::trim(line);
                if (!line.size())
                {
                    continue;
                }

                // parse the file
                //  <action> <cmd> <ip>
                boost::split(params, line, boost::is_any_of(" "), boost::token_compress_on);
                if (params.size() != 3 || params[0] != "permit" || params[1].size() != 1)
                {
                    return -1;
                }

                int cmd = -1;
                switch (params[1][0])
                {
                case 'c':
                    cmd = 1;
                    break;
                case 'b':
                    cmd = 2;
                    break;
                default:
                    return -1;
                }
                // <ip>= "xxx.xxx.xxx.xxx"
                boost::split(ips, params[2], boost::is_any_of("."), boost::token_compress_on);

                if (ips.size() != 4)
                {
                    return -1;
                }

                if (cmd != cd_)
                {
                    continue;
                }

                int config_ip[4];
                for (int i = 0; i < 4; i++)
                {
                    try
                    {
                        config_ip[i] = boost::lexical_cast<int>(ips[i]);
                        if (config_ip[i] < 0 || config_ip[i] > 255)
                        {
                            return -1;
                        }
                    }
                    catch (std::exception &e)
                    {
                        if (ips[i].size() == 1 && ips[i][0] == '*')
                        {
                            config_ip[i] = ALL_IP;
                        }
                    }
                }

                int dst_ip[4];
                boost::split(ips, server_endpoint_.address().to_string(), boost::is_any_of("."), boost::token_compress_on);
                for (int i = 0; i < 4; ++i)
                {
                    dst_ip[i] = boost::lexical_cast<int>(ips[i]);
                }

                int idx;
                for (idx = 0; idx < 4; idx++)
                {
                    if (config_ip[idx] == ALL_IP)
                    {
                        continue;
                    }
                    if (config_ip[idx] != dst_ip[idx])
                    {
                        break;
                    }
                }

                if (idx == 4)
                {
                    return 0;
                }
            }
            file.close();
        }
        return -1;
    }
    void do_reply(int status, WORD dst_port, DWORD dst_IP)
    {
        auto self(shared_from_this());

        SOCKS4_REPLY reply;

        reply.vn = 0;
        reply.cd = status ? REQUEST_GRANTED : REQUEST_REJECTED;
        reply.dst_ip = dst_IP;
        reply.dst_port = dst_port;

        cout << "<S_IP>: " << client_socket_.remote_endpoint().address().to_string() << endl;
        cout << "<S_PORT>: " << client_socket_.remote_endpoint().port() << endl;
        cout << "<D_IP>: " << server_endpoint_.address().to_string() << endl;
        cout << "<D_PORT>: " << server_endpoint_.port() << endl;
        if (cd_ == 1)
        {
            cout << "<Command>: CONNECT" << endl;
        }
        else if (cd_ == 2)
        {
            cout << "<Command>: BIND" << endl;
        }
        if (status)
        {
            cout << "<Reply>: Accept" << endl;
        }
        else
        {
            cout << "<Reply>: Reject" << endl;
        }

        boost::asio::async_write(client_socket_, boost::asio::buffer((char *)&reply, sizeof(reply)),
                                 [this, self, status](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         if (status)
                                         {
                                             if (cd_ == 1)
                                             {
                                                 // Start relaying traffic on both directions
                                                 do_client_read();
                                                 do_server_read();
                                             }
                                             else if (cd_ == 2)
                                             {
                                                 reply_cnt_++;

                                                 if (reply_cnt_ == 2)
                                                 {
                                                     do_client_read();
                                                     do_server_read();
                                                 }
                                             }
                                         }
                                     }
                                 });
    }
    void do_connect()
    {
        auto self(shared_from_this());
        server_socket_.async_connect(
            server_endpoint_,
            [this, self](boost::system::error_code ec)
            {
                if (!ec)
                {
                    do_reply(1, 0, 0);
                }
                else
                {
                    do_reply(0, 0, 0);
                }
            });
    }
    void do_bind()
    {
        auto self(shared_from_this());
        int port = 9487;
        reply_cnt_ = 0;

        DWORD client_ip = ip_to_dword(client_socket_.local_endpoint().address().to_string());

        while (true)
        {
            try
            {
                // Bind and listen a port
                acceptor_ptr_ = new tcp::acceptor(io_context_, tcp::endpoint(tcp::v4(), port));
                break;
            }
            catch (std::exception &e)
            {
                port++;
            }
        }

        // Send SOCKS4 REPLY to SOCKS client to tell which port to connect
        do_reply(1, int_to_port(port), client_ip);

        acceptor_ptr_->async_accept(
            [this, self, port, client_ip](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    if (server_endpoint_.address().to_string() != socket.remote_endpoint().address().to_string())
                    {
                        return;
                    }

                    server_socket_ = tcp::socket(std::move(socket));

                    // Accept connection from destination and send another SOCKS4 REPLY to SOCKS client
                    do_reply(1, int_to_port(port), client_ip);
                }
            });
    }
    void do_client_read()
    {
        auto self(shared_from_this());
        client_socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                       [this, self](boost::system::error_code ec, std::size_t length)
                                       {
                                           if (!ec)
                                           {
                                               do_server_write(length);
                                           }
                                           else
                                           {
                                               server_socket_.close();
                                           }
                                       });
    }
    void do_client_write(int length)
    {
        auto self(shared_from_this());

        boost::asio::async_write(client_socket_, boost::asio::buffer(data2_, length),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     do_server_read();
                                 });
    }
    void do_server_read()
    {
        auto self(shared_from_this());
        server_socket_.async_read_some(boost::asio::buffer(data2_, max_length),
                                       [this, self](boost::system::error_code ec, std::size_t length)
                                       {
                                           if (!ec)
                                           {
                                               do_client_write(length);
                                           }
                                           else
                                           {
                                               client_socket_.close();
                                           }
                                       });
    }

    void do_server_write(int length)
    {
        auto self(shared_from_this());

        boost::asio::async_write(server_socket_, boost::asio::buffer(data_, length),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     do_client_read();
                                 });
    }
    void do_resolve(string host_name, string port)
    {
        auto self(shared_from_this());
        resolver_.async_resolve(string(host_name),
                                string(port),
                                [this, self](boost::system::error_code ec, tcp::resolver::results_type endpoints)
                                {
                                    if (!ec)
                                    {
                                        for (auto it = endpoints.cbegin(); it != endpoints.cend(); it++)
                                        {
                                            server_endpoint_ = *it;
                                            break;
                                        }

                                        int status = check_firewall();
                                        if (status == -1)
                                        {
                                            // rejected
                                            do_reply(0, 0, 0);
                                            return;
                                        }

                                        if (cd_ == 1)
                                        {
                                            // CONNECT
                                            do_connect();
                                            return;
                                        }
                                        else if (cd_ == 2)
                                        {
                                            // BIND
                                            do_bind();
                                        }
                                    }
                                    else
                                    {
                                        do_reply(0, 0, 0);
                                    }
                                });
    }
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
    char data2_[max_length];
    boost::asio::io_context &io_context_;
    string uri, method;
    BYTE cd_;

    tcp::socket client_socket_;
    tcp::socket server_socket_;
    tcp::resolver resolver_;
    tcp::endpoint server_endpoint_;
    int reply_cnt_;
    tcp::acceptor *acceptor_ptr_;
};

class server
{
public:
    server(boost::asio::io_context &io_context, short port)
        : io_context_(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
          signal_(io_context, SIGCHLD)

    {
        wait_for_signal();
        do_accept();
    }

private:
    void wait_for_signal()
    {
        signal_.async_wait(
            [this](boost::system::error_code /*ec*/, int /*signo*/)
            {
                if (acceptor_.is_open())
                {
                    int status = 0;
                    while (waitpid(-1, &status, WNOHANG) > 0)
                    {
                    }

                    wait_for_signal();
                }
            });
    }
    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    pid_t pid;

                    io_context_.notify_fork(boost::asio::io_context::fork_prepare);
                    if ((pid = fork()))
                    {
                        // Parent

                        io_context_.notify_fork(boost::asio::io_context::fork_parent);
                        socket.close();
                        do_accept();
                    }
                    else if (pid == 0)
                    {
                        // Child

                        io_context_.notify_fork(boost::asio::io_context::fork_child);

                        signal_.cancel();
                        acceptor_.close();

                        std::make_shared<session>(std::move(socket), io_context_)->start();
                    }
                    else
                    {
                        // Error
                        cerr << "[do_accept x] in fail\n";
                        exit(1);
                    }
                }
                else
                {
                    do_accept();
                }
            });
    }
    boost::asio::io_context &io_context_;
    tcp::acceptor acceptor_;
    boost::asio::signal_set signal_;
};

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: http_server <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}