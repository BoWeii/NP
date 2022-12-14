#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <vector>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>
#include <unordered_map>
#include <stdlib.h>
#include <string>
#include <fstream>

using boost::asio::ip::tcp;
using namespace std;
#define REPLY_LENGTH 8
#define REQEUST_LENGTH 9
#define REQUEST_GRANTED 90
#define REQUEST_REJECTED 91

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;

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

struct socks_info
{
    socks_info()
    {
        enable = false;
    }

    bool enable;
    string host_name;
    string port;
};

struct socks4_request
{
    socks4_request()
    {
    }

    socks4_request(tcp::endpoint endpoint)
    {
        vn = 4;
        cd = 1; // CONNECT
        dst_port = int_to_port(endpoint.port());
        dst_IP = ip_to_dword(endpoint.address().to_string());
        null_byte = 0;
    }

    BYTE vn;
    BYTE cd;
    WORD dst_port;
    DWORD dst_IP;
    BYTE null_byte;
};

struct socks4_reply
{
    socks4_reply()
    {
    }

    socks4_reply(char *data)
    {
        vn = data[0];
        cd = data[1];
        dst_port = int_to_port(*((WORD *)&data[2]));
        dst_ip = *((DWORD *)&data[4]);
    }

    BYTE vn;
    BYTE cd;
    WORD dst_port;
    DWORD dst_ip;
};

class client
    : public std::enable_shared_from_this<client>
{
public:
    client(boost::asio::io_context &io_context, connect_info info, socks_info sinfo)
        : resolver_(boost::asio::make_strand(io_context)),
          socket_(io_context)
    {
        info_ = info;
        socks_info_ = sinfo;
        data_[max_length] = '\0';

        // read ./test_case/t1.txt
        string file_path = "./test_case/" + info_.testcase_name;
        ifstream testcase(file_path);

        if (testcase.is_open())
        {
            string line;
            while (getline(testcase, line))
            {
                testcase_.push_back(line + "\n");
            }
            testcase.close();
        }
    }
    void start()
    {
        do_resolve();
    }

private:
    void do_resolve()
    {
        // use resolver to do_connect the np_single_golden
        auto self(shared_from_this());
        resolver_.async_resolve(
            string(info_.host_name),
            string(info_.port),
            [this, self](boost::system::error_code ec, tcp::resolver::results_type endpoints)
            {
                if (!ec)
                {
                    for (auto it = endpoints.cbegin(); it != endpoints.cend(); it++)
                    {
                        endpoint_ = *it;
                        break;
                    }
                    do_resolve_socks();
                }
                else
                {
                    cerr << "[error] resolve failed (" << info_.server << "," << info_.host_name << "," << info_.port << "," << endpoint_ << ")" << endl;
                }
            });
    }
    void do_resolve_socks()
    {
        auto self(shared_from_this());
        if (socks_info_.enable)
        {
            resolver_.async_resolve(string(socks_info_.host_name),
                                    string(socks_info_.port),
                                    [this, self](boost::system::error_code ec, tcp::resolver::results_type endpoints)
                                    {
                                        if (!ec)
                                        {
                                            for (auto it = endpoints.cbegin(); it != endpoints.cend(); it++)
                                            {
                                                socks_endpoint_ = *it;
                                                break;
                                            }
                                            do_connect_socks();
                                        }
                                        else
                                        {
                                            // normal connect without sock
                                            do_connect();
                                        }
                                    });
        }
    }
    void do_connect_socks()
    {
        auto self(shared_from_this());
        socket_.async_connect(socks_endpoint_,
                              [this, self](boost::system::error_code ec)
                              {
                                  if (!ec)
                                  {

                                      do_send_socks4_request();
                                  }
                                  else
                                  {
                                      // normal connect without sock
                                      do_connect();
                                  }
                              });
    }
    void do_send_socks4_request()
    {
        auto self(shared_from_this());
        struct socks4_request request(endpoint_);

        // 9 = 1 + 1 + 2 + 4 + 1(NULL)
        boost::asio::async_write(socket_, boost::asio::buffer((char *)&request.vn, REQEUST_LENGTH),
                                 [this, self](boost::system::error_code ec, std::size_t /**length*/)
                                 {
                                     if (!ec)
                                     {
                                         do_read_socks4_reply();
                                     }
                                 });
    }
    void do_read_socks4_reply()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, REPLY_LENGTH),
                                [this, self](boost::system::error_code ec, size_t length)
                                {
                                    if (!ec)
                                    {
                                        if (length != REPLY_LENGTH)
                                        {
                                            return;
                                        }
                                        struct socks4_reply reply = (data_);
                                        if (reply.vn != 0 || reply.cd != REQUEST_GRANTED)
                                        {
                                            return;
                                        }

                                        // SOCKS4 connection established
                                        do_read();
                                    }
                                });
    }
    void do_connect()
    {
        auto self(shared_from_this());
        socket_.async_connect(endpoint_,
                              [this, self](boost::system::error_code ec)
                              {
                                  if (!ec)
                                  {
                                      do_read();
                                  }
                                  else
                                  {
                                      cerr << "[error] connect failed (" << info_.server << "," << info_.host_name << "," << info_.port << "," << endpoint_ << ")" << endl;
                                  }
                              });
    }
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                [this, self](boost::system::error_code ec, size_t length)
                                {
                                    if (!ec)
                                    {
                                        // the data read from np_single_golden

                                        // prevent printing the noise data which is out of length range
                                        data_[length] = '\0';
                                        string data = string(data_);
                                        output_shell(data);

                                        memset(data_, 0, max_length + 1);

                                        if (data.find("%") == string::npos)
                                        {
                                            // not read the end of line yet
                                            do_read();
                                        }
                                        else
                                        {
                                            do_write();
                                        }
                                    }
                                    else
                                    {
                                        cerr << "[error] read failed (" << info_.server << "," << info_.host_name << "," << info_.port << "," << endpoint_ << ")" << endl;
                                    }
                                });
    }
    void do_write()
    {
        string str;
        if (!testcase_.size())
        {
            return;
        }
        else
        {
            str = string(testcase_[0]);
            testcase_.erase(testcase_.begin());
        }

        output_command(str);

        // the data write to np_single_golden
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(str.c_str(), str.length()),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         do_read();
                                     }
                                     else
                                     {
                                         cerr << "[error] write failed (" << info_.server << "," << info_.host_name << "," << info_.port << "," << endpoint_ << ")" << endl;
                                     }
                                 });
    }

    void output_shell(string content)
    {
        html_escape(content);
        string str = "<script>document.getElementById('";
        str += info_.server;
        str += "').innerHTML += '";
        str += content;
        str += "';</script>";
        cout << str;
    }
    void output_command(string content)
    {
        html_escape(content);
        string str = "<script>document.getElementById('";
        str += info_.server;
        str += "').innerHTML += '<b>";
        str += content;
        str += "</b>';</script>";
        cout << str;
    }
    void html_escape(string &str)
    {
        // do not replace the order
        boost::replace_all(str, "&", "&amp;");
        boost::replace_all(str, "\"", "&quot;");
        boost::replace_all(str, "\'", "&apos;");
        boost::replace_all(str, "<", "&lt;");
        boost::replace_all(str, ">", "&gt;");
        boost::replace_all(str, "\n", "&NewLine;");
        boost::replace_all(str, "\r", "");
    }

    enum
    {
        max_length = 1024
    };

    // one for null byte
    char data_[max_length + 1];
    tcp::resolver resolver_;
    tcp::socket socket_;
    connect_info info_;
    tcp::endpoint endpoint_;
    vector<string> testcase_;
    socks_info socks_info_;
    tcp::endpoint socks_endpoint_;
};

