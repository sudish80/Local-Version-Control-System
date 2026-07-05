#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>
#include <sstream>
#include <map>

namespace vcs {

static std::map<std::string, std::string> parseConfig(const std::string& data) {
    std::map<std::string, std::string> cfg;
    std::istringstream iss(data);
    std::string line, section;
    while (std::getline(iss, line)) {
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            auto end = line.find(']');
            if (end != std::string::npos) section = line.substr(1, end - 1) + ".";
        } else {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                auto key = line.substr(0, eq);
                auto val = line.substr(eq + 1);
                trim(key); trim(val);
                cfg[section + key] = val;
            }
        }
    }
    return cfg;
}

static std::string serializeConfig(const std::map<std::string, std::string>& cfg) {
    std::ostringstream oss;
    std::string currentSection;
    for (auto& [key, val] : cfg) {
        auto dot = key.find('.');
        if (dot == std::string::npos) continue;
        auto section = key.substr(0, dot);
        auto name = key.substr(dot + 1);
        if (section != currentSection) {
            currentSection = section;
            oss << "[" << section << "]\n";
        }
        oss << "\t" << name << " = " << val << "\n";
    }
    return oss.str();
}

int cmd_config(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args) {
    Output out;

    auto vcsDir = Repository::findVcsDir();
    if (vcsDir.empty()) { out.error("Not a VCS repository"); return 1; }

    auto configFile = vcsDir + "/config";
    auto data = FileSystem::exists(configFile) ? FileSystem::readTextFile(configFile) : "";
    auto cfg = parseConfig(data);

    if (args.size() >= 2) {
        std::string key = args[0], val = args[1];
        if (options.find("unset") != options.end()) {
            cfg.erase(key);
            out.success("Unset " + key);
        } else {
            cfg[key] = val;
            out.success("Set " + key + " = " + val);
        }
        FileSystem::writeTextFile(configFile, serializeConfig(cfg));
    } else if (args.size() == 1) {
        auto it = cfg.find(args[0]);
        if (it != cfg.end()) std::cout << it->second << "\n";
    } else if (options.find("list") != options.end() || options.find("l") != options.end()) {
        for (auto& [k, v] : cfg) std::cout << k << "=" << v << "\n";
    } else {
        for (auto& [k, v] : cfg) std::cout << k << "=" << v << "\n";
    }
    return 0;
}

} // namespace vcs
