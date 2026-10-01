#pragma once
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "draw.h"
#include "lanscanner.h"

enum SubID { SUB_MAIN=0, SUB_DIRECT, SUB_LAN, SUB_SETTINGS, SUB_CONNECTED };

struct Option
{
    std::string text;
    std::string rightText;
    std::string footer;
    bool isBool = false;
    bool* boolPtr = nullptr;
    bool isSubmenu = false;
    int subId = SUB_MAIN;
    bool isAction = false;
    std::function<void()> action;
    bool isEmpty = false;
    bool hasAction = false;

    Option() {}
    Option(const std::string& t, const std::string& r="", const std::string& f="")
        : text(t), rightText(r), footer(f) {}
};

struct Sub
{
    int id;
    std::string header;
    std::string subheader;
    std::vector<Option> opts;
    Sub(int i, const std::string& h, const std::string& s="") : id(i), header(h), subheader(s) {}
    void Add(const Option& o) { opts.push_back(o); }
    int Size() const { return (int)opts.size(); }
};

class NativeMenu
{
public:
    bool isOpen = false;
    bool isConnected = false;
    int cur = 0, sel = 0, vis = 10;
    std::string username = "Cowboy", serverIp = "127.0.0.1";
    int serverPort = 7777;
    std::vector<LanScanner::ServerInfo> lanServers;
    std::string lanDebug;
    std::function<void()> onConnect, onDisconnect;

    std::vector<Sub> subs;

    NativeMenu() { Build(); }

    void Build()
    {
        subs.clear();
        if (isConnected)
        {
            Sub s(SUB_CONNECTED, "RDR2 COOP", "CONECTADO");
            s.Add({"Usuario", username});
            {
                Option o("Desconectar"); o.isAction=true;
                o.action=[this]{ if(onDisconnect)onDisconnect(); Close(); };
                s.Add(o);
            }
            s.Add({"Cerrar menu"});
            subs.push_back(s);
        }
        else
        {
            Sub main(SUB_MAIN, "RDR2 COOP", "MULTIPLAYER MOD");
            {
                Option o("Conectar directamente","","IP y puerto manual");
                o.isSubmenu=true; o.subId=SUB_DIRECT; main.Add(o);
            }
            {
                Option o("Servidores LAN","","Buscar en red local");
                o.isSubmenu=true; o.subId=SUB_LAN; main.Add(o);
            }
            {
                Option o("Ajustes","","Usuario y opciones");
                o.isSubmenu=true; o.subId=SUB_SETTINGS; main.Add(o);
            }
            subs.push_back(main);

            Sub direct(SUB_DIRECT, "CONEXION DIRECTA");
            {
                Option o("Conectar"); o.isAction=true;
                o.action=[this]{ if(onConnect)onConnect(); Close(); }; direct.Add(o);
            }
            direct.Add({"IP del servidor", serverIp});
            direct.Add({"Puerto", std::to_string(serverPort)});
            direct.Add({"Usuario", username});
            {
                Option o("Volver"); o.isSubmenu=true; o.subId=SUB_MAIN; direct.Add(o);
            }
            subs.push_back(direct);

            Sub lan(SUB_LAN, "SERVIDORES LAN");
            if (lanServers.empty()) {
                Option o("Buscando..."); o.isEmpty=true; lan.Add(o);
                if (!lanDebug.empty()) lan.Add({lanDebug, "", ""});
            } else {
                for (auto& s : lanServers) {
                    Option o(s.name.empty()?s.ip:s.name,
                            std::to_string(s.players)+"/"+std::to_string(s.maxPlayers));
                    o.isAction=true;
                    std::string ip=s.ip; int port=s.port;
                    o.action=[this,ip,port]{serverIp=ip;serverPort=port;if(onConnect)onConnect();Close();};
                    lan.Add(o);
                }
            }
            { Option o("Volver"); o.isSubmenu=true; o.subId=SUB_MAIN; lan.Add(o); }
            subs.push_back(lan);

            Sub set(SUB_SETTINGS, "AJUSTES");
            set.Add({"Usuario", username});
            set.Add({"Puerto", std::to_string(serverPort)});
            { Option o("Volver"); o.isSubmenu=true; o.subId=SUB_MAIN; set.Add(o); }
            subs.push_back(set);
        }
    }

    void Open() { isOpen=true; cur=0; sel=0; Build(); }
    void Close() { isOpen=false; }

    void Nav(int d) {
        int max=subs[cur].Size()-1;
        if(d>0){if(sel<max)sel++;}
        else{if(sel>0)sel--;}
    }

    void Select() {
        if(sel>=subs[cur].Size())return;
        Option& o=subs[cur].opts[sel];
        if(o.isSubmenu){ for(int i=0;i<(int)subs.size();i++) if(subs[i].id==o.subId){cur=i;sel=0;break;} }
        else if(o.action) o.action();
        else if(o.isAction){ if(onConnect)onConnect(); Close(); }
    }

