#pragma once
#include <string>
#include "../script.h"

#define NATIVE_DRAW_SPRITE                    0xC9884ECADE94CB34ULL
#define NATIVE_SET_SCRIPT_GFX_DRAW_ORDER     0xCFCC78391C8B3814ULL
#define NATIVE_DRAW_RECT                      0x405224591DF02025ULL
#define NATIVE_GET_SCREEN_COORD_FROM_WORLD    0xCB50D7AFCC8B0EC6ULL
#define NATIVE_VAR_STRING                     0xFA925AC00EB830B9ULL
#define NATIVE_BG_SET_TEXT_COLOR              0x16FA5CE47F184F1EULL
#define NATIVE_BG_DISPLAY_TEXT                0x16794E044C9EFB58ULL
// HUD text (for simple labels — not nullsub in SP)
#define NATIVE_SET_TEXT_SCALE                 0x4170B650590B3B00ULL
#define NATIVE_SET_TEXT_COLOR_RGBA            0x50A41AD966910F03ULL
#define NATIVE_SET_TEXT_CENTRE                0xBE5261939FBECB8CULL
#define NATIVE_SET_TEXT_DROPSHADOW            0x1BE39DBAA7263CA5ULL
#define NATIVE_DRAW_TEXT                      0xD79334A4BB99BAD1ULL

namespace Draw
{
    const float SCREEN_WIDTH  = (float)GetSystemMetrics(0);
    const float SCREEN_HEIGHT = (float)GetSystemMetrics(1);

    struct Color { int r, g, b, a; };
    void format(std::string& str, const std::string& arg) { size_t p = str.find("%s"); if (p != std::string::npos) str.replace(p, 2, arg); }
    const int FONT_BODY = 0, FONT_TITLE = 16;

    inline void DrawSprite(const char* txd, const char* txn, float sx, float sy, float w, float h, int r = 255, int g = 255, int b = 255, int a = 255, bool cen = false)
    {
        float x = sx / SCREEN_WIDTH; float y = sy / SCREEN_HEIGHT;
        float ww = w / SCREEN_WIDTH; float hh = h / SCREEN_HEIGHT;
        if (!cen) { x += ww * 0.5f; y += hh * 0.5f; }
        invoke<void>(NATIVE_SET_SCRIPT_GFX_DRAW_ORDER, 0);
        invoke<void>(NATIVE_DRAW_SPRITE, txd, txn, x, y, ww, hh, 0.0f, r, g, b, a, false);
    }

    inline void DrawText(const std::string& text, int font, int r, int g, int b, int a, float px, float py, int sz, bool cen = false)
    {
        // Font names by index
        const char* fn[] = {"body","body1","catalog1","catalog2","catalog3","catalog4","catalog5","chalk","Debug_BOLD","FixedWidthNumbers","Font5","gamername","handwritten","ledger","RockstarTAG","SOCIAL_CLUB_COND_BOLD","title","wantedPostersGeneric"};
        std::string fstr = std::string("$") + fn[font % 18];
        std::string align = cen ? "Center" : "Left";

        std::string fmt = "<TEXTFORMAT RIGHTMARGIN='0'><P ALIGN='%s'><FONT FACE='%s' LETTERSPACING='0' SIZE='%s'>~s~%s</FONT></P><TEXTFORMAT>";
        format(fmt, align); format(fmt, fstr); format(fmt, std::to_string(sz)); format(fmt, text);

        float x = cen ? -1.0f + (px / SCREEN_WIDTH) * 2.0f : px / SCREEN_WIDTH;
        float y = py / SCREEN_HEIGHT;

        invoke<void>(NATIVE_BG_SET_TEXT_COLOR, r, g, b, a);
        char* str = invoke<char*>(NATIVE_VAR_STRING, 10, "LITERAL_STRING", (char*)fmt.c_str());
        invoke<void>(NATIVE_BG_DISPLAY_TEXT, str, x, y);
    }

    inline void DrawTextCenter(const std::string& t, int f, int r, int g, int b, int a, float x, float y, int s) { DrawText(t,f,r,g,b,a,x,y,s,true); }

