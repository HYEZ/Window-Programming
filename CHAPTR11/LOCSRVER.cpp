/* Chapter 11. LocSrver.c */

/* Find a server by reading the mailslot that is used to broadcast server names. */
/* This function illustrates a mailslot client. */

#include "Everything.h"
		/* The mailslot name is defined in "ClientServer.h". */
#include "ClientServer.h"

BOOL LocateServer (char *pPipeName, DWORD size)
{
	/* pPipeName has space for "size" characters */
	HANDLE hMailSlot;
	MS_MESSAGE serverMsg;
	BOOL found = FALSE;
	DWORD cbRead;

	hMailSlot = CreateMailslotA(MS_SRVNAME, 0, CS_TIMEOUT, NULL);
	if (hMailSlot == INVALID_HANDLE_VALUE)
		ReportError ("MS create error.", 11, TRUE);

	/* Communicate with the server to be certain that it is running.
		The server must have time to find the mailslot and send the pipe name. */
	
	while (!found) {
		printf ("Looking for a server.\n");
		found = ReadFile (hMailSlot, &serverMsg, sizeof(serverMsg), &cbRead, NULL);
	}

	if (found) printf ("Server has been located. Pipe Name: %s.\n", serverMsg.msName);
	else printf ("Server NOT located.\n");

	/* Close the mailslot. */

	CloseHandle (hMailSlot);
	if (found) {
		pPipeName[MAX_PATH] = '\0';
		/* Fail if the pipe name does not fit in the buffer provided */
		found = (strncpy_s (pPipeName, size, serverMsg.msName, size) == 0);
	}
	return found;
}
