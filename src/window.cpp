#include "window.hpp"

CWindow::CWindow() { this->setDirty(true); this->setFullscreen(false); this->setIsFloating(false); }
CWindow::~CWindow() { }