    void Back() {
        if(cur==0||cur==SUB_CONNECTED) return;
        cur=0; sel=0;
    }

    void SetLan(const std::vector<LanScanner::ServerInfo>& s){lanServers=s;}
    void SetLanDbg(const std::string& s){lanDebug=s;}

    void Tick()
    {
        if (!isOpen) return;

        int saveCur = cur;
        int saveSel = sel;
        Build();
        cur = saveCur < (int)subs.size() ? saveCur : 0;
        sel = std::min(saveSel, subs[cur].Size() - 1);
        Sub& sm = subs[cur];
        int num = sm.Size();
        int visCount = std::min(vis, num);
        int visStart = sel < vis ? 0 : sel - vis + 1;

        const float INCR = Draw::SCREEN_HEIGHT * 0.050f;

        Draw::DrawSprite("generic_textures", "inkroller_1a", 25, 25, 575, 1025, 0,0,0,230);
        Draw::DrawSprite("generic_textures", "menu_header_1a", 90, 51, 445, 110, 255,255,255,255);
        Draw::DrawSprite("generic_textures", "menu_bar", 312.5f, 971.5f, 445, 2, 255,255,255,175, true);

        Draw::DrawTextCenter(sm.header, Draw::FONT_TITLE, 245,245,245,255, 312.5f, 103.75f, 45);
        Draw::DrawTextCenter(sm.subheader, Draw::FONT_TITLE, 245,245,245,255, 312.5f, 172, 22);

        Draw::DrawSprite("menu_textures", "scroller_left_top", 90, 215, 207.5f, 25);
        Draw::DrawSprite("menu_textures", "scroller_right_top", 327.5f, 215, 207.5f, 25);
        if (sel >= vis)
            Draw::DrawSprite("menu_textures", "scroller_arrow_top", 312.5f, 227.5f, 30, 25);
        else
            Draw::DrawSprite("menu_textures", "scroller_line_up", 312.5f, 227.5f, 30, 25);

        float btY = 244 + visCount * INCR;
        Draw::DrawSprite("menu_textures", "scroller_left_bottom", 90, btY, 207.5f, 25);
        Draw::DrawSprite("menu_textures", "scroller_right_bottom", 327.5f, btY, 207.5f, 25);
        if (num <= visCount)
            Draw::DrawSprite("menu_textures", "scroller_line_down", 312.5f, 256.5f+num*INCR, 30, 25);
        else if (sel == num-1)
            Draw::DrawSprite("menu_textures", "scroller_line_down", 312.5f, 256.5f+visCount*INCR, 30, 25);
        else
            Draw::DrawSprite("menu_textures", "scroller_arrow_bottom", 312.5f, 256.5f+visCount*INCR, 30, 25);

        float boxY = sel < vis ? 269 + sel * INCR : 269 + (vis-1) * INCR;
        float selTopY = boxY - 25 + 3;
        float selBotY = boxY + 25 - 3;
        Draw::DrawSprite("menu_textures", "crafting_highlight_l", 87, boxY, 19, 54, 204,0,0,255, true);
        Draw::DrawSprite("menu_textures", "crafting_highlight_r", 538, boxY, 19, 54, 204,0,0,255, true);
        Draw::DrawSprite("menu_textures", "crafting_highlight_t", 312.5f, selTopY, 453, 22, 204,0,0,255, true);
        Draw::DrawSprite("menu_textures", "crafting_highlight_b", 312.5f, selBotY, 453, 22, 204,0,0,255, true);

        for (int i = 0; i < visCount; i++)
        {
            int idx = visStart + i;
            if (idx >= num) break;

            Option& o = sm.opts[idx];
            float optY = 269 + i * INCR;
            float textY = 275 - 22 + i * INCR;

            Draw::DrawSprite("generic_textures", "selection_box_bg_1c", 312.5f, optY, 445, 50, 50,50,50,110, true);

            Draw::DrawText(o.text, Draw::FONT_BODY, 245,245,245,255, 98, textY, 22);

            if (!o.rightText.empty())
            {
                Draw::DrawTextRight(o.rightText, Draw::FONT_BODY, 245,245,245,255, 527, textY, 22);
            }

            if (o.isBool && o.boolPtr && *o.boolPtr)
            {
                Draw::DrawSprite("generic_textures", "tick", 510, optY, 30, 30, 255,255,255,255);
                Draw::DrawSprite("generic_textures", "tick_box", 510, optY, 30, 30, 255,255,255,255);
            }
        }

        if (sel >= 0 && sel < num)
        {
            Option& o = sm.opts[sel];
            if (!o.footer.empty())
            {
                Draw::DrawTextCenter(o.footer, Draw::FONT_BODY, 245,245,245,255, 312.5f, 977.5f, 18);
            }
        }

        std::string cnt = std::to_string(sel+1) + " of " + std::to_string(num);
        Draw::DrawTextRight(cnt, Draw::FONT_BODY, 144,144,144,230, 535, 243+visCount*INCR, 20);
    }
};