#include "window.hpp"
#include "windowManager.hpp"

CWindow::CWindow() { this->setPinned(false); this->setDraggingTiled(false); this->setIsPseudotiled(false); this->setSplitRatio(1); this->setDockHidden(false); this->setRealBorderColor(0); this->setEffectiveBorderColor(0); this->setFirstOpen(true); this->setConstructed(false); this->setTransient(false); this->setLastUpdatePosition(Vector2D(0,0)); this->setLastUpdateSize(Vector2D(0,0)); this->setDock(false); this->setUnderFullscreen(false); this->setIsSleeping(true); this->setFirstAnimFrame(true); this->setIsAnimated(false); this->setDead(false); this->setMasterChildIndex(0); this->setMaster(false); this->setCanKill(false); this->setImmovable(false); this->setNoInterventions(false); this->setDirty(true); this->setFullscreen(false); this->setIsFloating(false); this->setParentNodeID(0); this->setChildNodeAID(0); this->setChildNodeBID(0); this->setName(""); }
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

        const auto REVERSESPLITRATIO = 2.f - m_fSplitRatio;

        g_pWindowManager->getWindowFromDrawable(m_iChildNodeAID)->setPosition(m_vecPosition);
        g_pWindowManager->getWindowFromDrawable(m_iChildNodeBID)->setPosition(m_vecPosition + (HORIZONTAL ? Vector2D(m_vecSize.x / 2.f * m_fSplitRatio, 0) : Vector2D(0, m_vecSize.y / 2.f * m_fSplitRatio)));

        g_pWindowManager->getWindowFromDrawable(m_iChildNodeAID)->setSize(Vector2D(m_vecSize.x / (HORIZONTAL ? 2 / m_fSplitRatio : 1), m_vecSize.y / (HORIZONTAL ? 1 : 2 / m_fSplitRatio)));
        g_pWindowManager->getWindowFromDrawable(m_iChildNodeBID)->setSize(Vector2D(m_vecSize.x / (HORIZONTAL ? 2 / REVERSESPLITRATIO : 1), m_vecSize.y / (HORIZONTAL ? 1 : 2 / REVERSESPLITRATIO)));

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

void CWindow::bringTopRecursiveTransients() {
    // check if its enabled
    if (ConfigManager::getInt("intelligent_transients") != 1)
        return;

    // if this is a floating window, top
    if (m_bIsFloating && m_iDrawable > 0)
        g_pWindowManager->setAWindowTop(m_iDrawable);

    // first top all the children if floating
    for (auto& c : m_vecChildren) {
        if (const auto PWINDOW = g_pWindowManager->getWindowFromDrawable(c); PWINDOW) {
            if (PWINDOW->getIsFloating())
                g_pWindowManager->setAWindowTop(c);
        }
    }

    // THEN top their children
    for (auto& c : m_vecChildren) {
        if (const auto PCHILD = g_pWindowManager->getWindowFromDrawable(c); PCHILD) {
            // recurse
            PCHILD->bringTopRecursiveTransients();
        }
    }
}

void CWindow::addTransientChild(xcb_window_t w) {
    m_vecChildren.push_back(w);
}