#include <stdio.h>
#include <windows.h>
#include <string>
//EXE В ПАПЦІ SOLUTION А НЕ ПРОЕКТУ
//C:\Users\gavaj\source\repos\СompNetworksLab1\x64\Debug
int main(int argc, char* argv[])
{
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);
    //  перевірка на  Single Instance
    //  іменований м'ютекс для всього застосунку
    HANDLE hSingleInstanceMutex = CreateMutexA(NULL, FALSE, "Global\\Lab1_SingleInstanceMutex");
    // використовуємо GetLastError() для перевірки стану
    if (GetLastError() == ERROR_ALREADY_EXISTS && argc == 1)
    {
        printf("ПОМИЛКА: Головний процес вже запущено! Дозволено лише один екземпляр.\n");
        CloseHandle(hSingleInstanceMutex);
        system("pause");
        return 0;
    }
    //логіка процесу нащадка якщо є аргументи командного рядка
    if (argc >= 3)
    {
        // дескриптор анонімного м'ютекса, переданого по спадковості
        HANDLE hInheritedMutex = (HANDLE)(size_t)strtoull(argv[1], NULL, 10);
        int childId = atoi(argv[2]); // номер процесу
        // відкриваємо іменований семафор, створений батьківським процесом
        HANDLE hSemaphore = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "Global\\Lab1_Semaphore");
        if (!hSemaphore) {
            printf("Нащадок %d: Помилка відкриття семафора!\n", childId);
            return 1;
        }
        printf("Нащадок %d: створено. Очікую доступу (семафор)...\n", childId);
        // організація очікування (не більше 3-х процесів одночасно)
        WaitForSingleObject(hSemaphore, INFINITE);
        // критична секція
        printf(">>> Нащадок %d: ПРАЦЮЄ.\n", childId);
        Sleep(1500); // Штучна затримка для ілюстрації роботи (1.5 сек)
        printf("<<< Нащадок %d: ЗАВЕРШУЄ РОБОТУ.\n", childId);
        // кінець критичної секції
        // звільнення місця для наступних процесів
        ReleaseSemaphore(hSemaphore, 1, NULL);
        // закриваємо дескриптори
        CloseHandle(hSemaphore);
        CloseHandle(hInheritedMutex);
        return 0;
    }
    // головний процес (якщо програму запущено без аргументів)
    printf("ГОЛОВНИЙ ПРОЦЕС ЗАПУЩЕНО. PID: %lu\n\n", GetCurrentProcessId());
    // створення анонімного м'ютекса для передачі по спадковості
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE; // TRUE дозволяє спадковість дескриптора
    HANDLE hAnonMutex = CreateMutexA(&sa, FALSE, NULL);
    //створення семафора (обмеження: не більше 3-х потоків одночасно)
    HANDLE hSemaphore = CreateSemaphoreA(NULL, 3, 3, "Global\\Lab1_Semaphore");
    //  створення 10 процесів нащадків
    const int NUM_CHILDREN = 10;
    HANDLE hChildren[NUM_CHILDREN];
    for (int i = 0; i < NUM_CHILDREN; ++i)
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmdLn[256];
        // формуємо командний рядок: "ім'я_файлу.exe дескриптор_м'ютекса номер_процесу"
        sprintf_s(cmdLn, "\"%s\" %llu %d", argv[0], (unsigned long long)(size_t)hAnonMutex, i + 1);
        // TRUE у 5-му параметрі ОБОВ'ЯЗКОВЕ для передачі об'єктів по спадковості
        if (CreateProcessA(NULL, cmdLn, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            hChildren[i] = pi.hProcess;
            CloseHandle(pi.hThread); 
        }
        else
        {
            printf("Помилка створення процесу %d\n", i + 1);
        }
    }
    // таймер(5 секунд)
    HANDLE hTimer = CreateWaitableTimerA(NULL, FALSE, "Global\\Lab1_Timer");
    LARGE_INTEGER dueTime;
    // час вказується у 100-наносекундних інтервалах,від'ємне значення = відносний час від поточного 
    // 5 секунд = 5 * 10,000,000
    dueTime.QuadPart = -50000000LL;
    printf("\nБатьківський процес: Запуск таймера на 5 секунд...\n");
    SetWaitableTimer(hTimer, &dueTime, 0, NULL, NULL, FALSE);
    // очікування спрацьовування таймера
    WaitForSingleObject(hTimer, INFINITE);
    printf("\nБатьківський процес: 5 секунд пройшло! Перевірка стану процесів-нащадків...\n\n");
    // перевірка завершення роботи всіх процесів
    // чекаємо поки всі 10 процесів перейдуть у сигнальний стан (завершаться)
    WaitForMultipleObjects(NUM_CHILDREN, hChildren, TRUE, INFINITE);
    printf("\nУсі 10 процесів-нащадків успішно завершили свою роботу (перейшли в сигнальний стан)!\n");
    // очищення ресурсів
    for (int i = 0; i < NUM_CHILDREN; ++i)
    {
        CloseHandle(hChildren[i]);
    }
    CloseHandle(hTimer);
    CloseHandle(hSemaphore);
    CloseHandle(hAnonMutex);
    CloseHandle(hSingleInstanceMutex);

    system("pause");
    return 0;
}