#include "vcs/repository.hpp"
#include "vcs/diff.hpp"
#include "vcs/output.hpp"
#include <iostream>
#include <set>

namespace vcs {

static void diffCommits(Repository& repo, const std::string& hash1, const std::string& hash2) {
    auto commit1 = repo.objects().read(hash1);
    auto commit2 = repo.objects().read(hash2);
    if (commit1.type != ObjectType::Commit || commit2.type != ObjectType::Commit) {
        std::cerr << "Invalid commit hash\n"; return;
    }

    auto getTreeHash = [](const Object& c) -> std::string {
        std::string d(c.content.begin(), c.content.end());
        std::istringstream iss(d); std::string line;
        while (std::getline(iss, line))
            if (line.substr(0, 5) == "tree ") return line.substr(5);
        return "";
    };

    auto tree1 = getTreeHash(commit1);
    auto tree2 = getTreeHash(commit2);
    if (tree1.empty() || tree2.empty()) return;

    auto files1v = repo.objects().parseTree(tree1);
    auto files2v = repo.objects().parseTree(tree2);
    std::map<std::string, std::string> files1, files2;
    for (auto& e : files1v) files1[e.name] = e.hash;
    for (auto& e : files2v) files2[e.name] = e.hash;

    std::set<std::string> all;
    for (auto& [k, _] : files1) all.insert(k);
    for (auto& [k, _] : files2) all.insert(k);

    for (auto& name : all) {
        auto it1 = files1.find(name);
        auto it2 = files2.find(name);
        if (it1 == files1.end()) {
            auto obj = repo.objects().read(it2->second);
            auto lines = DiffEngine::splitLines(std::string(obj.content.begin(), obj.content.end()));
            std::cout << "\033[32m+++ b/" << name << "\033[0m\n";
            for (auto& l : lines) std::cout << "\033[32m+" << l << "\033[0m\n";
        } else if (it2 == files2.end()) {
            auto obj = repo.objects().read(it1->second);
            auto lines = DiffEngine::splitLines(std::string(obj.content.begin(), obj.content.end()));
            std::cout << "\033[31m--- a/" << name << "\033[0m\n";
            for (auto& l : lines) std::cout << "\033[31m-" << l << "\033[0m\n";
        } else if (it1->second != it2->second) {
            auto obj1 = repo.objects().read(it1->second);
            auto obj2 = repo.objects().read(it2->second);
            auto oldC = std::string(obj1.content.begin(), obj1.content.end());
            auto newC = std::string(obj2.content.begin(), obj2.content.end());
            auto diffs = DiffEngine::diffLines(oldC, newC);
            std::cout << "diff --git a/" << name << " b/" << name << "\n";
            for (auto& d : diffs) {
                if (d.type == DiffLine::ADDITION) std::cout << "\033[32m+" << d.content << "\033[0m";
                else if (d.type == DiffLine::DELETION) std::cout << "\033[31m-" << d.content << "\033[0m";
                else std::cout << " " << d.content;
            }
        }
    }
}

int cmd_diff(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args) {
    Output out;

    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    Repository repo;

    if (args.size() >= 2) {
        auto hash1 = repo.refs().resolveRef(args[0]);
        if (hash1.empty()) hash1 = repo.objects().resolveShortHash(args[0]);
        if (hash1.empty()) hash1 = args[0];
        auto hash2 = repo.refs().resolveRef(args[1]);
        if (hash2.empty()) hash2 = repo.objects().resolveShortHash(args[1]);
        if (hash2.empty()) hash2 = args[1];
        diffCommits(repo, hash1, hash2);
        return 0;
    }

    bool staged = options.find("staged") != options.end() ||
                  options.find("cached") != options.end();
    if (staged) repo.printDiff("--staged");
    else repo.printDiff();
    return 0;
}

} // namespace vcs
