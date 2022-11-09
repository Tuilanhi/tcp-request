#include "TCPRequestChannel.h"

using namespace std;

/**
 * If the ip_address is empty, then the channel is created on the server side, otherwise it is created
 * on the client side
 *
 * @param _ip_address the IP address of the server (if client side) or the client (if server side)
 * @param _port_no the port number to use for the connection
 */
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
    / int inet_pton()
    */

    if (_ip_address == "")
    {
        struct sockaddr_in server;
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(atoi(_port_no.c_str()));
        if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }
        if (listen(sockfd, 1024) < 0)
        {
            perror("listen failed");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        struct sockaddr_in client;
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }
        client.sin_family = AF_INET;
        client.sin_port = htons(atoi(_port_no.c_str()));
        if (inet_pton(AF_INET, _ip_address.c_str(), &client.sin_addr.s_addr) <= 0)
        {
            perror("Convert failed");
            exit(EXIT_FAILURE);
        }
        if (connect(sockfd, (struct sockaddr *)&client, sizeof(client)) < 0)
        {
            perror("connect failed");
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * It creates a TCPRequestChannel object with a given socket file descriptor
 *
 * @param _sockfd The socket file descriptor for the connection.
 */
TCPRequestChannel::TCPRequestChannel(int _sockfd)
{
    sockfd = _sockfd;
}

/**
 * It closes the socket file descriptor
 */
TCPRequestChannel::~TCPRequestChannel()
{
    // close the sockfd
    close(sockfd);
}

/**
 * It accepts a connection from a client and returns the socket file descriptor of the client
 *
 * @return The sockfd of the client
 */
int TCPRequestChannel::accept_conn()
{
    // struct sockaddr_storage
    struct sockaddr_storage server;
    // implementing accept(...)
    socklen_t socket_size = sizeof(server);
    int accept_socket = accept(sockfd, (struct sockaddr *)&server, &socket_size);
    if (accept_socket < 0)
    {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }
    // return value the sockfd of the client
    return accept_socket;
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
