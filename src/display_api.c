#include "ssd1306.h"


#include "display_api.h"


display_text(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s) {
    ssd1306_draw_string(p, x, y, scale, s);
}
    
