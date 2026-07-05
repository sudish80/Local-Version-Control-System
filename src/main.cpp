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
