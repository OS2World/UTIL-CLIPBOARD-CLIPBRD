#define USE_OS2_TOOLKIT_HEADERS
#define INCL_DOSERRORS
#define INCL_DOSMEMMGR
#define INCL_DOSPROCESS
#define INCL_DOSQUEUES
#define INCL_DOSSESMGR
#define INCL_WINCLIPBOARD
#include <os2.h>

#include <assert.h>
#include <getopt.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

#ifdef __GNUC__
#define NEVER_RETURNS void volatile
#else
#define NEVER_RETURNS void
#endif

#define ECHODEF         FALSE
#define OPTSTR          "?acehprwx"
#define PAGESIZE        4096
#define PIPESIZE        (sizeof (ULONG))
#define MAX_BUFSIZE     (64 * 1024 * 1024)
#define EOL             ((char) 26)


typedef enum
        {
          ACTION_NONE,
          ACTION_APPEND,
          ACTION_CLEAR,
          ACTION_EXECUTE,
          ACTION_PREPEND,
          ACTION_READ,
          ACTION_WRITE
        }
        ACTION;

typedef struct
        {
          USHORT SessionID;
          USHORT ResultCode;
        }
        TERMINFO;
typedef TERMINFO *PTERMINFO;


const CHAR szPMClipboardServer [] = "pmclip.exe";
const CHAR szQueueName [] = "\\QUEUES\\SG\\CLIPBOARD";

ACTION action        = ACTION_NONE;

BOOL   fEcho         = ECHODEF,
       fClipbrdOpen  = FALSE,
       fDataSet      = FALSE,
       fStdinRedir,
       fStdoutRedir;


char   *myname       = NULL;

HAB    hab           = NULLHANDLE;

HFILE  hfRead        = NULLHANDLE,
       hfWrite       = NULLHANDLE;

PVOID  pBuf          = NULL,
       pShareBuf     = NULL,
       pClipbrdBuf   = NULL;

ULONG  ulBufSize     = 0,
       ulClipbrdSize;


NEVER_RETURNS panic (const char *format, ...)
{
  va_list arg_ptr;

  fprintf (stderr, "%s: ", myname);
  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
  putc ('\n', stderr);
  exit (EXIT_FAILURE);
}

void allocate_buf (void)
{
  APIRET rc;

  rc = DosAllocMem (&pBuf, MAX_BUFSIZE, PAG_READ | PAG_WRITE);
  if (rc != NO_ERROR)
    panic ("error %lu on DosAllocMem", (unsigned long) rc);
}

void allocate_sharebuf (ULONG ulSize)
{
  APIRET rc;

  assert (ulBufSize > 0);

  rc = DosAllocSharedMem (&pShareBuf,
                          NULL,
                          ulSize,
                          PAG_COMMIT | OBJ_GIVEABLE |
                          PAG_READ   | PAG_WRITE    );
  if (rc != NO_ERROR)
    panic ("error %lu on DosAllocSharedMem", (unsigned long) rc);

}

void commit_buf (PVOID pBuf, ULONG ulBufSize)
{
  APIRET rc;

  rc = DosSetMem (pBuf,
                  ulBufSize,
                  PAG_COMMIT  |
                  PAG_DEFAULT);
  if (rc != NO_ERROR)
    panic ("error %lu on DosSetMem", (unsigned long) rc);
}

