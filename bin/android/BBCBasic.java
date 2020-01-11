package com.rtrussell.bbcbasic;

import org.libsdl.app.SDLActivity; 

public class BBCBasic extends SDLActivity
{
    protected String[] getLibraries() {
        return new String[] {
            "hidapi",
            "SDL2",
            // "SDL2_image",
            // "SDL2_mixer",
            "SDL2_net",
            "SDL2_ttf",
            "main"
        };
    }
}
