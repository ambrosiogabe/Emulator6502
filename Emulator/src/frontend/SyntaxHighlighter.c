#include "frontend/SyntaxHighlighter.h"

#include <stb/stb_ds.h>

// Declare the `tree_sitter_asm6502` function, which is
// implemented by the `tree-sitter-asm6502` library.
const TSLanguage* tree_sitter_asm6502(void);

static emu_CodeTheme themes[emu_CodeTheme_Type_Length];
static TSParser* parser = NULL;

void emu_SyntaxHighlighter_init()
{
	// Create a parser.
	parser = ts_parser_new();

	// Set the parser's language
	ts_parser_set_language(parser, tree_sitter_asm6502());

	// Set up default themes
	{
		themes[emu_CodeTheme_Type_Dark] = (emu_CodeTheme){
			.defaultColor = ImGui_ColorConvertU32ToFloat4(IM_COL32_BLACK),
			.styles = NULL,
		};

		emu_CodeTheme* darkTheme = &themes[emu_CodeTheme_Type_Dark];
		stbds_shput(darkTheme->styles, "@function.builtin", ImGui_ColorConvertU32ToFloat4(0x79c0ff));
		stbds_shput(darkTheme->styles, "@constant.numeric", ImGui_ColorConvertU32ToFloat4(0x79c0ff));
		stbds_shput(darkTheme->styles, "@constant.builtin", ImGui_ColorConvertU32ToFloat4(0x79c0ff));
		stbds_shput(darkTheme->styles, "@operator", ImGui_ColorConvertU32ToFloat4(0xa5d6ff));
		stbds_shput(darkTheme->styles, "@variable", ImGui_ColorConvertU32ToFloat4(0xc9d1d9));
		stbds_shput(darkTheme->styles, "@string", ImGui_ColorConvertU32ToFloat4(0xa5d6ff));
		stbds_shput(darkTheme->styles, "@constant", ImGui_ColorConvertU32ToFloat4(0x79c0ff));
		stbds_shput(darkTheme->styles, "@error", ImGui_ColorConvertU32ToFloat4(0xf85149));
		stbds_shput(darkTheme->styles, "@comment", ImGui_ColorConvertU32ToFloat4(0x8b949e));
	}

	{
		themes[emu_CodeTheme_Type_Light] = (emu_CodeTheme){
			.defaultColor = ImGui_ColorConvertU32ToFloat4(IM_COL32_BLACK),
			.styles = NULL,
		};

		emu_CodeTheme* lightTheme = &themes[emu_CodeTheme_Type_Light];
		stbds_shput(lightTheme->styles, "@function.builtin", ImGui_ColorConvertU32ToFloat4(0x0550ae));
		stbds_shput(lightTheme->styles, "@constant.numeric", ImGui_ColorConvertU32ToFloat4(0x0550ae));
		stbds_shput(lightTheme->styles, "@constant.builtin", ImGui_ColorConvertU32ToFloat4(0x0550ae));
		stbds_shput(lightTheme->styles, "@operator", ImGui_ColorConvertU32ToFloat4(0x0a3069));
		stbds_shput(lightTheme->styles, "@variable", ImGui_ColorConvertU32ToFloat4(0x24292f));
		stbds_shput(lightTheme->styles, "@string", ImGui_ColorConvertU32ToFloat4(0x0a3069));
		stbds_shput(lightTheme->styles, "@constant", ImGui_ColorConvertU32ToFloat4(0x0550ae));
		stbds_shput(lightTheme->styles, "@error", ImGui_ColorConvertU32ToFloat4(0xcf222e));
		stbds_shput(lightTheme->styles, "@comment", ImGui_ColorConvertU32ToFloat4(0x57606a));
	}

	{
		themes[emu_CodeTheme_Type_CatpuccinMocha] = (emu_CodeTheme){
			.defaultColor = ImGui_ColorConvertU32ToFloat4(IM_COL32_BLACK),
			.styles = NULL,
		};

		emu_CodeTheme* mochaTheme = &themes[emu_CodeTheme_Type_CatpuccinMocha];
		stbds_shput(mochaTheme->styles, "@function.builtin", ImGui_ColorConvertU32ToFloat4(0xcba6f7));
		stbds_shput(mochaTheme->styles, "@constant.numeric", ImGui_ColorConvertU32ToFloat4(0xfab387));
		stbds_shput(mochaTheme->styles, "@constant.builtin", ImGui_ColorConvertU32ToFloat4(0xfab387));
		stbds_shput(mochaTheme->styles, "@operator", ImGui_ColorConvertU32ToFloat4(0x89dceb));
		stbds_shput(mochaTheme->styles, "@variable", ImGui_ColorConvertU32ToFloat4(0xcdd6f4));
		stbds_shput(mochaTheme->styles, "@string", ImGui_ColorConvertU32ToFloat4(0xa6e3a1));
		stbds_shput(mochaTheme->styles, "@constant", ImGui_ColorConvertU32ToFloat4(0xfab387));
		stbds_shput(mochaTheme->styles, "@error", ImGui_ColorConvertU32ToFloat4(0xf38ba8));
		stbds_shput(mochaTheme->styles, "@comment", ImGui_ColorConvertU32ToFloat4(0x9399b2));
	}
}

void emu_SyntaxHighlighter_free()
{
	ts_parser_delete(parser);

	for (int i = 0; i < emu_CodeTheme_Type_Length; i++)
	{
		stbds_shfree(themes[i].styles);
	}
}

emu_CodeTheme const* const emu_SyntaxHighlighter_getTheme(emu_CodeTheme_Type type)
{
	return &themes[type];
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

void emu_SyntaxHighlighter_freeSource(TSTree* tree)
{
	ts_tree_delete(tree);
}