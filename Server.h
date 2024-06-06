#ifndef SERVER_H
#define SERVER_H

#include <mutex>
#include <condition_variable>
#include <vector>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

// Declare the class
class ServerSide {
    private:
        static std::mutex mtx;
        static std::condition_variable cv;
        static int finished_threads;
        static std::vector<SOCKET> clientSockets;
        static SOCKET ListenSocket;

        static void ConnectionListener();
        static void HandleConnections(SOCKET clientSocket);
        static void TakeImage();
        static void CloseAllConnections();
        static void handleClient();
        static void SendToConnections(const std::vector<BYTE>& data);

    public:
        void runServer();
        
};




#endif