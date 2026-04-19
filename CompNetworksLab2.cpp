#include <windows.h>
#include <stdio.h>

// Об'єкти синхронізації
CRITICAL_SECTION csPair3;
CRITICAL_SECTION csSharedMem;
HANDLE hEvPos, hEvNeg;
HANDLE hGlobalSemaphore;

HANDLE hFile;
HANDLE hFileMapping;
char* pSharedBuffer;
int sharedOffset = 0;

struct ThreadData {
    int pairId;
    int isPositive;
    HANDLE hHeap;
};

void CheckError(const char* msg) {
    DWORD err = GetLastError();
    if (err != 0 && err != ERROR_ALREADY_EXISTS && err != ERROR_SUCCESS) {
        printf("\n[ERROR] %s: %lu\n", msg, err);
    }
}

DWORD WINAPI ThreadFunction(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;
    int pId = data->pairId;
    int isPos = data->isPositive;

    //обмежуємо роботу не більше 2-х потоків  одночасно
    WaitForSingleObject(hGlobalSemaphore, INFINITE);

    // Критична секція навколо циклу
    if (pId == 3) EnterCriticalSection(&csPair3);

    for (int i = 1; i <= 500; ++i) {

        //  Події всередині циклу
        if (pId == 2) {
            if (isPos) WaitForSingleObject(hEvPos, INFINITE);
            else WaitForSingleObject(hEvNeg, INFINITE);
        }

        int* pNum = (int*)HeapAlloc(data->hHeap, HEAP_ZERO_MEMORY, sizeof(int));
        if (pNum) {
            *pNum = isPos ? i : -i;
            int numTabs = (pId - 1) * 2 + (isPos ? 0 : 1);
            char buffer[128] = "";
            for (int t = 0; t < numTabs; t++) strcat_s(buffer, "\t\t");
            char valStr[32];
            sprintf_s(valStr, "[P%d:%d]", pId, *pNum);
            strcat_s(buffer, valStr);
            strcat_s(buffer, "\n");
            printf("%s", buffer);

            // Запис у проекцію файлу
            EnterCriticalSection(&csSharedMem);
            int len = (int)strlen(buffer);
            if (sharedOffset + len < 1024 * 1024) {
                memcpy(pSharedBuffer + sharedOffset, buffer, len);
                sharedOffset += len;
            }
            LeaveCriticalSection(&csSharedMem);

            HeapFree(data->hHeap, 0, pNum);
        }

        if (pId == 2) {
            if (isPos) SetEvent(hEvNeg);
            else SetEvent(hEvPos);
        }

        if (pId == 1) Sleep(1);
    }

    if (pId == 3) LeaveCriticalSection(&csPair3);

    // Звільняємо місце для наступної пари
    ReleaseSemaphore(hGlobalSemaphore, 1, NULL);
    return 0;
}

int main() {
    SetConsoleOutputCP(1251);

    // 1. Створення реального файлу та проекції
    hFile = CreateFileA("result.txt", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    hFileMapping = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 1024 * 1024, "Lab2Mem");
    CheckError("CreateFileMapping");
    pSharedBuffer = (char*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 1024 * 1024);

    // 2. Ініціалізація
    InitializeCriticalSectionAndSpinCount(&csPair3, 4000);
    InitializeCriticalSection(&csSharedMem);
    hEvPos = CreateEventA(NULL, FALSE, TRUE, NULL);
    hEvNeg = CreateEventA(NULL, FALSE, FALSE, NULL);
    hGlobalSemaphore = CreateSemaphoreA(NULL, 2, 2, NULL);

    HANDLE hThreads[6];
    ThreadData tData[6];
    int idx = 0;

    // 3. Створення потоків (Suspended)
    for (int pId = 1; pId <= 3; ++pId) {
        for (int isPos = 1; isPos >= 0; --isPos) {
            tData[idx].hHeap = HeapCreate(0, 8192, 0);
            tData[idx].pairId = pId;
            tData[idx].isPositive = isPos;

            hThreads[idx] = CreateThread(NULL, 0, ThreadFunction, &tData[idx], CREATE_SUSPENDED, NULL);

            // Пріоритети 
            SetThreadPriority(hThreads[idx], isPos ? THREAD_PRIORITY_ABOVE_NORMAL : THREAD_PRIORITY_BELOW_NORMAL);
            idx++;
        }
    }

    printf("Потоки створено. Натисніть Enter для запуску...\n");
    getchar();

    for (int i = 0; i < 6; ++i) ResumeThread(hThreads[i]);

    WaitForMultipleObjects(6, hThreads, TRUE, INFINITE);

    // Очищення
    UnmapViewOfFile(pSharedBuffer);
    CloseHandle(hFileMapping);
    SetFilePointer(hFile, sharedOffset, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);
    CloseHandle(hFile);

    printf("\nГотово! Результати в файлі result.txt\n");
    return 0;
}