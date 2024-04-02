#include "main.hpp"

#include "main_window.hpp"

int APIENTRY wWinMain(_In_ HINSTANCE _instance, _In_opt_ HINSTANCE _prev_instance, _In_ LPWSTR _cmdline, _In_ int _cmdshow)
{
    app::main_window mw(_instance);

    if (!mw.init())
    {
        return 1;
    }

    return mw.loop();
}
