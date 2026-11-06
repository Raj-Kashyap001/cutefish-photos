// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "photo.h"

#include "ui/window.h"
#include <cutefish/appearance/appearance.h>
#include "base/settings.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cutefish::ui {
class Menu;
class Toast;
class DialogWindow;
}
namespace cutefish::base {
class Process;
}

namespace photos {

// The Photos main window: the sidebar of albums, the grid of thumbnails or the
// single photo, and the info panel.
class PhotosWindow {
public:
    PhotosWindow();
    ~PhotosWindow();

    cutefish::ui::Window& window() { return m_window; }

    // A folder to browse, or an image file to show; empty uses the last one
    // opened, or the Pictures directory.
    void openArgument(const std::string& argument);

private:
    class Root;

    // Rescans the folder and refreshes the grid.
    void setFolder(const std::string& folder);
    void setSelection(const std::string& selection);
    void openPath(const std::string& path);
    void refreshItems();
    void navigate(int step);
    void toggleInfo();
    void toggleFavorite();
    void zoomIn();
    void zoomOut();
    void backToGrid();
    void chooseFolder();
    void applySearch();
    void updateHeader();
    void showDetail(int index);
    int indexOfPath(const std::string& path) const;

    // The global menu in the status bar, and what its rows do.
    void buildMainMenu();
    void showAbout();
    void showShortcutsDialog();
    int currentPhotoIndex() const;
    std::string currentPhotoPath() const;
    void showInFileManager();
    void showItemInFileManager(const std::string& path);
    void setAsWallpaper(const std::string& path);
    void showRenameDialog(int index);
    void moveToTrash(int index);
    void showGridContextMenu(int index, const cutefish::ui::PointF& windowPos);
    void showGridEmptyContextMenu(const cutefish::ui::PointF& windowPos);
    void showDetailContextMenu(const cutefish::ui::PointF& windowPos);
    void showSidebarContextMenu(int rowIndex, const cutefish::ui::PointF& windowPos);
    void rotateImage(double degrees);
    void rotatePreview(int deltaDegrees);
    void resetPreviewRotation();
    void savePreviewRotation();
    void flipImage(bool horizontal);
    void showResizeDialog(int index = -1);
    void showConvertDialog(int index = -1);
    void toggleSidebar();
    void toggleFullScreen();
    void copySelectedPaths(bool relative);
    void chooseAlbumFolder();
    void addAlbumFolder(const std::string& folder);
    void removeAlbumFolder(const std::string& folder);
    void runMagick(const std::vector<std::string>& arguments,
                   std::function<void(int exitCode, const std::string& errorOutput)> done);
    // Rescans the folder after an in-place edit and keeps what the user was looking at.
    void reloadAfterEdit();
    void showNotification(const std::string& text, int milliseconds = 4000);

    static const std::vector<double>& gridSizes();

    cutefish::ui::Window m_window;
    Root* m_root = nullptr;

    cutefish::base::Settings m_settings;
    LibraryData m_library;
    Favorites m_favorites;
    std::string m_folder;
    std::vector<std::string> m_customAlbums;
    std::string m_selection = "all";
    std::string m_search;
    std::vector<PhotoItem> m_items;
    int m_detailIndex = -1;
    bool m_detail = false;
    bool m_infoVisible = false;
    bool m_sidebarVisible = true;
    int m_zoomLevel = 1;   // index into gridSizes()

    std::unique_ptr<cutefish::ui::Menu> m_mainMenu;
    std::unique_ptr<cutefish::ui::Menu> m_contextMenu;
    std::unique_ptr<cutefish::ui::Toast> m_toast;
    std::unique_ptr<cutefish::ui::DialogWindow> m_aboutDialog;
    std::unique_ptr<cutefish::ui::DialogWindow> m_shortcutsDialog;
    std::unique_ptr<cutefish::base::Process> m_process;
    std::string m_magickError;
    cutefish::appearance::Appearance m_appearance;
};

} // namespace photos
