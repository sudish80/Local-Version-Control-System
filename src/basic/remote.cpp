#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_remote(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    Repository repo;

    if (args.empty()) {
        auto remotes = repo.remoteList();
        if (remotes.empty()) { out.info("No remotes configured"); return 0; }
        for (auto& r : remotes) {
            auto url = repo.remoteUrl(r);
            out.println(r + "\t" + url);
        }
        return 0;
    }

    auto subcmd = args[0];

    if (subcmd == "add") {
        if (args.size() < 3) { out.error("Usage: vcs remote add <name> <url>"); return 1; }
        if (repo.remoteAdd(args[1], args[2]))
            out.success("Added remote " + args[1] + " -> " + args[2]);
        else
            out.error("Failed to add remote");
        return 0;
    }

    if (subcmd == "remove" || subcmd == "rm") {
        if (args.size() < 2) { out.error("Usage: vcs remote remove <name>"); return 1; }
        auto path = repo.vcsDir() + "/remotes/" + args[1];
        if (FileSystem::remove(path))
            out.success("Removed remote " + args[1]);
        else
            out.error("Remote not found: " + args[1]);
        return 0;
    }

    out.error("Unknown remote subcommand: " + subcmd);
    return 1;
}

int cmd_clone(const std::vector<std::string>& args) {
    Output out;
    if (args.empty()) { out.error("Usage: vcs clone <url> [<dir>]"); return 1; }

    auto url = args[0];
    auto dir = (args.size() >= 2) ? args[1] : fs::path(url).filename().string();

    Repository repo(dir);
    if (repo.clone(url, dir)) {
        out.success("Cloned into " + dir);
        return 0;
    }
    out.error("Clone failed");
    return 1;
}

int cmd_fetch(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    Repository repo;

    auto remote = args.empty() ? "origin" : args[0];
    if (repo.fetch(remote)) {
        out.success("Fetched from " + remote);
        return 0;
    }
    out.error("Fetch failed");
    return 1;
}

int cmd_push(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    Repository repo;

    auto remote = args.empty() ? "origin" : args[0];
    if (repo.push(remote)) {
        out.success("Pushed to " + remote);
        return 0;
    }
    out.error("Push failed");
    return 1;
}

int cmd_pull(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    Repository repo;

    auto remote = args.empty() ? "origin" : args[0];
    if (repo.pull(remote)) {
        out.success("Pulled from " + remote);
        return 0;
    }
    out.error("Pull failed");
    return 1;
}

} // namespace vcs
