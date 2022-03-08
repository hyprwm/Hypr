#include "AnimationUtil.hpp"
#include "../windowManager.hpp"

void AnimationUtil::move() {

    static std::chrono::time_point lastFrame = std::chrono::high_resolution_clock::now();
    const double DELTA = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastFrame).count();
    lastFrame = std::chrono::high_resolution_clock::now();

    const double ANIMATIONSPEED = std::max(1.f / ((double)ConfigManager::getFloat("animations:speed") * DELTA) * 462.f, (double)1.f);
    
    bool updateRequired = false;
    // Now we are (or should be, lul) thread-safe.
    for (auto& window : g_pWindowManager->windows) {
        // check if window needs an animation.
        window.setIsAnimated(false);

        if (!window.getConstructed())
            continue;

        // Border animations
        if (window.getDrawable() > 0) {
            if (window.getEffectiveBorderColor().getAsUint32() != window.getRealBorderColor().getAsUint32() /* As uint32 to round and not spam */) {
                // interp border color if enabled
                const auto PREVCOLOR = window.getRealBorderColor().getAsUint32();

                if (ConfigManager::getInt("animations:borders") == 1) {
                    window.setRealBorderColor(parabolicColor(window.getRealBorderColor(), window.getEffectiveBorderColor(), ANIMATIONSPEED));
                } else {
                    window.setRealBorderColor(window.getEffectiveBorderColor());
                }

                if (COLORDELTAOVERX(PREVCOLOR, window.getRealBorderColor().getAsUint32(), 2)) {
                    updateRequired = true;
                    window.setDirty(true);
                }
            }
        }

        if (ConfigManager::getInt("animations:enabled") == 0 || window.getIsFloating()) {
            // Disabled animations. instant warps.

            if (VECTORDELTANONZERO(window.getRealPosition(), window.getEffectivePosition())
                || VECTORDELTANONZERO(window.getRealSize(), window.getEffectiveSize())) {
                    window.setDirty(true);
                    updateRequired = true;

                    window.setRealPosition(window.getEffectivePosition());
                    window.setRealSize(window.getEffectiveSize());
                }

            continue;
        }

        if (VECTORDELTANONZERO(window.getRealPosition(), window.getEffectivePosition())) {
            Debug::log(LOG, "Updating position animations for " + std::to_string(window.getDrawable()) + " delta: " + std::to_string(ANIMATIONSPEED));
            window.setIsAnimated(true);

            // we need to update it.
            window.setDirty(true);
            updateRequired = true;

            const auto EFFPOS = window.getEffectivePosition();
            const auto REALPOS = window.getRealPosition();

            window.setRealPosition(Vector2D(parabolic(REALPOS.x, EFFPOS.x, ANIMATIONSPEED), parabolic(REALPOS.y, EFFPOS.y, ANIMATIONSPEED)));
        }

        if (VECTORDELTANONZERO(window.getRealSize(), window.getEffectiveSize())) {
            Debug::log(LOG, "Updating size animations for " + std::to_string(window.getDrawable()) + " delta: " + std::to_string(ANIMATIONSPEED));
            window.setIsAnimated(true);

            // we need to update it.
            window.setDirty(true);
            updateRequired = true;

            const auto REALSIZ = window.getRealSize();
            const auto EFFSIZ = window.getEffectiveSize();

            window.setRealSize(Vector2D(parabolic(REALSIZ.x, EFFSIZ.x, ANIMATIONSPEED), parabolic(REALSIZ.y, EFFSIZ.y, ANIMATIONSPEED)));
        }

        // set not animated if already done here
        if (!VECTORDELTANONZERO(window.getRealPosition(), window.getEffectivePosition()) 
            && !VECTORDELTANONZERO(window.getRealSize(), window.getEffectiveSize())) {
                window.setIsAnimated(false);

                window.setRealSize(window.getEffectiveSize());
                window.setRealPosition(window.getEffectivePosition());
        }
    }

    for (auto& work : g_pWindowManager->workspaces) {
        work.setAnimationInProgress(false);
        if (VECTORDELTANONZERO(work.getCurrentOffset(), work.getGoalOffset())) {
            work.setAnimationInProgress(true);
            work.setCurrentOffset(Vector2D(parabolic(work.getCurrentOffset().x, work.getGoalOffset().x, ANIMATIONSPEED), parabolic(work.getCurrentOffset().y, work.getGoalOffset().y, ANIMATIONSPEED)));
            
            updateRequired = true;
            g_pWindowManager->setAllWorkspaceWindowsDirtyByID(work.getID());

            if (ConfigManager::getInt("animations:workspaces") == 0)
                work.setAnimationInProgress(false);
        }

        if (!work.getAnimationInProgress()) {
            if (!g_pWindowManager->isWorkspaceVisible(work.getID())) {
                if (work.getCurrentOffset().x != 1500000) {
                    g_pWindowManager->setAllWorkspaceWindowsDirtyByID(work.getID());
                    updateRequired = true;
                }

                work.setCurrentOffset(Vector2D(1500000, 1500000));
                work.setGoalOffset(Vector2D(1500000, 1500000));
            } else {
                if (work.getCurrentOffset().x != 0) {
                    g_pWindowManager->setAllWorkspaceWindowsDirtyByID(work.getID());
                    updateRequired = true;
                }

                work.setCurrentOffset(Vector2D(0, 0));
                work.setGoalOffset(Vector2D(0, 0));
            }
        }
    }

    if (updateRequired)
        emptyEvent();  // send a fake request to update dirty windows
}