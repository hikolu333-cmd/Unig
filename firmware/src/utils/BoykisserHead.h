//
// BoykisserHead — the "boykisser" (Mauzymice silly cat) photo mascot.
// Source art: the user's own picture (Photoroom cut-out), downsampled to
// 75x105 RGB565 + 1-bit alpha. Authored for ps=3 (one art pixel == one cell at
// ps/3 on any board); the draw fn scales it to the board's `ps` via runs of
// same-colour fillRect, skipping transparent pixels so the Matrix rain shows
// through around the frame.
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

// Pupil blobs measured on the art (unused for now — the photo mascot stays
// static; blink lids over a photo read as glitch, see v1.9.0 feedback).

// Pixel data lives in BoykisserHead.cpp (one flash copy shared by every TU).
extern const uint16_t BK_PIX[BK_PH * BK_PW];
extern const uint8_t  BK_MASK[(BK_PH * BK_PW + 7) / 8];

inline bool _bkOpaque(int sx, int sy) {
  int i = sy * BK_PW + sx;
  return BK_MASK[i >> 3] & (0x80 >> (i & 7));
}

// Photo mascot: no blink overlay (the art carries its own eyes) — it just
// sways with the head band like the pixel-art mascots.
template<typename T>
void boykisserDrawHead(T& dc, int ox, int oy, int ps, bool blink) {
  (void)blink;
  const int s = (ps >= 3) ? ps / 3 : 1;   // art authored at ps=3
  for (int y = 0; y < BK_PH * s; y++) {
    const int sy = y / s;
    for (int x = 0; x < BK_PW * s; ) {
      const int sx = x / s;
      if (!_bkOpaque(sx, sy)) { x++; continue; }
      const uint16_t c = BK_PIX[sy * BK_PW + sx];
      // extend the same-colour opaque run (start pixel always counts)
      int x2 = x + 1;
      while (x2 < BK_PW * s) {
        const int sx2 = x2 / s;
        if (!_bkOpaque(sx2, sy) || BK_PIX[sy * BK_PW + sx2] != c) break;
        x2++;
      }
      dc.fillRect(ox + x, oy + y, x2 - x, s, c);
      x = x2;
    }
  }
}
