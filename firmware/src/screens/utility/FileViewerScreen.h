#pragma once

#include "ui/templates/BaseScreen.h"
#include "ui/views/TextScrollView.h"

class FileViewerScreen : public BaseScreen
{
public:
  FileViewerScreen(const String& path) : _path(path) {}

  const char* title() override { return _titleBuf; }

  void onInit() override;
  void onUpdate() override;
  void onRender() override;

private:
  String _path;
  char _titleBuf[32] = "Viewer";

  String _content;
  const char** _lines = nullptr;
  uint16_t _lineCount = 0;
  uint16_t _scrollOffset = 0;
  uint16_t _visibleLines = 0;
  bool     _wrap = false;        // word-wrap mode (enabled for .txt)

#ifdef USE_LVGL_SHELL
  // LVGL mode: the whole line-split/wrap/scroll machinery is replaced by a
  // single word-wrapping label in a scrollable container.
  TextScrollView _view;
  lv_obj_t* _lblEmpty = nullptr;  // "(empty)" placeholder
#else
  void _parseLines();            // split on '\n' only (raw)
  void _parseLinesWrapped();     // word-wrap to body width (zero-copy, in-place)
#endif

  void _renderContent();
  void _goBack();
};
