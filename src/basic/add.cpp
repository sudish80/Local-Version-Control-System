#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <functional>
#include <iostream>

namespace vcs {

int cmd_add(const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs add <file>..."); return 1; }

    Repository repo;
    repo.index().load();
    int added = 0;

    for (auto& arg : args) {
        auto fullPath = repo.workDir() + "/" + arg;
        if (!FileSystem::exists(fullPath)) {
            out.warn(arg + " does not exist, skipping");
            continue;
        }

        if (repo.isIgnored(fullPath)) {
            out.warn(arg + " matches .vcsignore, skipping");
            continue;
        }

        if (FileSystem::isDirectory(fullPath)) {
            std::vector<TreeEntry> entries;
            repo.objects();
            std::function<void(const std::string&, const std::string&)> scan =
                [&](const std::string& dir, const std::string& prefix) {
                    auto items = FileSystem::listDirectory(dir);
                    for (auto& item : items) {
                        auto fp = dir + "/" + item;
                        auto rp = prefix.empty() ? item : prefix + "/" + item;
                        if (repo.isIgnored(fp)) continue;
                        if (FileSystem::isRegularFile(fp)) {
                            auto content = FileSystem::readFile(fp);
                            auto hash = repo.objects().storeBlob(content);
                            auto mtime = FileSystem::getLastModifiedTime(fp);
                            auto size = FileSystem::getFileSize(fp);
                            repo.index().stage(rp, hash, mtime, size);
                            out.println("Added: " + rp, Output::GREEN);
                            ++added;
                        } else if (FileSystem::isDirectory(fp)) {
                            scan(fp, rp);
                        }
                    }
                };
            scan(fullPath, arg);
        } else {
            auto content = FileSystem::readFile(fullPath);
            auto hash = repo.objects().storeBlob(content);
            auto mtime = FileSystem::getLastModifiedTime(fullPath);
            auto size = FileSystem::getFileSize(fullPath);
            repo.index().stage(arg, hash, mtime, size);
            out.println("Added: " + arg, Output::GREEN);
            ++added;
        }
    }

    repo.index().save();
    if (added > 0) out.success("Staged " + std::to_string(added) + " file(s)");
    return 0;
}

} // namespace vcs
