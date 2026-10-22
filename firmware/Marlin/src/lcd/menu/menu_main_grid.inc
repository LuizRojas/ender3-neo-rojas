/**
 * Menu principal em grade de icones (estilo smartwatch) para displays 128x64.
 *
 * 8 bolhas em 2 linhas de 4. O encoder anda entre as bolhas, o clique abre.
 * A bolha selecionada fica preenchida e o nome aparece na linha de baixo.
 * "Mais opcoes" abre o menu em lista original do Marlin, com todos os itens.
 *
 * Incluido no fim de menu_main.cpp.
 */

#if ENABLED(MAIN_MENU_GRID)

#if !HAS_MARLINUI_U8GLIB
  #error "MAIN_MENU_GRID requer um display grafico 128x64 (U8glib)."
#endif

#include "../dogm/marlinui_DOGM.h"
#include "../dogm/grid_icons.h"
#include "../../module/planner.h"

#if HAS_LEVELING
  #include "../../feature/bedlevel/bedlevel.h"
#endif

void menu_main_list();

#if ENABLED(LCD_BED_LEVELING)
  void menu_bed_leveling();
#endif
#if ENABLED(ASSISTED_TRAMMING_WIZARD)
  void goto_tramming_wizard();
#endif
#if ENABLED(LCD_BED_TRAMMING)
  void _lcd_level_bed_corners();
#endif

//
// Nivelamento simplificado: tudo o que se usa no dia a dia num lugar so
//
#if HAS_LEVELING

  void _lcd_toggle_bed_leveling();

  void menu_leveling_simple() {
    START_MENU();
    BACK_ITEM(MSG_MAIN);

    #if HAS_BED_PROBE
      // Um clique: home, malha com o sensor, liga o nivelamento e salva
      if (!printingIsActive())
        ACTION_ITEM(MSG_LEVEL_FULL_AUTO, []{
          queue.inject(F("G28\nG29\nM420 S1\nM500"));
          LCD_MESSAGE(MSG_LEVEL_FULL_AUTO_RUN);
          ui.return_to_status();
        });
    #endif

    #if ENABLED(PROBE_OFFSET_WIZARD)
      if (!printingIsActive()) SUBMENU(MSG_PROBE_WIZARD, goto_probe_offset_wizard);
    #endif

    #if ENABLED(BABYSTEP_ZPROBE_OFFSET)
      SUBMENU(MSG_GRID_FINE_Z, lcd_babystep_zoffset);
    #endif

    #if ENABLED(ASSISTED_TRAMMING_WIZARD)
      if (!printingIsActive()) SUBMENU(MSG_TRAMMING_WIZARD, goto_tramming_wizard);
    #endif

    #if ENABLED(LCD_BED_TRAMMING)
      if (!printingIsActive()) SUBMENU(MSG_BED_TRAMMING, _lcd_level_bed_corners);
    #endif

    if (leveling_is_valid()) {
      bool show_state = planner.leveling_active;
      EDIT_ITEM(bool, MSG_BED_LEVELING, &show_state, _lcd_toggle_bed_leveling);
    }

    #if ENABLED(LCD_BED_LEVELING)
      if (!printingIsActive()) SUBMENU(MSG_LEVEL_ADVANCED, menu_bed_leveling);
    #endif

    #if ENABLED(EEPROM_SETTINGS)
      ACTION_ITEM(MSG_STORE_EEPROM, ui.store_settings);
    #endif

    END_MENU();
  }

#endif // HAS_LEVELING

//
// Itens da grade
//
typedef void (*grid_action_t)();
struct GridItem { const unsigned char *icon; FSTR_P label; grid_action_t action; };

static void grid_open(const screenFunc_t screen) { ui.push_current_screen(); ui.goto_screen(screen); }

static void grid_back() { ui.go_back(); }
static void grid_more() { grid_open(menu_main_list); }
static void grid_temperature() { grid_open(menu_temperature); }
static void grid_motion() { grid_open(menu_motion); }
static void grid_configuration() { grid_open(menu_configuration); }
static void grid_tune() { grid_open(menu_tune); }

#if HAS_LEVELING
  static void grid_leveling() { grid_open(menu_leveling_simple); }
#endif

#if ENABLED(ADVANCED_PAUSE_FEATURE)
  static void grid_filament() { grid_open(menu_change_filament); }
#endif

#if ENABLED(SDSUPPORT)
  static void grid_media() {
    if (!card.isMounted()) card.mount();
    if (card.isMounted()) {
      grid_open(MEDIA_MENU_GATEWAY);
    }
    else {
      TERN_(HAS_SOUND, ui.completion_feedback(false));
      LCD_MESSAGE(MSG_NO_MEDIA);
      ui.return_to_status();
    }
  }
#endif

#if MACHINE_CAN_PAUSE
  static void grid_pause() { ui.pause_print(); }
  static void grid_resume() { ui.resume_print(); }
#endif

#if MACHINE_CAN_STOP
  static void grid_stop() {
    grid_open([]{
      MenuItem_confirm::select_screen(
        GET_TEXT_F(MSG_BUTTON_STOP), GET_TEXT_F(MSG_BACK),
        ui.abort_print, nullptr,
        GET_TEXT_F(MSG_STOP_PRINT), (const char *)nullptr, F("?")
      );
    });
  }
#endif

#if ENABLED(BABYSTEPPING)
  static void grid_babystep() {
    grid_open(TERN(BABYSTEP_ZPROBE_OFFSET, lcd_babystep_zoffset, lcd_babystep_z));
  }
