#include <windows.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);

    if (argc == 2)
    {
        char* mode = argv[1];

        // --- ПРОЦЕС А: Переказ з Рахунку 1 на Рахунок 2 ---
        if (strcmp(mode, "A") == 0) {
            printf("[Процес А] Ініційовано переказ: Рахунок 1 -> Рахунок 2.\n");

            printf("[Процес А] Блокування Рахунку 1...\n");
            HANDLE m1 = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "Global\\Bank_Account_1");
            WaitForSingleObject(m1, INFINITE);
            printf("[Процес А] Рахунок 1 успішно заблоковано.\n");

            // Затримка, щоб Процес Б встиг заблокувати Рахунок 2
            Sleep(1000);

            printf("[Процес А] Блокування Рахунку 2...\n");
            HANDLE m2 = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "Global\\Bank_Account_2");

            // тут виникає deadlock
            // Якщо інший процес буде примусово знищено, ми отримаємо WAIT_ABANDONED 
            if (WaitForSingleObject(m2, INFINITE) == WAIT_ABANDONED) return 1;

            printf("[Процес А] Переказ успішно завершено.\n");
            ReleaseMutex(m2); ReleaseMutex(m1);
        }

        // --- ПРОЦЕС Б: Переказ з Рахунку 2 на Рахунок 1 ---
        else if (strcmp(mode, "B") == 0) {
            printf("[Процес Б] Ініційовано переказ: Рахунок 2 -> Рахунок 1.\n");

            printf("[Процес Б] Блокування Рахунку 2...\n");
            HANDLE m2 = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "Global\\Bank_Account_2");
            WaitForSingleObject(m2, INFINITE);
            printf("[Процес Б] Рахунок 2 успішно заблоковано.\n");

            // Затримка, щоб Процес А встиг заблокувати Рахунок 1
            Sleep(1000);

            printf("[Процес Б] Блокування Рахунку 1...\n");
            HANDLE m1 = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "Global\\Bank_Account_1");

            // Ось тут виникає Deadlock
            if (WaitForSingleObject(m1, INFINITE) == WAIT_ABANDONED) return 1;

            printf("[Процес Б] Переказ успішно завершено.\n");
            ReleaseMutex(m1); ReleaseMutex(m2);
        }
        return 0;
    }


    // 1. Створення спільних ресурсів (М'ютексів)
    HANDLE hAcc1 = CreateMutexA(NULL, FALSE, "Global\\Bank_Account_1");
    HANDLE hAcc2 = CreateMutexA(NULL, FALSE, "Global\\Bank_Account_2");

    // 2. Одночасний запуск двох процесів-транзакцій
    STARTUPINFOA siA = { sizeof(siA) }, siB = { sizeof(siB) };
    PROCESS_INFORMATION piA, piB;
    char cmdA[256], cmdB[256];

    sprintf_s(cmdA, "\"%s\" A", argv[0]);
    sprintf_s(cmdB, "\"%s\" B", argv[0]);

    printf("[Головний Процес] Запуск паралельних транзакцій (Процес А та Процес Б)...\n");
    CreateProcessA(NULL, cmdA, NULL, NULL, FALSE, 0, NULL, NULL, &siA, &piA);
    CreateProcessA(NULL, cmdB, NULL, NULL, FALSE, 0, NULL, NULL, &siB, &piB);

    // 3. Очікування завершення з таймаутом (5 секунд)
    HANDLE hChildren[2] = { piA.hProcess, piB.hProcess };
    printf("[Головний Процес] Очікування завершення (Таймаут 5 сек)...\n\n");

    DWORD waitResult = WaitForMultipleObjects(2, hChildren, TRUE, 5000);

    // 4. Обробка результату (виявлення Deadlock)
    if (waitResult == WAIT_TIMEOUT) {
        printf("\n[Головний Процес] Виявлено взаємне блокування.\n");
        printf(" -> Процес А заблокував Рахунок 1 і чекає на Рахунок 2.\n");
        printf(" -> Процес Б заблокував Рахунок 2 і чекає на Рахунок 1.\n");

        printf("[Головний Процес] Примусове завершення завислих процесів...\n");
        TerminateProcess(piA.hProcess, 1);
        TerminateProcess(piB.hProcess, 1);
        printf("[Головний Процес] Процеси знищено. Ресурси звільнено.\n");
    }
    else {
        printf("\n[Головний Процес] Транзакції завершились успішно .\n");
    }

    // Очищення дескрипторів
    CloseHandle(piA.hProcess); CloseHandle(piA.hThread);
    CloseHandle(piB.hProcess); CloseHandle(piB.hThread);
    CloseHandle(hAcc1); CloseHandle(hAcc2);

    printf("\n");
    system("pause");
    return 0;
}