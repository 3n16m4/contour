/**
 * This file is part of the "libterminal" project
 *   Copyright (c) 2019-2020 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <terminal/Color.h>
#include <terminal/Commands.h>
#include <terminal/CommandBuilder.h>
#include <terminal/Logger.h>
#include <terminal/Parser.h>
#include <terminal/WindowSize.h>
#include <terminal/Hyperlink.h>
#include <terminal/Selector.h>
#include <terminal/InputGenerator.h> // MouseTransport
#include <terminal/VTType.h>
#include <terminal/ScreenBuffer.h>

#include <unicode/grapheme_segmenter.h>
#include <unicode/width.h>
#include <unicode/utf8.h>

#include <fmt/format.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <list>
#include <optional>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

/**
 * Terminal Screen.
 *
 * Implements the all Command types and applies all instruction
 * to an internal screen buffer, maintaining width, height, and history,
 * allowing the object owner to control which part of the screen (or history)
 * to be viewn.
 */
class Screen {
  public:
	using Cell = ScreenBuffer::Cell;
	using Cursor = ScreenBuffer::Cursor;
    using Reply = std::function<void(std::string const&)>;
    using Renderer = ScreenBuffer::Renderer;
    using ModeSwitchCallback = std::function<void(bool)>;
    using ResizeWindowCallback = std::function<void(unsigned int, unsigned int, bool)>;
    using SetApplicationKeypadMode = std::function<void(bool)>;
    using SetBracketedPaste = std::function<void(bool)>;
    using SetMouseProtocol = std::function<void(MouseProtocol, bool)>;
    using SetMouseTransport = std::function<void(MouseTransport)>;
    using SetMouseWheelMode = std::function<void(InputGenerator::MouseWheelMode)>;
	using OnSetCursorStyle = std::function<void(CursorDisplay, CursorShape)>;
    using OnBufferChanged = std::function<void(ScreenBuffer::Type)>;
    using Hook = std::function<void(std::vector<Command> const& commands)>;
    using NotifyCallback = std::function<void(std::string const&, std::string const&)>;

  public:
    /**
     * Initializes the screen with the given screen size and callbaks.
     *
     * @param _size screen dimensions in number of characters per line and number of lines.
     * @param _reply reply-callback with the data to send back to terminal input.
     * @param _logger an optional logger for logging various events.
     * @param _error an optional logger for errors.
     * @param _onCommands hook to the commands being executed by the screen.
     */
    Screen(WindowSize const& _size,
           std::optional<size_t> _maxHistoryLineCount,
           ModeSwitchCallback _useApplicationCursorKeys,
           std::function<void()> _onWindowTitleChanged,
           ResizeWindowCallback _resizeWindow,
           SetApplicationKeypadMode _setApplicationkeypadMode,
           SetBracketedPaste _setBracketedPaste,
           SetMouseProtocol _setMouseProtocol,
           SetMouseTransport _setMouseTransport,
           SetMouseWheelMode _setMouseWheelMode,
		   OnSetCursorStyle _setCursorStyle,
           Reply _reply,
           Logger const& _logger,
           bool _logRaw,
           bool _logTrace,
           Hook _onCommands,
           OnBufferChanged _onBufferChanged,
           std::function<void()> _bell,
           std::function<RGBColor(DynamicColorName)> _requestDynamicColor,
           std::function<void(DynamicColorName)> _resetDynamicColor,
           std::function<void(DynamicColorName, RGBColor const&)> _setDynamicColor,
           std::function<void(bool)> _setGenerateFocusEvents,
           NotifyCallback _notify
    );

    Screen(WindowSize const& _size,
           std::optional<size_t> _maxHistoryLineCount,
           ModeSwitchCallback _useApplicationCursorKeys,
           std::function<void()> _onWindowTitleChanged,
           ResizeWindowCallback _resizeWindow,
           SetApplicationKeypadMode _setApplicationkeypadMode,
           SetBracketedPaste _setBracketedPaste,
           SetMouseProtocol _setMouseProtocol,
           SetMouseTransport _setMouseTransport,
           SetMouseWheelMode _setMouseWheelMode,
		   OnSetCursorStyle _setCursorStyle,
           Reply _reply,
           Logger const& _logger
    ) : Screen{
        _size,
        _maxHistoryLineCount,
        std::move(_useApplicationCursorKeys),
        std::move(_onWindowTitleChanged),
        std::move(_resizeWindow),
        std::move(_setApplicationkeypadMode),
        std::move(_setBracketedPaste),
        std::move(_setMouseProtocol),
        std::move(_setMouseTransport),
        std::move(_setMouseWheelMode),
        std::move(_setCursorStyle),
        std::move(_reply),
        _logger,
        true, // logs raw output by default?
        true, // logs trace output by default?
        {}, {}, {}, {}, {}, {}, {}, {}
    } {}

