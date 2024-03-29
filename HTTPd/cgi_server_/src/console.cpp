#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <vector>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>
#include <unordered_map>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <memory>

using boost::asio::ip::tcp;
using namespace std;

extern boost::asio::io_context cgi_io_context;
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

void write_html(tcp::socket &socket, string str)
{
    // boost::asio::write(socket,boost::asio::buffer(str, str.size()));
    boost::asio::async_write(
        socket, boost::asio::buffer(str, str.size()),
        [](boost::system::error_code ec, size_t /*length*/)
        {
            if (ec)
            {
                cerr << ec << endl;
            }
        });
}

class client
    : public std::enable_shared_from_this<client>
{
public:
    client(boost::asio::io_context &io_context, connect_info info,  tcp::socket *output_socket)
        : resolver_(boost::asio::make_strand(io_context)),
          socket_(io_context),
          output_socket_(output_socket)
    {
        info_ = info;
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
                    do_connect();
                }
                else
                {
                    cerr << "[error] resolve failed (" << info_.server << "," << info_.host_name << "," << info_.port << "," << endpoint_ << ")" << endl;
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

                                        //prevent print the noise data which is out of range
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
        write_html(*output_socket_, str);
    }
    void output_command(string content)
    {
        html_escape(content);
        string str = "<script>document.getElementById('";
        str += info_.server;
        str += "').innerHTML += '<b>";
        str += content;
        str += "</b>';</script>";
        write_html(*output_socket_, str);
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
    tcp::socket *output_socket_;
    // shared_ptr<tcp::socket> output_socket_;

    connect_info info_;
    tcp::endpoint endpoint_;
    vector<string> testcase_;
};

void exec_console(tcp::socket socket_, unordered_map<string, string> &headers_)
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

        string query = headers_["QUERY_STRING"];
        string ct = "Content-type: text/html\r\n\r\n";

        write_html(socket_, ct);
        write_html(socket_, head);

        // parse query "h0=nplinux1.cs.nctu.edu.tw&p0=1234&f0=t1.txt&
        //              h1=nplinux2.cs.nctu.edu.tw&p1=5678&f1=t2.txt&
        //              h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4="
        vector<string> params;
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
                if (key.length() == 2)
                {
                    int n = key[1] - '0';
                    if (0 <= n && n < 5)
                    {
                        machine_idx = n;
                    }
                }

                if (machine_idx != -1)
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


        // start to output the body
        string thead = "<thead><tr>";
        write_html(socket_, thead);

        for (auto info : infos)
        {
            if (info.host_name != "")
            {
                string scope = R""""(<th scope="col">)"""";
                string host = info.host_name + ":" + info.port;
                string th = "</th>";
                write_html(socket_, scope);
                write_html(socket_, host);
                write_html(socket_, th);
            }
        }
        string tmp = "</tr></thead><tbody><tr>";
        write_html(socket_, tmp);

        for (auto info : infos)
        {
            if (info.host_name != "")
            {
                string td = R""""(<td><pre id=")"""";
                string server = info.server;
                string mb = R""""(" class="mb-0"></pre></td>)"""";
                write_html(socket_, td);
                write_html(socket_, server);
                write_html(socket_, mb);
            }
        }
        tmp = "</tr></tbody>";
        write_html(socket_, tmp);


        // shared_ptr<tcp::socket> output_socket_ptr(&socket_);
        boost::asio::io_context io_context;
        
        for (auto info : infos)
        {
            if (info.host_name != "")
            {
                make_shared<client>(io_context, info, &socket_)->start();
            }
        }
        io_context.run();
        socket_.close();
        return;
    }
    catch (std::exception &e)
    {
        std::cerr << "console exception: " << e.what() << "\n";
    }
    return;
}