#include "network_io.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <netinet/in.h>

using namespace IO_Utils;

TEST(NetworkIOTest, IPConversionValid)
{
    uint32_t ip;
    EXPECT_EQ(Socket::make_ip_address("127.0.0.1", ip), 0);
    EXPECT_EQ(ip, 0x100007F);
}

TEST(NetworkIOTest, IPConversionInvalid)
{
    uint32_t ip;
    EXPECT_EQ(Socket::make_ip_address("invalid.ip.address", ip), -1);

    EXPECT_EQ(Socket::make_ip_address("127.0.0.256", ip), -1);
}

TEST(NetworkIOTest, SocketToString)
{
    UDP_Socket udp_socket(0x100007F, 8080);
    EXPECT_EQ(udp_socket.socket_to_str(), "127.0.0.1:8080");

    TCP_Socket tcp_socket(0x100007F, 8080);
    EXPECT_EQ(tcp_socket.socket_to_str(), "127.0.0.1:8080");

    HTTP_Socket http_socket(0x100007F, 8080);
    EXPECT_EQ(http_socket.socket_to_str(), "127.0.0.1:8080");
}

TEST(NetworkIOTest, UDPSocketBind)
{
    UDP_Socket socket(INADDR_ANY, 0);
    int fd = socket.listen_or_bind();
    EXPECT_GT(fd, 0);

    sockaddr_in address;
    socklen_t len = sizeof(address);
    getsockname(fd, (sockaddr *)&address, &len);
    EXPECT_EQ(address.sin_family, AF_INET);

    close(fd);
}

TEST(NetworkIOTest, UDPConnectionSendReceive)
{

    UDP_Socket sender(INADDR_ANY, 0);
    int sender_fd = sender.listen_or_bind();
    ASSERT_GT(sender_fd, 0);

    UDP_Socket receiver(INADDR_ANY, 0);
    int receiver_fd = receiver.listen_or_bind();
    ASSERT_GT(receiver_fd, 0);

    sockaddr_in receiver_addr;
    socklen_t len = sizeof(receiver_addr);
    getsockname(receiver_fd, (sockaddr *)&receiver_addr, &len);

    UDP_Connection sender_conn(sender_fd);
    UDP_Connection receiver_conn(receiver_fd);

    auto receiver_socket = std::make_shared<UDP_Socket>(
        receiver_addr.sin_addr.s_addr,
        ntohs(receiver_addr.sin_port));

    Packet send_packet(receiver_socket);
    send_packet.data = {1, 2, 3, 4};
    EXPECT_EQ(sender_conn.send_packet(send_packet), 0);

    Packet recv_packet(nullptr);
    EXPECT_EQ(receiver_conn.recv_packet(recv_packet), 0);

    EXPECT_EQ(recv_packet.data, std::vector<uint8_t>({1, 2, 3, 4}));

    close(sender_fd);
    close(receiver_fd);
}

TEST(NetworkIOTest, TCPSocketConnectAccept)
{
    TCP_Socket server(INADDR_ANY, 0);
    int server_fd = server.listen_or_bind();
    ASSERT_GT(server_fd, 0);

    sockaddr_in server_addr;
    socklen_t len = sizeof(server_addr);
    getsockname(server_fd, (sockaddr *)&server_addr, &len);

    TCP_Socket client(server_addr.sin_addr.s_addr, ntohs(server_addr.sin_port));

    std::thread server_thread([&]
                              {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        ASSERT_GT(client_fd, 0);
        close(client_fd);});

    int client_fd = client.connect_socket();
    EXPECT_GT(client_fd, 0);

    server_thread.join();
    close(server_fd);
    close(client_fd);
}

TEST(NetworkIOTest, TCPConnectionSendReceive)
{
    TCP_Socket server(INADDR_ANY, 0);
    int server_fd = server.listen_or_bind();
    ASSERT_GT(server_fd, 0);

    sockaddr_in server_addr;
    socklen_t len = sizeof(server_addr);
    getsockname(server_fd, (sockaddr *)&server_addr, &len);

    std::thread server_thread([&]
                              {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        ASSERT_GT(client_fd, 0);
        
        TCP_Connection conn(client_fd);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        Packet recv_packet(nullptr);
        EXPECT_EQ(conn.recv_packet(recv_packet), 0);
        EXPECT_EQ(recv_packet.data, std::vector<uint8_t>({1, 2, 3}));
        
        Packet send_packet(nullptr);
        send_packet.data = {4, 5, 6};
        EXPECT_EQ(conn.send_packet(send_packet), 0);
        
        close(client_fd); });

    TCP_Socket client(server_addr.sin_addr.s_addr, ntohs(server_addr.sin_port));
    int client_fd = client.connect_socket();
    ASSERT_GT(client_fd, 0);

    TCP_Connection conn(client_fd);

    Packet send_packet(nullptr);
    send_packet.data = {1, 2, 3};
    EXPECT_EQ(conn.send_packet(send_packet), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    Packet recv_packet(nullptr);
    EXPECT_EQ(conn.recv_packet(recv_packet), 0);
    EXPECT_EQ(recv_packet.data, std::vector<uint8_t>({4, 5, 6}));

    server_thread.join();
    close(server_fd);
    close(client_fd);
}