#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>
#include <sstream>
#include <vector>

namespace vcs {

static std::string stateFile(const std::string& vcsDir) {
    return vcsDir + "/BISECT_STATE";
}

static bool save(const std::string& vcsDir,
                 const std::string& good, const std::string& bad,
                 const std::string& origHead) {
    std::ostringstream oss;
    oss << (good.empty() ? "-" : good) << "\n"
        << (bad.empty() ? "-" : bad) << "\n"
        << (origHead.empty() ? "-" : origHead) << "\n";
    return FileSystem::writeTextFile(stateFile(vcsDir), oss.str());
}

static bool load(const std::string& vcsDir,
                 std::string& good, std::string& bad,
                 std::string& origHead) {
    if (!FileSystem::exists(stateFile(vcsDir))) return false;
    auto data = FileSystem::readTextFile(stateFile(vcsDir));
    std::istringstream iss(data);
    std::string g, b, o;
    std::getline(iss, g);
    std::getline(iss, b);
    std::getline(iss, o);
    good = (g == "-") ? "" : g;
    bad = (b == "-") ? "" : b;
    origHead = (o == "-") ? "" : o;
    return !bad.empty();
}

int cmd_bisect(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }

    Repository repo;
    auto vcsDir = repo.vcsDir();

    if (args.empty()) { out.error("Usage: vcs bisect start|good|bad|reset [...]"); return 1; }

    auto subcmd = args[0];

    if (subcmd == "start") {
        auto headHash = repo.refs().resolveHead();
        if (headHash.empty()) { out.error("No commits"); return 1; }

        std::string bad = headHash, good;

        if (args.size() >= 2) {
            auto h = repo.refs().resolveRef(args[1]);
            if (h.empty()) h = repo.objects().resolveShortHash(args[1]);
            if (!h.empty()) bad = h;
        }
        if (args.size() >= 3) {
            auto h = repo.refs().resolveRef(args[2]);
            if (h.empty()) h = repo.objects().resolveShortHash(args[2]);
            if (!h.empty()) good = h;
        }

        if (bad == good) { out.error("Bad and good must differ"); return 1; }

        bool found = false;
        for (auto& c : repo.getParentChain(bad))
            if (c == good) { found = true; break; }
        if (!found && !good.empty()) {
            out.error("Good is not an ancestor of bad");
            return 1;
        }

        save(vcsDir, good, bad, headHash);

        auto chain = repo.getParentChain(bad);
        std::vector<std::string> range;
        for (auto& c : chain) {
            if (c == good) break;
            range.push_back(c);
        }
        if (range.empty()) { out.error("No commits between good and bad"); return 1; }

        auto mid = range[(range.size() - 1) / 2];
        out.info("Bisecting: " + std::to_string(range.size()) + " revisions to test");
        out.info("Checking out " + Reference::hashToShort(mid));
        repo.reset(mid, "hard");
        return 0;
    }

    if (subcmd == "good" || subcmd == "bad") {
        std::string good, bad, origHead;
        if (!load(vcsDir, good, bad, origHead)) {
            out.error("No bisect in progress");
            return 1;
        }

        auto current = repo.refs().resolveHead();
        if (subcmd == "good") good = current;
        else bad = current;

        save(vcsDir, good, bad, origHead);

        auto chain = repo.getParentChain(bad);
        std::vector<std::string> range;
        for (auto& c : chain) {
            if (c == good) break;
            range.push_back(c);
        }

        if (range.empty()) {
            out.info("No more revisions to test");
            FileSystem::remove(stateFile(vcsDir));
            repo.reset(origHead, "hard");
            return 0;
        }

        if (range.size() == 1) {
            out.success("First bad commit: " + Reference::hashToShort(range[0]));
            FileSystem::remove(stateFile(vcsDir));
            repo.reset(range[0], "hard");
            return 0;
        }

        auto midIdx = (range.size() - 1) / 2;
        auto currentHash = repo.refs().resolveHead();
        if (range[midIdx] == currentHash && range.size() > 1) midIdx = (midIdx + 1) % range.size();
        auto mid = range[midIdx];
        out.info("Bisecting: " + std::to_string(range.size()) + " revisions to test");
        out.info("Checking out " + Reference::hashToShort(mid));
        repo.reset(mid, "hard");
        return 0;
    }

    if (subcmd == "reset") {
        std::string good, bad, origHead;
        if (load(vcsDir, good, bad, origHead) && !origHead.empty())
            repo.reset(origHead, "hard");
        FileSystem::remove(stateFile(vcsDir));
        out.success("Bisect ended");
        return 0;
    }

    out.error("Unknown bisect subcommand: " + subcmd);
    return 1;
}

} // namespace vcs