    Screen(WindowSize const& _size, Logger const& _logger) :
        Screen{_size, std::nullopt, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, _logger, true, true, {}, {}, {}, {}, {}, {}, {}, {}} {}

    void setLogTrace(bool _enabled) { logTrace_ = _enabled; }
    bool logTrace() const noexcept { return logTrace_; }
    void setLogRaw(bool _enabled) { logRaw_ = _enabled; }
    bool logRaw() const noexcept { return logRaw_; }

    void setTerminalId(VTType _id) noexcept
    {
        terminalId_ = _id;
    }

    void setMaxHistoryLineCount(std::optional<size_t> _maxHistoryLineCount);
    size_t historyLineCount() const noexcept { return buffer_->historyLineCount(); }

    /// Writes given data into the screen.
    void write(char const* _data, size_t _size);

    void write(Command const& _command);

    /// Writes given data into the screen.
    void write(std::string_view const& _text) { write(_text.data(), _text.size()); }

    void write(std::u32string_view const& _text);

    /// Renders the full screen by passing every grid cell to the callback.
    void render(Renderer const& _renderer, size_t _scrollOffset = 0) const;

    /// Renders a single text line.
    std::string renderTextLine(cursor_pos_t _row) const { return buffer_->renderTextLine(_row); }

    /// Renders the full screen as text into the given string. Each line will be terminated by LF.
    std::string renderText() const { return buffer_->renderText(); }

    /// Takes a screenshot by outputting VT sequences needed to render the current state of the screen.
    ///
    /// @note Only the screenshot of the current buffer is taken, not both (main and alternate).
    ///
    /// @returns necessary commands needed to draw the current screen state,
    ///          including initial clear screen, and initial cursor hide.
    std::string screenshot() const { return buffer_->screenshot(); }

    void setFocus(bool _focused) { focused_ = _focused; }
    bool focused() const noexcept { return focused_; }

    // {{{ Command processor
    void operator()(Bell const& v);
    void operator()(FullReset const& v);
    void operator()(Linefeed const& v);
    void operator()(Backspace const& v);
    void operator()(DeviceStatusReport const& v);
    void operator()(ReportCursorPosition const& v);
    void operator()(ReportExtendedCursorPosition const& v);
    void operator()(SendDeviceAttributes const& v);
    void operator()(SendTerminalId const& v);
    void operator()(ClearToEndOfScreen const& v);
    void operator()(ClearToBeginOfScreen const& v);
    void operator()(ClearScreen const& v);
    void operator()(ClearScrollbackBuffer const& v);
    void operator()(EraseCharacters const& v);
    void operator()(ScrollUp const& v);
    void operator()(ScrollDown const& v);
    void operator()(ClearToEndOfLine const& v);
    void operator()(ClearToBeginOfLine const& v);
    void operator()(ClearLine const& v);
    void operator()(CursorNextLine const& v);
    void operator()(CursorPreviousLine const& v);
    void operator()(InsertCharacters const& v);
    void operator()(InsertLines const& v);
    void operator()(InsertColumns const& v);
    void operator()(DeleteLines const& v);
    void operator()(DeleteCharacters const& v);
    void operator()(DeleteColumns const& v);
    void operator()(HorizontalPositionAbsolute const& v);
    void operator()(HorizontalPositionRelative const& v);
    void operator()(HorizontalTabClear const& v);
    void operator()(HorizontalTabSet const& v);
    void operator()(Hyperlink const& v);
    void operator()(MoveCursorUp const& v);
    void operator()(MoveCursorDown const& v);
    void operator()(MoveCursorForward const& v);
    void operator()(MoveCursorBackward const& v);
    void operator()(MoveCursorToColumn const& v);
    void operator()(MoveCursorToBeginOfLine const& v);
    void operator()(MoveCursorTo const& v);
    void operator()(MoveCursorToLine const& v);
    void operator()(MoveCursorToNextTab const& v);
    void operator()(Notify const& v);
    void operator()(CursorBackwardTab const& v);
    void operator()(SaveCursor const& v);
    void operator()(RestoreCursor const& v);
    void operator()(Index const& v);
    void operator()(ReverseIndex const& v);
    void operator()(BackIndex const& v);
    void operator()(ForwardIndex const& v);
    void operator()(SetForegroundColor const& v);
    void operator()(SetBackgroundColor const& v);
    void operator()(SetUnderlineColor const& v);
    void operator()(SetCursorStyle const& v);
    void operator()(SetGraphicsRendition const& v);
    void operator()(SetMark const&);
    void operator()(SetMode const& v);
    void operator()(RequestMode const& v);
    void operator()(SetTopBottomMargin const& v);
    void operator()(SetLeftRightMargin const& v);
    void operator()(ScreenAlignmentPattern const& v);
    void operator()(SendMouseEvents const& v);
    void operator()(ApplicationKeypadMode const& v);
    void operator()(DesignateCharset const& v);
    void operator()(SingleShiftSelect const& v);
    void operator()(SoftTerminalReset const& v);
    void operator()(ChangeIconTitle const& v);
    void operator()(ChangeWindowTitle const& v);
    void operator()(ResizeWindow const& v);
    void operator()(SaveWindowTitle const& v);
    void operator()(RestoreWindowTitle const& v);
    void operator()(AppendChar const& v);

