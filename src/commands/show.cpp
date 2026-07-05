#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include "vcs/diff.hpp"
#include <iostream>
#include <sstream>

namespace vcs {

int cmd_show(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }

    Repository repo;

    std::string hash;
    if (!args.empty()) {
        hash = repo.refs().resolveRef(args[0]);
        if (hash.empty()) hash = repo.objects().resolveShortHash(args[0]);
        if (hash.empty()) hash = args[0];
    } else {
        hash = repo.refs().resolveHead();
    }

    if (hash.empty()) { out.error("No commits"); return 1; }

    try {
        auto obj = repo.objects().read(hash);
        if (obj.type != ObjectType::Commit) {
            out.error("Not a commit: " + hash); return 1;
        }

        std::string data(obj.content.begin(), obj.content.end());
        std::istringstream iss(data);
        std::string line, treeHash, parentHash, author;
        std::string message;
        while (std::getline(iss, line)) {
            if (line.substr(0, 5) == "tree ") treeHash = line.substr(5);
            else if (line.substr(0, 7) == "parent ") parentHash = line.substr(7);
            else if (line.substr(0, 7) == "author ") author = line.substr(7);
            else if (line.empty()) { std::getline(iss, message, '\0'); break; }
        }

        out.header("commit " + Reference::hashToShort(hash));
        if (!parentHash.empty())
            std::cout << "Parent:     " << Reference::hashToShort(parentHash) << "\n";
        std::cout << "Tree:       " << Reference::hashToShort(treeHash) << "\n";
        if (!author.empty()) std::cout << "Author:     " << author << "\n";
        std::cout << "\n    " << message << "\n\n";

        if (!treeHash.empty()) {
            auto currentFiles = repo.objects().parseTree(treeHash);
            std::map<std::string, std::string> currentFilesMap;
            for (auto& e : currentFiles) currentFilesMap[e.name] = e.hash;

            std::map<std::string, std::string> parentFilesMap;
            if (!parentHash.empty()) {
                try {
                    auto parentObj = repo.objects().read(parentHash);
                    std::string pd(parentObj.content.begin(), parentObj.content.end());
                    std::istringstream iss2(pd);
                    std::string pt;
                    while (std::getline(iss2, line))
                        if (line.substr(0, 5) == "tree ") { pt = line.substr(5); break; }
                    if (!pt.empty())
                        for (auto& e : repo.objects().parseTree(pt))
                            parentFilesMap[e.name] = e.hash;
                } catch (...) {}
            }

            out.header("Diff:");
            std::set<std::string> all;
            for (auto& [k, _] : currentFilesMap) all.insert(k);
            for (auto& [k, _] : parentFilesMap) all.insert(k);

            for (auto& name : all) {
                auto cur = currentFilesMap.find(name);
                auto par = parentFilesMap.find(name);
                if (cur == currentFilesMap.end() && par != parentFilesMap.end()) {
                    out.println("--- a/" + name, Output::RED);
                } else if (cur != currentFilesMap.end() && par == parentFilesMap.end()) {
                    out.println("--- /dev/null -> b/" + name, Output::GREEN);
                    auto obj = repo.objects().read(cur->second);
                    auto content = std::string(obj.content.begin(), obj.content.end());
                    printDiffToStdout(DiffEngine::diffLines("", content));
                } else if (cur != parentFilesMap.end() && cur->second != par->second) {
                    auto oldObj = repo.objects().read(par->second);
                    auto newObj = repo.objects().read(cur->second);
                    auto oldContent = std::string(oldObj.content.begin(), oldObj.content.end());
                    auto newContent = std::string(newObj.content.begin(), newObj.content.end());
                    out.println("--- a/" + name + " -> b/" + name, Output::CYAN);
                    printDiffToStdout(DiffEngine::diffLines(oldContent, newContent));
                }
            }
        }
    } catch (std::exception& e) {
        out.error(std::string("Show failed: ") + e.what());
        return 1;
    }
    return 0;
}

} // namespace vcs
