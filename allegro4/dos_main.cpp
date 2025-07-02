#include <allegro.h>
#include <cstdio>

int main() {
    allegro_init();
    install_keyboard();
    
    set_color_depth(8);
    set_gfx_mode(GFX_AUTODETECT, 320, 200, 0, 0);
    
    clear_to_color(screen, makecol(0, 0, 255));
    textout_centre_ex(screen, font, "SMB DOS with Allegro 4!", SCREEN_W/2, SCREEN_H/2, makecol(255, 255, 255), -1);
    
    readkey();
    
    allegro_exit();
    return 0;
}
END_OF_MAIN()
