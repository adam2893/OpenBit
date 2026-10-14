#include "bl/mod.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

namespace bl {
namespace {

struct KindName {
  const char* token;
  Widget kind;
};

const KindName kKinds[] = {
    {"Category", Widget::Category},
    {"Toggle", Widget::Toggle},
    {"SeekBar", Widget::SeekBar},
    {"CheckBoxFeature", Widget::CheckBoxFeature},
    {"CheckBox", Widget::CheckBox},
    {"ButtonOnOff", Widget::ButtonOnOff},
    {"ButtonLink", Widget::ButtonLink},
    {"Button", Widget::Button},
    {"CollapseAdd", Widget::CollapseAdd},
    {"Collapse", Widget::Collapse},
    {"InputLNum", Widget::InputNum},
    {"InputNum", Widget::InputNum},
    {"InputText", Widget::InputText},
    {"RichTextView", Widget::RichTextView},
    {"Spinner", Widget::Spinner},
    {"RadioButton", Widget::RadioButton},
    {"Link", Widget::Link},
    {"DonateImg", Widget::DonateImg},
};

Widget kindFor(const std::string& token) {
  for (const KindName& k : kKinds) if (token == k.token) return k.kind;
  return Widget::Unknown;
}

bool splitHead(const std::string& s, std::string& head, std::string& rest) {
  const size_t pos = s.find('_');
  if (pos == std::string::npos) return false;
  head = s.substr(0, pos);
  rest = s.substr(pos + 1);
  return true;
}

std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (unsigned char)s[a] <= ' ') ++a;
  while (b > a && (unsigned char)s[b - 1] <= ' ') --b;
  return s.substr(a, b - a);
}

bool isNumber(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) if (!((c >= '0' && c <= '9') || c == '-' || c == '.')) return false;
  return true;
}

}  // namespace

const char* widgetName(Widget w) {
  switch (w) {
    case Widget::Category: return "Category";
    case Widget::Toggle: return "Toggle";
    case Widget::SeekBar: return "SeekBar";
    case Widget::CheckBox: return "CheckBox";
    case Widget::CheckBoxFeature: return "CheckBoxFeature";
    case Widget::Button: return "Button";
    case Widget::ButtonOnOff: return "ButtonOnOff";
    case Widget::ButtonLink: return "ButtonLink";
    case Widget::Collapse: return "Collapse";
    case Widget::CollapseAdd: return "CollapseAdd";
    case Widget::InputNum: return "InputNum";
    case Widget::InputText: return "InputText";
    case Widget::RichTextView: return "RichTextView";
    case Widget::Spinner: return "Spinner";
    case Widget::RadioButton: return "RadioButton";
    case Widget::Link: return "Link";
    case Widget::DonateImg: return "DonateImg";
    default: return "Unknown";
  }
}

MenuNode parseMenuNode(const std::string& raw) {
  MenuNode node;
  node.raw = raw;
  std::string rest = raw;

  // "-3_Toggle_True_Auto Menu Size": the leading integer is the ListView row
  // index the native code uses to address the SharedPreferences entry.
  if (!rest.empty() && rest[0] == '-') {
    const size_t pos = rest.find('_');
    if (pos != std::string::npos) rest = rest.substr(pos + 1);
  }

  std::string head, tail;
  if (!splitHead(rest, head, tail)) {
    node.label = rest;
    return node;
  }
  node.kind = kindFor(head);
  // LGL toggle grammar: "_Toggle_<Label>" means default ON, an explicit
  // "True_"/"False_" token overrides it.
  if (node.kind == Widget::Toggle || node.kind == Widget::CheckBoxFeature) node.on = true;

  if (node.kind == Widget::CollapseAdd) {
    // "CollapseAdd_<child grammar>" -- the child is appended to the last
    // Collapse; parse it as a nested node.
    node.children.push_back(parseMenuNode(tail));
    node.label = node.children.front().label;
    return node;
  }

  std::string label = tail;
  // Optional Toggle default: "-1_Toggle_True_Foo" / "Toggle_False_Foo".
  std::string first, after;
  if (splitHead(label, first, after)) {
    if (first == "True" || first == "true" || first == "1") { node.on = true; label = after; }
    else if (first == "False" || first == "false" || first == "0") { node.on = false; label = after; }
  }
  // Optional SeekBar bounds: "SeekBar_Foo_1_10".
  if (node.kind == Widget::SeekBar) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : label) {
      if (c == '_') { parts.push_back(cur); cur.clear(); } else cur += c;
    }
    parts.push_back(cur);
    if (parts.size() >= 3 && isNumber(parts[parts.size() - 2]) && isNumber(parts.back())) {
      node.min = std::atof(parts[parts.size() - 2].c_str());
      node.max = std::atof(parts.back().c_str());
      node.value = node.min;
      label.clear();
      for (size_t i = 0; i + 2 < parts.size(); ++i) {
        if (i) label += '_';
        label += parts[i];
      }
    }
  }
  node.label = label;
  return node;
}

