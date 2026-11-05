// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace photos {

// One image file on disk.
struct PhotoItem {
    std::string path;   // absolute path
    std::string name;   // file name
    std::string album;  // top-level subfolder it came from, empty for the root
    int64_t modified = 0;   // seconds since the epoch
    uint64_t size = 0;      // bytes
};

// The images of a folder: everything below it, and one album per top-level
// subfolder that holds images.
struct LibraryData {
    std::string root;
    // Every image, chronological by modification time, then path.
    std::vector<PhotoItem> photos;
    std::vector<PhotoItem> rootPhotos;   // directly in root, not in an album
    struct Album {
        std::string name;
        std::string path;
        bool custom = false;
        std::vector<PhotoItem> photos;
    };
    std::vector<Album> albums;
    uint64_t totalSize = 0;
};

LibraryData scanLibrary(const std::string& folder, const std::vector<std::string>& customAlbums = {});
bool isImageFile(const std::string& fileName);

std::string humanSize(uint64_t bytes);
std::string typeDescription(const std::string& path);   // "PNG Image"
std::string formatDate(int64_t seconds);                // "Nov 5, 2026"

// The user's favorites, kept in one file of absolute paths.
class Favorites {
public:
    Favorites();

    const std::set<std::string>& paths() const { return m_paths; }
    bool contains(const std::string& path) const { return m_paths.count(path) != 0; }
    // Flips the path's membership and stores it; the new state.
    bool toggle(const std::string& path);

private:
    std::string storageFile() const;
    void load();
    void save();

    std::set<std::string> m_paths;
};

} // namespace photos
