#include "commute.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace esphome::commute {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// ---------------------------------------------------------------------------------------------------------------
// What the integration says
// ---------------------------------------------------------------------------------------------------------------

// The ways of travelling, in the order of the integration's MODES: the trail names them by this index.
enum Mode : uint8_t { WALKING, CYCLING, TRAIN, TRANSIT, CAR, UNKNOWN, MODES };
enum Phase : uint8_t { P_NONE, P_HOME, P_MAYBE, P_TO_WORK, P_WORK, P_TO_HOME, P_AWAY };

static const char *const MODE_NAMES[MODES] = {"walking", "cycling", "train", "transit", "car", "unknown"};
static const char *const MODE_KEYS[MODES] = {"mode_walking", "mode_cycling", "mode_train", "mode_transit", "mode_car",
                                             "mode_unknown"};
// Tessera's icon set has no walking figure, metro or tram: a runner and a bus stand in.
static const uint32_t MODE_ICONS[MODES] = {0xF070E /* run */, 0xF00A3 /* bike */, 0xF052C /* train */,
                                           0xF00E7 /* bus */, 0xF010B /* car */, 0xF1136 /* crosshairs-question */};
static const uint32_t ICON_HOME = 0xF02DC, ICON_WORK = 0xF00D6 /* briefcase */, ICON_MARKER = 0xF05F8;

static theme::Role mode_role(uint8_t m) {
  switch (m) {
    case WALKING: return theme::MARK_GREEN;
    case CYCLING: return theme::MARK_AMBER;
    case TRAIN: return theme::MARK_BLUE;
    case TRANSIT: return theme::MARK_PURPLE;
    case CAR: return theme::SLATE;
    default: return theme::SUBTLE;
  }
}

static uint8_t mode_of(const char *s) {
  if (s == nullptr) return UNKNOWN;
  for (uint8_t i = 0; i < MODES; ++i)
    if (strcmp(s, MODE_NAMES[i]) == 0) return i;
  return UNKNOWN;
}

static uint8_t phase_of(const char *s) {
  if (s == nullptr) return P_NONE;
  static const char *const names[] = {"", "home", "maybe", "to_work", "work", "to_home", "away"};
  for (uint8_t i = 1; i < 7; ++i)
    if (strcmp(s, names[i]) == 0) return i;
  return P_NONE;
}

static bool commuting(uint8_t phase) { return phase == P_TO_WORK || phase == P_TO_HOME; }

enum Why : uint8_t { W_SPEED = 1, W_ACTIVITY = 2, W_NS_TRACKER = 4, W_NS_TRIP = 8, W_KEPT = 16, W_VEHICLE = 32 };

struct Leg {
  uint8_t mode = UNKNOWN;
  uint8_t from = 0;    // percent of the way where it began
  uint32_t at = 0;     // when it began
};

// One person's commute, out of the attributes of their commute sensor.
struct Person {
  static constexpr int LEGS = 8, TRAIL = 40;
  bool valid = false;  // an entity with a phase
  char name[32] = "";
  uint8_t phase = P_NONE, mode = UNKNOWN, why = 0;
  int progress = -1, speed = -1, delay = 0;
  float ns_km = -1;
  uint32_t eta = 0, place_at = 0;
  char activity[24] = "", train[24] = "";
  Leg legs[LEGS];
  uint8_t nlegs = 0;
  int32_t te[TRAIL] = {}, tn[TRAIL] = {};
  uint8_t tm[TRAIL] = {};
  uint8_t ntrail = 0;
  int32_t work_e = 0, work_n = 0;
  bool has_work = false;

  static void copy(char *to, size_t size, const char *from) {
    snprintf(to, size, "%s", from ? from : "");
  }

  // `entity` is {"state", "name", "attributes"} as the app sends a tile its entity.
  void read(JsonObjectConst entity) {
    *this = Person();
    if (entity.isNull()) return;
    JsonObjectConst a = entity["attributes"];
    phase = phase_of(a["commute_phase"] | "");
    valid = phase != P_NONE;
    mode = mode_of(entity["state"] | "unknown");
    const char *who = a["person"] | "";
    if (!*who) who = entity["name"] | "";
    copy(name, sizeof(name), who);
    progress = a["progress"].isNull() ? -1 : std::clamp(a["progress"].as<int>(), 0, 100);
    speed = a["speed"].isNull() ? -1 : a["speed"].as<int>();
    delay = a["delay"] | 0;
    ns_km = a["ns_distance"].isNull() ? -1.0f : a["ns_distance"].as<float>();
    eta = a["eta_at"] | 0u;
    place_at = a["place_at"] | 0u;
    copy(activity, sizeof(activity), a["activity"] | "");
    copy(train, sizeof(train), a["train"] | "");
    for (JsonVariantConst w : a["basis"].as<JsonArrayConst>()) {
      const char *k = w | "";
      if (!strcmp(k, "speed")) why |= W_SPEED;
      else if (!strcmp(k, "activity")) why |= W_ACTIVITY;
      else if (!strcmp(k, "ns_tracker")) why |= W_NS_TRACKER;
      else if (!strcmp(k, "ns_trip")) why |= W_NS_TRIP;
      else if (!strcmp(k, "kept")) why |= W_KEPT;
      else if (!strcmp(k, "vehicle_default")) why |= W_VEHICLE;
    }
    JsonArrayConst modes = a["leg_modes"], from = a["leg_from"], times = a["leg_times"];
    for (size_t i = 0; i < modes.size() && nlegs < LEGS; ++i) {
      Leg &l = legs[nlegs++];
      l.mode = mode_of(modes[i] | "unknown");
      l.from = static_cast<uint8_t>(std::clamp(from[i] | 0, 0, 100));
      l.at = times[i] | 0u;
    }
    JsonArrayConst trail = a["trail"];
    for (size_t i = 0; i + 2 < trail.size() && ntrail < TRAIL; i += 3) {
      te[ntrail] = trail[i] | 0;
      tn[ntrail] = trail[i + 1] | 0;
      tm[ntrail] = static_cast<uint8_t>(std::min(trail[i + 2] | 5, 5));
      ++ntrail;
    }
    JsonArrayConst work = a["work_offset"];
    if (work.size() == 2) {
      work_e = work[0] | 0;
      work_n = work[1] | 0;
      has_work = true;
    }
  }

  bool to_home() const { return phase == P_TO_HOME; }
  int percent() const { return std::max(0, progress); }
  // The leg that ends where the next begins, the last one where the person is now.
  int leg_end(int i) const { return i + 1 < nlegs ? legs[i + 1].from : percent(); }
};

// A text with two names filled in: "{a} - {b}".
static std::string fill2(const char *text, const char *n1, const std::string &v1, const char *n2, const std::string &v2) {
  return tessera::fill(tessera::fill(text, n1, v1).c_str(), n2, v2);
}