void read_buf (void)
{
  APIRET rc;

  BOOL   fEol          = FALSE;

  PVOID  pReadBuf      = pBuf;

  ULONG  ulCommitSize  = PAGESIZE,
         ulReadBufSize,
         ulActual;

  assert (pBuf != NULL);

  commit_buf (pReadBuf, ulCommitSize);
  do
  {
    ulReadBufSize = ulCommitSize;

    do
    {
      rc = DosRead (fileno (stdin),
                    pReadBuf,
                    fStdinRedir ? ulReadBufSize : 1,
                    &ulActual);
      if (rc != NO_ERROR)
        panic ("error %lu on DosRead", (unsigned long) rc);
      if (!fStdinRedir)
        fEol = (*((char *) pReadBuf) == EOL);

      pReadBuf += ulActual;
      ulBufSize += ulActual;
      ulReadBufSize -= ulActual;
    } while (ulReadBufSize > 0 && ulActual > 0 && !fEol);

    if (ulReadBufSize == 0)
    {
      ulCommitSize <<= 1;
      commit_buf (pReadBuf, ulCommitSize);
    }
  } while (ulActual > 0 && !fEol);

  if (fEol)
  {
    assert (ulBufSize > 0);
    ulBufSize--;   /* don't want trailing Ctrl-Z */
  }

  if (ulBufSize > 0)
  {
    if (fEcho)
    {
      rc = DosWrite (fileno (stdout),
                     pBuf,
                     ulBufSize - 1,   /* excluding end-zero */
                     &ulActual);
      if (rc != NO_ERROR)
        panic ("error %lu on DosWrite", (unsigned long) rc);
      assert (ulActual == ulBufSize - 1);
    }

    pReadBuf = pBuf + ulBufSize - 1;
    if (*((char *) pReadBuf) != '\0')
    {
      *((char *) pReadBuf + 1) = '\0';
      ulBufSize++;
    }
  }
}

void open_clipbrd ()
{
  if (!fClipbrdOpen)
  {
    fClipbrdOpen = WinOpenClipbrd (hab);
    if (!fClipbrdOpen)
      panic ("error on WinOpenClipbrd");
  }

  assert (fClipbrdOpen);
}

void set_clipbrd_data (void)
{
  assert (ulBufSize > 0);

  open_clipbrd ();
  fDataSet = WinSetClipbrdData (hab, (ULONG) pShareBuf, CF_TEXT, CFI_POINTER);
  if (!fDataSet)
    panic ("error on WinSetClipbrdData");
}

void get_clipbrd_data (void)
{
  APIRET      rc;

  BYTE        Priority;

  CHAR        szArg [16];

  HQUEUE      hqQueue;

  PID         pid;

  PTERMINFO   pTerminationInfo;

  REQUESTDATA Request;

  STARTDATA   StartData;

  ULONG       ulFormat = 0,   /* WinQueryClipbrdFmtInfo seems to return an */
                              /* USHORT, so we'd better initialize it to 0 */
              ulSessionID,
              ulActual;

  if (WinQueryClipbrdFmtInfo (hab, CF_TEXT, &ulFormat))
  {
    assert (ulFormat == CFI_POINTER);

    rc = DosCreatePipe (&hfRead, &hfWrite, PIPESIZE);
    if (rc != NO_ERROR)
      panic ("error %lu on DosCreatePipe");

    rc = DosCreateQueue (&hqQueue, QUE_FIFO | QUE_CONVERT_ADDRESS, szQueueName);
    if (rc != NO_ERROR)
      panic ("error %lu on DosCreateQueue (%s)", rc, szQueueName);

    rc = DosAllocSharedMem (&pClipbrdBuf,
                            NULL,
                            MAX_BUFSIZE,
                            OBJ_GETTABLE |
                            PAG_READ     | PAG_WRITE);
    if (rc != NO_ERROR)
      panic ("error %lu on DosAllocSharedMem", (unsigned long) rc);

    StartData.Length =      32;
    StartData.Related =     SSF_RELATED_CHILD;
    StartData.FgBg =        SSF_FGBG_BACK;
    StartData.TraceOpt =    SSF_TRACEOPT_NONE;
    StartData.PgmTitle =    NULL;
    StartData.PgmName =     szPMClipboardServer;
    sprintf (szArg, ARG_FORMAT, LO (pClipbrdBuf), HI (pClipbrdBuf),
                                LO (hfWrite),     HI (hfWrite));
    StartData.PgmInputs =   szArg;
    StartData.TermQ =       szQueueName;
    StartData.Environment = NULL;
    StartData.InheritOpt =  SSF_INHERTOPT_PARENT;
    StartData.SessionType = SSF_TYPE_PM;
    assert (!fClipbrdOpen);
    rc = DosStartSession (&StartData, &ulSessionID, &pid);
    if (rc != NO_ERROR)
      panic ("%s: error %lu on DosStartSession", szPMClipboardServer, rc);

    rc = DosReadQueue (hqQueue,
                       &Request,
                       &ulActual,
                       (PPVOID) &pTerminationInfo,
                       0,
                       DCWW_WAIT,
                       &Priority,
                       NULLHANDLE);
    if (rc != NO_ERROR)
      panic ("error %lu on DosReadQueue", rc);
    assert (Request.ulData == 0 &&
            pTerminationInfo != NULL &&
            pTerminationInfo->SessionID == ulSessionID);

    if (pTerminationInfo->ResultCode != EXIT_SUCCESS)
    {
      assert (pTerminationInfo->ResultCode == EXIT_FAILURE);
      DosFreeMem (pTerminationInfo);
      panic ("%s: an error occured", szPMClipboardServer);
    }
    DosFreeMem (pTerminationInfo);

    rc = DosRead (hfRead, &ulClipbrdSize, sizeof (ulClipbrdSize), &ulActual);
    if (rc != NO_ERROR)
      panic ("error %lu on DosRead from pipe", rc);
    assert (ulActual == sizeof (ulClipbrdSize));
  }
  else
    panic ("no text in clipboard");
}

