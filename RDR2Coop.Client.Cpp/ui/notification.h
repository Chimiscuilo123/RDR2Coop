#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "draw.h"

enum NotifyType
{
    NOTIFY_ADVANCED,
    NOTIFY_TIP,
    NOTIFY_TOP,
    NOTIFY_OBJECTIVE,
    NOTIFY_CENTER_TEXT,
    NOTIFY_RIGHT_TEXT
};

struct Notification
{
    std::string title;
    std::string subtitle;
    NotifyType type;
    int duration;   // ms
    DWORD startMs;
    std::string textureDict;
    std::string textureName;

    Notification(const std::string& t, const std::string& s, NotifyType ty, int dur,
                 const std::string& dict = "", const std::string& tex = "")
        : title(t), subtitle(s), type(ty), duration(dur), startMs(GetTickCount()),
          textureDict(dict), textureName(tex) {}

    float GetAlpha() const
    {
        DWORD elapsed = GetTickCount() - startMs;
        if (elapsed >= (DWORD)duration) return 0.0f;
        float fadeOut = (duration > 500) ? std::min(1.0f, (float)(duration - elapsed) / 250.0f) : 1.0f;
        return std::min(1.0f, fadeOut);
    }

    bool IsExpired() const { return GetTickCount() - startMs >= (DWORD)duration; }
};

class NotifyManager
{
public:
    std::vector<Notification> notifications;

    void Advanced(const std::string& title, const std::string& subtitle,
                  const std::string& dict = "generic_textures",
                  const std::string& tex = "tick", int duration = 4000)
    {
        notifications.push_back(Notification(title, subtitle, NOTIFY_ADVANCED, duration, dict, tex));
    }

    void Tip(const std::string& text, int duration = 4000)
    {
        notifications.push_back(Notification(text, "", NOTIFY_TIP, duration));
    }

    void Top(const std::string& text, const std::string& location = "", int duration = 4000)
    {
        notifications.push_back(Notification(text, location, NOTIFY_TOP, duration));
    }

    void Objective(const std::string& text, int duration = 4000)
    {
        notifications.push_back(Notification(text, "", NOTIFY_OBJECTIVE, duration));
    }

    void CenterText(const std::string& text, int duration = 3000)
    {
        notifications.push_back(Notification(text, "", NOTIFY_CENTER_TEXT, duration));
    }

    void RightText(const std::string& text, int duration = 4000)
    {
        notifications.push_back(Notification(text, "", NOTIFY_RIGHT_TEXT, duration));
    }

    void Tick()
    {
        float sw = Draw::SCREEN_WIDTH;
        float sh = Draw::SCREEN_HEIGHT;

        for (int i = (int)notifications.size() - 1; i >= 0; i--)
        {
            auto& n = notifications[i];
            if (n.IsExpired()) { notifications.erase(notifications.begin() + i); continue; }

            float alpha = n.GetAlpha();
            int a = (int)(alpha * 255);

            switch (n.type)
            {
                case NOTIFY_ADVANCED:
                {
                    float x = 20, y = 20 + i * 80;
                    Draw::DrawSprite("generic_textures", "inkroller_1a", x, y-5, 350, 70, 0,0,0,a);
                    if (!n.textureDict.empty() && !n.textureName.empty())
                        Draw::DrawSprite(n.textureDict.c_str(), n.textureName.c_str(), x+12, y+12, 40, 40, 255,255,255,a);
                    Draw::DrawText(n.title, Draw::FONT_TITLE, 220,185,80,a, x+60, y+6, 28);
                    Draw::DrawText(n.subtitle, Draw::FONT_BODY, 245,245,245,a, x+60, y+34, 22);
                    break;
                }
                case NOTIFY_TIP:
                {
                    float tw = n.title.size() * 11.0f;
                    float bx = sw * 0.5f - tw * 0.5f - 10;
                    float by = sh * 0.8f;
                    Draw::DrawSprite("generic_textures", "inkroller_1a", bx, by-5, tw+20, 35, 0,0,0,a);
                    Draw::DrawTextCenter(n.title, Draw::FONT_BODY, 245,245,245,a, sw*0.5f, by+4, 24);
                    break;
                }
                case NOTIFY_TOP:
                {
                    float tw = n.title.size() * 11.0f;
                    float bx = sw * 0.5f - tw * 0.5f - 10;
                    Draw::DrawSprite("generic_textures", "inkroller_1a", bx, 80, tw+20, 50, 0,0,0,a);
                    Draw::DrawTextCenter(n.title, Draw::FONT_BODY, 245,245,245,a, sw*0.5f, 90, 26);
                    if (!n.subtitle.empty())
                        Draw::DrawTextCenter(n.subtitle, Draw::FONT_BODY, 180,155,100,a, sw*0.5f, 115, 20);
                    break;
                }
                case NOTIFY_OBJECTIVE:
                {
                    float tw = n.title.size() * 14.0f;
                    float bx = sw * 0.5f - tw * 0.5f - 20;
                    float by = sh * 0.35f;
                    Draw::DrawSprite("generic_textures", "inkroller_1a", bx, by-8, tw+40, 55, 0,0,0,a);
                    Draw::DrawSprite("generic_textures", "menu_header_1a", bx+5, by, tw+30, 3, 180,140,55,a);
                    Draw::DrawTextCenter(n.title, Draw::FONT_TITLE, 255,230,130,a, sw*0.5f, by+10, 32);
                    break;
                }
                case NOTIFY_CENTER_TEXT:
                {
                    float tw = n.title.size() * 16.0f;
                    float bx = sw * 0.5f - tw * 0.5f - 15;
                    float by = sh * 0.45f;
                    Draw::DrawSprite("generic_textures", "inkroller_1a", bx, by-6, tw+30, 42, 0,0,0,a);
                    Draw::DrawTextCenter(n.title, Draw::FONT_TITLE, 255,255,255,a, sw*0.5f, by+7, 30);
                    break;
                }
                case NOTIFY_RIGHT_TEXT:
                {
                    float tw = n.title.size() * 10.0f;
                    Draw::DrawSprite("generic_textures", "inkroller_1a", sw-tw-40, 100 + i*40, tw+20, 35, 0,0,0,a);
                    Draw::DrawText(n.title, Draw::FONT_BODY, 245,245,245,a, sw-tw-35, 107 + i*40, 22);
                    break;
                }
            }
        }
    }
};