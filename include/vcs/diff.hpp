#pragma once

#include "filesystem.hpp"
#include "object.hpp"
#include "index.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace vcs {

struct DiffLine {
    enum Type { CONTEXT, ADDITION, DELETION };
    Type type;
    std::string content;
};

class DiffEngine {
public:
    static std::vector<std::string> splitLines(const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line + "\n");
        }
        return lines;
    }

    static std::vector<DiffLine> diffLines(const std::string& oldText,
                                            const std::string& newText) {
        auto oldLines = splitLines(oldText);
        auto newLines = splitLines(newText);
        return computeDiff(oldLines, newLines);
    }

private:
    struct LCSMatrix {
        std::vector<std::vector<int>> dp;
    };

    static LCSMatrix buildLCS(const std::vector<std::string>& a,
                               const std::vector<std::string>& b) {
        int m = static_cast<int>(a.size());
        int n = static_cast<int>(b.size());
        LCSMatrix matrix;
        matrix.dp.resize(m + 1, std::vector<int>(n + 1, 0));
        for (int i = 1; i <= m; ++i) {
            for (int j = 1; j <= n; ++j) {
                if (a[i - 1] == b[j - 1]) {
                    matrix.dp[i][j] = matrix.dp[i - 1][j - 1] + 1;
                } else {
                    matrix.dp[i][j] = std::max(matrix.dp[i - 1][j],
                                               matrix.dp[i][j - 1]);
                }
            }
        }
        return matrix;
    }

    static std::vector<DiffLine> computeDiff(const std::vector<std::string>& oldLines,
                                              const std::vector<std::string>& newLines) {
        auto matrix = buildLCS(oldLines, newLines);
        std::vector<DiffLine> result;
        int i = static_cast<int>(oldLines.size());
        int j = static_cast<int>(newLines.size());

        std::vector<std::pair<int, int>> trace;
        while (i > 0 || j > 0) {
            if (i > 0 && j > 0 && oldLines[i - 1] == newLines[j - 1]) {
                trace.push_back({0, i - 1});
                --i; --j;
            } else if (j > 0 && (i == 0 || matrix.dp[i][j - 1] >= matrix.dp[i - 1][j])) {
                trace.push_back({1, j - 1});
                --j;
            } else if (i > 0) {
                trace.push_back({-1, i - 1});
                --i;
            }
        }

        for (auto it = trace.rbegin(); it != trace.rend(); ++it) {
            DiffLine dl;
            if (it->first == 0) {
                dl.type = DiffLine::CONTEXT;
                dl.content = oldLines[it->second];
            } else if (it->first == 1) {
                dl.type = DiffLine::ADDITION;
                dl.content = newLines[it->second];
            } else {
                dl.type = DiffLine::DELETION;
                dl.content = oldLines[it->second];
            }
            result.push_back(dl);
        }
        return result;
    }
};

inline void printDiffToStdout(const std::vector<DiffLine>& diffs) {
    for (auto& d : diffs) {
        if (d.type == DiffLine::ADDITION)
            std::cout << "\033[32m+" << d.content << "\033[0m";
        else if (d.type == DiffLine::DELETION)
            std::cout << "\033[31m-" << d.content << "\033[0m";
        else
            std::cout << " " << d.content;
    }
}

} // namespace vcs
