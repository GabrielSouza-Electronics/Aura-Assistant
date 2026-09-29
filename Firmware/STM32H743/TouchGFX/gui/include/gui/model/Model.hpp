#ifndef MODEL_HPP
#define MODEL_HPP
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
protected:
    ModelListener* modelListener;
};

#endif // MODEL_HPP
