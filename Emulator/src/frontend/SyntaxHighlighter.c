#include "frontend/SyntaxHighlighter.h"

#include <stb/stb_ds.h>

// Declare the `tree_sitter_asm6502` function, which is
// implemented by the `tree-sitter-asm6502` library.
const TSLanguage* tree_sitter_asm6502(void);

static emu_CodeTheme themes[emu_CodeTheme_Type_Length];
static TSParser* parser = NULL;

static emu_CodeTheme* configuredTheme = NULL;

/** 
* Converts Hex to ImVec4. Example 0xFFAABB -> ImVec4(R, G, B, A) where hex is
*     0xRRGGBB
* Alpha is assumed to be 1.0
*/
static ImVec4 hexToColor(uint32 hex)
{
	return (ImVec4)
	{
		.x = ((hex >> 16) & 0xFF) / 255.0f, // Red
		.y = ((hex >> 8) & 0xFF) / 255.0f, // Green
		.z = (hex & 0xFF) / 255.0f, // Blue
		.w = 1.0f // Alpha
	};
}

/**
* Converts Hex to ImVec4. Example 0xFFAABBCC -> ImVec4(R, G, B, A) where hex is
*     0xRRGGBBAA
*/
static ImVec4 hexToColorWithAlpha(uint32 hex)
{
	return (ImVec4)
	{
		.x = ((hex >> 24) & 0xFF) / 255.0f, // Red
			.y = ((hex >> 16) & 0xFF) / 255.0f, // Green
			.z = ((hex >> 8) & 0xFF) / 255.0f, // Blue
			.w = ((hex) & 0xFF) / 255.0f  // Alpha
	};
}