int clipbrd_append (void)
{
  allocate_buf ();
  read_buf ();
  if (ulBufSize > 0)
  {
    get_clipbrd_data ();
    assert (ulClipbrdSize > 0);

    allocate_sharebuf (ulClipbrdSize + ulBufSize - 1);
    memcpy (pShareBuf, pClipbrdBuf, ulClipbrdSize);
    assert (*((char *) pShareBuf + ulClipbrdSize - 1) == '\0');
    memcpy (pShareBuf + ulClipbrdSize - 1, pBuf, ulBufSize);

    set_clipbrd_data ();
  }

  return EXIT_SUCCESS;
}

int clipbrd_clear (void)
{
  open_clipbrd ();
  if (!WinEmptyClipbrd (hab))
    panic ("error on WinClearClipbrd");

  return EXIT_SUCCESS;
}

int clipbrd_prepend (void)
{
  allocate_buf ();
  read_buf ();
  if (ulBufSize > 0)
  {
    get_clipbrd_data ();
    assert (ulClipbrdSize > 0);

    allocate_sharebuf (ulBufSize + ulClipbrdSize - 1);
    memcpy (pShareBuf, pBuf, ulBufSize);
    assert (*((char *) pShareBuf + ulBufSize - 1) == '\0');
    memcpy (pShareBuf + ulBufSize - 1, pClipbrdBuf, ulClipbrdSize);

    set_clipbrd_data ();
  }

  return EXIT_SUCCESS;
}

int clipbrd_read (void)
{
  APIRET rc;

  ULONG  ulActual;

  get_clipbrd_data ();

  rc = DosWrite (fileno (stdout),
                 pClipbrdBuf,
                 ulClipbrdSize - 1,   /* excluding end-zero */
                 &ulActual);
  if (rc != NO_ERROR)
    panic ("error %lu on DosWrite", (unsigned long) rc);
  assert (ulActual == ulClipbrdSize - 1);

  return EXIT_SUCCESS;
}

int clipbrd_execute (void)
{
  if (fEcho)
    clipbrd_read ();
  else
    get_clipbrd_data ();
  return system ((char *) pClipbrdBuf);
}

int clipbrd_write (void)
{
  allocate_buf ();
  read_buf ();
  if (ulBufSize > 0)
  {
    allocate_sharebuf (ulBufSize);
    memcpy (pShareBuf, pBuf, ulBufSize);
    set_clipbrd_data ();
  }
  else
    clipbrd_clear ();

  return EXIT_SUCCESS;
}

