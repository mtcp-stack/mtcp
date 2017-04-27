#include <cstdio>
#include <odp.h>
#include <odp/helper/linux.h>

int main(int argc ODP_UNUSED, const char *argv[] ODP_UNUSED)
{

	printf("\tODP API version: %s\n", odp_version_api_str());
	printf("\tODP implementation version: %s\n", odp_version_impl_str());

	return 0;
}
