#include "AnimationUtil.hpp"
#include "../windowManager.hpp"

void AnimationUtil::move() {

    static std::chrono::time_point lastFrame = std::chrono::high_resolution_clock::now();
    const double DELTA = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastFrame).count();

    // wait for the main thread to be idle
    while (g_pWindowManager->mainThreadBusy) {
        ;
    }

    // set state to let the main thread know to wait.
    g_pWindowManager->animationUtilBusy = true;

    const double ANIMATIONSPEED = ((double)1 / (double)ConfigManager::getFloat("anim.speed")) * DELTA;

    
    bool updateRequired = false;
    // Now we are (or should be, lul) thread-safe.
    for (auto& window : g_pWindowManager->windows) {
        // check if window needs an animation.

        if (ConfigManager::getInt("anim.enabled") == 0) {
            // Disabled animations. instant warps.
            window.setRealPosition(window.getEffectivePosition());
            window.setRealSize(window.getEffectiveSize());

            if (VECTORDELTANONZERO(window.getRealPosition(), window.getEffectivePosition())
                || VECTORDELTANONZERO(window.getRealSize(), window.getEffectiveSize())) {
                    window.setDirty(true);
                    updateRequired = true;
                }

            continue;
        }

        if (VECTORDELTANONZERO(window.getRealPosition(), window.getEffectivePosition())) {
            Debug::log(LOG, "Updating position animations for " + std::to_string(window.getDrawable()) + " delta: " + std::to_string(ANIMATIONSPEED));

            // we need to update it.
            window.setDirty(true);
            updateRequired = true;

            const auto EFFPOS = window.getEffectivePosition();
            const auto REALPOS = window.getRealPosition();

            window.setRealPosition(Vector2D(parabolic(REALPOS.x, EFFPOS.x, ANIMATIONSPEED), parabolic(REALPOS.y, EFFPOS.y, ANIMATIONSPEED)));
        }

        if (VECTORDELTANONZERO(window.getRealSize(), window.getEffectiveSize())) {
            Debug::log(LOG, "Updating size animations for " + std::to_string(window.getDrawable()) + " delta: " + std::to_string(ANIMATIONSPEED));

            // we need to update it.
            window.setDirty(true);
            updateRequired = true;

            const auto REALSIZ = window.getRealSize();
            const auto EFFSIZ = window.getEffectiveSize();

            window.setRealSize(Vector2D(parabolic(REALSIZ.x, EFFSIZ.x, ANIMATIONSPEED), parabolic(REALSIZ.y, EFFSIZ.y, ANIMATIONSPEED)));
        }
    }

    if (updateRequired)
        emptyEvent();  // send a fake request to update dirty windows

    // restore anim state
    g_pWindowManager->animationUtilBusy = false;

    lastFrame = std::chrono::high_resolution_clock::now();
}