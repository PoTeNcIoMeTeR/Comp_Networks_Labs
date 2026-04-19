#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>

static const int N = 4; 

static double A[N][N + 1];         
static double solution[N];         
static double init_A[N][N + 1];    

static HANDLE hMatrixMutex;   
static HANDLE hPrintMutex;     
static HANDLE hSemaphore;      
static HANDLE hBarrierEvent1;  
static HANDLE hBarrierEvent2;  
static volatile LONG barrierCount = 0;

static HANDLE hPipeRead, hPipeWrite; 

static void barrier_wait() {
    // атомарно збільшуємо лічильник потоків
    LONG arrived = InterlockedIncrement(&barrierCount);

    if (arrived == N) {
        ResetEvent(hBarrierEvent2);
        SetEvent(hBarrierEvent1);
    }
    else {
        WaitForSingleObject(hBarrierEvent1, INFINITE);
    }

    // фаза виходу: гарантуємо, що всі потоки пройшли, перш ніж готувати бар'єр до нового циклу
    if (InterlockedDecrement(&barrierCount) == 0) {
        ResetEvent(hBarrierEvent1);
        SetEvent(hBarrierEvent2);
    }
    else {
        WaitForSingleObject(hBarrierEvent2, INFINITE);
    }
}

//  виведення тексту з різних потоків
static void safe_print(const std::string& msg) {
    WaitForSingleObject(hPrintMutex, INFINITE);
    std::cout << msg << std::flush;
    ReleaseMutex(hPrintMutex);
}

// головна функція обчислень для кожного потоку
static DWORD WINAPI elimination_thread(LPVOID param) {
    int tid = (int)(intptr_t)param;

    for (int step = 0; step < N - 1; ++step) {
        if (tid > step) {
            WaitForSingleObject(hSemaphore, INFINITE); 
            WaitForSingleObject(hMatrixMutex, INFINITE); 

            double factor = A[tid][step] / A[step][step];
            for (int j = step; j <= N; ++j) {
                A[tid][j] -= factor * A[step][j];
            }

            ReleaseMutex(hMatrixMutex); 
            ReleaseSemaphore(hSemaphore, 1, NULL);

            std::ostringstream oss;
            oss << "  [потік " << tid << "] обробив рядок на кроці " << step << "\n";
            safe_print(oss.str());
        }
        barrier_wait();
    }

    for (int row = N - 1; row >= 0; --row) {
        if (tid == row) {
            double sum = A[row][N];
            for (int j = row + 1; j < N; ++j) {
                sum -= A[row][j] * solution[j];
            }
            solution[row] = sum / A[row][row];

            std::ostringstream oss;
            oss << "  [потік " << tid << "] обчислив x[" << row << "] = " << solution[row] << "\n";
            safe_print(oss.str());
        }
        barrier_wait();
    }

    if (tid == 0) {
        DWORD written;
        WriteFile(hPipeWrite, solution, sizeof(double) * N, &written, NULL);
        CloseHandle(hPipeWrite); 
    }

    return 0;
}

int main() {
    system("chcp 65001 > nul");
    double data[N][N + 1] = {
    { 2,  1, -1,  1,  5},
    { 1,  3,  1, -2,  2},
    { 1, -1,  4,  1, 10},
    { 1,  1, -1,  5, 18}
    };

    // копіюємо дані в робочу та резервну матриці
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j <= N; ++j) {
            A[i][j] = init_A[i][j] = data[i][j];
        }
    }

    // створення об'єктів синхронізації winapi
    hMatrixMutex = CreateMutex(NULL, FALSE, NULL);
    hPrintMutex = CreateMutex(NULL, FALSE, NULL);
    hSemaphore = CreateSemaphore(NULL, N / 2, N / 2, NULL); // максимум n/2 потоків одночасно
    hBarrierEvent1 = CreateEvent(NULL, TRUE, FALSE, NULL);  // подія з ручним скиданням
    hBarrierEvent2 = CreateEvent(NULL, TRUE, FALSE, NULL);

    // створення анонімного каналу для зв'язку між потоками
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0);

    HANDLE hThreads[N];
    std::cout << "запуск обчислень слар (гаусс)...\n";

    // запуск потоків
    for (int i = 0; i < N; ++i) {
        hThreads[i] = CreateThread(NULL, 0, elimination_thread, (LPVOID)(intptr_t)i, 0, NULL);
    }

    double result[N];
    DWORD bytesRead;
    // головний потік чекає на дані з pipe та виводить їх
    if (ReadFile(hPipeRead, result, sizeof(double) * N, &bytesRead, NULL)) {
        std::cout << "\nрезультати отримані через pipe:\n";
        for (int i = 0; i < N; ++i) {
            std::cout << "  x" << (i + 1) << " = " << std::fixed << std::setprecision(4) << result[i] << "\n";
        }
    }

    // чекаємо повного завершення всіх потоків
    WaitForMultipleObjects(N, hThreads, TRUE, INFINITE);

    // перевірка розв'язку
    std::cout << "\nперевірка:\n";
    for (int i = 0; i < N; ++i) {
        double lhs = 0;
        // підстановка
        for (int j = 0; j < N; ++j) lhs += init_A[i][j] * result[j];
        std::cout << "  рівняння " << i + 1 << ": " << lhs << " == " << init_A[i][N]
            << (std::abs(lhs - init_A[i][N]) < 1e-6 ? " (ok)" : " (error)") << "\n";
    }

    // закриваємо всі дескриптори
    for (int i = 0; i < N; ++i) CloseHandle(hThreads[i]);
    CloseHandle(hMatrixMutex); CloseHandle(hPrintMutex);
    CloseHandle(hSemaphore); CloseHandle(hBarrierEvent1); CloseHandle(hBarrierEvent2);
    CloseHandle(hPipeRead);

    return 0;
}