void emu_SyntaxHighlighter_init()
{
	// Create a parser.
	parser = ts_parser_new();

	// Set the parser's language
	ts_parser_set_language(parser, tree_sitter_asm6502());

	// Set up default themes
	{
		themes[emu_CodeTheme_Type_Dark] = (emu_CodeTheme){
			.defaultColor = hexToColor(0x8b949e),
			.bgColor = hexToColor(0x0d1117),
			.cursorColor = hexToColor(0xc9d1d9),
			.styles = NULL,
		};

		emu_CodeTheme* darkTheme = &themes[emu_CodeTheme_Type_Dark];
		stbds_shput(darkTheme->styles, "function.builtin", hexToColor(0x79c0ff));
		stbds_shput(darkTheme->styles, "constant.numeric", hexToColor(0x79c0ff));
		stbds_shput(darkTheme->styles, "constant.builtin", hexToColor(0x79c0ff));
		stbds_shput(darkTheme->styles, "operator", hexToColor(0xa5d6ff));
		stbds_shput(darkTheme->styles, "variable", hexToColor(0xc9d1d9));
		stbds_shput(darkTheme->styles, "string", hexToColor(0xa5d6ff));
		stbds_shput(darkTheme->styles, "constant", hexToColor(0x79c0ff));
		stbds_shput(darkTheme->styles, "error", hexToColor(0xf85149));
		stbds_shput(darkTheme->styles, "comment", hexToColor(0x8b949e));
	}

	{
		themes[emu_CodeTheme_Type_Light] = (emu_CodeTheme){
			.defaultColor = hexToColor(0x57606a),
			.bgColor = hexToColor(0xffffff),
			.cursorColor = hexToColor(0x24292f),
			.styles = NULL,
		};

		emu_CodeTheme* lightTheme = &themes[emu_CodeTheme_Type_Light];
		stbds_shput(lightTheme->styles, "function.builtin", hexToColor(0x0550ae));
		stbds_shput(lightTheme->styles, "constant.numeric", hexToColor(0x0550ae));
		stbds_shput(lightTheme->styles, "constant.builtin", hexToColor(0x0550ae));
		stbds_shput(lightTheme->styles, "operator", hexToColor(0x0a3069));
		stbds_shput(lightTheme->styles, "variable", hexToColor(0x24292f));
		stbds_shput(lightTheme->styles, "string", hexToColor(0x0a3069));
		stbds_shput(lightTheme->styles, "constant", hexToColor(0x0550ae));
		stbds_shput(lightTheme->styles, "error", hexToColor(0xcf222e));
		stbds_shput(lightTheme->styles, "comment", hexToColor(0x57606a));
	}

	{
		themes[emu_CodeTheme_Type_CatpuccinMocha] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xcdd6f4),
			.bgColor = hexToColor(0x1e1e2e),
			.cursorColor = hexToColor(0xcdd6f4),
			.styles = NULL,
		};

		emu_CodeTheme* mochaTheme = &themes[emu_CodeTheme_Type_CatpuccinMocha];
		stbds_shput(mochaTheme->styles, "function.builtin", hexToColor(0xcba6f7));
		stbds_shput(mochaTheme->styles, "constant.numeric", hexToColor(0xfab387));
		stbds_shput(mochaTheme->styles, "constant.builtin", hexToColor(0xfab387));
		stbds_shput(mochaTheme->styles, "operator", hexToColor(0x89dceb));
		stbds_shput(mochaTheme->styles, "variable", hexToColor(0xcdd6f4));
		stbds_shput(mochaTheme->styles, "string", hexToColor(0xa6e3a1));
		stbds_shput(mochaTheme->styles, "constant", hexToColor(0xfab387));
		stbds_shput(mochaTheme->styles, "error", hexToColor(0xf38ba8));
		stbds_shput(mochaTheme->styles, "comment", hexToColor(0x9399b2));
	}

	{
		themes[emu_CodeTheme_Type_AyuDark] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xbfbdb6),
			.bgColor = hexToColor(0x0f1419),
			.cursorColor = hexToColor(0xff8f40),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_AyuDark];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0xe6b450));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xd2a6ff));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0xd2a6ff));
		stbds_shput(theme->styles, "operator", hexToColor(0xff8f40));
		stbds_shput(theme->styles, "variable", hexToColor(0xbfbdb6));
		stbds_shput(theme->styles, "string", hexToColor(0xaad94c));
		stbds_shput(theme->styles, "constant", hexToColor(0xd2a6ff));
		stbds_shput(theme->styles, "error", hexToColor(0xf07178));
		stbds_shput(theme->styles, "comment", hexToColor(0x5c6773));
	}

	{
		themes[emu_CodeTheme_Type_Dracula] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xf8f8f2),
			.bgColor = hexToColor(0x282A36),
			.cursorColor = hexToColor(0xBD93F9),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_Dracula];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0x50fa7b));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xBD93F9));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0xBD93F9));
		stbds_shput(theme->styles, "operator", hexToColor(0xff79c6));
		stbds_shput(theme->styles, "variable", hexToColor(0xf8f8f2));
		stbds_shput(theme->styles, "string", hexToColor(0xf1fa8c));
		stbds_shput(theme->styles, "constant", hexToColor(0xBD93F9));
		stbds_shput(theme->styles, "error", hexToColor(0xff5555));
		stbds_shput(theme->styles, "comment", hexToColor(0x6272A4));
	}

	{
		themes[emu_CodeTheme_Type_EverforestDark] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xd3c6aa),
			.bgColor = hexToColor(0x2d353b),
			.cursorColor = hexToColor(0xd3c6aa),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_EverforestDark];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0xa7c080));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xd699b6));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0xd699b6));
		stbds_shput(theme->styles, "operator", hexToColor(0xe69875));
		stbds_shput(theme->styles, "variable", hexToColor(0xd3c6aa));
		stbds_shput(theme->styles, "string", hexToColor(0x83c092));
		stbds_shput(theme->styles, "constant", hexToColor(0xd699b6));
		stbds_shput(theme->styles, "error", hexToColor(0xe67e80));
		stbds_shput(theme->styles, "comment", hexToColor(0x859289));
	}

	{
		themes[emu_CodeTheme_Type_GruvboxMaterial] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xd4be98),
			.bgColor = hexToColor(0x282828),
			.cursorColor = hexToColor(0xd4be98),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_GruvboxMaterial];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0xa9b665));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xd3869b));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0xd3869b));
		stbds_shput(theme->styles, "operator", hexToColor(0xe78a4e));
		stbds_shput(theme->styles, "variable", hexToColor(0xd4be98));
		stbds_shput(theme->styles, "string", hexToColor(0x89b482));
		stbds_shput(theme->styles, "constant", hexToColor(0xd4be98));
		stbds_shput(theme->styles, "error", hexToColor(0xea6962));
		stbds_shput(theme->styles, "comment", hexToColor(0x928374));
	}

	{
		themes[emu_CodeTheme_Type_Gruvbox] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xebdbb2),
			.bgColor = hexToColor(0x282828),
			.cursorColor = hexToColor(0xbdae93),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_Gruvbox];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0xfabd2f));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xd3869b));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0xd3869b));
		stbds_shput(theme->styles, "operator", hexToColor(0xd3869b));
		stbds_shput(theme->styles, "variable", hexToColor(0xebdbb2));
		stbds_shput(theme->styles, "string", hexToColor(0xb8bb26));
		stbds_shput(theme->styles, "constant", hexToColor(0xd3869b));
		stbds_shput(theme->styles, "error", hexToColor(0xfb4934));
		stbds_shput(theme->styles, "comment", hexToColor(0x928374));
	}

	{
		themes[emu_CodeTheme_Type_Kanagawa] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xDCD7BA),
			.bgColor = hexToColor(0x1F1F28),
			.cursorColor = hexToColor(0xDCD7BA),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_Kanagawa];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0x7FB4CA));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xD27E99));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0xFFA066));
		stbds_shput(theme->styles, "constant", hexToColor(0xFFA066));
		stbds_shput(theme->styles, "operator", hexToColor(0xC0A36E));
		stbds_shput(theme->styles, "variable", hexToColor(0xDCD7BA));
		stbds_shput(theme->styles, "string", hexToColor(0x98BB6C));
		stbds_shput(theme->styles, "error", hexToColor(0xE82424));
		stbds_shput(theme->styles, "comment", hexToColor(0x727169));
	}

	{
		themes[emu_CodeTheme_Type_Nord] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xD8DEE9),
			.bgColor = hexToColor(0x2e3440),
			.cursorColor = hexToColor(0xD8DEE9),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_Nord];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0x8FBCBB));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xB48EAD));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0x81A1C1));
		stbds_shput(theme->styles, "constant", hexToColor(0xD8DEE9));
		stbds_shput(theme->styles, "operator", hexToColor(0x81A1C1));
		stbds_shput(theme->styles, "variable", hexToColor(0xD8DEE9));
		stbds_shput(theme->styles, "string", hexToColor(0xA3BE8C));
		stbds_shput(theme->styles, "error", hexToColor(0xBF616A));
		stbds_shput(theme->styles, "comment", hexToColor(0x616e88));
	}

	{
		themes[emu_CodeTheme_Type_OneDark] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xABB2BF),
			.bgColor = hexToColor(0x282C34),
			.cursorColor = hexToColor(0xABB2BF),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_OneDark];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0x61AFEF));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xD19A66));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0xD19A66));
		stbds_shput(theme->styles, "constant", hexToColor(0x56B6C2));
		stbds_shput(theme->styles, "operator", hexToColor(0xC678DD));
		stbds_shput(theme->styles, "variable", hexToColor(0xABB2BF));
		stbds_shput(theme->styles, "string", hexToColor(0x98C379));
		stbds_shput(theme->styles, "error", hexToColor(0xE06C75));
		stbds_shput(theme->styles, "comment", hexToColor(0x5C6370));
	}

	{
		themes[emu_CodeTheme_Type_RosePine] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xe0def4),
			.bgColor = hexToColor(0x191724),
			.cursorColor = hexToColor(0xe0def4),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_RosePine];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0xeb6f92));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xf6c177));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0xeb6f92));
		stbds_shput(theme->styles, "constant", hexToColor(0x9ccfd8));
		stbds_shput(theme->styles, "operator", hexToColor(0x908caa));
		stbds_shput(theme->styles, "variable", hexToColor(0xe0def4));
		stbds_shput(theme->styles, "string", hexToColor(0xf6c177));
		stbds_shput(theme->styles, "error", hexToColor(0xeb6f92));
		stbds_shput(theme->styles, "comment", hexToColor(0x6e6a86));
	}

	{
		themes[emu_CodeTheme_Type_SolarizedLight] = (emu_CodeTheme){
			.defaultColor = hexToColor(0x586e75),
			.bgColor = hexToColor(0xfdf6e3),
			.cursorColor = hexToColor(0x586e75),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_SolarizedLight];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0x268bd2));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0x2aa198));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0x2aa198));
		stbds_shput(theme->styles, "constant", hexToColor(0x2aa198));
		stbds_shput(theme->styles, "operator", hexToColor(0x859900));
		stbds_shput(theme->styles, "variable", hexToColor(0x586e75));
		stbds_shput(theme->styles, "string", hexToColor(0x2aa198));
		stbds_shput(theme->styles, "error", hexToColor(0xdc322f));
		stbds_shput(theme->styles, "comment", hexToColor(0x93a1a1));
	}

	{
		themes[emu_CodeTheme_Type_TokyoNight] = (emu_CodeTheme){
			.defaultColor = hexToColor(0xc0caf5),
			.bgColor = hexToColor(0x1a1b26),
			.cursorColor = hexToColor(0xc0caf5),
			.styles = NULL,
		};

		emu_CodeTheme* theme = &themes[emu_CodeTheme_Type_TokyoNight];
		stbds_shput(theme->styles, "function.builtin", hexToColor(0x2ac3de));
		stbds_shput(theme->styles, "constant.numeric", hexToColor(0xff9e64));
		stbds_shput(theme->styles, "constant.builtin", hexToColor(0x2ac3de));
		stbds_shput(theme->styles, "constant", hexToColor(0xff9e64));
		stbds_shput(theme->styles, "operator", hexToColor(0x89ddff));
		stbds_shput(theme->styles, "variable", hexToColor(0xc0caf5));
		stbds_shput(theme->styles, "string", hexToColor(0x9ece6a));
		stbds_shput(theme->styles, "error", hexToColor(0xdb4b4b));
		stbds_shput(theme->styles, "comment", hexToColor(0x565f89));
	}

	emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_CatpuccinMocha);
}