namespace {

void renderNode(const MenuNode& n, int depth, std::string& out) {
  if (n.kind == Widget::CollapseAdd) {
    // CollapseAdd is a container-only row: it appends the child below the last
    // Collapse instead of drawing a line of its own.
    for (const MenuNode& c : n.children) renderNode(c, depth, out);
    return;
  }
  out.append(size_t(depth) * 2, ' ');
  out += widgetName(n.kind);
  out += "  " + n.label;
  if (n.kind == Widget::Toggle || n.kind == Widget::CheckBox ||
      n.kind == Widget::CheckBoxFeature) {
    out += n.on ? "   [on]" : "   [off]";
  }
  if (n.kind == Widget::SeekBar) {
    std::ostringstream v;
    v << "   [" << n.value << " .. " << n.min << ".." << n.max << "]";
    out += v.str();
  }
  out += "\n";
  for (const MenuNode& c : n.children) renderNode(c, depth + 1, out);
}

}  // namespace

std::string Menu::render() const {
  std::string out;
  for (const MenuNode& n : nodes) renderNode(n, 1, out);
  return out;
}

Menu buildMenu() {
  // Verbatim runtime output of the mod's native SettingsList() / sub2025s().
  static const char* kSettingsList[] = {
      "Category_<font color='red'>Settings</font>",
      "-1_Toggle_Save Feature",
      "-3_Toggle_True_Auto Menu Size",
      "Category_<font color='red'>Game Info</font>",
  };
  static const char* kSub2025[] = {
      "Collapse_NOTE",
      "CollapseAdd_RichTextView_<div style=\"text-align: left;\"><font color='yellow'>"
      "\xe2\x9e\xa5 You need to install Google Play Services for the mod to work.</font></div>",
      "Category_<font color='red'>MOD FEATURES</font>",
      "Button_Free In-App Purchase",
      "Category_<font color='red'>CONTACT US</font>",
  };
  Menu menu;
  for (const char* s : kSub2025) menu.nodes.push_back(parseMenuNode(s));
  for (const char* s : kSettingsList) menu.nodes.push_back(parseMenuNode(s));
  return menu;
}

bool Mod::grant(const std::string& key) {
  Entitlement* e = state_.findEntitlement(trim(key));
  if (!e) return false;
  e->owned = true;
  state_.vars.userHasPurchasedBefore = true;
  ++state_.vars.numTimesPurchasesMade;

  // Mirror the side effects the real *Purchased grant points have on GameVars
  // (il2cpp_dump.txt Game.GameVars backing fields).
  const std::string grant = e->grantMethod ? e->grantMethod : "";
  if (grant == "BitizenshipPurchased") state_.vars.userBoughtBitizenship = true;
  if (grant == "GodModePurchased") state_.vars.userBoughtGodMode = true;
  if (grant == "RemoveAdsPurchased") state_.vars.ads = false;
  return true;
}

int Mod::grantAll() {
  int n = 0;
  for (Entitlement& e : state_.entitlements) {
    if (!e.grantMethod) continue;
    if (grant(e.grantMethod)) ++n;
  }
  freeInAppPurchase = true;
  return n;
}

}  // namespace bl
