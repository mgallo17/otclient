/*
 * Copyright (c) 2010-2026 OTClient <https://github.com/edubart/otclient>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef __APPLE__

#include "sdl2window.h"
#include <framework/core/application.h>
#include <framework/graphics/image.h>

SDL2Window::SDL2Window()
{
    // Key mappings are handled in sdlKeyToFwKey
    m_defaultCursor = nullptr;
}

void SDL2Window::init()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        g_logger.fatal(stdext::format("SDL2 init failed: %s", SDL_GetError()));
        return;
    }

    // Request OpenGL 2.1 context (compatible with OTClient's renderer)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    m_window = SDL_CreateWindow(
        "OTClient",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!m_window) {
        g_logger.fatal(stdext::format("SDL2 window creation failed: %s", SDL_GetError()));
        return;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        g_logger.fatal(stdext::format("SDL2 GL context creation failed: %s", SDL_GetError()));
        return;
    }

    SDL_GL_MakeCurrent(m_window, m_glContext);

    // Initialize GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        g_logger.fatal(stdext::format("GLEW init failed: %s", (const char*)glewGetErrorString(err)));
        return;
    }

    // Get initial window size
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    m_size = Size(w, h);

    int x, y;
    SDL_GetWindowPosition(m_window, &x, &y);
    m_position = Point(x, y);

    // Enable text input
    SDL_StartTextInput();

    m_created = true;
    m_visible = true;
    m_focused = true;

    m_defaultCursor = SDL_GetDefaultCursor();
}

void SDL2Window::terminate()
{
    SDL_StopTextInput();

    for (auto* cursor : m_cursors) {
        if (cursor)
            SDL_FreeCursor(cursor);
    }
    m_cursors.clear();

    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void SDL2Window::move(const Point& pos)
{
    SDL_SetWindowPosition(m_window, pos.x, pos.y);
    m_position = pos;
}

void SDL2Window::resize(const Size& size)
{
    SDL_SetWindowSize(m_window, size.width(), size.height());
    m_size = size;
}

void SDL2Window::show()
{
    SDL_ShowWindow(m_window);
    m_visible = true;
}

void SDL2Window::hide()
{
    SDL_HideWindow(m_window);
    m_visible = false;
}

void SDL2Window::maximize()
{
    updateUnmaximizedCoords();
    SDL_MaximizeWindow(m_window);
    m_maximized = true;
}

void SDL2Window::poll()
{
    SDL_Event event;
    bool needsResizeUpdate = false;

    fireKeysPress();

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT: {
                if (m_onClose)
                    m_onClose();
                break;
            }

            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED: {
                        Size newSize(event.window.data1, event.window.data2);
                        if (m_size != newSize) {
                            m_size = newSize;
                            needsResizeUpdate = true;
                        }
                        break;
                    }
                    case SDL_WINDOWEVENT_MOVED: {
                        m_position = Point(event.window.data1, event.window.data2);
                        updateUnmaximizedCoords();
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_GAINED: {
                        m_focused = true;
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_LOST: {
                        m_focused = false;
                        releaseAllKeys();
                        break;
                    }
                    case SDL_WINDOWEVENT_MAXIMIZED: {
                        m_maximized = true;
                        break;
                    }
                    case SDL_WINDOWEVENT_RESTORED: {
                        m_maximized = false;
                        break;
                    }
                    case SDL_WINDOWEVENT_SHOWN: {
                        m_visible = true;
                        break;
                    }
                    case SDL_WINDOWEVENT_HIDDEN: {
                        m_visible = false;
                        break;
                    }
                }
                break;
            }

            case SDL_KEYDOWN: {
                Fw::Key keyCode = sdlKeyToFwKey(event.key.keysym.sym);
                updateKeyboardModifiers(SDL_GetModState());
                processKeyDown(keyCode);
                break;
            }

            case SDL_KEYUP: {
                Fw::Key keyCode = sdlKeyToFwKey(event.key.keysym.sym);
                updateKeyboardModifiers(SDL_GetModState());
                processKeyUp(keyCode);
                break;
            }

            case SDL_TEXTINPUT: {
                // Don't process text input if Ctrl/Cmd or Alt is held
                SDL_Keymod mod = SDL_GetModState();
                if (mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT))
                    break;

                std::string text = event.text.text;
                if (!text.empty() && m_onInputEvent) {
                    InputEvent inputEvent;
                    inputEvent.type = Fw::KeyTextInputEvent;
                    inputEvent.keyText = text;
                    m_onInputEvent(inputEvent);
                }
                break;
            }

            case SDL_MOUSEMOTION: {
                if (m_onInputEvent) {
                    InputEvent inputEvent;
                    inputEvent.type = Fw::MouseMoveInputEvent;
                    inputEvent.mousePos = Point(event.motion.x, event.motion.y);
                    m_inputEvent.mousePos = inputEvent.mousePos;
                    m_onInputEvent(inputEvent);
                }
                break;
            }

            case SDL_MOUSEBUTTONDOWN: {
                if (m_onInputEvent) {
                    InputEvent inputEvent;
                    inputEvent.type = Fw::MousePressInputEvent;
                    inputEvent.mousePos = Point(event.button.x, event.button.y);
                    m_inputEvent.mousePos = inputEvent.mousePos;

                    switch (event.button.button) {
                        case SDL_BUTTON_LEFT:
                            inputEvent.mouseButton = Fw::MouseLeftButton;
                            break;
                        case SDL_BUTTON_RIGHT:
                            inputEvent.mouseButton = Fw::MouseRightButton;
                            break;
                        case SDL_BUTTON_MIDDLE:
                            inputEvent.mouseButton = Fw::MouseMidButton;
                            break;
                        default:
                            inputEvent.mouseButton = Fw::MouseLeftButton;
                            break;
                    }
                    m_mouseButtonStates |= (1u << inputEvent.mouseButton);
                    updateKeyboardModifiers(SDL_GetModState());
                    inputEvent.keyboardModifiers = m_inputEvent.keyboardModifiers;
                    m_onInputEvent(inputEvent);
                }
                break;
            }

            case SDL_MOUSEBUTTONUP: {
                if (m_onInputEvent) {
                    InputEvent inputEvent;
                    inputEvent.type = Fw::MouseReleaseInputEvent;
                    inputEvent.mousePos = Point(event.button.x, event.button.y);
                    m_inputEvent.mousePos = inputEvent.mousePos;

                    switch (event.button.button) {
                        case SDL_BUTTON_LEFT:
                            inputEvent.mouseButton = Fw::MouseLeftButton;
                            break;
                        case SDL_BUTTON_RIGHT:
                            inputEvent.mouseButton = Fw::MouseRightButton;
                            break;
                        case SDL_BUTTON_MIDDLE:
                            inputEvent.mouseButton = Fw::MouseMidButton;
                            break;
                        default:
                            inputEvent.mouseButton = Fw::MouseLeftButton;
                            break;
                    }
                    m_mouseButtonStates &= ~(1u << inputEvent.mouseButton);
                    updateKeyboardModifiers(SDL_GetModState());
                    inputEvent.keyboardModifiers = m_inputEvent.keyboardModifiers;
                    m_onInputEvent(inputEvent);
                }
                break;
            }

            case SDL_MOUSEWHEEL: {
                if (m_onInputEvent) {
                    InputEvent inputEvent;
                    inputEvent.type = Fw::MouseWheelInputEvent;
                    inputEvent.mousePos = m_inputEvent.mousePos;
                    inputEvent.wheelDirection = event.wheel.y > 0 ? Fw::MouseWheelUp : Fw::MouseWheelDown;
                    updateKeyboardModifiers(SDL_GetModState());
                    inputEvent.keyboardModifiers = m_inputEvent.keyboardModifiers;
                    m_onInputEvent(inputEvent);
                }
                break;
            }
        }
    }

    if (needsResizeUpdate && m_onResize)
        m_onResize(m_size);
}

void SDL2Window::swapBuffers()
{
    SDL_GL_SwapWindow(m_window);
}

void SDL2Window::showMouse()
{
    SDL_ShowCursor(SDL_ENABLE);
    m_mouseHidden = false;
}

void SDL2Window::hideMouse()
{
    SDL_ShowCursor(SDL_DISABLE);
    m_mouseHidden = true;
}

void SDL2Window::setMouseCursor(int cursorId)
{
    if (cursorId >= 0 && cursorId < (int)m_cursors.size() && m_cursors[cursorId]) {
        SDL_SetCursor(m_cursors[cursorId]);
    }
}

void SDL2Window::restoreMouseCursor()
{
    SDL_SetCursor(SDL_GetDefaultCursor());
}

void SDL2Window::setTitle(std::string_view title)
{
    SDL_SetWindowTitle(m_window, std::string(title).c_str());
}

void SDL2Window::setMinimumSize(const Size& minimumSize)
{
    SDL_SetWindowMinimumSize(m_window, minimumSize.width(), minimumSize.height());
    m_minimumSize = minimumSize;
}

void SDL2Window::setFullscreen(bool fullscreen)
{
    if (m_fullscreen == fullscreen)
        return;

    SDL_SetWindowFullscreen(m_window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    m_fullscreen = fullscreen;
}

void SDL2Window::setVerticalSync(bool enable)
{
    SDL_GL_SetSwapInterval(enable ? 1 : 0);
    m_vsync = enable;
}

void SDL2Window::setIcon(const std::string& file)
{
    ImagePtr image = Image::load(file);
    if (!image)
        return;

    int width = image->getWidth();
    int height = image->getHeight();
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        (void*)image->getPixelData(),
        width, height, 32, width * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
    );
    if (surface) {
        SDL_SetWindowIcon(m_window, surface);
        SDL_FreeSurface(surface);
    }
}

void SDL2Window::setClipboardText(std::string_view text)
{
    SDL_SetClipboardText(std::string(text).c_str());
}

Size SDL2Window::getDisplaySize()
{
    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) == 0)
        return Size(mode.w, mode.h);
    return Size(800, 600);
}

std::string SDL2Window::getClipboardText()
{
    char* text = SDL_GetClipboardText();
    if (text) {
        std::string result(text);
        SDL_free(text);
        return result;
    }
    return {};
}

std::string SDL2Window::getPlatformType()
{
    return "SDL2-macOS";
}

int SDL2Window::internalLoadMouseCursor(const ImagePtr& image, const Point& hotSpot)
{
    int width = image->getWidth();
    int height = image->getHeight();

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        (void*)image->getPixelData(),
        width, height, 32, width * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
    );

    if (!surface)
        return -1;

    SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotSpot.x, hotSpot.y);
    SDL_FreeSurface(surface);

    if (!cursor)
        return -1;

    m_cursors.push_back(cursor);
    return (int)m_cursors.size() - 1;
}

void SDL2Window::updateKeyboardModifiers(SDL_Keymod mod)
{
    m_inputEvent.keyboardModifiers = 0;

    // On macOS: Command (GUI) acts as Ctrl for the game
    if (mod & KMOD_GUI)
        m_inputEvent.keyboardModifiers |= Fw::KeyboardCtrlModifier;
    // Ctrl still works as Ctrl too
    if (mod & KMOD_CTRL)
        m_inputEvent.keyboardModifiers |= Fw::KeyboardCtrlModifier;
    if (mod & KMOD_ALT)
        m_inputEvent.keyboardModifiers |= Fw::KeyboardAltModifier;
    if (mod & KMOD_SHIFT)
        m_inputEvent.keyboardModifiers |= Fw::KeyboardShiftModifier;
}

Fw::Key SDL2Window::sdlKeyToFwKey(SDL_Keycode key)
{
    switch (key) {
        case SDLK_ESCAPE: return Fw::KeyEscape;
        case SDLK_TAB: return Fw::KeyTab;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return Fw::KeyEnter;
        case SDLK_BACKSPACE: return Fw::KeyBackspace;
        case SDLK_INSERT: return Fw::KeyInsert;
        case SDLK_DELETE: return Fw::KeyDelete;
        case SDLK_PAUSE: return Fw::KeyPause;
        case SDLK_PRINTSCREEN: return Fw::KeyPrintScreen;
        case SDLK_HOME: return Fw::KeyHome;
        case SDLK_END: return Fw::KeyEnd;
        case SDLK_PAGEUP: return Fw::KeyPageUp;
        case SDLK_PAGEDOWN: return Fw::KeyPageDown;
        case SDLK_UP: return Fw::KeyUp;
        case SDLK_DOWN: return Fw::KeyDown;
        case SDLK_LEFT: return Fw::KeyLeft;
        case SDLK_RIGHT: return Fw::KeyRight;
        case SDLK_NUMLOCKCLEAR: return Fw::KeyNumLock;
        case SDLK_SCROLLLOCK: return Fw::KeyScrollLock;
        case SDLK_CAPSLOCK: return Fw::KeyCapsLock;
        case SDLK_LCTRL:
        case SDLK_RCTRL: return Fw::KeyCtrl;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT: return Fw::KeyShift;
        case SDLK_LALT:
        case SDLK_RALT: return Fw::KeyAlt;
        case SDLK_LGUI:
        case SDLK_RGUI: return Fw::KeyMeta;
        case SDLK_MENU: return Fw::KeyMenu;
        case SDLK_SPACE: return Fw::KeySpace;

        // Function keys
        case SDLK_F1: return Fw::KeyF1;
        case SDLK_F2: return Fw::KeyF2;
        case SDLK_F3: return Fw::KeyF3;
        case SDLK_F4: return Fw::KeyF4;
        case SDLK_F5: return Fw::KeyF5;
        case SDLK_F6: return Fw::KeyF6;
        case SDLK_F7: return Fw::KeyF7;
        case SDLK_F8: return Fw::KeyF8;
        case SDLK_F9: return Fw::KeyF9;
        case SDLK_F10: return Fw::KeyF10;
        case SDLK_F11: return Fw::KeyF11;
        case SDLK_F12: return Fw::KeyF12;

        // Letters (SDL gives lowercase, Fw::Key uses uppercase ASCII)
        case SDLK_a: return Fw::KeyA;
        case SDLK_b: return Fw::KeyB;
        case SDLK_c: return Fw::KeyC;
        case SDLK_d: return Fw::KeyD;
        case SDLK_e: return Fw::KeyE;
        case SDLK_f: return Fw::KeyF;
        case SDLK_g: return Fw::KeyG;
        case SDLK_h: return Fw::KeyH;
        case SDLK_i: return Fw::KeyI;
        case SDLK_j: return Fw::KeyJ;
        case SDLK_k: return Fw::KeyK;
        case SDLK_l: return Fw::KeyL;
        case SDLK_m: return Fw::KeyM;
        case SDLK_n: return Fw::KeyN;
        case SDLK_o: return Fw::KeyO;
        case SDLK_p: return Fw::KeyP;
        case SDLK_q: return Fw::KeyQ;
        case SDLK_r: return Fw::KeyR;
        case SDLK_s: return Fw::KeyS;
        case SDLK_t: return Fw::KeyT;
        case SDLK_u: return Fw::KeyU;
        case SDLK_v: return Fw::KeyV;
        case SDLK_w: return Fw::KeyW;
        case SDLK_x: return Fw::KeyX;
        case SDLK_y: return Fw::KeyY;
        case SDLK_z: return Fw::KeyZ;

        // Numbers
        case SDLK_0: return Fw::Key0;
        case SDLK_1: return Fw::Key1;
        case SDLK_2: return Fw::Key2;
        case SDLK_3: return Fw::Key3;
        case SDLK_4: return Fw::Key4;
        case SDLK_5: return Fw::Key5;
        case SDLK_6: return Fw::Key6;
        case SDLK_7: return Fw::Key7;
        case SDLK_8: return Fw::Key8;
        case SDLK_9: return Fw::Key9;

        // Numpad
        case SDLK_KP_0: return Fw::KeyNumpad0;
        case SDLK_KP_1: return Fw::KeyNumpad1;
        case SDLK_KP_2: return Fw::KeyNumpad2;
        case SDLK_KP_3: return Fw::KeyNumpad3;
        case SDLK_KP_4: return Fw::KeyNumpad4;
        case SDLK_KP_5: return Fw::KeyNumpad5;
        case SDLK_KP_6: return Fw::KeyNumpad6;
        case SDLK_KP_7: return Fw::KeyNumpad7;
        case SDLK_KP_8: return Fw::KeyNumpad8;
        case SDLK_KP_9: return Fw::KeyNumpad9;

        // Punctuation / symbols
        case SDLK_EXCLAIM: return Fw::KeyExclamation;
        case SDLK_QUOTEDBL: return Fw::KeyQuote;
        case SDLK_HASH: return Fw::KeyNumberSign;
        case SDLK_DOLLAR: return Fw::KeyDollar;
        case SDLK_PERCENT: return Fw::KeyPercent;
        case SDLK_AMPERSAND: return Fw::KeyAmpersand;
        case SDLK_QUOTE: return Fw::KeyApostrophe;
        case SDLK_LEFTPAREN: return Fw::KeyLeftParen;
        case SDLK_RIGHTPAREN: return Fw::KeyRightParen;
        case SDLK_ASTERISK: return Fw::KeyAsterisk;
        case SDLK_PLUS: return Fw::KeyPlus;
        case SDLK_COMMA: return Fw::KeyComma;
        case SDLK_MINUS: return Fw::KeyMinus;
        case SDLK_PERIOD: return Fw::KeyPeriod;
        case SDLK_SLASH: return Fw::KeySlash;
        case SDLK_COLON: return Fw::KeyColon;
        case SDLK_SEMICOLON: return Fw::KeySemicolon;
        case SDLK_LESS: return Fw::KeyLess;
        case SDLK_EQUALS: return Fw::KeyEqual;
        case SDLK_GREATER: return Fw::KeyGreater;
        case SDLK_QUESTION: return Fw::KeyQuestion;
        case SDLK_AT: return Fw::KeyAtSign;
        case SDLK_LEFTBRACKET: return Fw::KeyLeftBracket;
        case SDLK_BACKSLASH: return Fw::KeyBackslash;
        case SDLK_RIGHTBRACKET: return Fw::KeyRightBracket;
        case SDLK_CARET: return Fw::KeyCaret;
        case SDLK_UNDERSCORE: return Fw::KeyUnderscore;
        case SDLK_BACKQUOTE: return Fw::KeyGrave;

        default: return Fw::KeyUnknown;
    }
}

#endif // __APPLE__
