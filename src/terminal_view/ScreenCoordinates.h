#pragma once

#include <terminal/Screen.h>
#include <terminal/Color.h>

#include <ostream>
#include <utility>

#include <QtCore/QPoint>

namespace terminal::view {

struct ScreenCoordinates {
    WindowSize screenSize;
    unsigned cellWidth;
    unsigned cellHeight;
    unsigned textBaseline;
    int leftMargin = 0;
    int bottomMargin = 0;

    /// Maps screen coordinates to target surface coordinates.
    ///
    /// @param col screen coordinate's column (between 1 and number of screen columns)
    /// @param row screen coordinate's line (between 1 and number of screen lines)
    ///
    /// @return 2D point into drawing coordinate system
    constexpr QPoint map(cursor_pos_t col, cursor_pos_t row) const noexcept {
        return QPoint{
            static_cast<int>(leftMargin + (col - 1) * cellWidth),
#if defined(LIBTERMINAL_VIEW_NATURAL_COORDS) && LIBTERMINAL_VIEW_NATURAL_COORDS
            static_cast<int>(bottomMargin + (screenSize.rows - row) * cellHeight)
#else
            static_cast<int>((row - 1) * cellHeight)
#endif
        };
    }
};

inline std::ostream& operator<<(std::ostream& os, terminal::view::ScreenCoordinates const& _coords)
{
    os << "screen: " << _coords.screenSize.columns<< 'x' << _coords.screenSize.rows
       << ", cell:" << _coords.cellWidth << 'x' << _coords.cellHeight
       << ", base: " << _coords.textBaseline
       << ", margin: " << _coords.leftMargin << 'x' << _coords.bottomMargin;
    return os;
}

} // end namespace
