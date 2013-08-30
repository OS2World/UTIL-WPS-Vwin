/***********************************************/
/*                                             */
/* VWIN: VIO Window manipulation               */
/*                                             */
/* Peter Flass <Flass@LBDC.Senate.State.NY.US> */
/*                                             */
/* Function: Minimize/restore OS/2 command     */
/*           window via keyboard command.      */
/*                                             */
/* Operation: * Get the parent process id of   */
/*              the current process (the OS/2  */
/*              command window).               */
/*            * Start a PM process and pass    */
/*              PID. Then in the PM process:   */
/*            * Retrieve the PM SwitchList     */
/*            * Find the VIO process in the    */
/*              switchlist; get its HWND.      */
/*            * Send the 'MINIMIZE' or 'RESTORE*/
/*              command to the VIO window.     */
/*                                             */
/* Requirements: Compiled witn EMX 0.9c and    */
/*               OS/2 Warp 4.                  */
/*                                             */
/* Note: A simpler version that doesn't need   */
/*       the auxiliary process will work in an */
/*       OS/2 window but not a fullscreen sess.*/
/*                                             */
/***********************************************/

#define INCL_DOSPROCESS
#define INCL_DOSSESMGR
#define INCL_WINSWITCHLIST
#define INCL_WINFRAMEMGR
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const UCHAR *pszCmds[] = { "MIN",       "RES",      "" };
const USHORT usMsgs[] = { SC_MINIMIZE, SC_RESTORE };

VOID viowin( INT argc, PSZ argv[], ULONG ulPpid, USHORT usMsg );
VOID fullscr( INT argc, PSZ argv[], ULONG ulPpid, USHORT usMsg );

main(INT argc, PSZ argv[]) {
   PTIB   ptib;
   PPIB   ppib;
   APIRET ulRc;
   UCHAR  szCmd[16];
   USHORT usMsg;
   INT    i; 

   /*--------------------------------*/
   /* Get process id and type        */
   /*--------------------------------*/
   ulRc = DosGetInfoBlocks(&ptib,&ppib);

   /*--------------------------------*/
   /* If PM process(3), retrieve PID */
   /* from argv and execute command  */
   /*--------------------------------*/
   if(ppib->pib_ultype==3) pm(argc,argv);

   /*--------------------------------*/
   /* Get the command                */
   /*--------------------------------*/
   memset(szCmd,'\0',sizeof(szCmd));
   if(argc<2) usage(argv[0]);
   strncpy(szCmd,argv[1],15);
   strupr(szCmd);
   for(i=0;*pszCmds[i]!='\0';i++) {
      if(strncmp(szCmd,pszCmds[i],strlen(pszCmds[i]))==0) break;  
      }
   if(*pszCmds[i]=='\0') usage(argv[0]);
   usMsg = usMsgs[i];

   /*--------------------------------*/
   /* If Fullscreen(0)               */
   /* Start PM session and pass      */
   /* ID of parent process           */
   /*--------------------------------*/
   if(ppib->pib_ultype==0) 
     fullscr(argc,argv,ppib->pib_ulppid,usMsg);

   /*--------------------------------*/
   /* If VIO(2), pass pid of         */
   /* parent process directly        */
   /*--------------------------------*/
   if(ppib->pib_ultype==2) 
     viowin(argc,argv,ppib->pib_ulppid,usMsg);

   /*--------------------------------*/
   /* Otherwise error                */
   /*--------------------------------*/
   printf("%s: This program must be run in an OS/2 session\n",argv[0]);
   exit(1);                  /* Error detatched process or DOS real-mode */
   }

/***********************************************/
/* VIO Window, start PM process                */
/*   Call window procedure directly            */
/***********************************************/
VOID viowin( INT argc, PSZ argv[], ULONG ulPpid, USHORT usMsg ) {
   UCHAR     szPpid[16];
   UCHAR     szMsg[16];
   VOID     *pParm[3];

   memset((PSZ)&szPpid,'\x0',sizeof szPpid);
   sprintf(szPpid,"%08X",ulPpid);
   memset((PSZ)&szMsg,'\x0',sizeof szMsg);
   sprintf(szMsg,"%04X",usMsg);
   pParm[0] = "";
   pParm[1] = (PVOID)&szPpid;
   pParm[2] = (PVOID)&szMsg;
   pm(3,&pParm);
   exit(0);
   }

