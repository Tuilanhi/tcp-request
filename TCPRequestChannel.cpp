#include "TCPRequestChannel.h"

using namespace std;

TCPRequestChannel::TCPRequestChannel(const std::string _ip_address, const std::string _port_no)
{
    /*
    / if (server side)
    /{
    / create socket with domain, type, and protocol (AF_INET domain (IPv4) and SOCK_STREAM type)
    / bind to assign address to socket to allow server to listen on port_no (sockaddr_in structures in the call to bind(...))
    / mark socket as listening (call to listen(...))
    /}
    / else if(client side)
    / {
    / create socket with domain, type, and protocol
    / connect socket to an IP address of the server
    /}
    // int inet_pton()
    */

    if (_ip_address == "")
    {
        struct sockaddr_in server;
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(atoi(_port_no.c_str()));
        bind(sockfd, (struct sockaddr *)&server, sizeof(server));
        listen(sockfd, 1024);
    }
    else
    {
        struct sockaddr_in client;
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        client.sin_family = AF_INET;
        client.sin_port = htons(atoi(_port_no.c_str()));
        inet_pton(AF_INET, _ip_address.c_str(), &client.sin_addr.s_addr);
        connect(sockfd, (struct sockaddr *)&client, sizeof(client));
    }
}

TCPRequestChannel::TCPRequestChannel(int _sockfd)
{
    sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel()
{
    // close the sockfd
    close(sockfd);
}

int TCPRequestChannel::accept_conn()
{
    // struct sockaddr_storage
    struct sockaddr_storage storage_socket;
    // implementing accept(...)
    socklen_t socket_size = sizeof(storage_socket);
    accept(sockfd, (struct sockaddr *)&storage_socket, &socket_size);
    // return value the sockfd of the client
    cout << "accept con" << endl;
    return sockfd;
}

// read/write, receive/send
int TCPRequestChannel::cread(void *msgbuf, int msgsize)
{
    return read(sockfd, msgbuf, msgsize);
}

int TCPRequestChannel::cwrite(void *msgbuf, int msgsize)
{
    return write(sockfd, msgbuf, msgsize);
}
