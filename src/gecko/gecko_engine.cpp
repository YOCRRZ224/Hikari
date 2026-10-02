#include "gecko_engine.hpp"

GeckoEngine::GeckoEngine()
    : initialized_(false)
{
}

GeckoEngine::~GeckoEngine()
{
    shutdown();
}

bool GeckoEngine::initialize()
{
    if (initialized_)
        return true;

    /*
     * Gecko initialization will live here.
     *
     * We deliberately do not dlopen libxul.so or depend
     * on Firefox's private symbols. The actual Gecko
     * runtime integration will be added once we have
     * the appropriate Mozilla build/runtime interface.
     */

    initialized_ = true;
    return true;
}

void GeckoEngine::shutdown()
{
    if (!initialized_)
        return;

    initialized_ = false;
}

bool GeckoEngine::is_initialized() const
{
    return initialized_;
}
