#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace stt_caption {
inline bool latin_word(wchar_t c) {
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
           (c >= L'0' && c <= L'9') || c == L'-' || c == L'_';
}

template<class Measure>
bool reflow(const std::wstring& text, int width, unsigned max_lines, Measure measure, std::wstring& result) {
    if (width <= 0 || max_lines == 0) return false;

    std::vector<std::wstring> lines(1);
    for (wchar_t c : text) {
        if (c == L'\n') lines.emplace_back();
        else if (c != L'\r') lines.back() += c;
    }
    if (lines.size() <= max_lines && std::all_of(lines.begin(), lines.end(),
        [&](const auto& line) { return measure(line) <= width; })) {
        result = text;
        return true;
    }

    std::wstring flat;
    std::vector<size_t> authored_breaks;
    for (const auto& line : lines) {
        const auto first = line.find_first_not_of(L' ');
        if (first == std::wstring::npos) continue;
        if (!flat.empty()) {
            authored_breaks.push_back(flat.size());
            if (latin_word(flat.back()) || latin_word(line[first])) flat += L' ';
        }
        flat += line.substr(first, line.find_last_not_of(L' ') - first + 1);
    }
    if (flat.empty()) return false;

    const size_t length = flat.size();
    std::vector<bool> protected_break(length + 1);
    // Shared glossary for terms that should stay together when reflowing.
    constexpr std::wstring_view phrases[] = {L"簡稱", L"痕跡", L"Number Ten", L"宇宙調查防衛機構"};
    for (auto phrase : phrases) {
        for (size_t start = flat.find(phrase); start != std::wstring::npos;
             start = flat.find(phrase, start + 1)) {
            std::fill(protected_break.begin() + start + 1,
                      protected_break.begin() + start + phrase.size(), true);
        }
    }

    constexpr int unreachable = std::numeric_limits<int>::max();
    constexpr std::wstring_view closing = L"，。、！？：；）］】』」…,.!?;:)]";
    constexpr std::wstring_view opening = L"（［【『「([";
    std::vector<std::vector<int>> costs(max_lines + 1, std::vector<int>(length + 1, unreachable));
    std::vector<std::vector<size_t>> next_break(max_lines + 1, std::vector<size_t>(length + 1));
    for (auto& row : costs) row[length] = 0;

    for (size_t start = length; start-- > 0;) {
        if (flat[start] == L' ') continue;
        for (size_t end = start + 1; end <= length; ++end) {
            if (flat[end - 1] == L' ' || (flat[end - 1] >= 0xd800 && flat[end - 1] <= 0xdbff)) continue;
            const int measured = measure(flat.substr(start, end - start));
            if (measured > width) break;
            size_t next = end;
            while (next < length && flat[next] == L' ') ++next;

            const int slack = (width - measured) * 100 / width;
            int cost = 1000 + slack * slack;
            if (next < length) {
                if (!std::binary_search(authored_breaks.begin(), authored_breaks.end(), end) &&
                    closing.find(flat[end - 1]) == std::wstring_view::npos) cost += 10000;
                if (protected_break[end] || (end == next && latin_word(flat[end - 1]) && latin_word(flat[next])))
                    cost += 1000000;
                if (closing.find(flat[next]) != std::wstring_view::npos ||
                    opening.find(flat[end - 1]) != std::wstring_view::npos) cost += 2000000;
            }
            for (unsigned remaining = 1; remaining <= max_lines; ++remaining) {
                const int tail = costs[remaining - 1][next];
                if (tail != unreachable && cost + tail < costs[remaining][start]) {
                    costs[remaining][start] = cost + tail;
                    next_break[remaining][start] = end;
                }
            }
        }
    }

    if (costs[max_lines][0] == unreachable) return false;
    result.clear();
    size_t start = 0;
    for (unsigned remaining = max_lines; start < length; --remaining) {
        const size_t end = next_break[remaining][start];
        if (!result.empty()) result += L'\n';
        result.append(flat, start, end - start);
        start = end;
        while (start < length && flat[start] == L' ') ++start;
    }
    return true;
}
}
