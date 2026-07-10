#ifndef __WDIALOG_H_
#define __WDIALOG_H_
//
namespace NWorld
{
//
class CWorld;
class CUICmdPlayDialog;
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdPlayDialog* MakePlayDialogCommand( CWorld *pWorld, int nDialogID, bool bHeroOnLeft = true ); // build (not queue) -- for DialogPlay's wait id
void PlayDialog( CWorld *pWorld, int nDialogID, bool bHeroOnLeft = true );
void PlayDialogAsAcks( CWorld *pWorld, int nDialogID ); // the dialog is played back through acks
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __WDIALOG_H_