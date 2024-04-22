#include "main.hpp"

#include "main_window.hpp"

#include <combaseapi.h>

int APIENTRY wWinMain(_In_ HINSTANCE _instance, _In_opt_ HINSTANCE _prev_instance, _In_ LPWSTR _cmdline, _In_ int _cmdshow)
{
    auto hr = ::CoInitializeEx(0, COINIT_MULTITHREADED);
    if (hr == S_OK)
    {
        app::main_window mw(_instance);

        if (!mw.init())
        {
            ::CoUninitialize();
            return 1;
        }
        ::CoUninitialize();
        return mw.loop();
    }
    return 0;
}
