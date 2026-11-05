// SPDX-License-Identifier: BSD-3-Clause
#include "photo.h"

#include "base/paths.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace photos {

static const char* imageExtensions[] = {
    "png", "jpg", "jpeg", "jpe", "webp", "gif", "bmp", "ico", "qoi",
    "tif", "tiff", "heif", "heic", "avif", "jxl", "svg",
    "ppm", "pgm", "pbm", "pnm", "xpm", "tga", "exr",
};

bool isImageFile(const std::string& fileName)
{
    const size_t dot = fileName.rfind('.');
    if (dot == std::string::npos)
        return false;
    std::string extension = fileName.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    for (const char* known : imageExtensions)
        if (extension == known)
            return true;
    return false;
}

static int64_t modifiedSeconds(const std::filesystem::directory_entry& entry)
{
    std::error_code error;
    const auto time = entry.last_write_time(error);
    if (error)
        return 0;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
    return int64_t(seconds);
}

static uint64_t fileSize(const std::filesystem::directory_entry& entry)
{
    std::error_code error;
    const auto size = entry.file_size(error);
    return error ? 0 : uint64_t(size);
}

static PhotoItem makeItem(const std::filesystem::directory_entry& entry, const std::string& album)
{
    PhotoItem item;
    item.path = entry.path().string();
    item.name = entry.path().filename().string();
    item.album = album;
    item.modified = modifiedSeconds(entry);
    item.size = fileSize(entry);
    return item;
}

static void sortChronological(std::vector<PhotoItem>& items)
{
    std::stable_sort(items.begin(), items.end(), [](const PhotoItem& a, const PhotoItem& b) {
        if (a.modified != b.modified)
            return a.modified < b.modified;
        return a.path < b.path;
    });
}

LibraryData scanLibrary(const std::string& folder, const std::vector<std::string>& customAlbums)
{
    LibraryData data;
    data.root = folder;

    std::error_code error;
    const std::filesystem::path rootPath = std::filesystem::canonical(folder, error);
    if (!error && std::filesystem::is_directory(rootPath)) {
        // One album per top-level subfolder that contains at least one image.
        std::vector<std::filesystem::directory_entry> subfolders;
        for (const auto& entry : std::filesystem::directory_iterator(rootPath, error)) {
            if (error)
                break;
            const std::string name = entry.path().filename().string();
            if (name.empty() || name.front() == '.')
                continue;
            std::error_code inner;
            if (entry.is_directory(inner))
                subfolders.push_back(entry);
        }

        for (const auto& entry : subfolders) {
            const std::string album = entry.path().filename().string();
            LibraryData::Album result;
            result.name = album;
            result.path = entry.path().string();
            result.custom = false;
            std::error_code walkError;
            for (const auto& file : std::filesystem::recursive_directory_iterator(
                     entry.path(), std::filesystem::directory_options::skip_permission_denied, walkError)) {
                if (walkError)
                    break;
                if (!file.is_regular_file())
                    continue;
                if (!isImageFile(file.path().filename().string()))
                    continue;
                result.photos.push_back(makeItem(file, album));
            }
            sortChronological(result.photos);
            if (result.photos.empty())
                continue;
            data.albums.push_back(std::move(result));
        }

        for (const auto& entry : std::filesystem::directory_iterator(rootPath, error)) {
            if (error)
                break;
            if (!entry.is_regular_file())
                continue;
            if (!isImageFile(entry.path().filename().string()))
                continue;
            data.rootPhotos.push_back(makeItem(entry, {}));
        }
        sortChronological(data.rootPhotos);
    }

    // Add custom external albums
    for (const auto& customPath : customAlbums) {
        std::error_code customErr;
        const std::filesystem::path cPath = std::filesystem::canonical(customPath, customErr);
        if (customErr || !std::filesystem::is_directory(cPath))
            continue;
        // Avoid duplicate albums if it was already inside the library folder
        bool alreadyAdded = false;
        for (const auto& existing : data.albums) {
            if (existing.path == cPath.string()) {
                alreadyAdded = true;
                break;
            }
        }
        if (alreadyAdded)
            continue;

        const std::string albumName = cPath.filename().string();
        LibraryData::Album result;
        result.name = albumName;
        result.path = cPath.string();
        result.custom = true;
        std::error_code walkErr;
        for (const auto& file : std::filesystem::recursive_directory_iterator(
                 cPath, std::filesystem::directory_options::skip_permission_denied, walkErr)) {
            if (walkErr)
                break;
            if (!file.is_regular_file())
                continue;
            if (!isImageFile(file.path().filename().string()))
                continue;
            result.photos.push_back(makeItem(file, albumName));
        }
        sortChronological(result.photos);
        data.albums.push_back(std::move(result));
    }

    std::sort(data.albums.begin(), data.albums.end(),
              [](const auto& a, const auto& b) { return a.name < b.name; });

    for (const auto& item : data.rootPhotos)
        data.photos.push_back(item);
    for (const auto& album : data.albums)
        for (const auto& item : album.photos)
            data.photos.push_back(item);
    sortChronological(data.photos);

    for (const auto& item : data.photos)
        data.totalSize += item.size;
    return data;
}