/***********************************************/
/* Fullscreen session, start PM process        */
/*   and pass ppid                             */
/***********************************************/
VOID fullscr( INT argc, PSZ argv[], ULONG ulPpid, USHORT usMsg ) {
   UCHAR     ucPath[CCHMAXPATH+1];
   STARTDATA sd;
   APIRET    ulRc;
   PID       pidChildProc;
   ULONG     ulChildSess;
   UCHAR     szParm[24];

   memset((PSZ)&szParm,'\x0',sizeof szParm);
   sprintf(szParm,"%08X %04X",ulPpid,usMsg);

   /*--------------------------------*/
   /* Get my full path name          */
   /*--------------------------------*/
   memset((PSZ)&ucPath,'\x0',sizeof ucPath);
   _execname(ucPath,sizeof ucPath);

   /*--------------------------------*/
   /* Build 'STARTDATA' structrure   */
   /*--------------------------------*/
   memset((PSZ)&sd,'\x0',sizeof sd);
   sd.Length      = sizeof sd;
   sd.PgmName     = ucPath;
   sd.PgmInputs   = szParm;
   sd.SessionType = SSF_TYPE_PM;
   sd.PgmControl  = SSF_CONTROL_MINIMIZE|SSF_CONTROL_INVISIBLE;  

   /*--------------------------------*/
   /* Start PM session to send msg   */
   /*--------------------------------*/
   ulRc = DosStartSession( &sd, &ulChildSess, &pidChildProc );
   if(ulRc!=0)
      printf("%s: DosStartSession RC=%d\n",argv[0],ulRc);
   exit(0);
   }

/***********************************************/
/* PM process: retrive caller's ppid & command */
/*   and issue command                         */
/* ( *NO Message Queue* )                      */
/***********************************************/
pm( INT argc, PSZ argv[] ) {
   ULONG    ulPpid;
   USHORT   usCmd;
   HAB      habA;
   HWND     hwndVio;
   PSWBLOCK pswb;
   PSWCNTRL pswc;
   APIRET   ulRc;
   ULONG    ulItems;
   ULONG    ulBufSz;
   INT      i;

   /*--------------------------------*/
   /* ARGV(0)=Program path           */
   /* ARGV(1)=PPID                   */
   /*--------------------------------*/
   if(argc<3) return;

   /*--------------------------------*/
   /* Get PID of VIO process         */
   /*--------------------------------*/
   sscanf(argv[1],"%8x",&ulPpid);

   /*--------------------------------*/
   /* Get Command code               */
   /*--------------------------------*/
   sscanf(argv[2],"%4hx",&usCmd);

   /*--------------------------------*/
   /* Get copy of switch list        */
   /*--------------------------------*/
   habA = WinInitialize(0);
   ulItems = WinQuerySwitchList(habA,NULL,0); /* Get # items */
   ulBufSz = ulItems * sizeof(SWENTRY) + sizeof(HSWITCH);
   pswb = malloc(ulBufSz);
   ulItems = WinQuerySwitchList(habA,pswb,ulBufSz);
   if (ulItems==0) {
      exit(1);
      }

   /*--------------------------------*/
   /* Find VIO pid in switch list    */
   /*--------------------------------*/
   for(i=0;i<ulItems;i++) {
      if(pswb->aswentry[i].swctl.idProcess == ulPpid) break;
      } /* end for i */
   if(i>=ulItems) {    /* Not found */
      exit(1);
      }

   /*--------------------------------*/
   /* Send the command to the window */
   /*--------------------------------*/
   hwndVio = pswb->aswentry[i].swctl.hwnd; /* VIO Window handle */
   ulRc = WinPostMsg(
              hwndVio,
              WM_SYSCOMMAND,
              MPFROMSHORT(usCmd),
              MPFROM2SHORT(CMDSRC_OTHER,FALSE)
             );
   ulRc = WinTerminate(habA);
   free(pswb);
   }

usage( UCHAR *pgm ) {
   printf("Usage: %s MIN|RES\n",pgm);
   exit(1);
   }

