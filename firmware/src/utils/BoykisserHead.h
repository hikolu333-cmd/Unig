//
// BoykisserHead — the "boykisser" (Mauzymice silly cat) photo mascot.
// TWO art phases from the user's own pictures: eyes open and eyes closed, so
// the mascot blinks with real frames (drawn-on lids read as glitch on a
// photo). 75x105 RGB565 + 1-bit alpha (shared mask), authored for ps=3; the
// draw fn scales to the board's `ps` via same-colour fillRect runs, skipping
// transparent pixels so the Matrix rain shows around the frame.
//
// Registered in utils/Mascot.h (see "To ADD a mascot" there).
//

#pragma once
#include <Arduino.h>

// Registry size is in *cells* (the head band scales w*ps), so cells are
// art-pixels/3 to match the ps=3 authoring resolution.
static constexpr int BK_W  = 25;   // cells
static constexpr int BK_H  = 35;   // cells
static constexpr int BK_PW = 75;        // art pixel width
static constexpr int BK_PH = 105;        // art pixel height

// Pixel data lives in BoykisserHead.cpp (one flash copy shared by every TU).
extern const uint16_t BK_PIX_OPEN[BK_PH * BK_PW];    // eyes open
extern const uint16_t BK_PIX_CLOSED[BK_PH * BK_PW];   // eyes closed (blink frame)
extern const uint8_t  BK_MASK[(BK_PH * BK_PW + 7) / 8];

inline bool _bkOpaque(int sx, int sy) {
  int i = sy * BK_PW + sx;
  return BK_MASK[i >> 3] & (0x80 >> (i & 7));
}

// `blink` swaps in the closed-eyes art phase; both frames come from the
// user's pictures and share one silhouette, so the swap is seamless.
template<typename T>
void boykisserDrawHead(T& dc, int ox, int oy, int ps, bool blink) {
  const uint16_t* art = blink ? BK_PIX_CLOSED : BK_PIX_OPEN;
  const int s = (ps >= 3) ? ps / 3 : 1;   // art authored at ps=3
  for (int y = 0; y < BK_PH * s; y++) {
    const int sy = y / s;
    for (int x = 0; x < BK_PW * s; ) {
      const int sx = x / s;
      if (!_bkOpaque(sx, sy)) { x++; continue; }
      const uint16_t c = art[sy * BK_PW + sx];
      // extend the same-colour opaque run (start pixel always counts)
      int x2 = x + 1;
      while (x2 < BK_PW * s) {
        const int sx2 = x2 / s;
        if (!_bkOpaque(sx2, sy) || art[sy * BK_PW + sx2] != c) break;
        x2++;
      }
      dc.fillRect(ox + x, oy + y, x2 - x, s, c);
      x = x2;
    }
  }
}