std::string humanSize(uint64_t bytes)
{
    char buffer[64];
    if (bytes < 1024) {
        std::snprintf(buffer, sizeof buffer, "%llu B", (unsigned long long)bytes);
        return buffer;
    }
    double value = double(bytes);
    const char* units[] = {"KB", "MB", "GB", "TB"};
    int unit = -1;
    do {
        value /= 1024.0;
        ++unit;
    } while (value >= 1024.0 && unit < 3);
    if (value >= 100)
        std::snprintf(buffer, sizeof buffer, "%.0f %s", value, units[unit]);
    else if (value >= 10)
        std::snprintf(buffer, sizeof buffer, "%.1f %s", value, units[unit]);
    else
        std::snprintf(buffer, sizeof buffer, "%.2f %s", value, units[unit]);
    return buffer;
}

std::string typeDescription(const std::string& path)
{
    const size_t dot = path.rfind('.');
    std::string extension = dot == std::string::npos ? std::string() : path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::toupper(c)); });
    if (extension.empty())
        return "Image";
    return extension + " Image";
}

std::string formatDate(int64_t seconds)
{
    if (seconds <= 0)
        return {};
    const std::time_t value = std::time_t(seconds);
    std::tm tm {};
    if (!localtime_r(&value, &tm))
        return {};
    char buffer[64];
    // %e is a single space before single-digit days; collapse it below.
    std::strftime(buffer, sizeof buffer, "%b %e, %Y", &tm);
    std::string out = buffer;
    const size_t space = out.find(' ');
    if (space != std::string::npos && out.substr(space + 1, 1) == " ")
        out.erase(space + 1, 1);
    return out;
}

Favorites::Favorites()
{
    load();
}

std::string Favorites::storageFile() const
{
    return cutefish::base::paths::dataHome() + "/cutefish-photos/favorites";
}

void Favorites::load()
{
    std::ifstream file(storageFile());
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty())
            m_paths.insert(line);
    }
}

void Favorites::save()
{
    const std::string file = storageFile();
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(file).parent_path(), error);
    const std::string temporary = file + ".new";
    {
        std::ofstream out(temporary, std::ios::trunc);
        for (const std::string& path : m_paths)
            out << path << '\n';
    }
    std::filesystem::rename(temporary, file, error);
    if (error)
        std::filesystem::remove(temporary);
}

bool Favorites::toggle(const std::string& path)
{
    const auto it = m_paths.find(path);
    bool added;
    if (it != m_paths.end()) {
        m_paths.erase(it);
        added = false;
    } else {
        m_paths.insert(path);
        added = true;
    }
    save();
    return added;
}

} // namespace photos
