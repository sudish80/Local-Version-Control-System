#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include "vcs/diff.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

namespace vcs {

int cmd_blame(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs blame <file>"); return 1; }

    Repository repo;
    auto fileName = args[0];
    auto fullPath = repo.workDir() + "/" + fileName;

    if (!FileSystem::exists(fullPath)) {
        out.error("File not found: " + fileName);
        return 1;
    }

    auto headHash = repo.refs().resolveHead();
    if (headHash.empty()) { out.error("No commits"); return 1; }

    std::string headData;
    try {
        auto headObj = repo.objects().read(headHash);
        headData.assign(headObj.content.begin(), headObj.content.end());
    } catch (...) { out.error("Cannot read HEAD"); return 1; }

    std::istringstream iss(headData);
    std::string line, headTree;
    while (std::getline(iss, line))
        if (line.substr(0, 5) == "tree ") { headTree = line.substr(5); break; }

    if (headTree.empty()) { out.error("No tree in HEAD"); return 1; }

    std::map<std::string, std::string> headFiles;
    for (auto& e : repo.objects().parseTree(headTree))
        headFiles[e.name] = e.hash;

    auto it = headFiles.find(fileName);
    if (it == headFiles.end()) { out.error("File not in HEAD"); return 1; }

    std::string currentContent;
    try {
        auto obj = repo.objects().read(it->second);
        currentContent.assign(obj.content.begin(), obj.content.end());
    } catch (...) { out.error("Cannot read file blob"); return 1; }

    auto currentLines = DiffEngine::splitLines(currentContent);
    size_t numLines = currentLines.size();
    if (numLines == 0) { out.info("Empty file"); return 0; }

    std::vector<std::string> blameCommit(numLines, headHash);
    std::vector<bool> blameDone(numLines, false);

    std::string current = headHash;
    while (!current.empty()) {
        try {
            auto obj = repo.objects().read(current);
            if (obj.type != ObjectType::Commit) break;

            std::string data(obj.content.begin(), obj.content.end());
            std::istringstream iss(data);
            std::string line2, treeHash, parentHash;
            while (std::getline(iss, line2)) {
                if (line2.substr(0, 5) == "tree ") treeHash = line2.substr(5);
                else if (line2.substr(0, 7) == "parent ") parentHash = line2.substr(7);
            }

            if (!treeHash.empty()) {
                std::map<std::string, std::string> fileMap;
                for (auto& e : repo.objects().parseTree(treeHash))
                    fileMap[e.name] = e.hash;

                auto fit = fileMap.find(fileName);
                if (fit != fileMap.end()) {
                    auto blob = repo.objects().read(fit->second);
                    std::string content(blob.content.begin(), blob.content.end());
                    auto lines = DiffEngine::splitLines(content);
                    auto diffs = DiffEngine::diffLines(content, currentContent);

                    size_t ctxIdx = 0;
                    for (auto& d : diffs) {
                        if (d.type == DiffLine::ADDITION) {
                            if (ctxIdx < numLines && !blameDone[ctxIdx]) {
                                blameCommit[ctxIdx] = current;
                                blameDone[ctxIdx] = false;
                            }
                            ++ctxIdx;
                        } else if (d.type == DiffLine::CONTEXT) {
                            if (ctxIdx < numLines) {
                                blameDone[ctxIdx] = true;
                                ++ctxIdx;
                            }
                        } else {
                        }
                    }
                }
            }

            current = parentHash;
        } catch (...) { break; }
    }

    for (size_t i = 0; i < numLines; ++i) {
        auto shortHash = Reference::hashToShort(blameCommit[i]);
        std::cout << "\033[33m" << shortHash << "\033[0m " << currentLines[i];
    }

    return 0;
}

} // namespace vcs
