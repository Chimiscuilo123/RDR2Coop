#pragma once
#include <string>
#include <vector>
#include <chrono>
#include "draw.h"

class ChatBox
{
public:
    static constexpr int   MAX_MESSAGES = 10;
    static constexpr float FADE_SEC     = 12.0f;
    static constexpr float PANEL_X      = 0.008f;
    static constexpr float PANEL_W      = 0.310f;
    static constexpr float LINE_H       = 0.027f;
    static constexpr float PAD          = 0.006f;
    static constexpr float BOTTOM_Y     = 0.940f;

    bool        isOpen = false;
    std::string input;

    void Open()  { isOpen = true; input.clear(); }
    void Close() { isOpen = false; input.clear(); }

    void AddChar(char c) { if (input.size() < 100) input += c; }
    void Backspace()     { if (!input.empty()) input.pop_back(); }

    std::string SubmitAndClose()
    {
        std::string msg = input;
        // trim
        while (!msg.empty() && msg.back() == ' ') msg.pop_back();
        isOpen = false; input.clear();
        return msg;
    }

    void AddMessage(const std::string& user, const std::string& msg)
    {
        _messages.push_back({user, msg, Now()});
        if (_messages.size() > MAX_MESSAGES) _messages.erase(_messages.begin());
    }

    void Tick()
    {
        double now = Now();

        if (!isOpen)
        {
            _messages.erase(std::remove_if(_messages.begin(), _messages.end(),
                [&](const Entry& e){ return now - e.time > FADE_SEC; }),
                _messages.end());
        }

        if (_messages.empty() && !isOpen) return;

        int   count  = (int)_messages.size();
        int   extra  = isOpen ? 1 : 0;
        float totalH = (count + extra) * LINE_H + PAD * 2;
        float startY = BOTTOM_Y - totalH;

        Draw::Rect(PANEL_X, startY, PANEL_W, totalH, Draw::C::ChatBg);
        Draw::Rect(PANEL_X, startY, 0.003f, totalH,
            {Draw::C::AccentDim.r, Draw::C::AccentDim.g, Draw::C::AccentDim.b, 160});

        for (int i = 0; i < count; i++)
        {
            auto& e   = _messages[i];
            float y   = startY + PAD + i * LINE_H;
            float age = (float)(now - e.time);
            int alpha = isOpen ? 230 : (int)std::max(0.f, 230.f * (1.f - age / FADE_SEC));

            bool  sys   = (e.username == "Sistema");
            Draw::Color uc = sys ? Draw::Color{180,180,60,alpha}
                                 : Draw::Color{Draw::C::Accent.r, Draw::C::Accent.g, Draw::C::Accent.b, alpha};

            std::string tag = "[" + e.username + "] ";
            float tagW = Draw::TextWidth(tag, Draw::SCALE_CHAT);
            Draw::Text(tag, PANEL_X + PAD + 0.003f, y, Draw::SCALE_CHAT, uc);
            Draw::Text(e.message, PANEL_X + PAD + 0.003f + tagW, y, Draw::SCALE_CHAT,
                {Draw::C::TextNormal.r, Draw::C::TextNormal.g, Draw::C::TextNormal.b, alpha});
        }

        if (isOpen)
        {
            float y = startY + PAD + count * LINE_H;
            Draw::Rect(PANEL_X, y, PANEL_W, LINE_H, {20, 15, 8, 220});
            Draw::Rect(PANEL_X, y, PANEL_W, 0.002f, Draw::C::Accent);
            Draw::Text(">", PANEL_X + PAD, y + 0.003f, Draw::SCALE_CHAT + 0.04f, Draw::C::Accent);
            Draw::Text(input + "_", PANEL_X + PAD + 0.016f, y + 0.004f, Draw::SCALE_CHAT,
                {220, 220, 255, 255});
        }
    }

private:
    struct Entry { std::string username, message; double time; };
    std::vector<Entry> _messages;

    static double Now()
    {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }
};