    inline void DrawTextRight(const std::string& text, int font, int r, int g, int b, int a, float rx, float y, int sz)
    {
        const char* fn[] = {"body","body1","catalog1","catalog2","catalog3","catalog4","catalog5","chalk","Debug_BOLD","FixedWidthNumbers","Font5","gamername","handwritten","ledger","RockstarTAG","SOCIAL_CLUB_COND_BOLD","title","wantedPostersGeneric"};
        std::string fstr = std::string("$") + fn[font % 18];
        std::string rm = std::to_string((int)(SCREEN_WIDTH - rx));
        std::string fmt = "<TEXTFORMAT RIGHTMARGIN='%s'><P ALIGN='Right'><FONT FACE='%s' LETTERSPACING='0' SIZE='%s'>~s~%s</FONT></P><TEXTFORMAT>";
        format(fmt, rm); format(fmt, fstr); format(fmt, std::to_string(sz)); format(fmt, text);

        invoke<void>(NATIVE_BG_SET_TEXT_COLOR, r, g, b, a);
        char* str = invoke<char*>(NATIVE_VAR_STRING, 10, "LITERAL_STRING", (char*)fmt.c_str());
        invoke<void>(NATIVE_BG_DISPLAY_TEXT, str, 0.0f, y / SCREEN_HEIGHT);
    }

    inline void Rect(float x, float y, float w, float h, Color c)
    {
        invoke<void>(NATIVE_DRAW_RECT, x + w * 0.5f, y + h * 0.5f, w, h, c.r, c.g, c.b, c.a, false, false);
    }

    namespace C
    {
        constexpr Color HeaderTop={18,12,5,240},Accent={180,140,55,255},AccentDim={120,90,35,200};
        constexpr Color PanelBg={8,8,8,215},RowAlt={14,12,10,180},SelBg={180,140,55,55};
        constexpr Color SelOutline={180,140,55,200},FooterBg={18,12,5,230},SubtitleBg={25,18,8,220};
        constexpr Color TextTitle={220,185,80,255},TextSub={180,155,100,200},TextNormal={230,225,215,255};
        constexpr Color TextSelected={255,230,130,255},TextDisabled={110,100,85,200},TextValue={160,200,160,255};
        constexpr Color TextDanger={200,80,60,255},ChatBg={8,8,8,160},StatusBg={8,8,8,170};
        constexpr Color PingGood={80,200,80,255},PingMid={220,160,60,255},PingBad={200,60,60,255};
    }

    inline void Text(const std::string& txt, float x, float y, float s, Color c, int f=0, bool cen=false)
    {
        DrawText(txt, 0, c.r, c.g, c.b, c.a, x*SCREEN_WIDTH, y*SCREEN_HEIGHT, (int)(s*70), cen);
    }
    inline float TextWidth(const std::string& txt, float s) { return (float)txt.size() * s * 0.033f; }
    inline void TextRight(const std::string& txt, float rx, float y, float s, Color c)
    {
        DrawText(txt,0,c.r,c.g,c.b,c.a,rx*SCREEN_WIDTH-txt.size()*s*0.033f*SCREEN_WIDTH,y*SCREEN_HEIGHT,(int)(s*70));
    }
    inline Color PingColor(int ms) {
        if(ms<0)return{140,140,140,255}; if(ms<80)return C::PingGood; if(ms<150)return C::PingMid; return C::PingBad;
    }
    constexpr float PANEL_X=0.008f,PANEL_W=0.225f,HEADER_H=0.090f,SUBHEADER_H=0.030f,ROW_H=0.036f,FOOTER_H=0.026f,PAD=0.008f;
    constexpr float SCALE_TITLE=0.52f,SCALE_SUB=0.27f,SCALE_OPTION=0.30f,SCALE_VALUE=0.28f,SCALE_FOOTER=0.24f,SCALE_CHAT=0.27f;
    inline void Divider(float x, float y, float w, int a=200) { Rect(x,y,w,0.002f,{C::Accent.r,C::Accent.g,C::Accent.b,a}); }

    inline void WorldLabel(const std::string& txt, float wx, float wy, float wz,
                           int r=255, int g=230, int b=100, int a=255)
    {
        float sx=0,sy=0;
        if(!invoke<bool>(NATIVE_GET_SCREEN_COORD_FROM_WORLD,wx,wy,wz,&sx,&sy))return;
        if(sx<0.01f||sx>0.99f||sy<0.01f||sy>0.95f)return;
        float px=sx*SCREEN_WIDTH, py=sy*SCREEN_HEIGHT, w=txt.size()*14+20;
        DrawSprite("generic_textures","inkroller_1a",px-w*0.5f,py-12,w,24,0,0,0,170);
        DrawText(txt, FONT_TITLE, r,g,b,a, px-w*0.5f+8, py-8, 22);
    }
}