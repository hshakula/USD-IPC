#include "plugin.h"
#include "dcc.h"

int main() {
    pxr::DCC dcc;
    pxr::Plugin plugin(dcc);

    while (true) {
        dcc.Update();
        plugin.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
