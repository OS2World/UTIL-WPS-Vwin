/* REXX                                             */
/* Function to test 'VWIN' routine                  */
/* Type 'vwintest' from OS/2 window or              */
/*   OS/2 fullscreen session                        */
/* This function should minimize the command window */
/*   and restore it after approximately 10 sec.     */
/* Peter Flass. <Flass@LBDC.Senate.State.NY.US>     */
/* May, 1998                                        */

call RxFuncAdd 'SysSleep', 'RexxUtil', 'SysSleep'
'vwin min'
call SysSleep 10
'vwin res'
exit
