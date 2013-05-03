/* ************************************************************* *\
 * theme_symbols.h                                               *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Structures for symbol parsing.                   *
\* ************************************************************* */
 

typedef struct
{
	char string[11];
	int  value;
} theme_symbol_t;


theme_symbol_t operators[] =
{
	{ "over"       , CAIRO_OPERATOR_OVER        },
	{ "dest_over"  , CAIRO_OPERATOR_DEST_OVER   },
	{ "source"     , CAIRO_OPERATOR_SOURCE      },
	{ "xor"        , CAIRO_OPERATOR_XOR         },
	{ "add"        , CAIRO_OPERATOR_ADD         },
	{ "difference" , CAIRO_OPERATOR_DIFFERENCE  },
	{ "lighten"    , CAIRO_OPERATOR_LIGHTEN     },
	{ "darken"     , CAIRO_OPERATOR_DARKEN      },
	{ "color_dodge", CAIRO_OPERATOR_COLOR_DODGE },
	{ "color_burn" , CAIRO_OPERATOR_COLOR_BURN  },
	{ {0}          , 0                          }
};

theme_symbol_t border_types[] =
{
	{ "solid"   , BORDER_TYPE_SOLID    },
	{ "topleft" , BORDER_TYPE_TOPLEFT  },
	{ "topright", BORDER_TYPE_TOPRIGHT },
	{ {0}       , 0                    }
};

theme_symbol_t orientations[] =
{
	{ "left-right", ORIENT_LEFTRIGHT },
	{ "right-left", ORIENT_RIGHTLEFT },
	{ "bottom-top", ORIENT_BOTTOMTOP },
	{ "top-bottom", ORIENT_TOPBOTTOM },
	{ {0}         , 0                }
};

theme_symbol_t fill_rules[] =
{
	{ "empty", FILL_EMPTY_RELATIVE },
	{ "full" , FILL_FULL_RELATIVE  },
	{ {0}    , 0                   }
};
