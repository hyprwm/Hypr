#include "window.hpp"
#include "windowManager.hpp"

CWindow::CWindow() { this->setDirty(true); this->setFullscreen(false); this->setIsFloating(false); this->setParentNodeID(0); this->setChildNodeAID(0); this->setChildNodeBID(0); this->setName(""); }
CWindow::~CWindow() { }

void CWindow::generateNodeID() {

    int64_t lowest = -1;
    for (auto& window : g_pWindowManager->windows) {
        if ((int64_t)window.getDrawable() < lowest) {
            lowest = (int64_t)window.getDrawable();
        }
    }

    m_iDrawable = lowest - 1;
}

void CWindow::setDirtyRecursive(bool val) {
    m_bDirty = val;

    if (m_iChildNodeAID != 0) {
        g_pWindowManager->getWindowFromDrawable(m_iChildNodeAID)->setDirtyRecursive(val);
        g_pWindowManager->getWindowFromDrawable(m_iChildNodeBID)->setDirtyRecursive(val);
    }
}

void CWindow::recalcSizePosRecursive() {
    if (m_iChildNodeAID != 0) {
        const auto HORIZONTAL = m_vecSize.x > m_vecSize.y;
        g_pWindowManager->getWindowFromDrawable(m_iChildNodeAID)->setPosition(m_vecPosition);
        g_pWindowManager->getWindowFromDrawable(m_iChildNodeBID)->setPosition(m_vecPosition + (HORIZONTAL ? Vector2D(m_vecSize.x / 2.f, 0) : Vector2D(0, m_vecSize.y / 2.f)));
        
        g_pWindowManager->getWindowFromDrawable(m_iChildNodeAID)->setSize(Vector2D(m_vecSize.x / (HORIZONTAL ? 2 : 1), m_vecSize.y / (HORIZONTAL ? 1 : 2)));
        g_pWindowManager->getWindowFromDrawable(m_iChildNodeBID)->setSize(Vector2D(m_vecSize.x / (HORIZONTAL ? 2 : 1), m_vecSize.y / (HORIZONTAL ? 1 : 2)));

        g_pWindowManager->getWindowFromDrawable(m_iChildNodeAID)->setDirty(true);
        g_pWindowManager->getWindowFromDrawable(m_iChildNodeBID)->setDirty(true);

        if (m_iChildNodeAID < 0) {
            g_pWindowManager->getWindowFromDrawable(m_iChildNodeAID)->recalcSizePosRecursive();
        }

        if (m_iChildNodeBID < 0) {
            g_pWindowManager->getWindowFromDrawable(m_iChildNodeBID)->recalcSizePosRecursive();
        }
    }
}