    void operator()(RequestDynamicColor const& v);
    void operator()(RequestTabStops const& v);
    void operator()(ResetDynamicColor const& v);
    void operator()(SetDynamicColor const& v);

    void operator()(DumpState const&);
    // }}}

    // reset screen
    void resetSoft();
    void resetHard();

    WindowSize const& size() const noexcept { return size_; }
    void resize(WindowSize const& _newSize);

    /// {{{ viewport management API
    size_t scrollOffset() const noexcept { return scrollOffset_; }
    bool isAbsoluteLineVisible(cursor_pos_t _row) const noexcept;
    bool scrollUp(size_t _numLines);
    bool scrollDown(size_t _numLines);
    bool scrollToTop();
    bool scrollToBottom();
    bool scrollMarkUp();
    bool scrollMarkDown();
    //}}}

    bool isCursorInsideMargins() const noexcept { return buffer_->isCursorInsideMargins(); }

    Coordinate realCursorPosition() const noexcept { return buffer_->realCursorPosition(); }
    Coordinate cursorPosition() const noexcept { return buffer_->cursorPosition(); }
    Cursor const& realCursor() const noexcept { return buffer_->cursor; }

    // Tests if given coordinate is within the visible screen area.
    constexpr bool contains(Coordinate const& _coord) const noexcept
    {
        return 1 <= _coord.row && _coord.row <= size_.rows
            && 1 <= _coord.column && _coord.column <= size_.columns;
    }

    Cell const& currentCell() const noexcept
    {
        return *buffer_->currentColumn;
    }

    Cell const& operator()(cursor_pos_t _row, cursor_pos_t _col) const noexcept
    {
        return buffer_->at(_row, _col);
    }

    Cell& currentCell() noexcept
    {
        return *buffer_->currentColumn;
    }

    Cell& currentCell(Cell value)
    {
        *buffer_->currentColumn = std::move(value);
        return *buffer_->currentColumn;
    }

    void moveCursorTo(Coordinate to);

    Cell& absoluteAt(Coordinate const& _coord)
    {
        return buffer_->absoluteAt(_coord);
    }

    Cell const& absoluteAt(Coordinate const& _coord) const
    {
        return const_cast<Screen&>(*this).absoluteAt(_coord);
    }

    // Gets a reference to the cell relative to screen origin (top left, 1:1).
    Cell& at(Coordinate const& _coord) noexcept
    {
        auto const y = static_cast<cursor_pos_t>(currentBuffer().savedLines.size() + _coord.row - scrollOffset_);
        return absoluteAt(Coordinate{y, _coord.column});
    }

    // Gets a reference to the cell relative to screen origin (top left, 1:1).
    Cell const& at(cursor_pos_t _rowNr, cursor_pos_t _colNr) const noexcept
    {
        return const_cast<Screen&>(*this).at(Coordinate{_rowNr, _colNr});
    }

    /// Retrieves the cell at given cursor, respecting origin mode.
    Cell& withOriginAt(cursor_pos_t row, cursor_pos_t col) { return buffer_->withOriginAt(row, col); }

    bool isPrimaryScreen() const noexcept { return buffer_ == &primaryBuffer_; }
    bool isAlternateScreen() const noexcept { return buffer_ == &alternateBuffer_; }

    ScreenBuffer const& currentBuffer() const noexcept { return *buffer_; }
    ScreenBuffer& currentBuffer() noexcept { return *buffer_; }

