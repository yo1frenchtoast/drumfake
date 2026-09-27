package org.drumfreake.app;

import org.libsdl.app.SDLActivity;

// Loads libSDL3.so and libmain.so (the Drumf(re)ake app) and runs SDL_main().
public class DrumfreakeActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }
}
