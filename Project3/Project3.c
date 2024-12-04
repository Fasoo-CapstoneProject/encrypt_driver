#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib") // Winsock 라이브러리 링크

#define PORT 8080
#define BUFFER_SIZE 1024

void handle_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);

    if (bytes_received == SOCKET_ERROR) {
        printf("Error receiving data: %d\n", WSAGetLastError());
        closesocket(client_socket);
        return;
    }

    buffer[bytes_received] = '\0';
    printf("Received path: %s\n", buffer);

    // 클라이언트에게 응답 전송
    const char* response = "Path received successfully";
    send(client_socket, response, strlen(response), 0);

    closesocket(client_socket);
}

int main() {
    WSADATA wsaData;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Listen for connections
    if (listen(server_socket, 5) == SOCKET_ERROR) {
        printf("Listen failed: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept client connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET) {
            printf("Accept failed: %d\n", WSAGetLastError());
            continue;
        }

        handle_client(client_socket);
    }

    // Cleanup
    closesocket(server_socket);
    WSACleanup();
    return 0;
}