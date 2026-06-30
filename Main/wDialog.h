#ifndef __WDIALOG_H_
#define __WDIALOG_H_
//
namespace NWorld
{
//
class CWorld;
class CUICmdPlayDialog;
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdPlayDialog* MakePlayDialogCommand( CWorld *pWorld, int nDialogID ); // build (not queue) -- for DialogPlay's wait id
void PlayDialog( CWorld *pWorld, int nDialogID );
void PlayDialogAsAcks( CWorld *pWorld, int nDialogID ); // ������ ������������� ����� ack-�
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __WDIALOG_H_