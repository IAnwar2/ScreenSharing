#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include "Client.h"
#include <iostream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <thread>


// Link with ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

SOCKET ConnectSocket;
size_t imageSize;
HBITMAP hBitmap = NULL;
HWND hwnd = NULL;

void ClientSide::runClient(){
    std::cout << "in client" << std::endl;

    HINSTANCE hInstance = GetModuleHandle(NULL);
    ClientSide client;
    client.CreateImageWindow(hInstance, SW_SHOWNORMAL);    
    testConnection();

    while(true)
        std::this_thread::sleep_for(std::chrono::milliseconds(17));
}

void ClientSide::testConnection() {
    WSADATA wsaData;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return;
    }

    struct addrinfo *result = NULL, *ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo("127.0.0.1", "1234", &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return;
    }

    // Attempt to connect to the first address returned by the call to getaddrinfo
    ConnectSocket = INVALID_SOCKET;
    ptr = result;

    // Create a SOCKET for connecting to server
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return;
    }

    // Connect to server
    iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        std::cerr << "Unable to connect to server!" << std::endl;
        WSACleanup();
        return;
    }

    const char *sendbuf = "Hello from client";
    char recvbuf[512];
    int recvbuflen = 512;

    // Send an initial buffer
    iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "send failed: " << WSAGetLastError() << std::endl;
        closesocket(ConnectSocket);
        WSACleanup();
        return;
    }

    std::cout << "Bytes Sent: " << iResult << std::endl;

    // Shutdown the connection for sending since no more data will be sent
    // iResult = shutdown(ConnectSocket, SD_SEND);
    // if (iResult == SOCKET_ERROR) {
    //     std::cerr << "shutdown failed: " << WSAGetLastError() << std::endl;
    //     closesocket(ConnectSocket);
    //     WSACleanup();
    //     return;
    // }

    // Receive until the peer closes the connection
    // do {
        // iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        // if (iResult > 0)
        //     std::cout << "Bytes received: " << iResult << std::endl;
        // else if (iResult == 0)
        //     std::cout << "Connection closed" << std::endl;
        // else
        //     std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
    // } while (iResult > 0);

    // Cleanup
    // closesocket(ConnectSocket);
    // WSACleanup();

    std::cout<<"connectedSocket: " << ConnectSocket << std::endl;

    // while(true) 
    //     sendInput(); // do later

    ReceiveImages(ConnectSocket);
    
    closesocket(ConnectSocket);
    WSACleanup();
}

void ClientSide::sendInput(){
    // std::system("cls");
    std::cout << "Enter Text to Send: " << std::endl;
    std::string message;
    // std::getline(std::cin, message);
    std::cin>>message;

    int iResult;

    const char *sendbuf = message.c_str();

    iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "send failed: " << WSAGetLastError() << std::endl;
        // closesocket(ConnectSocket);
        // WSACleanup();
        return;
    }

    std::cout << "Bytes Sent: " << iResult << std::endl;
}

