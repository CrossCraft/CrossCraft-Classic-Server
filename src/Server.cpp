#include "Server.hpp"
#include "Client.hpp"

const int DEFAULT_PORT = 25565;

namespace ClassicServer
{
    Server::Server()
    {
        SC_APP_INFO("Server Starting...");
        SC_APP_INFO("Server: Creating Socket...");

        listener_socket = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));

        if (listener_socket == -1)
            throw std::runtime_error("Fatal: Could not open socket! Errno: " +
                std::to_string(errno));

        sockaddr_in sockaddr{};
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_addr.s_addr = INADDR_ANY;
        sockaddr.sin_port = htons(DEFAULT_PORT);

        if (bind(listener_socket, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
            throw std::runtime_error(
                "Fatal: Could not bind socket address! Port: " +
                std::to_string(DEFAULT_PORT) + ". Errno: " + std::to_string(errno));
        }

        SC_APP_INFO("Server: Socket Created!");

        //TODO: Setup World!

        SC_APP_INFO("Server: Creating Listener...");
        listener_thread = create_scopeptr<std::thread>(Server::connection_listener, this);
    }

    Server::~Server()
    {
#if BUILD_PLAT == BUILD_WINDOWS
        closesocket(listener_socket);
#else
        ::close(listener_socket);
#endif
    }

    void Server::update(float dt)
    {
        //TODO: Update World & Clients
    }

    auto Server::connection_listener(Server *server) -> void
    {
        while (true) {
            SC_APP_INFO("Server: Ready and Listening For Connections!");

            sockaddr_in socketAddress{};
            socketAddress.sin_family = AF_INET;
            socketAddress.sin_addr.s_addr = INADDR_ANY;
            socketAddress.sin_port = htons(DEFAULT_PORT);

            if (::listen(server->listener_socket, 1) < 0) {
                throw std::runtime_error("Fatal: Could not listen on socket. Errno: " +
                    std::to_string(errno));
            }

            auto addressLen = sizeof(socketAddress);
            auto conn =
                static_cast<int>(accept(server->listener_socket, (struct sockaddr*)&socketAddress,
                    (socklen_t*)&addressLen));
            SC_APP_INFO("Connecting...");

            if (conn < 0) {
                SC_APP_ERROR("COULD NOT CONNECT!");
                continue;
            }

            SC_APP_INFO("New Connection from " +
                std::string((inet_ntoa(socketAddress.sin_addr))) +
                " on port " + std::to_string(ntohs(socketAddress.sin_port)));

            auto client = new Client(conn);
            client->PlayerID = server->id_count;
            server->clients->emplace(server->id_count++, client);
        }
    }
}