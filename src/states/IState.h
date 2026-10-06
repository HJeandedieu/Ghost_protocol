#pragma once

class IState {
   public:
    virtual ~IState() = default;
    virtual void enter() = 0;
    virtual void exit() = 0;
    virtual void update(float dt) = 0;
    virtual void render(float alpha) = 0;
};