VOID APIENTRY exitfunc (VOID)
{
  if (pShareBuf != NULL && !fDataSet)
    DosFreeMem (pShareBuf);
  if (pBuf != NULL)
    DosFreeMem (pBuf);
  if (pClipbrdBuf != NULL)
    DosFreeMem (pClipbrdBuf);
  if (hfWrite != NULLHANDLE)
    DosClose (hfWrite);
  if (hfRead != NULLHANDLE)
    DosClose (hfRead);
  if (fClipbrdOpen)
  {
    assert (hab != NULLHANDLE);
    WinCloseClipbrd (hab);
  }
  if (hab != NULLHANDLE)
    WinTerminate (hab);

  DosExitList (EXLST_EXIT, (PFNEXITLIST) exitfunc);
}

NEVER_RETURNS usage (void)
{
  printf ("Usage: %s [-<action>] [-e]\n\n"                                \
          "actions:\n"                                                    \
          "  -c  clear clipboard\n"                                       \
          "  -r  copy clipboard to stdout\n"                              \
          "  -w  copy stdin to clipboard\n"                               \
          "  -a  append stdin to clipboard\n"                             \
          "  -p  prepend stdin to clipboard\n"                            \
          "  -x  execute content of clipboard via cmd.exe\n"              \
          "if no action is specified, then if only stdin is redirected, " \
          "-w is supposed,\n"                                             \
          "else if only stdout is redirected, -r is supposed;\n"          \
          "if both are redirected, -we is supposed.\n\n"                  \
          "  -e  echo stdin to stdout (with -w, -a, -p or -x)\n", myname);
  exit (1);
}

void guess_action (void)
{
  assert (action == ACTION_NONE);

  if (fStdinRedir && fStdoutRedir)
  {
    action = ACTION_WRITE;
    fEcho = TRUE;
  }
  else if (fStdinRedir)
    action = ACTION_WRITE;
  else if (fStdoutRedir)
    action = ACTION_READ;
}

void set_action (ACTION a)
{
  if (action != ACTION_NONE)
    usage ();
  action = a;
}


int main (int argc, char *argv [])
{
  int opt;

  myname       = argv [0];
  fStdinRedir  = !isatty (fileno (stdin));
  fStdoutRedir = !isatty (fileno (stdout));

  do
  {
    opt = getopt (argc, argv, OPTSTR);
    switch (opt)
    {
      case 'a' :

        set_action (ACTION_APPEND);
        break;

      case 'c' :

        set_action (ACTION_CLEAR);
        break;

      case 'e' :

        fEcho = !ECHODEF;
        break;

      case 'h' :

        opt = '?';
        break;

      case 'p' :

        set_action (ACTION_PREPEND);
        break;

      case 'r' :

        set_action (ACTION_READ);
        break;

      case 'w' :

        set_action (ACTION_WRITE);
        break;

      case 'x' :

        set_action (ACTION_EXECUTE);
        break;

      case '?' :
      case EOF :

        break;

      default :

        assert (FALSE);
    }
  } while (opt != '?' && opt != EOF);

  if (action == ACTION_NONE)
    guess_action ();

  if (action == ACTION_NONE || opt == '?')
    usage ();
  else
  {
    if (NO_ERROR != DosExitList (EXLST_ADD | ROUTINE_ORDER,
                                 (PFNEXITLIST) exitfunc))
      panic ("error on DosExitList");

    if (NULLHANDLE == (hab = WinInitialize (0)))
      panic ("error on WiniInitialize");

    switch (action)
    {
      case ACTION_APPEND :

        return clipbrd_append ();

      case ACTION_CLEAR :

        return clipbrd_clear ();

      case ACTION_EXECUTE :

        return clipbrd_execute ();

      case ACTION_PREPEND :

        return clipbrd_prepend ();

      case ACTION_READ :

        return clipbrd_read ();

      case ACTION_WRITE :

        return clipbrd_write ();

      default :

        assert (FALSE);
        return EXIT_FAILURE;
    }
  }
}
