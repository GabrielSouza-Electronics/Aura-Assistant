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

    void bind(Model* m)
    {
        model = m;
    }
protected:
    Model* model;
};

#endif // MODELLISTENER_HPP
