#ifndef CLIENT_H
#define CLIENT_H

#include <winsock2.h>
#include <windows.h>
#include <vector>


// Declare the class
class ClientSide {
    private:
        HBITMAP hBitmap = NULL;
        HWND hwnd = NULL;

        void testConnection();
        void sendInput();
        void ReceiveImages(SOCKET clientSocket);
        void CreateImageWindow(HINSTANCE hInstance, int nCmdShow);
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    public:
        void runClient();
};


#endif