    bool isModeEnabled(Mode m) const noexcept
    {
        if (m == Mode::UseAlternateScreen)
            return isAlternateScreen();
        else
            return buffer_->enabledModes_.find(m) != end(buffer_->enabledModes_);
    }

    bool verticalMarginsEnabled() const noexcept { return isModeEnabled(Mode::Origin); }
    bool horizontalMarginsEnabled() const noexcept { return isModeEnabled(Mode::LeftRightMargin); }

    Margin const& margin() const noexcept { return buffer_->margin_; }
    ScreenBuffer::Lines const& scrollbackLines() const noexcept { return buffer_->savedLines; }

    void setTabWidth(unsigned int _value)
    {
        // TODO: Find out if we need to have that attribute per buffer or if having it across buffers is sufficient.
        primaryBuffer_.tabWidth = _value;
        alternateBuffer_.tabWidth = _value;
    }

    /**
     * Returns the n'th saved line into the history scrollback buffer.
     *
     * @param _lineNumberIntoHistory the 1-based offset into the history buffer.
     *
     * @returns the textual representation of the n'th line into the history.
     */
    std::string renderHistoryTextLine(cursor_pos_t _lineNumberIntoHistory) const;

    std::string const& windowTitle() const noexcept { return windowTitle_; }

    std::optional<size_t> findPrevMarker(size_t _currentScrollOffset) const
    {
        return buffer_->findPrevMarker(_currentScrollOffset);
    }

    std::optional<size_t> findNextMarker(size_t _currentScrollOffset) const
    {
        return buffer_->findNextMarker(_currentScrollOffset);
    }

    ScreenBuffer::Type bufferType() const noexcept { return buffer_->type_; }

    /// Tests whether some area has been selected.
    bool isSelectionAvailable() const noexcept { return selector_ && selector_->state() != Selector::State::Waiting; }

    /// Returns list of ranges that have been selected.
    std::vector<Selector::Range> selection() const;

    /// Sets or resets to a new selection.
    void setSelector(std::unique_ptr<Selector> _selector) { selector_ = std::move(_selector); }

    /// Tests whether or not some grid cells are selected.
    bool selectionAvailable() const noexcept { return !!selector_; }

    Selector const* selector() const noexcept { return selector_.get(); }
    Selector* selector() noexcept { return selector_.get(); }

    /// Clears current selection, if any currently present.
    void clearSelection() { selector_.reset(); }

    /// Renders only the selected area.
    void renderSelection(Renderer const& _render) const;

  private:
    void setBuffer(ScreenBuffer::Type _type);

    // interactive replies
    void reply(std::string const& message)
    {
        if (reply_)
            reply_(message);
    }

    template <typename... Args>
    void reply(std::string const& fmt, Args&&... args)
    {
        reply(fmt::format(fmt, std::forward<Args>(args)...));
    }

  private:
    Hook const onCommands_;
    Logger const logger_;
    bool logRaw_ = false;
    bool logTrace_ = false;
    bool focused_ = true;
    ModeSwitchCallback useApplicationCursorKeys_;
    std::function<void()> onWindowTitleChanged_;
    ResizeWindowCallback resizeWindow_;
    SetApplicationKeypadMode setApplicationkeypadMode_;
    SetBracketedPaste setBracketedPaste_;
    SetMouseProtocol setMouseProtocol_;
    SetMouseTransport setMouseTransport_;
    SetMouseWheelMode setMouseWheelMode_;
	OnSetCursorStyle setCursorStyle_;
    Reply const reply_;

    CommandBuilder commandBuilder_;
    Parser parser_;
    unsigned long instructionCounter_ = 0;

    VTType terminalId_ = VTType::VT525;

    ScreenBuffer primaryBuffer_;
    ScreenBuffer alternateBuffer_;
    ScreenBuffer* buffer_;

    WindowSize size_;
    std::optional<size_t> maxHistoryLineCount_;
    std::string windowTitle_{};
    std::stack<std::string> savedWindowTitles_{};

    size_t scrollOffset_ = 0;

    std::unique_ptr<Selector> selector_;

    OnBufferChanged onBufferChanged_{};
    std::function<void()> bell_{};
    std::function<RGBColor(DynamicColorName)> requestDynamicColor_{};
    std::function<void(DynamicColorName)> resetDynamicColor_{};
    std::function<void(DynamicColorName, RGBColor const&)> setDynamicColor_{};
    std::function<void(bool)> setGenerateFocusEvents_{};

    NotifyCallback notify_{};
};

}  // namespace terminal
