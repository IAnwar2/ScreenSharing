#define WIN32_LEAN_AND_MEAN
#define PORT "1234"

#include "Server.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <string.h>
#include <chrono>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <iomanip>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

std::mutex ServerSide::mtx;
std::condition_variable ServerSide::cv;

std::vector<SOCKET> ServerSide::clientSockets;
SOCKET ServerSide::ListenSocket;

// Global Variables
int ServerSide::finished_threads = 0;


void ServerSide::runServer()
{
    std::cout << "In Server" << std::endl;

    std::thread([]() { ConnectionListener(); }).detach();
    std::thread([]() { TakeImage(); }).detach();

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return finished_threads >= 2; });

    std::cout << "GoodBye :)" << std::endl;
}

void ServerSide::ConnectionListener()
{
    WSADATA wsaData;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;

        {
            std::lock_guard<std::mutex> lock(mtx);
            finished_threads +=2;
        }
        cv.notify_all();  
        return; 
    }

    struct addrinfo *result = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, PORT, &hints, &result); // Maybe look into randomizing the port # for better security?
    std::cout << "getaddrinfo: " << result->ai_addr << std::endl;
    
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();

        {
            std::lock_guard<std::mutex> lock(mtx);
            finished_threads +=2;
        }
        cv.notify_all();  
        return; 
    }

    // Create a SOCKET for the server
    ListenSocket = INVALID_SOCKET;
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        {
            std::lock_guard<std::mutex> lock(mtx);
            finished_threads +=2;
        }
        cv.notify_all();  
        return; 
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "bind failed with error: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        {
            std::lock_guard<std::mutex> lock(mtx);
            finished_threads +=2;
        }
        cv.notify_all();  
        return; 
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(ListenSocket);
        WSACleanup();

        {
            std::lock_guard<std::mutex> lock(mtx);
            finished_threads +=2;
        }
        cv.notify_all();  
        return; 
    }

    std::cout << "Server is listening on port " << PORT << std::endl;

    // Loop to accept multiple clients
    while (true) {
        SOCKET ClientSocket = INVALID_SOCKET;
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            closesocket(ListenSocket);
            WSACleanup();

            {
                std::lock_guard<std::mutex> lock(mtx);
                finished_threads +=3;
            }
            cv.notify_all();  
            return; 
        }

        std::cout << "Connection accepted" << std::endl;
        clientSockets.push_back(ClientSocket);

        std::thread([ClientSocket]() { HandleConnections(ClientSocket); }).detach();

    }

    // thread has closed
    {
        std::lock_guard<std::mutex> lock(mtx);
        finished_threads++;
    }
    cv.notify_all();    
}

void ServerSide::HandleConnections(SOCKET clientSocket)
{
    char buffer[1024];
    int bytesReceived;

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientSocket, &readfds);

        // Set timeout to NULL to wait indefinitely for readability
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100 milliseconds

        int selectResult = select(0, &readfds, NULL, NULL, &timeout);
        if (selectResult > 0) {
            if (FD_ISSET(clientSocket, &readfds)) 
            {
                // Receive data from the client
                bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
                std::cout << "found some shit" << std::endl;

                if (bytesReceived > 0) 
                {
                    // Process the received data
                    buffer[bytesReceived] = '\0'; // Null-terminate the received data
                    std::cout << "Received: " << buffer << std::endl;

                } else if (bytesReceived == -1)
                {
                    // Connection closed by the client
                    std::cout << "Client disconnected. Recieved " << bytesReceived << " bytes."<< std::endl;

                    clientSockets.erase(
                        std::remove_if(clientSockets.begin(), clientSockets.end(), [clientSocket](SOCKET s) {
                            return s == clientSocket;
                        }), clientSockets.end());

                    return; // Exit the loop if the client disconnected
                } 

            }
        } else if (selectResult == 0) {
            // Timeout occurred, continue the loop
            continue;
        } else {
            // Error in select
            std::cerr << "select() failed: " << WSAGetLastError() << std::endl;
            break; // Exit the loop on error
        }
    }
}

