#pragma once
#include <string>
#include <map>
#include <fstream>

class Language
{
    std::map<std::string, std::string> _map;
public:
    void Load(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open()) return;
        std::string line;
        while (std::getline(f, line))
        {
            if (line.empty() || line[0] == '#') continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (!val.empty() && val.back() == '\r') val.pop_back();
            _map[key] = val;
        }
    }
    const char* Get(const char* key) const
    {
        auto it = _map.find(key);
        return (it != _map.end()) ? it->second.c_str() : key;
    }
    std::string GetStr(const char* key) const { return Get(key); }
};