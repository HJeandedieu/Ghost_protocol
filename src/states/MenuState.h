#pragma once
#include <array>
#include <functional>
#include <string>

#include "core/Config.h"
#include "core/Input.h"
#include "core/SaveStore.h"
#include "states/IState.h"
#include "ui/Widgets.h"
class Renderer;
class MenuState : public IState {
   public:
    MenuState(const Input& input, Renderer& renderer, UiConfig config, const Settings& settings,
              std::function<void()> start, std::function<bool(const Settings&)> save,
              std::function<void()> quit, std::string error = "",
              std::function<void()> settingsBack = {}, const Scores* scores = nullptr);
    void enter() override {}
    void exit() override {}
    void update(float dt) override;
    void render(float alpha) override;

   private:
    enum class Page { Menu, Settings, Credits };
    WidgetBounds bounds(int row) const;
    void changePage(Page page);
    int rows() const;
    const Input& input_;
    Renderer& renderer_;
    UiConfig config_;
    const Settings& settings_;
    std::function<void()> start_;
    std::function<bool(const Settings&)> save_;
    std::function<void()> quit_;
    std::function<void()> settingsBack_;
    std::string error_;
    const Scores* scores_ = nullptr;
    Page page_ = Page::Menu;
    int focus_ = 0;
    int dragged_ = -1;
    float elapsed_ = 0;
    Vec2 lastPointer_{-1, -1};
    std::array<Button, 9> buttons_;
};