void emu_SyntaxHighlighter_free()
{
	ts_parser_delete(parser);

	for (int i = 0; i < emu_CodeTheme_Type_Length; i++)
	{
		stbds_shfree(themes[i].styles);
	}
}

emu_CodeTheme const* emu_SyntaxHighlighter_getTheme(emu_CodeTheme_Type type)
{
	return configuredTheme;
}

void emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type type)
{
	configuredTheme = &themes[type];
}

ImVec4 emu_SyntaxHighlighter_getColor(emu_CodeTheme const* const theme, const char* selector)
{
	if (stbds_shgeti(((emu_CodeTheme* const)theme)->styles, selector) >= 0)
	{
		return stbds_shget(((emu_CodeTheme* const)theme)->styles, selector);
	}

	return theme->defaultColor;
}

TSTree* emu_SyntaxHighlighter_highlightSource(const char* source, uint32 sourceLength)
{
	// Build a syntax tree based on source code stored in a string.
	return ts_parser_parse_string(
		parser,
		NULL,
		source,
		sourceLength
	);
}

TSTree* emu_SyntaxHighlighter_editHighlights(TSTree* oldTree, const char* source, uint32 sourceLength)
{
	// Build a syntax tree based on source code stored in a string.
	return ts_parser_parse_string(
		parser,
		oldTree,
		source,
		sourceLength
	);
}

void emu_SyntaxHighlighter_freeSource(TSTree* tree)
{
	ts_tree_delete(tree);
}