void ServerSide::TakeImage()
{
    

    while(true)
    {
        if (clientSockets.size() > 0)
        {
            // Basic graphical setup
            HDC hScreenDC = GetDC(NULL);
            HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

            int nWidth = GetSystemMetrics(SM_CXSCREEN);
            int nHeight = GetSystemMetrics(SM_CYSCREEN);

            // Create a compatible bitmap from the Screen DC
            HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, nWidth, nHeight);

            // Select the compatible bitmap into the memory device context
            HBITMAP holdBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

            // Bit block transfer into our compatible memory DC
            BitBlt(hMemoryDC, 0, 0, nWidth, nHeight, hScreenDC, 0, 0, SRCCOPY);

            // Save the bitmap into a BITMAPINFO structure
            BITMAP bmp;
            GetObject(hBitmap, sizeof(BITMAP), &bmp);

            BITMAPINFOHEADER bi;
            bi.biSize = sizeof(BITMAPINFOHEADER);
            bi.biWidth = bmp.bmWidth;
            bi.biHeight = bmp.bmHeight;
            bi.biPlanes = 1;
            bi.biBitCount = 32;
            bi.biCompression = BI_RGB;
            bi.biSizeImage = 0;
            bi.biXPelsPerMeter = 0;
            bi.biYPelsPerMeter = 0;
            bi.biClrUsed = 0;
            bi.biClrImportant = 0;

            std::vector<BYTE> buffer;
            DWORD dwSize;

            dwSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;

            buffer.resize(dwSize);


            // Use GetDIBits to copy the bits into our buffer
            if (!GetDIBits(hMemoryDC, hBitmap, 0, (UINT)bmp.bmHeight, buffer.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
                DeleteObject(hBitmap);
                DeleteDC(hMemoryDC);
                ReleaseDC(NULL, hScreenDC);
            }

            //Use to check if images are being taken
            // uint64_t value = 0;
            // std::cout<<"Image value is: "<<std::endl;
            // for (int i = 0; i < 5; i++)
            // {
            //     value = (value << 8) | buffer[i];
            //     std::cout<<value<< ".";
            // }
            // std::cout<<std::endl;


            std::ostringstream oss;
            oss << "IMAGE_DATA:" << buffer.size();
            std::string dataSize = oss.str();
            std::vector<BYTE> imageDataSize(dataSize.begin(), dataSize.end());

            SendToConnections(imageDataSize); // Send Image size first

            SendToConnections(buffer); // Send image data
            // std::cout<<"Sent Data"<<std::endl;

            // Clean up
            SelectObject(hMemoryDC, holdBitmap);
            DeleteObject(hBitmap);
            DeleteDC(hMemoryDC);
            ReleaseDC(NULL, hScreenDC);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(17)); //A little less than 60 fps? 16.66 ms
    }
    
    // thread has closed
    {
        std::lock_guard<std::mutex> lock(mtx);
        finished_threads++;
    }
    cv.notify_all();
}

void ServerSide::CloseAllConnections()
{
    for (SOCKET s : clientSockets) {
        closesocket(s);
    }
    closesocket(ListenSocket);
    WSACleanup();
}

void ServerSide::SendToConnections(const std::vector<BYTE>& data)
{
     for (SOCKET s : clientSockets) {
        int totalBytesSent = 0;
        int dataSize = data.size();
        const char* dataPtr = reinterpret_cast<const char*>(data.data());

        while (totalBytesSent < dataSize) {
            int bytesSent = send(s, dataPtr + totalBytesSent, dataSize - totalBytesSent, 0);
            if (bytesSent == SOCKET_ERROR) {
                std::cerr << "Failed to send data: " << WSAGetLastError() << std::endl;
                break;
            }
            totalBytesSent += bytesSent;
        }
    }

}