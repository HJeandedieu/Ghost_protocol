#pragma once
class Player;
// Presentation state only; never writes entity health or armor.
class HealthHud {
   public:
    void damaged() { flashRemaining_ = kDrainSeconds; }
    void update(float dt, const Player& player);
    bool initialized() const { return initialized_; }
    float displayedHp() const { return hp_.shown; }
    float displayedArmor() const { return armor_.shown; }
    float maximumHp() const { return maximumHp_; }
    float maximumArmor() const { return maximumArmor_; }
    float flashFraction() const { return flashRemaining_ / kDrainSeconds; }

   private:
    static constexpr float kDrainSeconds = 0.3f;
    struct Meter {
        float shown = 0, start = 0, target = 0, elapsed = 0;
    };
    void advance(Meter& meter, float value, float dt);
    Meter hp_, armor_;
    bool initialized_ = false;
    float maximumHp_ = 0;
    float maximumArmor_ = 0;
    float flashRemaining_ = 0;
};
