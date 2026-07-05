#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_mv(const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.size() < 2) { out.error("Usage: vcs mv <source> <destination>"); return 1; }

    Repository repo;
    auto src = args[0];
    auto dst = args[1];
    auto srcFull = repo.workDir() + "/" + src;
    auto dstFull = repo.workDir() + "/" + dst;

    if (!FileSystem::exists(srcFull)) { out.error("Source does not exist: " + src); return 1; }

    repo.index().load();
    auto hash = repo.index().getHash(src);
    if (hash.empty()) { out.error(src + " is not tracked"); return 1; }

    if (!FileSystem::rename(srcFull, dstFull)) {
        out.error("Failed to rename " + src + " to " + dst);
        return 1;
    }

    repo.index().unstage(src);
    auto mtime = FileSystem::getLastModifiedTime(dstFull);
    auto size = FileSystem::getFileSize(dstFull);
    repo.index().stage(dst, hash, mtime, size);
    repo.index().save();

    out.success("Moved " + src + " to " + dst);
    return 0;
}

} // namespace vcs
