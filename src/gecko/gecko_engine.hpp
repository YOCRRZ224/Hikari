#pragma once

class GeckoEngine
{
public:
    GeckoEngine();
    ~GeckoEngine();

    bool initialize();
    void shutdown();

    bool is_initialized() const;

private:
    bool initialized_;
};
