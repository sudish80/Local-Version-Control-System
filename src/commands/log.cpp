#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <map>

namespace vcs {

struct GraphNode {
    std::string hash;
    std::vector<std::string> parents;
    std::string message;
    int column = 0;
};

static std::map<std::string, std::vector<std::string>> buildLabelMap(Repository& repo) {
    std::map<std::string, std::vector<std::string>> labels;
    auto branches = repo.refs().listBranches();
    for (auto& b : branches) {
        auto hash = repo.refs().getBranchRef(b);
        auto trimPos = hash.find_last_not_of(" \n\r\t");
        if (trimPos != std::string::npos) hash = hash.substr(0, trimPos + 1);
        labels[hash].push_back("\033[32m" + b + "\033[0m");
    }
    auto tags = repo.refs().listTags();
    for (auto& t : tags) {
        auto hash = repo.refs().getTagRef(t);
        auto trimPos = hash.find_last_not_of(" \n\r\t");
        if (trimPos != std::string::npos) hash = hash.substr(0, trimPos + 1);
        labels[hash].push_back("\033[36m" + t + "\033[0m");
    }
    auto currentBranch = repo.refs().getCurrentBranch();
    if (!currentBranch.empty()) {
        auto hash = repo.refs().getBranchRef(currentBranch);
        auto trimPos = hash.find_last_not_of(" \n\r\t");
        if (trimPos != std::string::npos) hash = hash.substr(0, trimPos + 1);
        labels[hash].push_back("\033[33mHEAD -> " + currentBranch + "\033[0m");
    }
    auto headHash = repo.refs().resolveHead();
    if (!headHash.empty() && repo.refs().isHeadDetached())
        labels[headHash].push_back("\033[33mHEAD\033[0m");
    return labels;
}

static std::string formatLabels(const std::vector<std::string>& refs) {
    if (refs.empty()) return "";
    std::string result;
    for (auto& r : refs) result += " (" + r + ")";
    return result;
}

int cmd_log(const std::map<std::string, std::string>& options,
            const std::vector<std::string>& args) {
    Output out;

    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }

    Repository repo;
    std::string hash;
    if (!args.empty()) hash = args[0];

    int maxCount = -1;
    auto countIt = options.find("n");
    if (countIt != options.end()) {
        try { maxCount = std::stoi(countIt->second); } catch (...) {}
    }

    bool oneline = options.find("oneline") != options.end();
    bool graph = options.find("graph") != options.end();
    auto labels = buildLabelMap(repo);

    if (graph) {
        auto headHash = hash.empty() ? repo.refs().resolveHead() : hash;
        if (headHash.empty()) { std::cout << "No commits\n"; return 0; }

        std::vector<GraphNode> nodes;
        std::string current = headHash;
        std::set<std::string> visited;
        int count = 0;

        while (!current.empty() && visited.find(current) == visited.end()
               && (maxCount < 0 || count < maxCount)) {
            try {
                auto obj = repo.objects().read(current);
                if (obj.type != ObjectType::Commit) break;
                visited.insert(current);

                std::string data(obj.content.begin(), obj.content.end());
                GraphNode node;
                node.hash = current;

                std::istringstream iss(data);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.substr(0, 7) == "parent ")
                        node.parents.push_back(line.substr(7));
                    else if (line.empty())
                        { std::getline(iss, node.message); break; }
                }
                node.column = count;
                nodes.push_back(node);
                current = node.parents.empty() ? "" : node.parents[0];
                ++count;
            } catch (...) { break; }
        }

        int cols = std::min(static_cast<int>(nodes.size()), 6);

        for (size_t i = 0; i < nodes.size(); ++i) {
            std::string graphLine;
            auto& node = nodes[i];
            int col = std::min(static_cast<int>(i), cols - 1);

            for (int c = 0; c < cols; ++c) {
                if (c == col) {
                    graphLine += "* ";
                } else if (c < col) {
                    bool connected = false;
                    for (size_t j = i + 1; j < nodes.size() && j < i + 3; ++j) {
                        if (std::find(nodes[j].parents.begin(), nodes[j].parents.end(),
                                      node.hash) != nodes[j].parents.end()) {
                            graphLine += "| "; connected = true; break;
                        }
                    }
                    if (!connected) graphLine += "  ";
                } else { graphLine += "  "; }
            }

            auto shortHash = Reference::hashToShort(node.hash);
            auto msg = node.message;
            auto nlPos = msg.find('\n');
            if (nlPos != std::string::npos) msg = msg.substr(0, nlPos);
            auto refStr = formatLabels(labels[node.hash]);

            if (oneline) {
                std::cout << "\033[33m" << graphLine << shortHash << "\033[0m" << refStr << " " << msg << "\n";
            } else {
                std::cout << "\033[33m" << graphLine << "commit " << shortHash << "\033[0m" << refStr << "\n";
                if (!node.parents.empty()) {
                    std::cout << graphLine << "Parent: ";
                    for (auto& p : node.parents)
                        std::cout << Reference::hashToShort(p) << " ";
                    std::cout << "\n";
                }
                std::cout << graphLine << "    " << msg << "\n\n";
            }
        }
        return 0;
    }

    if (oneline) {
        auto headHash = hash.empty() ? repo.refs().resolveHead() : hash;
        if (headHash.empty()) { std::cout << "No commits\n"; return 0; }

        std::string current = headHash;
        int count = 0;
        while (!current.empty() && (maxCount < 0 || count < maxCount)) {
            try {
                auto obj = repo.objects().read(current);
                if (obj.type != ObjectType::Commit) break;
                std::string data(obj.content.begin(), obj.content.end());
                auto pos = data.rfind("\n\n");
                std::string msg = (pos != std::string::npos) ? data.substr(pos + 2) : "";
                auto nlPos = msg.find('\n');
                if (nlPos != std::string::npos) msg = msg.substr(0, nlPos);

                auto refStr = formatLabels(labels[current]);
                std::cout << Reference::hashToShort(current) << refStr << " " << msg << "\n";

                std::istringstream iss(data);
                std::string line, parent;
                while (std::getline(iss, line))
                    if (line.substr(0, 7) == "parent ") { parent = line.substr(7); break; }
                current = parent;
                ++count;
            } catch (...) { break; }
        }
    } else {
        repo.printLog(hash, maxCount);
    }
    return 0;
}

} // namespace vcs
