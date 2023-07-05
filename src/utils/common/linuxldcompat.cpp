#include "tier1/strtools.h"

void AwfulTerribleNoGoodHackToMakeDllLoadingWorkOnLinux(char* argv[]) {
	if ( !getenv("NO_LD_WRAP") )
	{
		char* pPath = getenv("LD_LIBRARY_PATH");
		char szBuffer[4096];
		char exe[ MAX_PATH ];
		char exePath[ MAX_PATH ];
		if ( !readlink( "/proc/self/exe", exe, sizeof(exe)) )
		{
			printf( "readlink failed\n" );
		}

		strncpy( exePath, exe, MAX_PATH-1 );
		V_StripFilename( exePath );

		snprintf( szBuffer, sizeof( szBuffer ) - 1, "LD_LIBRARY_PATH=%s:%s", exePath, pPath );

		char* const newEnv[] = { szBuffer, "NO_LD_WRAP=1", NULL };
		execve( exe, argv, newEnv );
	}
}
