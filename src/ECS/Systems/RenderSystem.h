#pragma once
#include <entt/entt.hpp>
#include "ECS/Components.h"

class RenderSystem {
public:
    void Draw(entt::registry& registry);
    void HandleInput(entt::registry& registry);   // settlement click-to-select

private:
    entt::entity selectedSettlement = entt::null;

    void DrawStockpilePanel(const Settlement& s, const Stockpile& sp) const;
};
