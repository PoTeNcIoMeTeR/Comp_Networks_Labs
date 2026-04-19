#include <windows.h>
#include <stdio.h>
#include <string>

#define MAILSLOT_PATH L"\\\\.\\mailslot\\DiscoveryV11"
#define PIPE_PATH L"\\\\.\\pipe\\ForumV11"

DWORD WINAPI Receiver(LPVOID lpParam) {//потік який ловить повідомлення від сервера
    HANDLE hPipe = (HANDLE)lpParam;
    char buf[1024];
    DWORD rd, avail;

    while (TRUE) {
        if (PeekNamedPipe(hPipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {// перевіряємо наявність даних кожні 10 мс
            if (ReadFile(hPipe, buf, sizeof(buf), &rd, NULL) && rd > 0) {
                printf("\r[ФОРУМ]: %s\n> ", buf);
                fflush(stdout);
            }
        }
        Sleep(10);
    }
    return 0;
}

int main() {
    SetConsoleOutputCP(1251); SetConsoleCP(1251);
    printf("=== КЛІЄНТ  ===\n");

    HANDLE hFile = INVALID_HANDLE_VALUE;
    while (hFile == INVALID_HANDLE_VALUE) {//намагаємось підключитись до поштової скриньки сервера
        hFile = CreateFile(MAILSLOT_PATH, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) Sleep(500);
    }
    DWORD wr; WriteFile(hFile, "PING", 5, &wr, NULL); CloseHandle(hFile);

    HANDLE hPipe = CreateFile(PIPE_PATH, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);//підключення до pipe
    if (hPipe == INVALID_HANDLE_VALUE) return 1;

    char nick[50];
    printf("Введіть нік: ");
    fgets(nick, 50, stdin);
    nick[strcspn(nick, "\n")] = 0;

    
    CreateThread(NULL, 0, Receiver, (LPVOID)hPipe, 0, NULL);// запускаємо потік читання
    Sleep(100);
    
    std::string joinMsg = "Користувач [" + std::string(nick) + "] увійшов!";// handshake
    WriteFile(hPipe, joinMsg.c_str(), (DWORD)joinMsg.length() + 1, &wr, NULL);
    FlushFileBuffers(hPipe);

    char msg[400];//відправка повідомлень
    while (TRUE) {
        printf("> ");
        if (fgets(msg, 400, stdin)) {
            msg[strcspn(msg, "\n")] = 0;
            if (strlen(msg) > 0) {
                std::string fullMsg = std::string(nick) + ": " + msg;
                WriteFile(hPipe, fullMsg.c_str(), (DWORD)fullMsg.length() + 1, &wr, NULL);
                FlushFileBuffers(hPipe);
            }
        }
    }
    return 0;
}