int main()
{
    try
    {
        // refer the sample_console.py
        string head = R""""(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>NP Project 3 Sample Console</title>
    <link
      rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
      integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
      crossorigin="anonymous"
    />
    <link
      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
      rel="stylesheet"
    />
    <link
      rel="icon"
      type="image/png"
      href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
    />
    <style>
      * {
        font-family: 'Source Code Pro', monospace;
        font-size: 1rem !important;
      }
      body {
        background-color: #212529;
      }
      pre {
        color: #cccccc;
      }
      b {
        color: #01b468;
      }
    </style>
  </head>
  <body>
    <table class="table table-dark table-bordered">
    )"""";

        string query = getenv("QUERY_STRING");
        cout << "Content-type: text/html\r\n\r\n";
        cout << head;

        // parse query "h0=nplinux1.cs.nctu.edu.tw&p0=1234&f0=t1.txt&
        //              h1=nplinux2.cs.nctu.edu.tw&p1=5678&f1=t2.txt&
        //              h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4="
        vector<string> params;
        socks_info socks_info_;
        vector<connect_info> infos(5);
        boost::split(params, query, boost::is_any_of("&"), boost::token_compress_on);

        for (auto param : params)
        {
            // param = "key=value"
            auto split_idx = param.find("=");

            if (split_idx != string::npos)
            {
                string key = param.substr(0, split_idx);
                string value = param.substr(split_idx + 1);

                int machine_idx = -1;
                bool use_socks = false;

                if (key.length() == 2)
                {
                    if (key[0] == 's')
                    {
                        // For SOCKS server
                        use_socks = true;
                        socks_info_.enable = true;
                    }
                    else
                    {
                        int n = key[1] - '0';
                        if (0 <= n && n < 5)
                        {
                            machine_idx = n;
                        }
                    }
                }

                if (use_socks)
                {
                    switch (key[1])
                    {
                    case 'h':
                        socks_info_.host_name = value;
                        break;
                    case 'p':
                        socks_info_.port = value;
                        break;
                    default:
                        break;
                    }
                }

                else if (machine_idx != -1)
                {
                    switch (key[0])
                    {
                    case 'h':
                        infos[machine_idx].host_name = value;
                        infos[machine_idx].server = "s" + to_string(machine_idx);
                        break;
                    case 'p':
                        infos[machine_idx].port = value;
                        break;
                    case 'f':
                        infos[machine_idx].testcase_name = value;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        boost::asio::io_context io_context;

        // start to output the body
        cout << "<thead><tr>";
        for (auto info : infos)
        {
            if (info.host_name != "")
            {
                cout << R""""(<th scope="col">)"""";
                cout << info.host_name << ":" << info.port;
                cout << "</th>";
            }
        }
        cout << "</tr></thead>";
        cout << "<tbody><tr>";
        for (auto info : infos)
        {
            if (info.host_name != "")
            {
                cout << R""""(<td><pre id=")"""";
                cout << info.server;
                cout << R""""(" class="mb-0"></pre></td>)"""";
            }
        }
        cout << "</tr></tbody>";

        for (auto info : infos)
        {
            if (info.host_name != "")
            {
                make_shared<client>(io_context, info, socks_info_)->start();
            }
        }
        io_context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}