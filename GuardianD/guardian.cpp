//
//Created by iyasar 2016
//
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <boost/asio.hpp>


class CGuardianSession : public std::enable_shared_from_this<CGuardianSession>
{
public:
    explicit CGuardianSession(boost::asio::io_service &io_service) :
                        msocket(io_service), mstrand(io_service)
    {}
    ~CGuardianSession() = default;
public:
    boost::asio::ip::tcp::socket& socket()
    {
        return msocket;
    }
    void process()
    {
        boost::asio::async_read_until(msocket, mreadBuf, "\r\n",
            std::bind(&CGuardianSession::handleRead, shared_from_this(),
                std::placeholders::_1, std::placeholders::_2));
    }
private:
    void handleRead(const boost::system::error_code& e, std::size_t bytes_transferred)
    {
        if (!e) {

        }
    }
    void handleWrite(const boost::system::error_code& e)
    {

    }
private:
    boost::asio::streambuf mreadBuf;
    boost::asio::io_service::strand mstrand;
    boost::asio::ip::tcp::socket msocket;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGuardianListener
{
public:
    CGuardianListener() : macceptor(mio_service)
    {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 34000);
        macceptor.open(endpoint.protocol());
        macceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        macceptor.bind(endpoint);
        macceptor.listen();
        startAccept();

    };
    ~CGuardianListener() = default;
public:
    void run()
    {
        std::vector<std::shared_ptr<std::thread> > threads;
        for (size_t i = 0; i < 10; ++i) {
            std::shared_ptr<std::thread> th
                (new std::thread(std::bind(static_cast<size_t(boost::asio::io_service::*)()>
                                                             (&boost::asio::io_service::run),
                                                              &mio_service)));
            threads.push_back(th);
        }

        std::for_each(threads.begin(), threads.end(), [](std::shared_ptr<std::thread> &th) {th->join();});

    }
private:
    void startAccept()
    {
        msession.reset(new CGuardianSession(mio_service));
        macceptor.async_accept(msession->socket(), std::bind(&CGuardianListener::handleAccept, this,
                               std::placeholders::_1));
    }
    void handleAccept(const boost::system::error_code& e)
    {
        if (!e)
        {
            msession->process();
        }

        startAccept();
    }

private:
    boost::asio::io_service mio_service;
    boost::asio::ip::tcp::acceptor macceptor;
    std::shared_ptr<CGuardianSession> msession;
};


int main()
{
    CGuardianListener cg;
    
    cg.run();

    return 0;
}