static std::string clock(uint32_t epoch) { return epoch ? tessera::clock_text(epoch) : std::string(); }

// "Train: 104 km/h, NS train nearby": what the guess is based on, in one line.
static std::string why_line(const tessera::Plugin *p, const Person &who, bool with_mode) {
  std::string out;
  auto add = [&](const std::string &part) {
    if (part.empty()) return;
    if (!out.empty()) out += ", ";
    out += part;
  };
  if (who.why & W_NS_TRACKER) add(p->text("why_ns_tracker"));
  if ((who.why & W_SPEED) && who.speed >= 0) add(tessera::format(p->text("why_speed"), who.speed));
  if ((who.why & W_ACTIVITY) && *who.activity) add(tessera::fill(p->text("why_activity"), "a", who.activity));
  if ((who.why & W_NS_TRIP) && *who.train) add(tessera::fill(p->text("why_ns_trip"), "a", who.train));
  if (who.why & W_VEHICLE) add(p->text("why_vehicle"));
  if (who.why & W_KEPT) add(p->text("why_kept"));
  if (out.empty()) out = p->text("why_none");
  if (!with_mode) return out;
  return std::string(p->text(MODE_KEYS[who.mode])) + ": " + out;
}

// ---------------------------------------------------------------------------------------------------------------
// Parts
// ---------------------------------------------------------------------------------------------------------------

static void circle(lv_obj_t *o) { lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0); }

static void box(lv_obj_t *o, int x, int y, int w, int h) {
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, std::max(1, w), std::max(1, h));
}

