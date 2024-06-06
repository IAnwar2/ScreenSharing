#include "Client.h"
#include "Server.h"
#include <iostream>

int main() {

    bool validChoice = false;
    std::string x; 

    while (!validChoice)
    {
        std::system("cls");
        std::cout << "Hello! Enter c for client and enter s for server: " << std::endl;
        std::cin >> x;
        if (x == "s")
        {
            ServerSide S;
            S.runServer();
            validChoice = true;
        } else if (x == "c")
        {
            ClientSide C;
            C.runClient();
            validChoice = true;
        }
    }

    return 0;
}