#endif

#define GRID_COLS     4
#define GRID_RADIUS   12
#define GRID_CELL_W   (LCD_PIXEL_WIDTH / GRID_COLS)
#define GRID_CELL_H   26
#define GRID_LABEL_Y  (LCD_PIXEL_HEIGHT - 1)

static void draw_grid_item(const GridItem &it, const uint8_t i, const bool sel) {
  const u8g_uint_t cx = GRID_CELL_W * (i % GRID_COLS) + GRID_CELL_W / 2,
                   cy = GRID_CELL_H * (i / GRID_COLS) + GRID_RADIUS;
  if (!PAGE_CONTAINS(cy - GRID_RADIUS, cy + GRID_RADIUS)) return;
  u8g.setColorIndex(1);
  if (sel) {
    u8g.drawDisc(cx, cy, GRID_RADIUS);
    u8g.setColorIndex(0);                       // icone "vazado" na bolha cheia
  }
  else
    u8g.drawCircle(cx, cy, GRID_RADIUS);
  u8g.drawBitmapP(cx - 8, cy - 8, 2, 16, it.icon);
  u8g.setColorIndex(1);
}

static void grid_screen(const GridItem *items, const uint8_t count) {
  ui.encoder_direction_menus();
  ENCODER_RATE_MULTIPLY(false);
  if (int32_t(ui.encoderPosition) < 0) ui.encoderPosition = 0;
  uint8_t sel = ui.encoderPosition / (ENCODER_STEPS_PER_MENU_ITEM);
  if (sel >= count) {
    sel = count - 1;
    ui.encoderPosition = sel * (ENCODER_STEPS_PER_MENU_ITEM);
  }
  encoderLine = sel;          // guardado no historico ao abrir um submenu
  screen_items = count;
  if (ui.first_page) ui.screen_changed = false;

  if (ui.use_click()) {
    items[sel].action();
    return;
  }

  if (!ui.should_draw()) return;

  for (uint8_t i = 0; i < count; i++) draw_grid_item(items[i], i, i == sel);

  if (PAGE_CONTAINS(GRID_LABEL_Y - (MENU_FONT_ASCENT), GRID_LABEL_Y)) {
    ui.set_font(FONT_MENU);
    const int w = utf8_strlen(items[sel].label) * (MENU_FONT_WIDTH);
    lcd_put_u8str(_MAX(0, (LCD_PIXEL_WIDTH - w) / 2), GRID_LABEL_Y, items[sel].label);
  }
}

#define GRID_ITEM(ICON, MSG, FN) items[n++] = { grid_icon_##ICON, GET_TEXT_F(MSG), FN }

void menu_main() {
  GridItem items[8];
  uint8_t n = 0;

  const bool busy = printingIsActive(),
             paused = TERN0(MACHINE_CAN_PAUSE, printingIsPaused());

  GRID_ITEM(back, MSG_BACK, grid_back);

  if (busy) {
    // Durante a impressao
    GRID_ITEM(tune, MSG_TUNE, grid_tune);
    #if MACHINE_CAN_PAUSE
      GRID_ITEM(pause, MSG_PAUSE_PRINT, grid_pause);
    #endif
    #if MACHINE_CAN_STOP
      GRID_ITEM(stop, MSG_STOP_PRINT, grid_stop);
    #endif
    #if ENABLED(BABYSTEPPING)
      GRID_ITEM(zoffset, MSG_GRID_FINE_Z, grid_babystep);
    #endif
    #if ENABLED(ADVANCED_PAUSE_FEATURE)
      GRID_ITEM(filament, MSG_FILAMENTCHANGE, grid_filament);
    #endif
    GRID_ITEM(heat, MSG_TEMPERATURE, grid_temperature);
  }
  else if (paused) {
    // Impressao pausada
    #if MACHINE_CAN_PAUSE
      GRID_ITEM(resume, MSG_RESUME_PRINT, grid_resume);
    #endif
    #if MACHINE_CAN_STOP
      GRID_ITEM(stop, MSG_STOP_PRINT, grid_stop);
    #endif
    #if ENABLED(ADVANCED_PAUSE_FEATURE)
      GRID_ITEM(filament, MSG_FILAMENTCHANGE, grid_filament);
    #endif
    GRID_ITEM(heat, MSG_TEMPERATURE, grid_temperature);
    GRID_ITEM(move, MSG_MOTION, grid_motion);
    GRID_ITEM(settings, MSG_CONFIGURATION, grid_configuration);
  }
  else {
    // Parada
    #if ENABLED(SDSUPPORT)
      GRID_ITEM(print, MSG_GRID_PRINT, grid_media);
    #endif
    GRID_ITEM(heat, MSG_TEMPERATURE, grid_temperature);
    GRID_ITEM(move, MSG_MOTION, grid_motion);
    #if HAS_LEVELING
      GRID_ITEM(level, MSG_GRID_LEVELING, grid_leveling);
    #endif
    #if ENABLED(ADVANCED_PAUSE_FEATURE)
      GRID_ITEM(filament, MSG_FILAMENTCHANGE, grid_filament);
    #endif
    GRID_ITEM(settings, MSG_CONFIGURATION, grid_configuration);
  }

  if (n < COUNT(items)) GRID_ITEM(more, MSG_GRID_MORE, grid_more);

  grid_screen(items, n);
}

#endif // MAIN_MENU_GRID
