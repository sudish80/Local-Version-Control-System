#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>
#include <sstream>

namespace vcs {

int cmd_commit(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }

    Repository repo;
    repo.index().load();

    auto staged = repo.index().getStagedFiles();
    if (staged.empty()) { out.error("Nothing to commit"); return 1; }

    std::string message;
    auto it = options.find("m");
    if (it == options.end()) it = options.find("message");
    if (it != options.end()) message = it->second;
    if (message.empty()) { out.error("Commit message required (-m)"); return 1; }

    std::string author = "Author <author@vcs.local>";
    auto authorIt = options.find("author");
    if (authorIt != options.end()) author = authorIt->second;

    bool amend = options.find("amend") != options.end();
    std::string parentHash;
    std::string oldTreeHash;

    if (amend) {
        parentHash = repo.refs().resolveHead();
        if (parentHash.empty()) { out.error("No commit to amend"); return 1; }
        try {
            auto oldCommit = repo.objects().read(parentHash);
            std::string oldData(oldCommit.content.begin(), oldCommit.content.end());
            std::istringstream iss(oldData);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.substr(0, 5) == "tree ") { oldTreeHash = line.substr(5); break; }
            }
        } catch (...) {}
    } else {
        parentHash = repo.refs().resolveHead();
    }

    std::vector<TreeEntry> treeEntries;
    if (amend && !oldTreeHash.empty()) {
        try {
            for (auto& e : repo.objects().parseTree(oldTreeHash)) {
                bool found = false;
                for (auto& s : staged) {
                    if (s.path == e.name) { found = true; break; }
                }
                if (!found) {
                    TreeEntry te;
                    te.mode = e.mode;
                    te.name = e.name;
                    te.hash = e.hash;
                    treeEntries.push_back(te);
                }
            }
        } catch (...) {}
    }

    for (auto& e : staged) {
        TreeEntry te;
        te.mode = "100644";
        te.name = e.path;
        te.hash = e.hash;
        treeEntries.push_back(te);
    }

    auto treeHash = repo.objects().storeTree(treeEntries);
    auto commitHash = repo.objects().storeCommit(treeHash, parentHash, author, message);

    auto branch = repo.refs().getCurrentBranch();
    if (!branch.empty()) {
        repo.refs().setBranchRef(branch, commitHash);
    } else {
        repo.refs().setHead(commitHash);
    }

    repo.index().clear();
    for (auto& e : treeEntries) {
        repo.index().stage(e.name, e.hash, 0, 0);
    }
    repo.index().save();

    out.success("Created commit " + Reference::hashToShort(commitHash));
    return 0;
}

} // namespace vcs
