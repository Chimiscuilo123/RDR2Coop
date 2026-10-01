#pragma once
#include <string>
#include <vector>
#include "draw.h"

class PlayerListPanel
{
public:
    bool        visible = false;
    std::string serverName = "RDR2 Coop";
    int         maxPlayers = 16;

    struct Entry { std::string name; int ping; bool local; };

    void Tick(const std::string& localName, int localPing,
              const std::vector<Entry>& remotes)
    {
        if (!visible) return;

        constexpr float x    = 0.730f;
        constexpr float w    = 0.260f;
        constexpr float rowH = 0.030f;
        constexpr float pad  = 0.008f;

        std::vector<Entry> rows;
        rows.push_back({localName, localPing, true});
        for (auto& r : remotes) rows.push_back(r);

        float headerH = Draw::HEADER_H * 0.65f;
        float bodyH   = rows.size() * rowH;
        float panelY  = 0.04f;

        // Header
        Draw::Rect(x, panelY, w, headerH, Draw::C::HeaderTop);
        Draw::Rect(x + w - 0.003f, panelY, 0.003f, headerH, Draw::C::Accent);
        Draw::Divider(x, panelY + headerH - 0.003f, w);

        Draw::Text("Jugadores", x + pad, panelY + 0.012f, 0.34f, Draw::C::TextTitle, 1);

        std::string cnt = std::to_string(rows.size()) + "/" + std::to_string(maxPlayers);
        Draw::TextRight(cnt, x + w - pad, panelY + 0.014f, Draw::SCALE_SUB, Draw::C::TextSub);
        Draw::Text(serverName, x + pad, panelY + headerH - 0.020f, Draw::SCALE_SUB, Draw::C::TextSub);

        // Body
        float bodyY = panelY + headerH;
        Draw::Rect(x, bodyY, w, bodyH, Draw::C::PanelBg);
        Draw::Rect(x + w - 0.003f, bodyY, 0.003f, bodyH,
            {Draw::C::AccentDim.r, Draw::C::AccentDim.g, Draw::C::AccentDim.b, 160});

        for (int i = 0; i < (int)rows.size(); i++)
        {
            auto& r  = rows[i];
            float ry = bodyY + i * rowH;

            if (i % 2 == 1) Draw::Rect(x, ry, w, rowH, Draw::C::RowAlt);
            if (r.local)    Draw::Rect(x, ry, w, rowH, Draw::C::SelBg);

            std::string prefix = r.local ? "* " : "  ";
            Draw::Color tc = r.local ? Draw::C::TextSelected : Draw::C::TextNormal;
            Draw::Text(prefix + r.name, x + pad, ry + 0.006f, Draw::SCALE_OPTION, tc);

            std::string pingStr = r.ping < 0 ? "--" : (std::to_string(r.ping) + "ms");
            Draw::TextRight(pingStr, x + w - pad, ry + 0.006f,
                Draw::SCALE_VALUE, Draw::PingColor(r.ping));
        }

        // Footer
        float footY = bodyY + bodyH;
        Draw::Rect(x, footY, w, Draw::FOOTER_H, Draw::C::FooterBg);
        Draw::Rect(x, footY, w, 0.002f,
            {Draw::C::AccentDim.r, Draw::C::AccentDim.g, Draw::C::AccentDim.b, 140});
        Draw::Text("Manten TAB para ver", x + pad, footY + 0.005f,
            Draw::SCALE_FOOTER, Draw::C::TextSub);
    }
};
