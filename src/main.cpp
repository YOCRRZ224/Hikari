#include <adwaita.h>

#include "browser_window.hpp"

int main(int argc, char* argv[])
{
    GtkApplication* app = gtk_application_new(
        "is.a.dev.yocrrz.Hikari",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(
        app,
        "activate",
        G_CALLBACK(on_application_activate),
        nullptr
    );

    int status = g_application_run(
        G_APPLICATION(app),
        argc,
        argv
    );

    g_object_unref(app);

    return status;
}
