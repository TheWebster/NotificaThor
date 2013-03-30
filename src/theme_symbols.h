typedef struct
{
	char string[11];
	int  value;
} theme_symbol_t;


theme_symbol_t operators[] =
{
	{ "over"   , CAIRO_OPERATOR_OVER   },
	{ "source" , CAIRO_OPERATOR_SOURCE },
	{ {0}      , 0                     }
};

theme_symbol_t border_types[] =
{
	{ "none"    , BORDER_TYPE_NONE     },
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
