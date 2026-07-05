#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>

namespace vcs {

struct CommandLine {
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> options;
    std::vector<std::string> positionalArgs;
};

class Parser {
public:
    static CommandLine parse(int argc, char* argv[]);
    static std::string getHelpString(const std::string& command = "");
    static void printUsage();
    static void printHelp(const std::string& command);

private:
    static std::vector<std::string> tokenize(const std::string& input);
};

inline CommandLine Parser::parse(int argc, char* argv[]) {
    CommandLine cl;
    if (argc < 2) { cl.command = "help"; return cl; }
    cl.command = argv[1];

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.substr(0, 2) == "--") {
            auto eqPos = arg.find('=');
            if (eqPos != std::string::npos) {
                cl.options[arg.substr(2, eqPos - 2)] = arg.substr(eqPos + 1);
            } else {
                cl.options[arg.substr(2)] = "true";
            }
        } else if (arg.substr(0, 1) == "-" && arg.size() > 1) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                cl.options[arg.substr(1)] = argv[++i];
            } else {
                cl.options[arg.substr(1)] = "true";
            }
        } else {
            cl.args.push_back(arg);
            cl.positionalArgs.push_back(arg);
        }
    }
    return cl;
}

inline std::string Parser::getHelpString(const std::string& command) {
    if (command == "init")     return "  init       Initialize a new repository";
    if (command == "add")      return "  add <file> Stage file(s) for commit";
    if (command == "commit")   return "  commit     Record changes to the repository";
    if (command == "branch")   return "  branch     List, create, or delete branches";
    if (command == "checkout") return "  checkout   Switch branches or restore working tree files";
    if (command == "status")   return "  status     Show working tree status";
    if (command == "log")      return "  log        Show commit logs";
    if (command == "diff")     return "  diff       Show changes between commits, working tree, etc";
    if (command == "merge")    return "  merge      Join two or more development histories together";
    if (command == "tag")      return "  tag        Create, list, or delete tags";
    if (command == "show")     return "  show       Show commit details and diff";
    if (command == "rm")       return "  rm         Remove files from working tree and index";
    if (command == "stash")    return "  stash      Stash changes in working directory";
    if (command == "mv")       return "  mv         Move or rename a file";
    if (command == "clean")    return "  clean      Remove untracked files";
    if (command == "config")   return "  config     Read/write configuration values";
    if (command == "revert")   return "  revert     Revert a commit by applying inverse diff";
    if (command == "gc")       return "  gc         Garbage collect unreachable objects";
    if (command == "blame")    return "  blame      Show commit attribution for each line of a file";
    if (command == "reset")    return "  reset      Reset HEAD to a specific state (--soft, --mixed, --hard)";
    if (command == "cherry-pick") return "  cherry-pick Apply changes from an existing commit";
    if (command == "rebase")   return "  rebase     Reapply commits on top of another branch";
    if (command == "bisect")   return "  bisect     Binary search for a breaking commit";
    return "";
}

inline void Parser::printUsage() {
    std::cout << "Usage: vcs <command> [<args>]\n\n";
    std::cout << "Available commands:\n";
    std::cout << getHelpString("init") << "\n";
    std::cout << getHelpString("add") << "\n";
    std::cout << getHelpString("commit") << "\n";
    std::cout << getHelpString("branch") << "\n";
    std::cout << getHelpString("checkout") << "\n";
    std::cout << getHelpString("status") << "\n";
    std::cout << getHelpString("log") << "\n";
    std::cout << getHelpString("diff") << "\n";
    std::cout << getHelpString("merge") << "\n";
    std::cout << getHelpString("tag") << "\n";
    std::cout << getHelpString("show") << "\n";
    std::cout << getHelpString("rm") << "\n";
    std::cout << getHelpString("stash") << "\n";
    std::cout << getHelpString("mv") << "\n";
    std::cout << getHelpString("clean") << "\n";
    std::cout << getHelpString("config") << "\n";
    std::cout << getHelpString("revert") << "\n";
    std::cout << getHelpString("gc") << "\n";
    std::cout << getHelpString("reset") << "\n";
    std::cout << getHelpString("blame") << "\n";
    std::cout << getHelpString("cherry-pick") << "\n";
    std::cout << getHelpString("rebase") << "\n";
    std::cout << getHelpString("bisect") << "\n";
}

