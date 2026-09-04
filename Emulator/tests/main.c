#include "utest.h"
#include "Emulator/VendorImpls.h"

UTEST_STATE();

int main(int argc, const char* const argv[])
{
	g_logger_set_level(g_logger_level_None);
	return utest_main(argc, argv);
}