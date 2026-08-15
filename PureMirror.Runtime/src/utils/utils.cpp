#include "pch.h"

#include "utils.h"

#include <console/console.h>

static BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
    const auto isMainWindow = [handle]() { return GetWindow(handle, GW_OWNER) == nullptr && IsWindowVisible(handle); };

    DWORD pID = 0;
    GetWindowThreadProcessId(handle, &pID);

    if (GetCurrentProcessId() != pID || !isMainWindow() || handle == GetConsoleWindow())
        return TRUE;

    *reinterpret_cast<HWND*>(lParam) = handle;

    return FALSE;
}

namespace Utils
{
    HWND GetProcessWindow()
    {
        HWND hwnd = nullptr;
        EnumWindows(::EnumWindowsCallback, reinterpret_cast<LPARAM>(&hwnd));

        while (!hwnd)
        {
            EnumWindows(::EnumWindowsCallback, reinterpret_cast<LPARAM>(&hwnd));
            LOG("[!] Waiting for window to appear.\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        char name[128];
        GetWindowTextA(hwnd, name, RTL_NUMBER_OF(name));
        LOG("[+] Got window with name: '%s'\n", name);

        return hwnd;
    }

    int GetCorrectDXGIFormat(int eCurrentFormat)
    {
        switch (eCurrentFormat)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        return eCurrentFormat;
    }

    // see https://github.com/jhu-cisst/cisst/blob/master/cisstOSAbstraction/code/osaSleep.cpp
    void osSleep(double timeInSeconds)
    {
#ifdef __APPLE__

        struct timespec ts;
        ts.tv_sec = static_cast<long>(timeInSeconds);
        ts.tv_nsec = static_cast<long>((timeInSeconds - ts.tv_sec) * nSecInSec);
        nanosleep(&ts, NULL);
#endif

        //#elif (CISST_OS == CISST_LINUX_RTAI)
        //        // check if this called by a real time task or not
        //        if (rt_is_hard_real_time(rt_buddy())) {
        //            rt_sleep(nano2count(static_cast< long >(timeInSeconds * nSecInSec)));
        //        } else {
        //            struct timespec ts;
        //            ts.tv_sec = static_cast< long >(timeInSeconds);
        //            ts.tv_nsec = static_cast< long >((timeInSeconds - ts.tv_sec) * nSecInSec);
        //            nanosleep(&ts, NULL);
        //        }

        //#elif (CISST_OS == CISST_LINUX_XENOMAI)
        //
        //        if (rt_task_self() != NULL) {
        //            RTIME ns = RTIME(timeInSeconds * 1000000000);
        //            int retval = 0;
        //            retval = rt_task_sleep(rt_timer_ns2ticks(ns));
        //            if (retval != 0) {
        //                CMN_LOG_RUN_ERROR << CMN_LOG_DETAILS << "rt_task_sleep failed. " << strerror(retval) << ": "
        //                << retval << std::endl;
        //            }
        //        } else {
        //            struct timespec ts;
        //            ts.tv_sec = static_cast< long >(timeInSeconds);
        //            ts.tv_nsec = static_cast< long >((timeInSeconds - ts.tv_sec) * nSecInSec);
        //            nanosleep(&ts, NULL);
        //        }

#ifdef _WIN32
        // A waitable timer seems to be better than the Windows Sleep().

        // We don't name the timer (third parameter) because CreateWaitableTimer will fail if the name
        // matches an existing name (e.g., if two threads call osaSleep).
        HANDLE waitTimer = CreateWaitableTimer(nullptr, TRUE, nullptr);
        if (waitTimer == nullptr)
        {
            return;
        }

        LARGE_INTEGER dueTime{};
        timeInSeconds *= -10.0 * 1000.0 * 1000.0;
        dueTime.QuadPart = static_cast<LONGLONG>(timeInSeconds);  // dueTime is in 100ns

        if (!SetWaitableTimer(waitTimer, &dueTime, 0, nullptr, nullptr, FALSE))
        {
            CloseHandle(waitTimer);
            return;
        }

        WaitForSingleObject(waitTimer, INFINITE);
        CloseHandle(waitTimer);
#endif

#ifdef __QNX__
        struct timespec ts;
        _uint64 nsec = (_uint64)(timeInSeconds * 1000.0 * 1000.0 * 1000.0);
        nsec2timespec(&ts, nsec);
        nanosleep(&ts, NULL);
#endif
    }
}  // namespace Utils
