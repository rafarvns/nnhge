// Spec 028 FR-26 (T6.1): every UI style struct is made of kinds spec 014 can load, so a game can describe a theme in a
// data file by declaring the style structs (and its own theme type) in its schema source. Compiled with
// -fsyntax-only -Werror by the ctest ui_style_structs_are_data_loadable; never linked (it would add types to the
// registry of nnh_tests).

#include "nnh/data.hpp"
#include "nnh/ui.hpp"

NNH_DATA_ENUM(nnh::UiBackgroundKind, None, Solid, NineSlice);
NNH_DATA_ENUM(nnh::UiFont, Builtin);
NNH_DATA_ENUM(nnh::UiAnchor, TopLeft, Top, TopRight, Left, Center, Right, BottomLeft, Bottom, BottomRight);
NNH_DATA_ENUM(nnh::Ease, Linear, QuadIn, QuadOut, QuadInOut, CubicIn, CubicOut, CubicInOut, SineIn, SineOut, SineInOut,
              BackIn, BackOut, BackInOut, ElasticIn, ElasticOut, ElasticInOut, BounceIn, BounceOut, BounceInOut);

NNH_DATA_STRUCT(nnh::UiBackground, NNH_DATA_FIELD(kind), NNH_DATA_FIELD(texture), NNH_DATA_FIELD(border),
                NNH_DATA_FIELD(color), NNH_DATA_FIELD(border_color), NNH_DATA_FIELD(region), NNH_DATA_FIELD(left),
                NNH_DATA_FIELD(top), NNH_DATA_FIELD(right), NNH_DATA_FIELD(bottom));
NNH_DATA_STRUCT(nnh::UiLook, NNH_DATA_FIELD(background), NNH_DATA_FIELD(text), NNH_DATA_FIELD(tint));
NNH_DATA_STRUCT(nnh::UiTextLook, NNH_DATA_FIELD(font), NNH_DATA_FIELD(scale), NNH_DATA_FIELD(shadow),
                NNH_DATA_FIELD(shadow_color));
NNH_DATA_STRUCT(nnh::UiImagePart, NNH_DATA_FIELD(texture), NNH_DATA_FIELD(region));
NNH_DATA_STRUCT(nnh::UiSliderParts, NNH_DATA_FIELD(track), NNH_DATA_FIELD(fill), NNH_DATA_FIELD(knob),
                NNH_DATA_FIELD(knob_width), NNH_DATA_FIELD(track_height));
NNH_DATA_STRUCT(nnh::UiToggleParts, NNH_DATA_FIELD(off), NNH_DATA_FIELD(on));
NNH_DATA_STRUCT(nnh::UiChoiceParts, NNH_DATA_FIELD(left), NNH_DATA_FIELD(right), NNH_DATA_FIELD(text_arrows));
NNH_DATA_STRUCT(nnh::UiListParts, NNH_DATA_FIELD(row), NNH_DATA_FIELD(track), NNH_DATA_FIELD(thumb),
                NNH_DATA_FIELD(scroll_width), NNH_DATA_FIELD(row_height));
NNH_DATA_STRUCT(nnh::UiProgressParts, NNH_DATA_FIELD(fill));
NNH_DATA_STRUCT(nnh::UiStyle, NNH_DATA_FIELD(states), NNH_DATA_FIELD(text), NNH_DATA_FIELD(padding),
                NNH_DATA_FIELD(slider), NNH_DATA_FIELD(toggle), NNH_DATA_FIELD(choice), NNH_DATA_FIELD(list),
                NNH_DATA_FIELD(progress));
NNH_DATA_STRUCT(nnh::UiCursorStyle, NNH_DATA_FIELD(image), NNH_DATA_FIELD(anchor), NNH_DATA_FIELD(x), NNH_DATA_FIELD(y),
                NNH_DATA_FIELD(outline), NNH_DATA_FIELD(outline_color), NNH_DATA_FIELD(move_frames),
                NNH_DATA_FIELD(ease), NNH_DATA_FIELD(pulse_frames), NNH_DATA_FIELD(pulse_opacity),
                NNH_DATA_FIELD(pulse_x));
/// A game's theme file type (the documented pattern: the game's own type, converted to a UiTheme by hand).
NNH_DATA_TYPE(nnh::UiTheme, "ui_theme", NNH_DATA_FIELD(label), NNH_DATA_FIELD(image), NNH_DATA_FIELD(progress),
              NNH_DATA_FIELD(panel), NNH_DATA_FIELD(button), NNH_DATA_FIELD(toggle), NNH_DATA_FIELD(slider),
              NNH_DATA_FIELD(choice), NNH_DATA_FIELD(list), NNH_DATA_FIELD(grid), NNH_DATA_FIELD(scroll_box),
              NNH_DATA_FIELD(cursor));
