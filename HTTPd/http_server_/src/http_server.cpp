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
#include <utility>
#include <boost/asio.hpp>
#include <vector>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <unordered_map>
#include <stdlib.h>
#include <sys/wait.h>
#include <boost/asio/signal_set.hpp>

using boost::asio::ip::tcp;
using namespace std;

static unordered_map<string, string> const http_method_table = {
    {"GET", "GET"}};

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length)
                                {
                                    if (!ec)
                                    {
                                        vector<string> lines;
                                        string version = "INIT";
                                        boost::split(lines, data_, boost::is_any_of("\r\n"), boost::token_compress_on);
                                        size_t split_idx = 0;

                                        // parse the HTTP request "<method> <uri> <version>"
                                        vector<string> line;
                                        boost::split(line, lines[0], boost::is_any_of(" "), boost::token_compress_on);
                                        lines.erase(lines.begin());

                                        auto it = http_method_table.find(line[0]);
                                        if (it != http_method_table.end())
                                        {
                                            method = it->second;
                                        }
                                        else
                                        {
                                            method = "UNKNOWN";
                                        }

                                        uri = line[1];
                                        version = line[2];

                                        // parse the header
                                        headers_.clear();

                                        for (auto line_ : lines)
                                        {
                                            // line_= "<header>: <value>"
                                            split_idx = line_.find(": ");
                                            if (split_idx != string::npos)
                                            {
                                                string header = line_.substr(0, split_idx);
                                                string value = line_.substr(split_idx, split_idx + 2); // +2 for ": "
                                                if (header == "Host")
                                                {
                                                    headers_["HTTP_HOST"] = value;
                                                }
                                            }
                                        }
                                        headers_["REQUEST_METHOD"] = method;
                                        headers_["REQUEST_URI"] = uri;
                                        headers_["SERVER_PROTOCOL"] = version;
                                        headers_["SERVER_ADDR"] = socket_.local_endpoint().address().to_string();
                                        headers_["SERVER_PORT"] = to_string(socket_.local_endpoint().port());
                                        headers_["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
                                        headers_["REMOTE_PORT"] = to_string(socket_.remote_endpoint().port());

                                        // uri = "URI?QUERY_STRING"
                                        split_idx = uri.find("?");
                                        if (split_idx != string::npos)
                                        {
                                            string query = uri.substr(split_idx + 1); //+1 for "?"
                                            uri = uri.substr(0, split_idx);
                                            headers_["QUERY_STRING"] = query;
                                        }
                                        else
                                        {
                                            headers_["QUERY_STRING"] = "";
                                        }
                                        if (uri[0] == '/')
                                        {
                                            uri.erase(0, 1);
                                        }
                                        if (uri.length() >= 5 && uri.substr(uri.length() - 4) == ".cgi")
                                        {
                                            do_exec_cgi();
                                        }
                                    }
                                });
    }

    void do_exec_cgi()
    {
        auto self(shared_from_this());
        char status_200[] = "HTTP/1.1 200 OK\r\n";
        boost::asio::async_write(socket_, boost::asio::buffer(status_200, strlen(status_200)),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         string exec_cgi_cmd = "./" + uri;

                                         if ((fork() > 0))
                                         {
                                             socket_.close();
                                         }
                                         else
                                         {
                                             auto sock_fd = socket_.native_handle();
                                             clearenv();

                                             for (auto header : headers_)
                                             {
                                                 setenv(header.first.c_str(), header.second.c_str(), 1);
                                             }
                                             dup2(sock_fd, STDIN_FILENO);
                                             dup2(sock_fd, STDOUT_FILENO);

                                             socket_.close();

                                             if (execlp(exec_cgi_cmd.c_str(), exec_cgi_cmd.c_str(), NULL) < 0)
                                             {
                                                 cerr << "[error] in execlp\n";
                                             }
                                             exit(0);
                                         }
                                         do_read();
                                     }
                                 });
    }

    void do_write(std::size_t length)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         do_read();
                                     }
                                 });
    }

    tcp::socket socket_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
    unordered_map<string, string> headers_;
    string uri, method;
};

class server
{
public:
    server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
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
                    std::make_shared<session>(std::move(socket))->start();
                }

                do_accept();
            });
    }

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