#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>
#include <sstream>

namespace vcs {

int cmd_stash(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }

    Repository repo;
    std::string subcmd = args.empty() ? "push" : args[0];

    if (subcmd == "push" || subcmd == "save") {
        repo.index().load();
        auto staged = repo.index().getStagedFiles();
        if (staged.empty()) {
            auto headHash = repo.refs().resolveHead();
            if (headHash.empty()) { out.info("Nothing to stash"); return 0; }

            if (!repo.index().getAllFiles().empty()) {
                std::vector<TreeEntry> entries;
                auto headObj = repo.objects().read(headHash);
                std::string hd(headObj.content.begin(), headObj.content.end());
                std::istringstream iss(hd);
                std::string line, treeHash;
                while (std::getline(iss, line))
                    if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }
                if (!treeHash.empty()) {
                    for (auto& e : repo.objects().parseTree(treeHash))
                        entries.push_back(e);
                }
                for (auto& e : repo.index().getAllFiles()) {
                    bool found = false;
                    for (auto& et : entries) {
                        if (et.name == e.path) { et.hash = e.hash; found = true; break; }
                    }
                    if (!found) {
                        TreeEntry te; te.mode = "100644";
                        te.name = e.path; te.hash = e.hash;
                        entries.push_back(te);
                    }
                }
                auto treeHash2 = repo.objects().storeTree(entries);
                std::string msg = "WIP on stash";
                auto it = options.find("m");
                if (it != options.end()) msg = it->second;
                auto commitHash = repo.objects().storeCommit(treeHash2, headHash, "Stash <stash@vcs.local>", msg);
                repo.setStashHash(commitHash);
                out.success("Stashed as " + Reference::hashToShort(commitHash));
            } else {
                out.info("Nothing to stash");
            }
            return 0;
        }

        std::vector<TreeEntry> entries;
        auto headHash = repo.refs().resolveHead();
        if (!headHash.empty()) {
            try {
                auto headObj = repo.objects().read(headHash);
                std::string hd(headObj.content.begin(), headObj.content.end());
                std::istringstream iss(hd);
                std::string line, treeHash;
                while (std::getline(iss, line))
                    if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }
                if (!treeHash.empty()) {
                    for (auto& e : repo.objects().parseTree(treeHash))
                        entries.push_back(e);
                }
            } catch (...) {}
        }

        for (auto& e : staged) {
            bool found = false;
            for (auto& et : entries) {
                if (et.name == e.path) { et.hash = e.hash; found = true; break; }
            }
            if (!found) {
                TreeEntry te; te.mode = "100644"; te.name = e.path; te.hash = e.hash;
                entries.push_back(te);
            }
        }

        auto treeHash = repo.objects().storeTree(entries);
        std::string msg2 = "WIP on stash";
        auto it2 = options.find("m");
        if (it2 != options.end()) msg2 = it2->second;
        auto commitHash = repo.objects().storeCommit(treeHash,
            headHash.empty() ? "" : headHash, "Stash <stash@vcs.local>", msg2);
        repo.setStashHash(commitHash);
        out.success("Stashed as " + Reference::hashToShort(commitHash));
    } else if (subcmd == "pop") {
        auto stashHash = repo.getStashHash();
        if (stashHash.empty()) { out.error("No stash found"); return 1; }

        try {
            auto stashObj = repo.objects().read(stashHash);
            std::string sd(stashObj.content.begin(), stashObj.content.end());
            std::istringstream iss(sd);
            std::string line, treeHash;
            while (std::getline(iss, line))
                if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }

            if (treeHash.empty()) { out.error("Invalid stash"); return 1; }

            auto treeEntries = repo.objects().parseTree(treeHash);
            repo.index().load();
            repo.index().clear();
            for (auto& e : treeEntries) {
                auto fullPath = repo.workDir() + "/" + e.name;
                FileSystem::writeFile(fullPath, repo.objects().read(e.hash).content);
                auto mtime = FileSystem::getLastModifiedTime(fullPath);
                auto size = FileSystem::getFileSize(fullPath);
                repo.index().stage(e.name, e.hash, mtime, size);
            }
            repo.index().save();
            repo.setStashHash("");
            out.success("Stash restored and removed");
        } catch (std::exception& e) {
            out.error(std::string("Stash pop failed: ") + e.what());
            return 1;
        }
    } else if (subcmd == "list") {
        auto stashHash = repo.getStashHash();
        if (stashHash.empty()) { out.info("No stashes"); return 0; }
        try {
            auto obj = repo.objects().read(stashHash);
            std::string sd(obj.content.begin(), obj.content.end());
            std::istringstream iss(sd);
            std::string line, msg;
            while (std::getline(iss, line)) {
                if (line.empty()) { std::getline(iss, msg, '\0'); break; }
            }
            std::cout << "stash@{0}: " << msg << " (" << Reference::hashToShort(stashHash) << ")\n";
        } catch (...) {}
    } else {
        out.error("Unknown stash subcommand: " + subcmd);
        return 1;
    }
    return 0;
}

} // namespace vcs
