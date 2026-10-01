#pragma once
#include <string>
#include "draw.h"

class StatusBar
{
public:
    bool        connected  = false;
    int         ping       = 0;
    int         players    = 0;
    int         maxPlayers = 16;
    std::string serverName;

    void Tick()
    {
        if (!connected) return;

        constexpr float x   = 0.740f;
        constexpr float y   = 0.008f;
        constexpr float w   = 0.252f;
        constexpr float h   = 0.030f;
        constexpr float pad = 0.007f;
        constexpr float sc  = 0.255f;

        Draw::Rect(x, y, w, h, Draw::C::StatusBg);
        Draw::Rect(x, y + h - 0.002f, w, 0.002f,
            {Draw::C::AccentDim.r, Draw::C::AccentDim.g, Draw::C::AccentDim.b, 200});

        Draw::Text("*", x + pad, y + 0.006f, sc, {80, 200, 80, 255});
        float dotW = Draw::TextWidth("* ", sc);
        Draw::Text(serverName, x + pad + dotW, y + 0.006f, sc, Draw::C::TextNormal);

        std::string right = std::to_string(ping) + "ms  "
                          + std::to_string(players) + "/" + std::to_string(maxPlayers);
        Draw::TextRight(right, x + w - pad, y + 0.006f, sc, Draw::PingColor(ping));
    }
};
