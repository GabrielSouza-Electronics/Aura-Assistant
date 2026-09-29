#ifndef MODEL_HPP
#define MODEL_HPP
#include <stdint.h>
#include <gui/common/MenuSound.hpp>

class ModelListener;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();
    void playMenuSound(MenuSound sound);
    void setHeroBreath(uint8_t level);
    void setCarouselLED(float angle, uint8_t visibility);
    void pulseMenuEnterLED();
    bool startupReady() const;
    void completeStartup();
protected:
    ModelListener* modelListener;
    uint8_t lastWifiLevel;   /* last level delivered to the listener */
};

#endif // MODEL_HPP
