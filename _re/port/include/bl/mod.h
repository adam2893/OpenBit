// bl/mod.h -- the mod's feature set, re-implemented natively.
//
// The original build is stock BitLife 3.25.1 plus the public LGL Mod Menu v5.3
// Java template and a custom native libmenu.so. The menu Java half
// (com.android.support.Menu) only draws the overlay and writes the toggles into
// SharedPreferences ("<pkg>_preferences", Preferences.changeFeatureBool(index, v));
// the actual cheat lives in libmenu.so, which ShadowHooks 2.0.1 inline-hooks
// ~50 Game.UIIAPManager "*Purchased" grant points (plus a matching batch of
// "Purchase*" initiators) and BitLife.UI.Containers.UIDarklyPaywallGenericIAP
// ::BuyButtonPress, so every product reports "already owned" and no Google Play
// transaction ever starts. See _re/REPORT.en.md sections 12 and 13.
#pragma once

#include "bl/game.h"

#include <string>
#include <vector>

namespace bl {

// The widget kinds recovered from com.android.support.Menu's builders.
enum class Widget {
  Category,
  Toggle,
  SeekBar,
  CheckBox,
  CheckBoxFeature,
  Button,
  ButtonOnOff,
  ButtonLink,
  Collapse,
  CollapseAdd,
  InputNum,
  InputText,
  RichTextView,
  Spinner,
  RadioButton,
  Link,
  DonateImg,
  Unknown,
};

const char* widgetName(Widget w);

// One menu node. `raw` is the original LGL grammar string untouched
// ("-1_Toggle_Save Feature", "SeekBar_Foo_1_10", "CollapseAdd_RichTextView_<div...>").
struct MenuNode {
  Widget kind = Widget::Unknown;
  std::string label;  // text after the kind prefix, e.g. "Save Feature"
  std::string raw;
  bool on = false;    // Toggle/CheckBox default, when the grammar states one
  double value = 0, min = 0, max = 0;
  std::vector<MenuNode> children;
};

struct Menu {
  std::vector<MenuNode> nodes;
  std::string render() const;  // indented text tree, for a console UI layer
};

// Parses one LGL grammar string: <Kind>_<Label>, with the optional "-N_" index
// prefix the real ListView rows carry and the optional "True_"/"False_" toggle
// default.
MenuNode parseMenuNode(const std::string& raw);

// The verbatim list the mod's native com.android.support.Menu.SettingsList()
// and .sub2025s() return at runtime (REPORT.en.md section 12).
Menu buildMenu();

class Mod {
 public:
  explicit Mod(GameState& state) : state_(state) {}

  // The mod's one cheat: "Free In-App Purchase". Marks every *Purchased grant
  // point as owned by setting the matching entitlement flag. Accepts a grant
  // method, purchase initiator, IAPManager field or short name; returns false
  // for an unknown key.
  bool grant(const std::string& key);
  int grantAll();

  // The two settings the real SettingsList() exposes.
  bool saveFeature = true;    // "-1_Toggle_Save Feature"        -- default ON
  bool autoMenuSize = true;   // "-3_Toggle_True_Auto Menu Size" -- grammar says True

  bool freeInAppPurchase = false;  // state of "Button_Free In-App Purchase"
  GameState& state() { return state_; }

 private:
  GameState& state_;
};

}  // namespace bl