static void show(lv_obj_t *o, bool on) {
  if (o == nullptr) return;
  if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// An icon in the middle of a round block.
struct Disc {
  lv_obj_t *ground{}, *glyph{};
  int size = 0;
  void make(lv_obj_t *parent, theme::Role fill, Font face) {
    ground = ui::block(parent, fill);
    circle(ground);
    glyph = ui::label(ground, face, theme::ON_ACCENT);
    lv_obj_set_style_text_align(glyph, LV_TEXT_ALIGN_CENTER, 0);
  }
  void place(int x, int y, int s) {
    size = s;
    box(ground, x, y, s, s);
    lv_obj_set_width(glyph, s);
    lv_obj_align(glyph, LV_ALIGN_CENTER, 0, 0);
  }
  void paint(uint32_t icon, theme::Role fill, theme::Role ink) {
    lv_obj_set_style_bg_color(ground, ui::color(fill), 0);
    ui::set_text(glyph, ui::icon(icon));
    ui::set_color(glyph, ink);
  }
  void visible(bool on) { show(ground, on); }
};

// The badge of a train's delay: "+3".
struct Badge {
  lv_obj_t *ground{}, *words{};
  Font face = Font::BODY;
  void make(lv_obj_t *parent, Font f) {
    face = f;
    ground = ui::block(parent, theme::AMBER_TRACK);
    words = ui::label(ground, f, theme::INK);
    lv_obj_set_style_text_align(words, LV_TEXT_ALIGN_CENTER, 0);
  }
  // Places it with its right edge at `right`, in the middle of `cy`; returns its width (0 when hidden).
  int paint(const std::string &text, int right, int cy) {
    show(ground, !text.empty());
    if (text.empty()) return 0;
    const int pad = ui::px(6), w = ui::text_width(text, face) + 2 * pad, h = ui::line_height(face) + ui::px(2);
    box(ground, right - w, cy - h / 2, w, h);
    lv_obj_set_style_bg_color(ground, ui::color(theme::AMBER_TRACK), 0);
    ui::set_color(words, theme::INK);
    lv_obj_set_width(words, w);
    lv_obj_align(words, LV_ALIGN_CENTER, 0, 0);
    ui::set_text(words, text);
    return w;
  }
};

static std::string delay_text(const tessera::Plugin *p, const Person &who) {
  if (who.mode != TRAIN || who.delay <= 0) return "";
  return tessera::format(p->text("delay"), who.delay);
}

// The trip so far: a track with a block for every leg in its colour, and a disc with the way of travelling where the
// person is now.
struct Ribbon {
  lv_obj_t *track{};
  lv_obj_t *leg[Person::LEGS]{};
  lv_obj_t *leg_icon[Person::LEGS]{};
  Disc marker;
  int x = 0, y = 0, w = 0, thick = 0, size = 0;
  bool icons = false;

  void make(lv_obj_t *parent, bool with_icons) {
    icons = with_icons;
    track = ui::block(parent, theme::TRACK);
    circle(track);
    for (int i = 0; i < Person::LEGS; ++i) {
      leg[i] = ui::block(parent, theme::MARK_BLUE);
      circle(leg[i]);
      if (icons) {
        leg_icon[i] = ui::label(leg[i], Font::ICON_SMALL, theme::ON_ACCENT);
        lv_obj_set_style_text_align(leg_icon[i], LV_TEXT_ALIGN_CENTER, 0);
      }
    }
    marker.make(parent, theme::MARK_BLUE, Font::ICON_SMALL);
  }

  // The ribbon's track from x to x + w, its middle at cy.
  void place(int x_, int cy, int w_, int thick_, int marker_) {
    x = x_, w = w_, thick = thick_, size = marker_;
    y = cy - thick / 2;
    box(track, x, y, w, thick);
    marker.place(x, cy - size / 2, size);
  }

  void paint(const Person &who) {
    const int gap = std::max(2, ui::px(3));
    const int pct = who.percent();
    for (int i = 0; i < Person::LEGS; ++i) {
      if (i >= who.nlegs) {
        show(leg[i], false);
        continue;
      }
      const int x0 = x + w * who.legs[i].from / 100, x1 = x + w * who.leg_end(i) / 100;
      const int lw = x1 - x0 - (i + 1 < who.nlegs ? gap : 0);
      show(leg[i], lw > 1);
      if (lw <= 1) continue;
      box(leg[i], x0, y, std::max(lw, thick), thick);
      lv_obj_set_style_bg_color(leg[i], ui::color(mode_role(who.legs[i].mode)), 0);
      if (icons) {
        const bool room = lw >= thick + ui::px(10) && thick >= ui::line_height(Font::ICON_SMALL) - ui::px(4);
        show(leg_icon[i], room);
        if (room) {
          ui::set_text(leg_icon[i], ui::icon(MODE_ICONS[who.legs[i].mode]));
          lv_obj_set_width(leg_icon[i], lw);
          lv_obj_align(leg_icon[i], LV_ALIGN_CENTER, 0, 0);
        }
      }
    }
    const int cx = std::clamp(x + w * pct / 100, x + size / 2, x + w - size / 2);
    lv_obj_set_pos(marker.ground, cx - size / 2, y + thick / 2 - size / 2);
    marker.paint(MODE_ICONS[who.mode], mode_role(who.mode), theme::ON_ACCENT);
    lv_obj_set_style_border_width(marker.ground, std::max(2, ui::px(4)), 0);
    lv_obj_set_style_border_color(marker.ground, ui::card_color(), 0);
    lv_obj_set_style_border_opa(marker.ground, LV_OPA_COVER, 0);
    lv_obj_move_foreground(marker.ground);
  }

  void visible(bool on) {
    show(track, on);
    for (auto *l : leg) show(l, on && l && !lv_obj_has_flag(l, LV_OBJ_FLAG_HIDDEN));
    marker.visible(on);
  }
};

// A ring for a single cell: an arc for every leg in its colour, the way of travelling in the middle.
struct Ring {
  lv_obj_t *back{};
  lv_obj_t *arc[Person::LEGS]{};
  Disc middle;
  int stroke = 0;

  static lv_obj_t *new_arc(lv_obj_t *parent) {
    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
    lv_arc_set_rotation(a, 270);  // 0 degrees at the top, clockwise
    return a;
  }

  void make(lv_obj_t *parent) {
    back = new_arc(parent);
    lv_obj_set_style_arc_rounded(back, false, LV_PART_MAIN);
    for (auto &a : arc) a = new_arc(parent);
    middle.make(parent, theme::MARK_BLUE, Font::ICON);
  }

  void place(int x, int y, int size) {
    stroke = std::max(4, size * 11 / 100);
    box(back, x, y, size, size);
    lv_obj_set_style_arc_width(back, stroke, LV_PART_MAIN);
    lv_arc_set_bg_angles(back, 0, 360);
    for (auto *a : arc) {
      box(a, x, y, size, size);
      lv_obj_set_style_arc_width(a, stroke, LV_PART_MAIN);
    }
    const int inner = size - 2 * stroke - 2 * ui::px(5);
    middle.place(x + (size - inner) / 2, y + (size - inner) / 2, inner);
  }

  void paint(const Person &who) {
    lv_obj_set_style_arc_color(back, ui::color(theme::TRACK), LV_PART_MAIN);
    for (int i = 0; i < Person::LEGS; ++i) {
      const bool on = i < who.nlegs && who.leg_end(i) > who.legs[i].from;
      show(arc[i], on);
      if (!on) continue;
      const int a0 = who.legs[i].from * 360 / 100, a1 = who.leg_end(i) * 360 / 100;
      lv_arc_set_bg_angles(arc[i], a0, std::max(a0 + 2, a1 - (i + 1 < who.nlegs ? 4 : 0)));
      lv_obj_set_style_arc_color(arc[i], ui::color(mode_role(who.legs[i].mode)), LV_PART_MAIN);
    }
    middle.paint(MODE_ICONS[who.mode], mode_role(who.mode), theme::ON_ACCENT);
  }
};

// What a tile shows while nobody is on the way: a round icon, a line, and a quieter line under it.
struct Idle {
  Disc disc;
  lv_obj_t *title{}, *sub{};
  int x = 0, w = 0;
  void make(lv_obj_t *parent) {
    disc.make(parent, theme::TRACK, Font::ICON);
    title = ui::label(parent, Font::TITLE, theme::INK);
    sub = ui::label(parent, Font::BODY, theme::MUTED);
  }
  void place(int width, int height) {
    const int s = std::min(height - ui::px(8), ui::px(56));
    const int text_h = ui::line_height(Font::TITLE) + ui::line_height(Font::BODY);
    const bool beside = width >= s * 3;
    if (beside) {
      disc.place(0, (height - s) / 2, s);
      x = s + ui::px(12);
      lv_obj_set_pos(title, x, (height - text_h) / 2);
      lv_obj_set_pos(sub, x, (height - text_h) / 2 + ui::line_height(Font::TITLE));
    } else {
      const int s2 = std::max(ui::px(24), std::min(s, height - text_h - ui::px(6)));
      disc.place(0, 0, s2);
      x = 0;
      lv_obj_set_pos(title, 0, height - text_h);
      lv_obj_set_pos(sub, 0, height - ui::line_height(Font::BODY));
    }
    w = width - x;
    lv_obj_set_width(title, w);
    lv_obj_set_width(sub, w);
  }
  void paint(uint32_t icon, bool accent, const std::string &t, const std::string &s) {
    disc.paint(icon, accent ? theme::ACCENT_TINT : theme::TRACK, accent ? theme::ACCENT_ICON : theme::MUTED);
    ui::set_text(title, t);
    ui::set_text(sub, s);
  }
  void visible(bool on) {
    disc.visible(on);
    show(title, on);
    show(sub, on);
  }
};

// The idle texts of a person who is not on the way.
static void idle_texts(const tessera::Plugin *p, const Person &who, uint32_t &icon, bool &accent, std::string &t,
                       std::string &s) {
  accent = false;
  if (!who.valid) {
    icon = ICON_MARKER;
    t = p->text("choose");
    s = "";
    return;
  }
  if (who.phase == P_MAYBE) {
    icon = MODE_ICONS[UNKNOWN];
    accent = true;
    t = p->text("maybe_title");
    const uint32_t left = who.nlegs ? who.legs[0].at : 0;
    s = left ? fill2(p->text("maybe_sub"), "name", who.name, "t", clock(left))
             : tessera::fill(p->text("maybe_sub_plain"), "name", who.name);
    return;
  }
  t = p->text("no_commute");
  if (who.phase == P_HOME) icon = ICON_HOME, s = tessera::fill(p->text("home_now"), "name", who.name);
  else if (who.phase == P_WORK) icon = ICON_WORK, s = tessera::fill(p->text("work_now"), "name", who.name);
  else icon = ICON_MARKER, s = tessera::fill(p->text("away_now"), "name", who.name);
}

// ---------------------------------------------------------------------------------------------------------------
// The tile of one person
// ---------------------------------------------------------------------------------------------------------------

class CommuteTile : public tessera::Tile {
 public:
  explicit CommuteTile(const Commute *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    w_ = c.width, h_ = c.height, tile_ = c.tile;
    snprintf(entity_, sizeof(entity_), "%s", c.entity ? c.entity : "");
    const bool one_cell = c.columns == 1 || (w_ < ui::px(300) && h_ < ui::px(150));
    kind_ = one_cell ? SMALL : (h_ < ui::px(190) ? WIDE : BIG);
    lv_obj_t *p = c.parent;
    idle_.make(p);
    idle_.place(w_, h_);
    if (kind_ == SMALL) {
      ring_.make(p);
      name_ = ui::label(p, Font::TITLE, theme::INK);
      pct_ = ui::label(p, Font::HEADLINE, theme::INK);
      eta_ = ui::label(p, Font::BODY, theme::MUTED);
      badge_.make(p, Font::BODY);
    } else {
      ribbon_.make(p, kind_ == BIG);
      from_ = ui::label(p, Font::ICON_SMALL, theme::MUTED);
      to_ = ui::label(p, Font::ICON_SMALL, theme::INK);
      pct_ = ui::label(p, kind_ == BIG ? Font::DIGITS : Font::HEADLINE, theme::INK);
      name_ = ui::label(p, Font::TITLE, theme::INK);
      dir_ = ui::label(p, Font::BODY, theme::MUTED);
      eta_ = ui::label(p, kind_ == BIG ? Font::HEADLINE : Font::BODY_LARGE, theme::INK);
      badge_.make(p, Font::BODY);
      why_ = ui::label(p, Font::BODY, theme::MUTED);
      if (kind_ == BIG) {
        arrive_ = ui::label(p, Font::BODY, theme::MUTED);
        rows_ = 6;  // as many as fit are shown, the newest last
        for (int i = 0; i < rows_; ++i) {
          row_disc_[i].make(p, theme::MARK_BLUE, Font::ICON_SMALL);
          row_name_[i] = ui::label(p, Font::BODY, theme::INK);
          row_time_[i] = ui::label(p, Font::BODY, theme::MUTED);
        }
      }
    }
  }

  void on_state(JsonObjectConst data) override {
    who_.read(data);
    if (!data["wait"].isNull()) who_.valid = false;
  }

  void on_tick(uint32_t) override {
    const bool on = who_.valid && commuting(who_.phase) && who_.progress >= 0;
    idle_.visible(!on);
    if (!on) {
      uint32_t icon;
      bool accent;
      std::string t, s;
      idle_texts(plugin_, who_, icon, accent, t, s);
      idle_.paint(icon, accent, t, s);
    }
    if (kind_ == SMALL) paint_small(on);
    else paint_wide(on);
  }

  void on_theme() override { on_tick(tessera::epoch()); }

  void on_tap() override {
    if (who_.valid) tessera::open_card(plugin_->plugin_id(), "trip", entity_, tile_, who_.name);
  }

 private:
  enum Kind : uint8_t { SMALL, WIDE, BIG };

  void paint_small(bool on) {
    for (lv_obj_t *o : {name_, pct_, eta_}) show(o, on);
    show(ring_.back, on);
    ring_.middle.visible(on);
    if (!on) {
      for (auto *a : ring_.arc) show(a, false);
      badge_.paint("", 0, 0);
      return;
    }
    const int size = std::min(h_, w_ * 46 / 100);
    ring_.place(0, (h_ - size) / 2, size);
    ring_.paint(who_);
    const int x = size + ui::px(12), tw = w_ - x;
    const int lh_name = ui::line_height(Font::TITLE), lh_eta = ui::line_height(Font::BODY);
    const std::string pct = std::to_string(who_.percent()) + "%";
    Font face = Font::VALUE;
    for (Font f : {Font::VALUE, Font::HEADLINE, Font::TITLE})
      if (face = f, ui::text_width(pct, f) <= tw) break;
    ui::set_font(pct_, face);
    const int lh_pct = ui::line_height(face);
    const int top = std::max(0, (h_ - lh_name - lh_pct - lh_eta) / 2);
    box(name_, x, top, tw, lh_name);
    box(pct_, x, top + lh_name, tw, lh_pct);
    ui::set_text(name_, who_.name);
    ui::set_text(pct_, pct);
    const std::string eta = clock(who_.eta);
    ui::set_text(eta_, eta);
    const int ew = ui::text_width(eta, Font::BODY);
    box(eta_, x, top + lh_name + lh_pct, std::min(tw, ew + 2), lh_eta);
    const std::string late = delay_text(plugin_, who_);
    const int bw = late.empty() ? 0 : ui::text_width(late, Font::BODY) + 2 * ui::px(6);
    badge_.paint(x + ew + ui::px(6) + bw <= w_ ? late : "", x + ew + ui::px(6) + bw, top + lh_name + lh_pct + lh_eta / 2);
  }

  void paint_wide(bool on) {
    for (lv_obj_t *o : {from_, to_, pct_, name_, dir_, eta_, why_, arrive_}) show(o, on);
    ribbon_.visible(on);
    for (int i = 0; i < rows_; ++i) {
      row_disc_[i].visible(false);
      show(row_name_[i], false);
      show(row_time_[i], false);
    }
    if (!on) {
      badge_.paint("", 0, 0);
      return;
    }
    const bool big = kind_ == BIG;
    // The top row: the percentage, the name and the way; the arrival on the right.
    const Font pf = big ? Font::DIGITS : Font::HEADLINE, ef = big ? Font::HEADLINE : Font::BODY_LARGE;
    const std::string pct = std::to_string(who_.percent()) + "%";
    const int lh_p = ui::line_height(pf), pw = ui::text_width(pct, pf) + ui::px(2);
    box(pct_, 0, 0, pw, lh_p);
    ui::set_text(pct_, pct);
    const std::string eta = clock(who_.eta);
    const std::string late = delay_text(plugin_, who_);
    const int lh_e = ui::line_height(ef), ew = ui::text_width(eta, ef) + ui::px(2);
    int right = w_;
    const int ecy = big ? ui::line_height(Font::BODY) + lh_e / 2 : lh_p / 2;
    const int bw = badge_.paint(late, right, ecy);
    if (bw) right -= bw + ui::px(6);
    box(eta_, right - ew, ecy - lh_e / 2, ew, lh_e);
    ui::set_text(eta_, eta);
    if (arrive_) {
      const std::string a = plugin_->text("arrive");
      const int aw = ui::text_width(a, Font::BODY) + ui::px(2);
      box(arrive_, w_ - aw, 0, aw, ui::line_height(Font::BODY));
      ui::set_text(arrive_, a);
    }
    const int nx = pw + ui::px(10), nw = std::max(0, right - ew - ui::px(10) - nx);
    const std::string dir = plugin_->text(who_.to_home() ? "to_home" : "to_work");
    if (big) {
      // Name over the way, beside the large digits.
      const int lt = ui::line_height(Font::TITLE), lb = ui::line_height(Font::BODY);
      const int ty = std::max(0, (lh_p - lt - lb) / 2);
      box(name_, nx, ty, nw, lt);
      box(dir_, nx, ty + lt, nw, lb);
      ui::set_text(dir_, dir + ", " + plugin_->text(MODE_KEYS[who_.mode]));
    } else {
      const int nmw = std::min(nw, ui::text_width(who_.name, Font::TITLE) + ui::px(2));
      box(name_, nx, (lh_p - ui::line_height(Font::TITLE)) / 2, nmw, ui::line_height(Font::TITLE));
      box(dir_, nx + nmw + ui::px(8), (lh_p - ui::line_height(Font::BODY)) / 2, std::max(1, nw - nmw - ui::px(8)),
          ui::line_height(Font::BODY));
      ui::set_text(dir_, dir);
    }
    ui::set_text(name_, who_.name);
    // The ribbon between the two ends.
    const int thick = big ? ui::px(26) : ui::px(16), msize = big ? ui::px(44) : ui::px(36);
    const int icon_w = ui::line_height(Font::ICON_SMALL);
    const int why_h = ui::line_height(Font::BODY);
    const int rcy = big ? lh_p + ui::px(8) + msize / 2 : lh_p + (h_ - lh_p - why_h) / 2;
    box(from_, 0, rcy - icon_w / 2, icon_w, icon_w);
    box(to_, w_ - icon_w, rcy - icon_w / 2, icon_w, icon_w);
    ui::set_text(from_, ui::icon(who_.to_home() ? ICON_WORK : ICON_HOME));
    ui::set_text(to_, ui::icon(who_.to_home() ? ICON_HOME : ICON_WORK));
    ribbon_.place(icon_w + ui::px(8), rcy, w_ - 2 * (icon_w + ui::px(8)), thick, msize);
    ribbon_.paint(who_);
    // Under it: the legs (on a large tile) and what the guess is based on.
    int y = rcy + msize / 2 + ui::px(8);
    if (big) {
      const int rh = ui::px(30), ds = ui::line_height(Font::ICON_SMALL) + ui::px(2);
      const int fit = std::clamp((h_ - why_h - y) / rh, 0, rows_);
      for (int r = 0, i = std::max(0, who_.nlegs - fit); i < who_.nlegs; ++i, ++r) {
        const Leg &l = who_.legs[i];
        row_disc_[r].visible(true);
        row_disc_[r].place(0, y + (rh - ds) / 2, ds);
        row_disc_[r].paint(MODE_ICONS[l.mode], mode_role(l.mode), theme::ON_ACCENT);
        const std::string mname = plugin_->text(MODE_KEYS[l.mode]);
        const int mw = std::min(w_ / 3, ui::text_width(mname, Font::BODY) + ui::px(2));
        box(row_name_[r], ds + ui::px(10), y + (rh - why_h) / 2, mw, why_h);
        ui::set_text(row_name_[r], mname);
        show(row_name_[r], true);
        const std::string when = i + 1 < who_.nlegs
                                     ? fill2(plugin_->text("span"), "a", clock(l.at), "b", clock(who_.legs[i + 1].at))
                                     : tessera::fill(plugin_->text("since"), "t", clock(l.at));
        const int tx = ds + ui::px(20) + mw;
        box(row_time_[r], tx, y + (rh - why_h) / 2, std::max(1, w_ - tx), why_h);
        ui::set_text(row_time_[r], when);
        show(row_time_[r], true);
        y += rh;
      }
    }
    box(why_, 0, h_ - why_h, w_, why_h);
    ui::set_text(why_, why_line(plugin_, who_, !big));
  }

  const Commute *plugin_;
  int w_ = 0, h_ = 0, tile_ = -1, rows_ = 0;
  Kind kind_ = WIDE;
  char entity_[96] = "";
  Person who_;
  Idle idle_;
  Ring ring_;
  Ribbon ribbon_;
  Badge badge_;
  lv_obj_t *name_{}, *pct_{}, *eta_{}, *dir_{}, *why_{}, *from_{}, *to_{}, *arrive_{};
  Disc row_disc_[6];
  lv_obj_t *row_name_[6]{}, *row_time_[6]{};
};

// ---------------------------------------------------------------------------------------------------------------
// The tile of several people: a lane each
// ---------------------------------------------------------------------------------------------------------------

static const char *const PEOPLE[4] = {"person_1", "person_2", "person_3", "person_4"};

class CommutesTile : public tessera::Tile {
 public:
  explicit CommutesTile(const Commute *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    w_ = c.width, h_ = c.height, tile_ = c.tile;
    lv_obj_t *p = c.parent;
    row_h_ = std::max(ui::px(40), ui::line_height(Font::TITLE) + ui::px(16));
    lanes_ = std::clamp(h_ / row_h_, 1, 4);
    header_ = h_ - lanes_ * row_h_ >= ui::line_height(Font::BODY) + ui::px(4) || lanes_ < 2;
    if (header_ && lanes_ > 1 && lanes_ * row_h_ + ui::line_height(Font::BODY) > h_) --lanes_;
    head_ = ui::label(p, Font::BODY, theme::MUTED);
    idle_.make(p);
    idle_.place(w_, h_);
    for (int i = 0; i < lanes_; ++i) {
      name_[i] = ui::label(p, Font::TITLE, theme::INK);
      status_[i] = ui::label(p, Font::BODY, theme::MUTED);
      eta_[i] = ui::label(p, Font::BODY_LARGE, theme::INK);
      badge_[i].make(p, Font::BODY);
      ribbon_[i].make(p, false);
    }
  }

  void on_state(JsonObjectConst data) override {
    JsonObjectConst ents = data["entities"];
    for (int i = 0; i < 4; ++i) who_[i].read(ents[PEOPLE[i]]);
  }

  void on_tick(uint32_t) override {
    // The people who were chosen, those on the way first.
    int order[4], count = 0, travelling = 0;
    for (int pass = 0; pass < 2; ++pass)
      for (int i = 0; i < 4; ++i) {
        const Person &w = who_[i];
        if (!w.valid) continue;
        const bool on = commuting(w.phase);
        if ((pass == 0) == on) order[count++] = i, travelling += on;
      }
    const bool idle = travelling == 0;
    idle_.visible(idle);
    show(head_, !idle && header_);
    if (idle) {
      idle_.paint(ICON_HOME, false, plugin_->text(count ? "nobody" : "choose_people"), "");
      for (int r = 0; r < lanes_; ++r) hide_lane(r);
      shown_ = 0;
      return;
    }
    int y = 0;
    if (header_) {
      const bool home = who_[order[0]].to_home();
      ui::set_text(head_, std::string(plugin_->text("on_the_way")) + ", " + plugin_->text(home ? "to_home" : "to_work"));
      box(head_, 0, 0, w_, ui::line_height(Font::BODY));
      y = ui::line_height(Font::BODY) + ui::px(2);
    }
    const int rh = std::min(ui::px(64), (h_ - y) / std::max(1, std::min(count, lanes_)));
    shown_ = std::min(count, lanes_);
    for (int r = 0; r < lanes_; ++r) {
      if (r >= count) {
        hide_lane(r);
        continue;
      }
      shown_index_[r] = order[r];
      paint_lane(r, who_[order[r]], y + r * rh, rh);
    }
    lane_top_ = y, lane_h_ = rh;
  }

  void on_theme() override { on_tick(tessera::epoch()); }

  void on_tap() override {
    const tessera::Point at = tessera::tap_point();
    int r = 0;
    if (at.y >= 0 && lane_h_ > 0) r = std::clamp((at.y - lane_top_) / lane_h_, 0, std::max(0, shown_ - 1));
    if (shown_ == 0) return;
    const Person &w = who_[shown_index_[r]];
    tessera::open_card(plugin_->plugin_id(), "trip", PEOPLE[shown_index_[r]], tile_, w.name);
  }

 private:
  void hide_lane(int r) {
    show(name_[r], false);
    show(status_[r], false);
    show(eta_[r], false);
    badge_[r].paint("", 0, 0);
    ribbon_[r].visible(false);
  }

  void paint_lane(int r, const Person &w, int y, int rh) {
    const int cy = y + rh / 2;
    const int nw = std::min(w_ / 4, ui::px(84));
    box(name_[r], 0, cy - ui::line_height(Font::TITLE) / 2, nw, ui::line_height(Font::TITLE));
    ui::set_text(name_[r], w.name);
    show(name_[r], true);
    const bool on = commuting(w.phase) && w.progress >= 0;
    ribbon_[r].visible(on);
    show(status_[r], !on);
    show(eta_[r], on);
    if (!on) {
      badge_[r].paint("", 0, 0);
      const char *key = w.phase == P_HOME ? "home" : w.phase == P_WORK ? "at_work" : w.phase == P_MAYBE ? "maybe_short" : "away";
      box(status_[r], nw + ui::px(10), cy - ui::line_height(Font::BODY) / 2, w_ - nw - ui::px(10),
          ui::line_height(Font::BODY));
      ui::set_text(status_[r], plugin_->text(key));
      return;
    }
    const std::string eta = clock(w.eta), late = delay_text(plugin_, w);
    const int right_w = ui::text_width("00:00", Font::BODY_LARGE) + ui::text_width("+00", Font::BODY) + ui::px(22);
    int right = w_;
    const int bw = badge_[r].paint(late, right, cy);
    if (bw) right -= bw + ui::px(6);
    const int ew = ui::text_width(eta, Font::BODY_LARGE) + ui::px(2);
    box(eta_[r], right - ew, cy - ui::line_height(Font::BODY_LARGE) / 2, ew, ui::line_height(Font::BODY_LARGE));
    ui::set_text(eta_[r], eta);
    const int msize = std::min(rh - ui::px(6), ui::line_height(Font::ICON_SMALL) + ui::px(8));
    const int rx = nw + ui::px(10);
    ribbon_[r].place(rx, cy, std::max(msize, w_ - right_w - rx - ui::px(10)), std::max(ui::px(8), msize / 3), msize);
    ribbon_[r].paint(w);
  }

  const Commute *plugin_;
  int w_ = 0, h_ = 0, tile_ = -1, lanes_ = 1, row_h_ = 0, shown_ = 0, lane_top_ = 0, lane_h_ = 0;
  int shown_index_[4] = {};
  bool header_ = false;
  Person who_[4];
  Idle idle_;
  lv_obj_t *head_{};
  lv_obj_t *name_[4]{}, *status_[4]{}, *eta_[4]{};
  Badge badge_[4];
  Ribbon ribbon_[4];
};

// ---------------------------------------------------------------------------------------------------------------
// The card: the map of the trip
// ---------------------------------------------------------------------------------------------------------------

class TripCard : public tessera::Card {
 public:
  explicit TripCard(const Commute *plugin) : plugin_(plugin) {}

  void open(const tessera::CardContext &c) override {
    w_ = c.width, h_ = c.height;
    snprintf(which_, sizeof(which_), "%s", c.entity ? c.entity : "");
    // A wide card puts the map beside the words, a narrow one above them.
    beside_ = w_ >= h_ * 5 / 4;
    lv_obj_t *p = c.parent;
    map_ = ui::block(p, theme::RAISED);
    lv_obj_set_style_radius(map_, ui::px(14), 0);
    lv_obj_remove_flag(map_, LV_OBJ_FLAG_SCROLLABLE);
    straight_ = new_line(map_);
    for (auto &l : line_) l = new_line(map_);
    rest_ = new_line(map_);
    pin_from_.make(map_, theme::CARD, Font::ICON_SMALL);
    pin_to_.make(map_, theme::CARD, Font::ICON_SMALL);
    marker_.make(map_, theme::MARK_BLUE, Font::ICON_SMALL);
    note_ = ui::label(p, Font::TITLE, theme::INK);
    pct_ = ui::label(p, beside_ ? Font::DIGITS : Font::VALUE, theme::INK);
    arrive_ = ui::label(p, Font::BODY, theme::MUTED);
    eta_ = ui::label(p, beside_ ? Font::HEADLINE : Font::TITLE, theme::INK);
    badge_.make(p, Font::BODY);
    legs_head_ = ui::label(p, Font::BODY, theme::MUTED);
    based_head_ = ui::label(p, Font::BODY, theme::MUTED);
    for (int i = 0; i < Person::LEGS; ++i) {
      row_disc_[i].make(p, theme::MARK_BLUE, Font::ICON_SMALL);
      row_name_[i] = ui::label(p, Font::BODY, theme::INK);
      row_time_[i] = ui::label(p, Font::BODY, theme::MUTED);
    }
    for (auto &b : based_) b = ui::label(p, Font::BODY, theme::INK);
  }

  void on_state(JsonObjectConst data) override {
    JsonObjectConst ents = data["entities"];
    if (!ents.isNull() && strncmp(which_, "person_", 7) == 0) who_.read(ents[which_]);
    else who_.read(data);
  }

  void on_tick(uint32_t) override {
    const bool on = who_.valid && commuting(who_.phase);
    const bool map = on && who_.has_work;
    // Where the map and the words go: the map only for a trip it can draw (a tile of several people sends no trail).
    const int gap = ui::px(16), top_h = beside_ ? 0 : ui::line_height(Font::VALUE) + ui::px(8);
    if (!map) mw_ = mh_ = 0, ix_ = 0, iy_ = 0, iw_ = w_;
    else if (beside_) mw_ = w_ * 56 / 100, mh_ = h_, ix_ = mw_ + gap, iy_ = 0, iw_ = w_ - ix_;
    else mw_ = w_, mh_ = std::max(ui::px(120), h_ * 38 / 100), ix_ = 0, iy_ = top_h + mh_ + ui::px(10), iw_ = w_;
    show(map_, map);
    if (map) box(map_, 0, beside_ ? 0 : top_h, mw_, mh_);
    show(note_, !on);
    if (!on) {
      ui::set_text(note_, plugin_->text(who_.valid ? "no_commute" : "choose"));
      box(note_, 0, (h_ - ui::line_height(Font::TITLE)) / 2, w_, ui::line_height(Font::TITLE));
      lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
    }
    if (map) paint_map();
    paint_info(on);
  }

  void on_theme() override { on_tick(tessera::epoch()); }

 private:
  static lv_obj_t *new_line(lv_obj_t *parent) {
    lv_obj_t *l = lv_line_create(parent);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(l, 0, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
  }

  // Metres east and north of home to pixels of the map: the trip, home and work all fit, north up.
  void fit() {
    int32_t e0 = 0, e1 = 0, n0 = 0, n1 = 0;
    auto take = [&](int32_t e, int32_t n) {
      e0 = std::min(e0, e), e1 = std::max(e1, e), n0 = std::min(n0, n), n1 = std::max(n1, n);
    };
    take(who_.work_e, who_.work_n);
    for (int i = 0; i < who_.ntrail; ++i) take(who_.te[i], who_.tn[i]);
    const int margin = ui::px(30);
    const float dx = std::max<int32_t>(e1 - e0, 200), dy = std::max<int32_t>(n1 - n0, 200);
    scale_ = std::min((mw_ - 2 * margin) / dx, (mh_ - 2 * margin) / dy);
    ce_ = (e0 + e1) / 2.0f, cn_ = (n0 + n1) / 2.0f;
  }
  lv_point_precise_t at(int32_t e, int32_t n) const {
    lv_point_precise_t p;
    p.x = static_cast<lv_value_precise_t>(mw_ / 2.0f + (e - ce_) * scale_);
    p.y = static_cast<lv_value_precise_t>(mh_ / 2.0f - (n - cn_) * scale_);
    return p;
  }

  void style_line(lv_obj_t *l, theme::Role role, int width, bool dashed) {
    lv_obj_set_style_line_color(l, ui::color(role), 0);
    lv_obj_set_style_line_width(l, width, 0);
    lv_obj_set_style_line_dash_width(l, dashed ? std::max(2, ui::px(3)) : 0, 0);
    lv_obj_set_style_line_dash_gap(l, dashed ? ui::px(8) : 0, 0);
  }

  void paint_map() {
    lv_obj_set_style_bg_color(map_, ui::color(theme::RAISED), 0);
    for (auto *l : line_) show(l, false);
    fit();
    const bool to_home = who_.phase == P_TO_HOME;
    const lv_point_precise_t home = at(0, 0), work = at(who_.work_e, who_.work_n);
    const lv_point_precise_t from = to_home ? work : home, to = to_home ? home : work;
    // The straight line between the two ends, faint.
    pts_[0] = from, pts_[1] = to;
    lv_line_set_points(straight_, pts_, 2);
    style_line(straight_, theme::TICK, std::max(1, ui::px(2)), true);
    show(straight_, true);
    // The trail, one line for every run of one way of travelling; a run reaches the first point of the next.
    int used = 2, lines = 0;
    for (int i = 0; i < who_.ntrail && lines < LINES;) {
      int j = i;
      while (j + 1 < who_.ntrail && who_.tm[j + 1] == who_.tm[i]) ++j;
      const int end = std::min(j + 1, who_.ntrail - 1);
      const int count = end - i + 1;
      if (count >= 2 && used + count <= POINTS) {
        for (int k = 0; k < count; ++k) pts_[used + k] = at(who_.te[i + k], who_.tn[i + k]);
        lv_line_set_points(line_[lines], pts_ + used, count);
        style_line(line_[lines], mode_role(who_.tm[i]), std::max(3, ui::px(7)), false);
        show(line_[lines], true);
        used += count, ++lines;
      }
      i = j + 1;
    }
    // Where the person is now, and the rest of the way, dotted.
    lv_point_precise_t now = from;
    if (who_.ntrail) now = at(who_.te[who_.ntrail - 1], who_.tn[who_.ntrail - 1]);
    else if (who_.progress > 0) {
      now.x = from.x + (to.x - from.x) * who_.progress / 100;
      now.y = from.y + (to.y - from.y) * who_.progress / 100;
    }
    pts_[used] = now, pts_[used + 1] = to;
    lv_line_set_points(rest_, pts_ + used, 2);
    style_line(rest_, theme::MUTED, std::max(2, ui::px(4)), true);
    show(rest_, true);
    const int ps = ui::line_height(Font::ICON_SMALL) + ui::px(10);
    pin(pin_from_, from, ps, to_home ? ICON_WORK : ICON_HOME, theme::MUTED);
    pin(pin_to_, to, ps, to_home ? ICON_HOME : ICON_WORK, theme::INK);
    const int ms = ps + ui::px(8);
    marker_.place(static_cast<int>(now.x) - ms / 2, static_cast<int>(now.y) - ms / 2, ms);
    marker_.paint(MODE_ICONS[who_.mode], mode_role(who_.mode), theme::ON_ACCENT);
    lv_obj_set_style_border_width(marker_.ground, std::max(2, ui::px(4)), 0);
    lv_obj_set_style_border_color(marker_.ground, ui::color(theme::RAISED), 0);
    lv_obj_move_foreground(marker_.ground);
  }

  void pin(Disc &d, lv_point_precise_t p, int size, uint32_t icon, theme::Role ink) {
    d.place(static_cast<int>(p.x) - size / 2, static_cast<int>(p.y) - size / 2, size);
    d.paint(icon, theme::CARD, ink);
    lv_obj_set_style_border_width(d.ground, std::max(1, ui::px(2)), 0);
    lv_obj_set_style_border_color(d.ground, ui::color(theme::LINE), 0);
  }

  void paint_info(bool on) {
    for (lv_obj_t *o : {pct_, arrive_, eta_, legs_head_, based_head_}) show(o, on);
    for (int i = 0; i < Person::LEGS; ++i) {
      row_disc_[i].visible(false);
      show(row_name_[i], false);
      show(row_time_[i], false);
    }
    for (auto *b : based_) show(b, false);
    badge_.paint("", 0, 0);
    if (!on) return;
    const int lb = ui::line_height(Font::BODY);
    // The percentage and the arrival: beside the map on top of the words, or one row above a narrow card's map.
    const Font pf = beside_ ? Font::DIGITS : Font::VALUE, ef = beside_ ? Font::HEADLINE : Font::TITLE;
    const int lp = ui::line_height(pf), le = ui::line_height(ef);
    const int ty = beside_ || !mh_ ? iy_ : 0;
    const std::string pct = std::to_string(who_.percent()) + "%";
    const int pw = ui::text_width(pct, pf) + ui::px(2);
    box(pct_, ix_, ty, pw, lp);
    ui::set_text(pct_, pct);
    const std::string a = plugin_->text("arrive"), eta = clock(who_.eta), late = delay_text(plugin_, who_);
    const int ew = ui::text_width(eta, ef) + ui::px(2);
    if (beside_) {
      const int ax = ix_ + pw + ui::px(16), ay = ty + (lp - lb - le) / 2;
      box(arrive_, ax, ay, std::max(1, ix_ + iw_ - ax), lb);
      box(eta_, ax, ay + lb, ew, le);
      const int bw = late.empty() ? 0 : ui::text_width(late, Font::BODY) + 2 * ui::px(6);
      badge_.paint(late, ax + ew + ui::px(8) + bw, ay + lb + le / 2);
    } else {
      // Right-aligned on the narrow card: "Arrives 18:21 +3".
      int right = ix_ + iw_;
      const int bw = badge_.paint(late, right, ty + lp / 2);
      if (bw) right -= bw + ui::px(6);
      box(eta_, right - ew, ty + (lp - le) / 2, ew, le);
      const int aw = ui::text_width(a, Font::BODY) + ui::px(2);
      box(arrive_, right - ew - ui::px(8) - aw, ty + (lp - lb) / 2 + ui::px(2), aw, lb);
    }
    ui::set_text(arrive_, a);
    ui::set_text(eta_, eta);
    int y = beside_ || !mh_ ? ty + lp + ui::px(10) : iy_;
    // The legs, the newest last; as many as leave room for two lines of what the guess is based on.
    const int rh = ui::line_height(Font::ICON_SMALL) + ui::px(6), ds = ui::line_height(Font::ICON_SMALL) + ui::px(2);
    const int legs_room = std::max(1, (h_ - y - lb - ui::px(4) - (lb + ui::px(8)) - 2 * (lb + ui::px(4))) / rh);
    const int shown = std::min<int>(who_.nlegs, legs_room);
    box(legs_head_, ix_, y, iw_, lb);
    ui::set_text(legs_head_, plugin_->text("legs"));
    y += lb + ui::px(4);
    for (int r = 0, i = who_.nlegs - shown; i < who_.nlegs; ++i, ++r) {
      const Leg &l = who_.legs[i];
      row_disc_[r].visible(true);
      row_disc_[r].place(ix_, y + (rh - ds) / 2, ds);
      row_disc_[r].paint(MODE_ICONS[l.mode], mode_role(l.mode), theme::ON_ACCENT);
      const std::string mname = plugin_->text(MODE_KEYS[l.mode]);
      const int mw = std::min(iw_ / 2, ui::text_width(mname, Font::BODY) + ui::px(2));
      box(row_name_[r], ix_ + ds + ui::px(10), y + (rh - lb) / 2, mw, lb);
      ui::set_text(row_name_[r], mname);
      show(row_name_[r], true);
      const std::string when = !l.at ? std::string()
                               : i + 1 < who_.nlegs ? fill2(plugin_->text("span"), "a", clock(l.at), "b", clock(who_.legs[i + 1].at))
                                                    : tessera::fill(plugin_->text("since"), "t", clock(l.at));
      const int tx = ix_ + ds + ui::px(20) + mw;
      box(row_time_[r], tx, y + (rh - lb) / 2, std::max(1, ix_ + iw_ - tx), lb);
      ui::set_text(row_time_[r], when);
      show(row_time_[r], !when.empty());
      y += rh;
    }
    // What the guess is based on, in the lines left.
    y += ui::px(8);
    box(based_head_, ix_, y, iw_, lb);
    ui::set_text(based_head_, plugin_->text("based_on"));
    y += lb + ui::px(2);
    std::string lines[5];
    int n = 0;
    if ((who_.why & W_NS_TRACKER) || (who_.ns_km >= 0 && who_.ns_km <= 1.5f)) lines[n++] = plugin_->text("ns_near_line");
    if (who_.speed >= 0) lines[n++] = tessera::format(plugin_->text("speed_line"), who_.speed);
    if (*who_.train) lines[n++] = tessera::fill(plugin_->text("ns_line"), "a", who_.train);
    lines[n++] = *who_.activity ? tessera::fill(plugin_->text("activity_line"), "a", who_.activity)
                                : std::string(plugin_->text("activity_none"));
    if (who_.place_at) lines[n++] = tessera::fill(plugin_->text("place_line"), "t", clock(who_.place_at));
    for (int i = 0; i < n && i < 5; ++i) {
      if (y + lb > h_) break;
      box(based_[i], ix_, y, iw_, lb);
      ui::set_text(based_[i], lines[i]);
      show(based_[i], true);
      y += lb + ui::px(4);
    }
  }

  static constexpr int LINES = 8, POINTS = Person::TRAIL + LINES + 4;
  const Commute *plugin_;
  int w_ = 0, h_ = 0, mw_ = 0, mh_ = 0, ix_ = 0, iy_ = 0, iw_ = 0;
  bool beside_ = false;
  char which_[96] = "";
  float scale_ = 1, ce_ = 0, cn_ = 0;
  Person who_;
  lv_point_precise_t pts_[POINTS]{};
  lv_obj_t *map_{}, *straight_{}, *rest_{}, *note_{};
  lv_obj_t *line_[LINES]{};
  Disc pin_from_, pin_to_, marker_;
  lv_obj_t *pct_{}, *arrive_{}, *eta_{}, *legs_head_{}, *based_head_{};
  Badge badge_;
  Disc row_disc_[Person::LEGS];
  lv_obj_t *row_name_[Person::LEGS]{}, *row_time_[Person::LEGS]{};
  lv_obj_t *based_[5]{};
};

// ---------------------------------------------------------------------------------------------------------------
// The plugin and its top bar item
// ---------------------------------------------------------------------------------------------------------------

tessera::BarItem Commute::bar() const {
  tessera::BarItem item;
  if (phase_ == nullptr || !phase_->has_state()) return item;
  const uint8_t phase = phase_of(phase_->state.c_str());
  if (!commuting(phase)) return item;
  const uint8_t mode = mode_ != nullptr && mode_->has_state() ? mode_of(mode_->state.c_str()) : uint8_t(UNKNOWN);
  const int pct = progress_ != nullptr && progress_->has_state() ? atoi(progress_->state.c_str()) : 0;
  const std::string who = person_ != nullptr && person_->has_state() ? person_->state : std::string();
  item.shown = true;
  item.icon = MODE_ICONS[mode];
  item.text = tessera::format(tessera::fill(text("bar"), "name", who).c_str(), std::clamp(pct, 0, 99));
  item.tone = Tone::ACCENT;
  return item;
}

void Commute::setup() {
  add_tile("commute", [this]() { return new CommuteTile(this); });
  add_tile("commutes", [this]() { return new CommutesTile(this); });
  add_card("trip", [this]() { return new TripCard(this); }, true);
  add_bar_item("progress", [this]() { return bar(); });
}

}  // namespace esphome::commute
