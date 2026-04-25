#include "shared.h"


UiPalette UI = { UI_BG, UI_FG, UI_ICON, UI_TEXT, UI_ACCENT, UI_LINE, UI_LABLE, UI_WARN, UI_OK };

void applyThemeToPalette(Theme t) {
  (void)t;
  UI.bg = BG_Dark;
  UI.fg = FG_Dark;
  UI.icon = MATRIX_GREEN;
  UI.text = TERM_TEXT;
  UI.line = TERM_LINE;
  UI.accent = MATRIX_GREEN;
  UI.lable = TERM_LABEL;
  UI.warn = TERM_WARN;
  UI.ok = MATRIX_GREEN;
}
