#include <vector>
#include <boost/asio.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <stdlib.h>
#include <unordered_map>
#include <string>
#include <fstream>
#include <io.h>
#include <stdio.h>
#include <fcntl.h>

using boost::asio::ip::tcp;
using namespace std;
void exec_panel(tcp::socket socket_);
void exec_console(tcp::socket socket_, unordered_map<string, string> &headers_);

static unordered_map<string, string> const http_method_table = {
    {"GET", "GET"} };

struct connect_info
{
    connect_info()
    {
        host_name = "";
    }

    string server;
    string host_name;
    string port;
    string testcase_name;
};

boost::asio::io_context cgi_io_context;

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
                    if (!uri.compare("panel.cgi"))
                    {
                        exec_panel(std::move(socket_));
                    }
                    else if (!uri.compare("console.cgi"))
                    {
                        exec_console(std::move(socket_), headers_);
                    }
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
    server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
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
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: http_server <port>\n";
            return 1;
        }

        server s(cgi_io_context, std::atoi(argv[1]));

        cgi_io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "cgi_server exception: " << e.what() << "\n";
    }
    system("PAUSE");
    return 0;
}