void ClientSide::ReceiveImages(SOCKET clientSocket) {
    char buffer[1024];
    int bytesReceived;
    size_t imgBytesReceived = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(clientSocket, &readfds);

    while (true) {
        fd_set tempfds = readfds;

        // Set timeout to 100 milliseconds
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100 milliseconds

        int selectResult = select(0, &tempfds, NULL, NULL, &timeout);
        if (selectResult > 0) {
            if (FD_ISSET(clientSocket, &tempfds)) {
                // Receive data from the client
                std::cout<<"here1"<<std::endl;

                bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

                std::cout<<"here2"<<std::endl;


                if (bytesReceived > 0) {
                    std::cout<<"here3"<<std::endl;

                    // Null-terminate the received data
                    buffer[bytesReceived] = '\0';

                    // Process the received data
                    std::string receivedData(buffer, bytesReceived);
                    if (receivedData.find("IMAGE_DATA:") == 0) {
                        std::string sizeStr = receivedData.substr(11);
                        imageSize = std::stoul(sizeStr); // Convert to size_t

                        // Prepare to receive the image data
                        std::vector<BYTE> dataBuffer(imageSize);
                        imgBytesReceived = 0;

                        while (imgBytesReceived < imageSize) {
                            int currentBytesReceived = recv(clientSocket, reinterpret_cast<char*>(dataBuffer.data()) + imgBytesReceived, imageSize - imgBytesReceived, 0);

                            if (currentBytesReceived == SOCKET_ERROR) {
                                std::cerr << "Failed to receive image data: " << WSAGetLastError() << std::endl;
                                return;
                            } else if (currentBytesReceived == 0) {
                                std::cout << "Server disconnected." << std::endl;
                                return;
                            }

                            imgBytesReceived += currentBytesReceived;
                        }

                        //Use to check if images are being taken
                        // uint64_t value = 0;
                        // std::cout<<"Image value is: "<<std::endl;
                        // for (int i = 0; i < 5; i++)
                        // {
                        //     value = (value << 8) | dataBuffer[i];
                        //     std::cout<<value<< ".";
                        // }
                        // std::cout<<std::endl;

                        

                        // Display the image

                       // Display the image
                        // BITMAPINFOHEADER bi;
                        // memcpy(&bi, dataBuffer.data(), sizeof(BITMAPINFOHEADER));

                        // HDC hScreenDC = GetDC(NULL);
                        // HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

                        // BYTE* pPixels = const_cast<BYTE*>(dataBuffer.data()) + sizeof(BITMAPINFOHEADER);

                        // HBITMAP hNewBitmap = CreateCompatibleBitmap(hScreenDC, bi.biWidth, bi.biHeight);
                        // SetDIBits(hMemoryDC, hNewBitmap, 0, bi.biHeight, pPixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

                        // // Clean up old bitmap
                        // if (hBitmap) {
                        //     DeleteObject(hBitmap);
                        // }

                        // hBitmap = hNewBitmap;

                        // // Invalidate window to trigger WM_PAINT
                        // InvalidateRect(hwnd, NULL, TRUE);

                        // // Clean up
                        // DeleteDC(hMemoryDC);
                        // ReleaseDC(NULL, hScreenDC);

                    } else {
                        std::cout << "Unexpected data format: " << receivedData << std::endl;
                        break;
                    }

                } else if (bytesReceived == 0) {
                    // Connection closed by the server
                    std::cout << "Server disconnected." << std::endl;
                    break;
                } else {
                    // Error in receiving data
                    std::cerr << "recv() failed: " << WSAGetLastError() << std::endl;
                    break;
                }
            }
        } else if (selectResult == 0) {
            // Timeout occurred, continue the loop
            continue;
        } else {
            // Error in select
            std::cerr << "select() failed: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    closesocket(clientSocket);
    WSACleanup();
}

void ClientSide::CreateImageWindow(HINSTANCE hInstance, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"Image Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc = ClientSide::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Image Display",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL,
        NULL,
        hInstance,
        this // Pass the pointer to the client instance
    );

    if (hwnd == NULL) {
        return;
    }

    ShowWindow(hwnd, nCmdShow);
}


LRESULT CALLBACK ClientSide::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ClientSide* client = nullptr;

    if (uMsg == WM_CREATE) {
        // Store the ClientSide pointer passed in CreateImageWindow
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        client = reinterpret_cast<ClientSide*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)client);
    } else {
        client = reinterpret_cast<ClientSide*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    switch (uMsg) {
    case WM_PAINT:
        if (client && client->hBitmap) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HDC hMemoryDC = CreateCompatibleDC(hdc);
            SelectObject(hMemoryDC, client->hBitmap);
            BITMAP bm;
            GetObject(client->hBitmap, sizeof(bm), &bm);
            BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, hMemoryDC, 0, 0, SRCCOPY);
            DeleteDC(hMemoryDC);
            EndPaint(hwnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}