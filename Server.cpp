#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>

#define PIPE_NAME L"\\\\.\\pipe\\ForumV11"
#define MAILSLOT_NAME L"\\\\.\\mailslot\\DiscoveryV11"

CRITICAL_SECTION cs;
std::vector<HANDLE> hClients;//список активних підключень

void Broadcast(const char* msg) {//розсилка повідомлень всім підключеним клієнтам
    std::vector<HANDLE> targets;// копіюємо список клієнтів, щоб не тримати секцію заблокованою під час запису
    EnterCriticalSection(&cs);//блокуємо доступ до списку клієнтів для інших потоків
    targets = hClients;
    LeaveCriticalSection(&cs);

    for (HANDLE h : targets) {//проходимось циклом по всіх клієнтах і відправляємо кожному з них повідомлення
        DWORD wr;
        if (WriteFile(h, msg, (DWORD)strlen(msg) + 1, &wr, NULL)) {
            FlushFileBuffers(h);
        }
    }
}

DWORD WINAPI ClientHandler(LPVOID lpParam) {//потік для обробки кожного клієнта
    HANDLE hPipe = (HANDLE)lpParam;
    char buf[1024];
    DWORD rd, avail;

    while (TRUE) {
        if (PeekNamedPipe(hPipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {//перевірка чи є нові дані
            if (ReadFile(hPipe, buf, sizeof(buf), &rd, NULL) && rd > 0) {//якщо є - читаємо
                printf("[SERVER LOG]: %s\n", buf);
                Broadcast(buf);
            }
        }
        else if (GetLastError() == ERROR_BROKEN_PIPE) {// клієнт закрив програму
            
            break;
        }
        Sleep(10);
    }

    EnterCriticalSection(&cs);//відключення клієнта
    for (auto it = hClients.begin(); it != hClients.end(); ++it) {
        if (*it == hPipe) { hClients.erase(it); break; }
    }
    LeaveCriticalSection(&cs);//блокуємо список
    CloseHandle(hPipe);
    return 0;
}

DWORD WINAPI Discovery(LPVOID lpParam) {//потік який слухає mailslot на наявність ping-запитів
    HANDLE hSlot = CreateMailslot(MAILSLOT_NAME, 0, MAILSLOT_WAIT_FOREVER, NULL);//створюємо mailslot
    char b[128]; DWORD r;
    while (ReadFile(hSlot, b, sizeof(b), &r, NULL)) printf("[DISCOVERY]: Ping!\n");//чекаємо на повідомлення
    return 0;
}

int main() {
    SetConsoleOutputCP(1251); SetConsoleCP(1251);
    InitializeCriticalSection(&cs);
    printf("=== СЕРВЕР  ===\n");
    CreateThread(NULL, 0, Discovery, NULL, 0, NULL);

    while (TRUE) {
        HANDLE hPipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, 2048, 2048, 0, NULL);

        if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {//додаємо нового клієнта до списку
            printf("[CONNECT]: Клієнт приєднався.\n");
            EnterCriticalSection(&cs);
            hClients.push_back(hPipe);
            LeaveCriticalSection(&cs);
            CreateThread(NULL, 0, ClientHandler, (LPVOID)hPipe, 0, NULL);//створюємо потік для робти з ним
        }
    }
    return 0;
}