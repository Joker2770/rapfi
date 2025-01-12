/*
 *  Rapfi, a Gomoku/Renju playing engine supporting piskvork protocol.
 *  Copyright (C) 2022  Rapfi developers
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils.h"

#include "iohelper.h"

#include <algorithm>
#include <chrono>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #if !defined(_MSC_VER)
        #define USE_WINDOWS_CODECVT
    #endif
#endif

Time now()
{
    static_assert(sizeof(Time) == sizeof(std::chrono::milliseconds::rep), "Time should be 64 bits");

    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
}

// -------------------------------------------------

std::string &trimInplace(std::string &s)
{
    if (s.empty())
        return s;

    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return s;
}

std::string &upperInplace(std::string &s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

std::string &replaceAll(std::string &str, std::string_view from, std::string_view to)
{
    size_t start = 0;
    while ((start = str.find(from, start)) != std::string::npos) {
        str.replace(start, from.length(), to);
        start += to.length();  // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

std::vector<std::string_view> split(std::string_view s, std::string_view delims, bool includeEmpty)
{
    std::vector<std::string_view> output;
    size_t                        first = 0;

    while (first < s.size()) {
        const auto second = s.find_first_of(delims, first);

        if (includeEmpty || first != second)
            output.emplace_back(s.substr(first, second - first));

        if (second == std::string_view::npos)
            break;

        first = second + 1;
    }

    return output;
}

// -------------------------------------------------

std::string timeText(Time time)
{
    if (time < 10000)
        return std::to_string(time) + "ms";
    else if (time < 1000000)
        return std::to_string(time / 1000) + "s";
    else if (time < 360000000)
        return std::to_string(time / 60000) + "min";
    else
        return std::to_string(time / 3600000) + "h";
}

std::string nodesText(uint64_t nodes)
{
    if (nodes < 10000)
        return std::to_string(nodes);
    else if (nodes < 10000000)
        return std::to_string(nodes / 1000) + "K";
    else if (nodes < 100000000000)
        return std::to_string(nodes / 1000000) + "M";
    else if (nodes < 100000000000000)
        return std::to_string(nodes / 1000000000) + "G";
    else
        return std::to_string(nodes / 1000000000000) + "T";
}

// -------------------------------------------------

std::string ACPToUTF8(std::string str)
{
#if defined(_WIN32)
    if (str.empty())
        return {};

    int nCodePage = GetACP();
    int wideSize  = MultiByteToWideChar(nCodePage, 0, str.c_str(), (int)str.length(), nullptr, 0);

    std::wstring wstr(wideSize + 1, '\0');
    MultiByteToWideChar(nCodePage,
                        0,
                        str.c_str(),
                        (int)str.length(),
                        wstr.data(),
                        (int)wstr.size());

    int utf8Size = WideCharToMultiByte(CP_UTF8,
                                       0,
                                       wstr.c_str(),
                                       (int)wstr.length(),
                                       nullptr,
                                       0,
                                       nullptr,
                                       nullptr);

    std::string utf8str(utf8Size, '\0');
    WideCharToMultiByte(CP_UTF8,
                        0,
                        wstr.c_str(),
                        (int)wstr.length(),
                        utf8str.data(),
                        (int)utf8str.size(),
                        nullptr,
                        nullptr);

    return utf8str;
#else
    return str;
#endif
}

std::string UTF8ToACP(std::string utf8str)
{
#if defined(_WIN32)
    if (utf8str.empty())
        return {};

    int nCodePage = GetACP();
    int wideSize =
        MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), (int)utf8str.length(), nullptr, 0);

    std::wstring wstr(wideSize + 1, '\0');
    MultiByteToWideChar(CP_UTF8,
                        0,
                        utf8str.c_str(),
                        (int)utf8str.length(),
                        wstr.data(),
                        (int)wstr.size());

    int utf8Size = WideCharToMultiByte(nCodePage,
                                       0,
                                       wstr.c_str(),
                                       (int)wstr.length(),
                                       nullptr,
                                       0,
                                       nullptr,
                                       nullptr);

    std::string str(utf8Size, '\0');
    WideCharToMultiByte(nCodePage,
                        0,
                        wstr.c_str(),
                        (int)wstr.length(),
                        str.data(),
                        (int)str.size(),
                        nullptr,
                        nullptr);

    return str;
#else
    return utf8str;
#endif
}

// -------------------------------------------------

std::filesystem::path pathFromString(const std::string &path)
{
#ifdef USE_WINDOWS_CODECVT
    // Use windows string conversion api due to mingw64 limitations.
    int nCodePage = GetACP();
    int convertResult =
        MultiByteToWideChar(nCodePage, 0, path.c_str(), (int)path.length(), nullptr, 0);

    if (convertResult > 0) {
        std::wstring widePath;
        widePath.resize(convertResult + 1);
        convertResult = MultiByteToWideChar(nCodePage,
                                            0,
                                            path.c_str(),
                                            (int)path.length(),
                                            widePath.data(),
                                            (int)widePath.size());

        if (convertResult > 0)
            return widePath;
    }

    return {};
#else
    return path;
#endif
}

std::string pathToString(const std::filesystem::path &path)
{
#ifdef USE_WINDOWS_CODECVT
    std::wstring widePath = path.wstring();

    // Use windows string conversion api due to mingw64 limitations.
    int nCodePage     = GetACP();
    int convertResult = WideCharToMultiByte(nCodePage,
                                            0,
                                            widePath.c_str(),
                                            (int)widePath.length(),
                                            nullptr,
                                            0,
                                            0,
                                            0);

    if (convertResult > 0) {
        std::string narrowPath;
        narrowPath.resize(convertResult + 1);
        convertResult = WideCharToMultiByte(nCodePage,
                                            0,
                                            widePath.c_str(),
                                            (int)widePath.length(),
                                            narrowPath.data(),
                                            (int)narrowPath.size(),
                                            0,
                                            0);

        if (convertResult > 0)
            return narrowPath.c_str();
    }

    return {};
#else
    return path.string();
#endif
}

std::vector<std::string> listAllFilesInDirRecursively(const std::string              &dirpath,
                                                      const std::vector<std::string> &extensions)
{
    auto inExts = [&](std::filesystem::path extension) {
        for (const auto &ext : extensions) {
            if (ext == extension)
                return true;
        }
        return false;
    };

    std::vector<std::string> filenames;
    for (auto &p : std::filesystem::recursive_directory_iterator(pathFromString(dirpath))) {
        if (p.is_regular_file() && inExts(p.path().extension()))
            filenames.push_back(pathToString(p.path()));
    }
    return filenames;
}

std::vector<std::string> makeFileListFromPathList(const std::vector<std::string> &paths,
                                                  const std::vector<std::string> &extensions)
{
    std::vector<std::string> filenames;
    for (const auto &path : paths) {
        if (std::filesystem::is_directory(pathFromString(path))) {
            auto fn = listAllFilesInDirRecursively(path, extensions);
            filenames.insert(filenames.end(), fn.begin(), fn.end());
        }
        else {
            filenames.push_back(path);
        }
    }
    return filenames;
}

bool ensureDir(std::string dirpath, bool raiseException)
{
    std::filesystem::path path = pathFromString(dirpath);
    std::error_code       ec;
    if (std::filesystem::exists(path, ec))
        return true;

    if (raiseException)
        return std::filesystem::create_directories(path);
    else
        return std::filesystem::create_directories(path, ec);
}