inline void Parser::printHelp(const std::string& command) {
    if (command == "init") {
        std::cout << "vcs init - Initialize a new repository\n\n";
        std::cout << "Usage: vcs init\n\n";
        std::cout << "Creates a new .vcs directory with objects, refs, and index files.\n";
    } else if (command == "add") {
        std::cout << "vcs add - Stage file(s) for commit\n\n";
        std::cout << "Usage: vcs add <file>...\n\n";
        std::cout << "Stages files for commit. Supports wildcards.\n";
    } else if (command == "commit") {
        std::cout << "vcs commit - Record changes to the repository\n\n";
        std::cout << "Usage: vcs commit -m \"message\"\n\n";
        std::cout << "Options:\n";
        std::cout << "  -m <msg>  Commit message\n";
        std::cout << "  --author  Override the author\n";
        std::cout << "  --amend   Amend the last commit\n";
    } else if (command == "branch") {
        std::cout << "vcs branch - List, create, or delete branches\n\n";
        std::cout << "Usage: vcs branch [<name>]\n";
        std::cout << "       vcs branch -d <name>\n\n";
        std::cout << "With no arguments, list branches.\n";
        std::cout << "With <name>, create a new branch.\n";
        std::cout << "With -d <name>, delete a branch.\n";
    } else if (command == "checkout") {
        std::cout << "vcs checkout - Switch branches or restore working tree files\n\n";
        std::cout << "Usage: vcs checkout <branch>\n";
    } else if (command == "status") {
        std::cout << "vcs status - Show working tree status\n\n";
        std::cout << "Usage: vcs status\n";
    } else if (command == "log") {
        std::cout << "vcs log - Show commit logs\n\n";
        std::cout << "Usage: vcs log [<hash>]\n";
        std::cout << "       vcs log --oneline\n";
    } else if (command == "diff") {
        std::cout << "vcs diff - Show changes\n\n";
        std::cout << "Usage: vcs diff\n";
        std::cout << "       vcs diff --staged\n";
        std::cout << "       vcs diff <commit1> <commit2>\n";
    } else if (command == "merge") {
        std::cout << "vcs merge - Join two development histories\n\n";
        std::cout << "Usage: vcs merge <branch>\n";
    } else if (command == "tag") {
        std::cout << "vcs tag - Create, list, or delete tags\n\n";
        std::cout << "Usage: vcs tag\n";
        std::cout << "       vcs tag <name>\n";
        std::cout << "       vcs tag -d <name>\n";
    } else if (command == "show") {
        std::cout << "vcs show - Show commit details and diff\n\n";
        std::cout << "Usage: vcs show [<hash>]\n";
    } else if (command == "rm") {
        std::cout << "vcs rm - Remove files from working tree and index\n\n";
        std::cout << "Usage: vcs rm [--cached] <file>...\n";
    } else if (command == "stash") {
        std::cout << "vcs stash - Stash changes in working directory\n\n";
        std::cout << "Usage: vcs stash push [-m <msg>]\n";
        std::cout << "       vcs stash pop\n";
        std::cout << "       vcs stash list\n";
    } else if (command == "mv") {
        std::cout << "vcs mv - Move or rename a file\n\n";
        std::cout << "Usage: vcs mv <source> <destination>\n";
    } else if (command == "clean") {
        std::cout << "vcs clean - Remove untracked files\n\n";
        std::cout << "Usage: vcs clean -f\n";
        std::cout << "       vcs clean -f -d\n";
        std::cout << "       vcs clean -n (dry run)\n\n";
        std::cout << "Options:\n";
        std::cout << "  -f, --force  Actually remove files\n";
        std::cout << "  -d           Remove untracked directories\n";
        std::cout << "  -n, --dry-run Show what would be removed\n";
    } else if (command == "config") {
        std::cout << "vcs config - Read/write configuration values\n\n";
        std::cout << "Usage: vcs config [<key> [<value>]]\n";
        std::cout << "       vcs config --list\n\n";
        std::cout << "With no arguments, lists all config values.\n";
        std::cout << "With <key>, shows the value.\n";
        std::cout << "With <key> <value>, sets the value.\n";
    } else if (command == "revert") {
        std::cout << "vcs revert - Revert a commit by applying inverse diff\n\n";
        std::cout << "Usage: vcs revert <commit>\n\n";
        std::cout << "Restores files to their state before the given commit.\n";
    } else if (command == "gc") {
        std::cout << "vcs gc - Garbage collect unreachable objects\n\n";
        std::cout << "Usage: vcs gc\n\n";
        std::cout << "Removes objects not reachable from any branch, tag, or stash.\n";
    } else if (command == "reset") {
        std::cout << "vcs reset - Reset HEAD to a specific state\n\n";
        std::cout << "Usage: vcs reset [--soft|--mixed|--hard] <commit>\n\n";
        std::cout << "  --soft    Move HEAD only, index unchanged\n";
        std::cout << "  --mixed   Move HEAD and reset index (default)\n";
        std::cout << "  --hard    Move HEAD, reset index, and overwrite working tree\n";
    } else if (command == "blame") {
        std::cout << "vcs blame - Show commit attribution for each line of a file\n\n";
        std::cout << "Usage: vcs blame <file>\n\n";
        std::cout << "Annotates each line with the commit hash that last modified it.\n";
    } else if (command == "cherry-pick") {
        std::cout << "vcs cherry-pick - Apply changes from an existing commit\n\n";
        std::cout << "Usage: vcs cherry-pick <commit>\n\n";
        std::cout << "Applies the diff of a commit onto the current HEAD.\n";
    } else if (command == "rebase") {
        std::cout << "vcs rebase - Reapply commits on top of another branch\n\n";
        std::cout << "Usage: vcs rebase <branch>\n\n";
        std::cout << "Finds common ancestor, then replays commits onto the target.\n";
    } else if (command == "bisect") {
        std::cout << "vcs bisect - Binary search for a breaking commit\n\n";
        std::cout << "Usage: vcs bisect start [<bad> [<good>]]\n";
        std::cout << "       vcs bisect good\n";
        std::cout << "       vcs bisect bad\n";
        std::cout << "       vcs bisect reset\n\n";
        std::cout << "Marks commits and narrows down the first bad commit.\n";
    } else {
        printUsage();
    }
}

} // namespace vcs
