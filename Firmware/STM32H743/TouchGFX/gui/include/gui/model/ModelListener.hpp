#ifndef MODELLISTENER_HPP
#define MODELLISTENER_HPP

#include <gui/model/Model.hpp>

class ModelListener
{
public:
    ModelListener() : model(0) {}
    
    virtual ~ModelListener() {}
    virtual void handUpdated(bool, float, float) {}
    virtual void handClicked() {}
    /* 0 = no IP (crossed icon), 1..3 = signal bars. Sent only on change. */
    virtual void wifiLevelUpdated(uint8_t) {}
    /* Settings menu. item: 0 Wi-Fi, 1 Bluetooth, 2 Brightness (same order
       as APP_UISetting_t). Sent only on change. */
    virtual void settingTextUpdated(uint8_t, const char*) {}
    /* 0 (min) .. 10 (max). Sent only on change. */
    virtual void volumeUpdated(uint8_t) {}

    void bind(Model* m)
    {
        model = m;
    }
protected:
    Model* model;
};

#endif // MODELLISTENER_HPP
