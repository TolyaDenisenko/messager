// LanMessengerAgent: Windows-служба. Запуск с ключом --console — как обычное консольное приложение (отладка).
#include <QCoreApplication>
#include <QMetaObject>
#include <windows.h>

#include "agentcore.h"

static const wchar_t kServiceName[] = L"LanMessengerAgent";
static SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
static SERVICE_STATUS g_status{};
static QCoreApplication *g_app = nullptr;

static void reportStatus(DWORD state, DWORD exitCode = NO_ERROR)
{
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwControlsAccepted = (state == SERVICE_START_PENDING) ? 0 : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_status.dwWin32ExitCode = exitCode;
    g_status.dwWaitHint = (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING) ? 5000 : 0;
    SetServiceStatus(g_statusHandle, &g_status);
}

static DWORD WINAPI serviceCtrl(DWORD ctrl, DWORD, LPVOID, LPVOID)
{
    if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN) {
        reportStatus(SERVICE_STOP_PENDING);
        if (g_app)
            QMetaObject::invokeMethod(g_app, &QCoreApplication::quit, Qt::QueuedConnection);
        return NO_ERROR;
    }
    return ctrl == SERVICE_CONTROL_INTERROGATE ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
}

static int runApp()
{
    int argc = 1;
    char name[] = "LanMessengerAgent";
    char *argv[] = {name, nullptr};
    QCoreApplication app(argc, argv);
    g_app = &app;
    AgentCore core;
    const bool ok = core.start();
    if (g_statusHandle)
        reportStatus(ok ? SERVICE_RUNNING : SERVICE_STOPPED, ok ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR);
    const int rc = ok ? app.exec() : 1;
    g_app = nullptr;
    return rc;
}

static void WINAPI serviceMain(DWORD, LPWSTR *)
{
    g_statusHandle = RegisterServiceCtrlHandlerExW(kServiceName, serviceCtrl, nullptr);
    if (!g_statusHandle)
        return;
    reportStatus(SERVICE_START_PENDING);
    runApp();
    reportStatus(SERVICE_STOPPED);
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
        if (QByteArray(argv[i]) == "--console")
            return runApp();

    SERVICE_TABLE_ENTRYW table[] = {{const_cast<LPWSTR>(kServiceName), serviceMain}, {nullptr, nullptr}};
    if (!StartServiceCtrlDispatcherW(table)) {
        if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
            return runApp(); // запущен не службой — работаем как консольное приложение
        return 1;
    }
    return 0;
}
