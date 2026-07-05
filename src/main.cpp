#include "vcs/parser.hpp"
#include "vcs/output.hpp"
#include "vcs/repository.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <map>

namespace vcs {

int cmd_init(const std::vector<std::string>& args);
int cmd_add(const std::vector<std::string>& args);
int cmd_commit(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args);
int cmd_branch(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args);
int cmd_checkout(const std::vector<std::string>& args);
int cmd_status(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args);
int cmd_log(const std::map<std::string, std::string>& options,
            const std::vector<std::string>& args);
int cmd_diff(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args);
int cmd_merge(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args);
int cmd_tag(const std::map<std::string, std::string>& options,
            const std::vector<std::string>& args);
int cmd_show(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args);
int cmd_rm(const std::map<std::string, std::string>& options,
           const std::vector<std::string>& args);
int cmd_stash(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args);
int cmd_mv(const std::vector<std::string>& args);
int cmd_clean(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args);
int cmd_config(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args);
int cmd_revert(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args);
int cmd_gc(const std::map<std::string, std::string>& options,
           const std::vector<std::string>& args);
int cmd_blame(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args);
int cmd_reset(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args);
int cmd_cherry_pick(const std::map<std::string, std::string>& options,
                    const std::vector<std::string>& args);
int cmd_rebase(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args);
int cmd_bisect(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args);
int cmd_fsck(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args);
int cmd_archive(const std::map<std::string, std::string>& options,
                const std::vector<std::string>& args);
int cmd_format_patch(const std::map<std::string, std::string>& options,
                     const std::vector<std::string>& args);
int cmd_am(const std::map<std::string, std::string>& options,
           const std::vector<std::string>& args);
int cmd_remote(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args);
int cmd_clone(const std::vector<std::string>& args);
int cmd_fetch(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args);
int cmd_push(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args);
int cmd_pull(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args);

} // namespace vcs

int main(int argc, char* argv[]) {
    auto cl = vcs::Parser::parse(argc, argv);
    vcs::Output out;

    try {
        if (cl.command == "init") {
            return vcs::cmd_init(cl.args);
        } else if (cl.command == "add") {
            return vcs::cmd_add(cl.args);
        } else if (cl.command == "commit") {
            return vcs::cmd_commit(cl.options, cl.args);
        } else if (cl.command == "branch") {
            return vcs::cmd_branch(cl.options, cl.args);
        } else if (cl.command == "checkout") {
            return vcs::cmd_checkout(cl.args);
        } else if (cl.command == "status") {
            return vcs::cmd_status(cl.options, cl.args);
        } else if (cl.command == "log") {
            return vcs::cmd_log(cl.options, cl.args);
        } else if (cl.command == "diff") {
            return vcs::cmd_diff(cl.options, cl.args);
        } else if (cl.command == "merge") {
            return vcs::cmd_merge(cl.options, cl.args);
        } else if (cl.command == "tag") {
            return vcs::cmd_tag(cl.options, cl.args);
        } else if (cl.command == "show") {
            return vcs::cmd_show(cl.options, cl.args);
        } else if (cl.command == "rm") {
            return vcs::cmd_rm(cl.options, cl.args);
        } else if (cl.command == "stash") {
            return vcs::cmd_stash(cl.options, cl.args);
        } else if (cl.command == "mv") {
            return vcs::cmd_mv(cl.args);
        } else if (cl.command == "clean") {
            return vcs::cmd_clean(cl.options, cl.args);
        } else if (cl.command == "config") {
            return vcs::cmd_config(cl.options, cl.args);
        } else if (cl.command == "revert") {
            return vcs::cmd_revert(cl.options, cl.args);
        } else if (cl.command == "gc") {
            return vcs::cmd_gc(cl.options, cl.args);
        } else if (cl.command == "blame") {
            return vcs::cmd_blame(cl.options, cl.args);
        } else if (cl.command == "reset") {
            return vcs::cmd_reset(cl.options, cl.args);
        } else if (cl.command == "cherry-pick") {
            return vcs::cmd_cherry_pick(cl.options, cl.args);
        } else if (cl.command == "rebase") {
            return vcs::cmd_rebase(cl.options, cl.args);
        } else if (cl.command == "bisect") {
            return vcs::cmd_bisect(cl.options, cl.args);
        } else if (cl.command == "fsck") {
            return vcs::cmd_fsck(cl.options, cl.args);
        } else if (cl.command == "archive") {
            return vcs::cmd_archive(cl.options, cl.args);
        } else if (cl.command == "format-patch") {
            return vcs::cmd_format_patch(cl.options, cl.args);
        } else if (cl.command == "am") {
            return vcs::cmd_am(cl.options, cl.args);
        } else if (cl.command == "remote") {
            return vcs::cmd_remote(cl.options, cl.args);
        } else if (cl.command == "clone") {
            return vcs::cmd_clone(cl.args);
        } else if (cl.command == "fetch") {
            return vcs::cmd_fetch(cl.options, cl.args);
        } else if (cl.command == "push") {
            return vcs::cmd_push(cl.options, cl.args);
        } else if (cl.command == "pull") {
            return vcs::cmd_pull(cl.options, cl.args);
        } else if (cl.command == "help" || cl.command == "--help" || cl.command == "-h") {
            if (!cl.args.empty()) vcs::Parser::printHelp(cl.args[0]);
            else vcs::Parser::printUsage();
            return 0;
        } else {
            out.error("Unknown command: " + cl.command);
            vcs::Parser::printUsage();
            return 1;
        }
    } catch (std::exception& e) {
        out.error(std::string("Error: ") + e.what());
        return 1;
    } catch (...) {
        out.error("Unknown error occurred");
        return 1;
    }
}
