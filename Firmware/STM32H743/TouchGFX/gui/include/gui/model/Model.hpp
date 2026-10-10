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
    void setChatAudioActive(bool active);
    bool readChatSpeechFrame(uint8_t& request);
    /* Settings: item 0 Wi-Fi, 1 Bluetooth, 2 Brightness, 3 Sound.
       delta +1 = cima (liga / aumenta), -1 = baixo (desliga / diminui). */
    void requestSetting(uint8_t item, int8_t delta);
    void setHeroBreath(uint8_t level);
    void setCarouselLED(float angle, uint8_t visibility);
    void pulseMenuEnterLED();
    void setLEDBreathColor(uint8_t red, uint8_t green, uint8_t blue);
    bool startupReady() const;
    void completeStartup();
protected:
    ModelListener* modelListener;
    uint8_t lastWifiLevel;   /* last level delivered to the listener */
    uint8_t lastVolume;
    uint32_t settingVersion[3];
#if !defined(STM32H743xx)
    /* estado simulado, so para testar a navegacao no simulador */
    bool    simWifi = true;
    bool    simBluetooth = true;
    uint8_t simBrightness = 70;
    uint8_t simVolume = 5;
#endif
};

#endif // MODEL_HPP
