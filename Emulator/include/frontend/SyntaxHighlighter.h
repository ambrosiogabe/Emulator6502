#ifndef EMU_SYNTAX_HIGHLIGHTER_H
#define EMU_SYNTAX_HIGHLIGHTER_H
#include "utils/SafeVendor.h"
#include <tree_sitter/api.h>
#include <dcimgui.h>

typedef struct emu_CodeTheme_StyleMap
{
	char* key;
	ImVec4 value;
} emu_CodeTheme_StyleMap;

typedef struct emu_CodeTheme
{
	ImVec4 defaultColor;
	emu_CodeTheme_StyleMap* styles;
	
} emu_CodeTheme;

typedef enum emu_CodeTheme_Type
{
	emu_CodeTheme_Type_Dark,
	emu_CodeTheme_Type_Light,
	emu_CodeTheme_Type_CatpuccinMocha,
	emu_CodeTheme_Type_Length,
} emu_CodeTheme_Type;

void emu_SyntaxHighlighter_init();
void emu_SyntaxHighlighter_free();

emu_CodeTheme const* const emu_SyntaxHighlighter_getTheme(emu_CodeTheme_Type type);
ImVec4 emu_SyntaxHighlighter_getColor(emu_CodeTheme const* const theme, const char* selector);

TSTree* emu_SyntaxHighlighter_highlightSource(const char* source, uint32 sourceLength);
void emu_SyntaxHighlighter_freeSource(TSTree* tree);

#endif