#define USE_OS2_TOOLKIT_HEADERS
#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#define INCL_WINCLIPBOARD
#include <os2.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"


BOOL  fClipbrdOpen = FALSE;

HAB   hab          = NULLHANDLE;

HMQ   hmq          = NULLHANDLE;


VOID APIENTRY exitfunc (VOID)
{
  if (fClipbrdOpen)
  {
    assert (hab != NULLHANDLE);
    WinCloseClipbrd (hab);
  }
  if (hmq != NULLHANDLE)
    WinDestroyMsgQueue (hmq);
  if (hab != NULLHANDLE)
    WinTerminate (hab);

  DosExitList (EXLST_EXIT, (PFNEXITLIST) exitfunc);
}


int main (int argc, char *argv [])
{
  HFILE  hfWrite;

  int    lo1, hi1, lo2, hi2;

  PVOID  pClipbrdBuf, pShareBuf;

  ULONG  ulFormat = 0,   /* WinQueryClipbrdFmtInfo seems to return an USHORT, */
                         /* so we'd better initialize it to 0                 */
         ulClipbrdSize,
         ulActual;

  if (2 != argc                                                  ||
      4 != sscanf (argv [1], ARG_FORMAT, &lo1, &hi1, &lo2, &hi2) ||
      NO_ERROR != DosExitList (EXLST_ADD | ROUTINE_ORDER,
                               (PFNEXITLIST) exitfunc)           ||
      NULLHANDLE == (hab = WinInitialize (0))                    ||
      NULLHANDLE == (hmq = WinCreateMsgQueue (hab, 0))           ||
      !WinQueryClipbrdFmtInfo (hab, CF_TEXT, &ulFormat)          ||
      !(fClipbrdOpen = WinOpenClipbrd (hab)))
  {
    exit (EXIT_FAILURE);
  }
  assert (ulFormat == CFI_POINTER);
  pShareBuf = (PVOID) JOIN (lo1, hi1);
  hfWrite   = (HFILE) JOIN (lo2, hi2);

  pClipbrdBuf = (PVOID) WinQueryClipbrdData (hab, CF_TEXT);
  ulClipbrdSize = strlen (pClipbrdBuf) + 1;   /* including end-zero */

  if (NO_ERROR !=
      DosGetSharedMem (pShareBuf, PAG_READ | PAG_WRITE)             ||
      NO_ERROR !=
      DosSetMem (pShareBuf, ulClipbrdSize, PAG_COMMIT | PAG_DEFAULT))
  {
    exit (EXIT_FAILURE);
  }
  memcpy (pShareBuf, pClipbrdBuf, ulClipbrdSize);

  if (NO_ERROR !=
      DosWrite (hfWrite, &ulClipbrdSize, sizeof (ulClipbrdSize), &ulActual))
  {
    exit (EXIT_FAILURE);
  }
  assert (ulActual == sizeof (ulClipbrdSize));

  return EXIT_SUCCESS;
}
