package com.rtrussell.bbcbasic;

import org.libsdl.app.SDLActivity; 

public class BBCBasic extends SDLActivity
{
    protected String[] getLibraries() {
        return new String[] {
            "c++_shared",
            "Box2D",
            // "hidapi",
            "SDL2",
            // "SDL2_image",
            // "SDL2_mixer",
            "SDL2_net",
            "SDL2_ttf",
            "main"
        };
    }

    /**
     * Override so that BBC2APK can set orientation in manifest
     */
    public void setOrientationBis(int w, int h, boolean resizable, String hint)
    {
    }
}
