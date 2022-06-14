#include "pico_graphics.hpp"

namespace pimoroni {
    PicoGraphics_PenP4::PicoGraphics_PenP4(uint16_t width, uint16_t height, void *frame_buffer)
    : PicoGraphics(width, height, frame_buffer) {
        this->pen_type = PEN_P4;
        if(this->frame_buffer == nullptr) {
            this->frame_buffer = (void *)(new uint8_t[buffer_size(width, height)]);
        }
        for(auto i = 0u; i < palette_size; i++) {
            palette[i] = {
                uint8_t(i << 4),
                uint8_t(i << 4),
                uint8_t(i << 4)
            };
            used[i] = false;
        }
    }
    void PicoGraphics_PenP4::set_pen(uint c) {
        color = c & 0xf;
     }
    void PicoGraphics_PenP4::set_pen(uint8_t r, uint8_t g, uint8_t b) {
        int pen = RGB(r, g, b).closest(palette, palette_size);
        if(pen != -1) color = pen;
    }
    int PicoGraphics_PenP4::update_pen(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
        i &= 0xf;
        palette[i] = {r, g, b};
        return i;
    }
    int PicoGraphics_PenP4::create_pen(uint8_t r, uint8_t g, uint8_t b) {
        // Create a colour and place it in the palette if there's space
        for(auto i = 0u; i < palette_size; i++) {
            if(!used[i]) {
                palette[i] = {r, g, b};
                used[i] = true;
                return i;
            }
        }
        return -1;
    }
    int PicoGraphics_PenP4::reset_pen(uint8_t i) {
        palette[i] = {0, 0, 0};
        used[i] = false;
        return i;
    }
    void PicoGraphics_PenP4::set_pixel(const Point &p) {
        // pointer to byte in framebuffer that contains this pixel
        uint8_t *buf = (uint8_t *)frame_buffer;
        uint8_t *f = &buf[(p.x / 2) + (p.y * bounds.w / 2)];

        uint8_t  o = (~p.x & 0b1) * 4; // bit offset within byte
        uint8_t  m = ~(0b1111 << o);   // bit mask for byte
        uint8_t  b = color << o;       // bit value shifted to position

        *f &= m; // clear bits
        *f |= b; // set value
    }
    
    void PicoGraphics_PenP4::get_dither_candidates(const RGB &col, const RGB *palette, size_t len, std::array<uint8_t, 16> &candidates) {
        RGB error;
        for(size_t i = 0; i < candidates.size(); i++) {
            candidates[i] = (col + error).closest(palette, len);
            error += (col - palette[candidates[i]]);
        }

        // sort by a rough approximation of luminance, this ensures that neighbouring
        // pixels in the dither matrix are at extreme opposites of luminence
        // giving a more balanced output
        std::sort(candidates.begin(), candidates.end(), [palette](int a, int b) {
            return palette[a].luminance() > palette[b].luminance();
        });
    }

    void PicoGraphics_PenP4::set_pixel_dither(const Point &p, const RGB &c) {
        if(!bounds.contains(p)) return;

        if(!cache_built) {
            for(uint i = 0; i < 512; i++) {
                RGB cache_col((i & 0x1C0) >> 1, (i & 0x38) << 2, (i & 0x7) << 5);
                get_dither_candidates(cache_col, palette, palette_size, candidate_cache[i]);
            }
            cache_built = true;
        }

        uint cache_key = ((c.r & 0xE0) << 1) | ((c.g & 0xE0) >> 2) | ((c.b & 0xE0) >> 5);
        //get_dither_candidates(c, palette, 256, candidates);

        // find the pattern coordinate offset
        uint pattern_index = (p.x & 0b11) | ((p.y & 0b11) << 2);

        // set the pixel
        //color = candidates[pattern[pattern_index]];
        color = candidate_cache[cache_key][pattern[pattern_index]];
        set_pixel(p);
    }
    void PicoGraphics_PenP4::scanline_convert(PenType type, conversion_callback_func callback) {
        if(type == PEN_RGB565) {
            // Cache the RGB888 palette as RGB565
            RGB565 cache[palette_size];
            for(auto i = 0u; i < palette_size; i++) {
                cache[i] = palette[i].to_rgb565();
            }

            // Treat our void* frame_buffer as uint8_t
            uint8_t *src = (uint8_t *)frame_buffer;

            // Allocate a per-row temporary buffer
            uint16_t row_buf[bounds.w];
            for(auto y = 0; y < bounds.h; y++) {
                /*if(scanline_interrupt != nullptr) {
                    scanline_interrupt(y);
                    // Cache the RGB888 palette as RGB565
                    for(auto i = 0u; i < 16; i++) {
                    cache[i] = palette[i].to_rgb565();
                    }
                }*/

                for(auto x = 0; x < bounds.w; x++) {
                    uint8_t c = src[(bounds.w * y / 2) + (x / 2)];
                    uint8_t  o = (~x & 0b1) * 4; // bit offset within byte
                    uint8_t  b = (c >> o) & 0xf; // bit value shifted to position
                    row_buf[x] = cache[b];
                }
                // Callback to the driver with the row data
                callback(row_buf, bounds.w * sizeof(RGB565));
            }
        }
    }
}