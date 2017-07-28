/*****************************************************************************
* File			: DboPackethandler_Lobby.cpp
* Author		: Hong sungbock
* Copyright		: (��)NTL
* Date			: 2007. 1. 16
* Abstract		: ���ӿ� �����Ͽ� ���� ���� ���Ա����� ��Ŷ �ڵ��� ������
*****************************************************************************
* Desc         : 
*****************************************************************************/

#include "precomp_dboclient.h"
#include "DboPacketHandler.h"

//framework
#include "NtlStage.h"

// table
#include "WorldTable.h"

// sound
#include "NtlSoundEventGenerator.h"

// presentation
#include "NtlPLVisualManager.h"
#include "NtlPLDojoContainer.h"

// simulation
#include "NtlNetSender.h"
#include "NtlSLEventFunc.h"
#include "NtlSLPacketGenerator.h"
#include "NtlWorldConceptTMQ.h"
#include "NtlWorldConceptTutorial.h"
#include "NtlWorldConceptTB.h"
#include "NtlCameraController.h"
#include "NtlStorageManager.h"
#include "NtlClientService.h"

// dbo
#include "StageManager.h"
#include "LogInStageState.h"
#include "CharStageState.h"
#include "GameStageState.h"
#include "MsgBoxManager.h"
#include "ChannelChangeGui.h"
#include "DialogManager.h"
#include "PetitionManager.h"
#include "LobbyManager.h"


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// character server packet hander class


void PacketHandler_LSCrateUserRes(void *pPacket)
{
	// event �߻�(login)
	CDboEventGenerator::LoginGuiEnable();

	sAU_LOGIN_CREATEUSER_RES *pCreateUserRes = (sAU_LOGIN_CREATEUSER_RES*)pPacket;
	if(pCreateUserRes->wResultCode == AUTH_SUCCESS)
	{
		GetAlarmManager()->AlarmMessage( DST_ACCOUNT_CREATE_SUCCESS );
	}
	else
	{
		GetAlarmManager()->AlarmMessage( pCreateUserRes->wResultCode, TRUE );
	}
}

void PacketHandler_LSLoginRes(void *pPacket)
{
	// event �߻�(login)
	CDboEventGenerator::LoginGuiEnable();

	sAU_LOGIN_RES *pLoginRes = (sAU_LOGIN_RES*)pPacket;
	if(pLoginRes->wResultCode == AUTH_SUCCESS)
	{
		NtlLogFilePrint("Connected");

		SConnectData *pConData = GetDboGlobal()->GetConnectData();  

		pConData->sCharCon.Init();
		memcpy(pConData->sCharCon.chAuthKey, pLoginRes->abyAuthKey, NTL_MAX_SIZE_AUTH_KEY);
		pConData->uiAccountId = pLoginRes->accountId;
		pConData->byLastServerFarmID = pLoginRes->lastServerFarmId;		

		// character server ip ����.
		pConData->ResetCharacterServer();
		for(RwInt32 i = 0; i < (RwInt32)pLoginRes->byServerInfoCount; i++)
		{
			pConData->AddCharacterServer(pLoginRes->aServerInfo[i].szCharacterServerIP, 
				pLoginRes->aServerInfo[i].wCharacterServerPortForClient,
				pLoginRes->aServerInfo[i].dwLoad);
		}


		// login server ����.
		CLoginPacketGenerator *pLoginPacketGenerator = GetDboGlobal()->GetLoginPacketGenerator(); 
		pLoginPacketGenerator->SendLogOutReq(pLoginRes->awchUserId, true); 


		eDBO_SERVICE_PUBLISHER eService = GetServiceInterface()->GetPublisherID();
		if( eDBO_SERVICE_PUBLISHER_CJI_KOR == eService )
		{
			SUserData*	pUserData	= GetDboGlobal()->GetUserData(); 

			memset( pUserData->wchUserID, 0, sizeof( pUserData->wchUserID ) );
			memcpy( pUserData->wchUserID, pLoginRes->awchUserId, sizeof( pLoginRes->awchUserId ) );

			Logic_LoadAccountOption();
			CDboEventGenerator::LoginEvent(LOGIN_EVENT_SUCCESS_LOGIN);
		}
		else
		{
			CNtlNetSender *pLoginNetSender = pLoginPacketGenerator->GetNetSender();
			pLoginNetSender->SetData(INVALID_HSERVER, NULL);

			GetLogInStageState()->ChangeState(LOGIN_STATE_CHAR_SERVER_CONNECT);
		}

		SUserData* pUserData = GetDboGlobal()->GetUserData(); 
		char chBuffer[128];
		::WideCharToMultiByte( GetACP(), 0, pUserData->wchUserID, -1, chBuffer, 128, NULL, NULL  );
		GetServiceInterface()->AttachSecuritySolutionUserID( chBuffer );
	}
	else
	{
		GetLogInStageState()->ChangeState(LOGIN_STATE_IDLE);
		GetAlarmManager()->AlarmMessage( pLoginRes->wResultCode, TRUE );

		if( pLoginRes->wResultCode == AUTH_USER_KICKOUT_WAIT_PLZ )
		{
			CDboEventGenerator::LoginEvent(LOGIN_EVENT_RETRY_LOGIN_REQ);
		}
	}	
}


void PacketHandler_LSLoginDisconnectRes(void *pPacket)
{
	SConnectData *pConnectData = GetDboGlobal()->GetConnectData();
	if(pConnectData->sAuthCon.byConnState == SConnectAtomic::SOCKET_STATE_CONNECT)
	{
		CNtlClientNet *pNet = GetDboGlobal()->GetNetwork(); 
		pConnectData->sAuthCon.byConnState = SConnectAtomic::SOCKET_STATE_DISCONNECT;
		pNet->Disconnect(pConnectData->sAuthCon.hSocket);
		pConnectData->sAuthCon.hSocket = INVALID_HSERVER;

		CDboEventGenerator::LoginEvent(LOGIN_EVENT_DISCONNECTED_AUTH_SERVER);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// character server packet hander class

//////////////////////////////////////////////////////////////////////////
// ĳ���� ������ �α����Ͽ���
void PacketHandler_CSLoginRes(void *pPacket)
{	
	sCU_LOGIN_RES *pLoginRes = (sCU_LOGIN_RES*)pPacket;

	CNtlStage *pStage = CStageManager::GetInstance()->GetActiveStage();
	std::string strStageName = pStage->GetName();

	if(pLoginRes->wResultCode != CHARACTER_SUCCESS)
	{
		if(strStageName == LOGIN_STAGE_NAME)
		{
			GetLogInStageState()->ChangeState(LOGIN_STATE_EXIT);
			GetAlarmManager()->AlarmMessage( pLoginRes->wResultCode, TRUE );
		}
		else
		{
			GetAlarmManager()->AlarmMessage( pLoginRes->wResultCode, TRUE );
			GetAlarmManager()->AlarmMessage( DST_CHAR_SERVER_CONNECT_FAIL );
		}

		return;
	}

	// ������ ������ �޶� ���������� ������ ���������� Ʋ�� �� �ִ�.
	// ������ �� �� �� �����Ѵ�

	SConnectData *pConData = GetDboGlobal()->GetConnectData();
	pConData->byLastServerFarmID = pLoginRes->lastServerFarmId;
	memcpy(pConData->m_acAllowFuncForDev, &pLoginRes->dwRaceAllowedFlag, sizeof(pLoginRes->dwRaceAllowedFlag));

	CDboEventGenerator::StageDelete(strStageName.c_str());
	CDboEventGenerator::StageCreate(CHARACTER_STAGE_NAME);
}

//////////////////////////////////////////////////////////////////////////
// ���� ���� ������ �޴´�
void PacketHandler_CSCharServerFarmInfo(void *pPacket)
{	
	sCU_SERVER_FARM_INFO* pResult = (sCU_SERVER_FARM_INFO*)pPacket;

	GetLobbyManager()->SetServerInfo( &(pResult->serverFarmInfo) );
}

//////////////////////////////////////////////////////////////////////////
// ���� ����Ʈ�� ��� �޾Ҵ�
void PacketHandler_CSCharServerListRes(void *pPacket)
{	
	sCU_CHARACTER_SERVERLIST_RES* pResult = (sCU_CHARACTER_SERVERLIST_RES*)pPacket;

	if(pResult->wResultCode != CHARACTER_SUCCESS)
	{
		GetAlarmManager()->AlarmMessage( pResult->wResultCode, TRUE );

		if( GetLobbyManager()->IsFistAcceptServer() )
		{
			GetCharStageState()->ChangeState(CHAR_STATE_IMMEDIATLY_RETURN_LOGIN_ENTER);
		}
		else
		{
			GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);
		}

		return;
	}		

	GetLobbyManager()->SetWaitServerInfo(FALSE);

	if(GetCharStageState()->IsState(CHAR_STATE_SELECT_IDLE))
	{
		GetCharStageState()->ReservateState(CHAR_STATE_SERVER_ENTER);
		GetCharStageState()->ChangeState(CHAR_STATE_SELECT_EXIT);
	}
	else
	{
		GetCharStageState()->ChangeState(CHAR_STATE_SERVER_ENTER);
	}

	// ���� ����Ʈ ���θ� �޾Ҵ�
	// ���� ������Ʈ �̺�Ʈ�� ������
	CDboEventGenerator::LobbyEvent(LMT_UPDATE_SERVER);
}

//////////////////////////////////////////////////////////////////////////
// ���� ���� �ϳ��� �޾Ҵ�
void PacketHandler_CSCharServerListOneRes(void * pPacket)
{	
	sCU_CHARACTER_SERVERLIST_ONE_RES* pResult = (sCU_CHARACTER_SERVERLIST_ONE_RES*)pPacket;

	if(pResult->wResultCode != CHARACTER_SUCCESS)
	{
		GetAlarmManager()->AlarmMessage( pResult->wResultCode, TRUE );

		if( GetLobbyManager()->IsFistAcceptServer() )
		{
			GetCharStageState()->ChangeState(CHAR_STATE_IMMEDIATLY_RETURN_LOGIN_ENTER);
		}
		else
		{
			GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);
		}

		return;
	}

	CDboEventGenerator::LobbyEvent(LMT_UPDATE_SERVER);

}

//////////////////////////////////////////////////////////////////////////
// ĳ���� ������ ä��(���� ����)������ �޴´�
void PacketHandler_CSCharServerChannelInfo(void *pPacket)
{
	sCU_SERVER_CHANNEL_INFO* pResult = (sCU_SERVER_CHANNEL_INFO*)pPacket;

	SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
	CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
	if( !pLobby )
	{
		DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
		return;
	}

	pLobby->SetChannelInfo(pResult->byCount, pResult->serverChannelInfo);
}


//////////////////////////////////////////////////////////////////////////
// �� ĳ���� ������ ���� ĳ���� ������ �޴´�
void PacketHandler_CSCharInfo(void *pPacket)
{
	sCU_CHARACTER_INFO* pResult = (sCU_CHARACTER_INFO*)pPacket;

	SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
	CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
	if( !pLobby )
	{
		DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
		return;
	}


	pLobby->SetCharacterInfo(pResult->byCount, pResult->sPcData);
	pLobby->SetDeletingCharcterInfo(pResult->asDelData);
}

//////////////////////////////////////////////////////////////////////////
// �� ĳ���� ������ ��� ĳ���� ������ �޾Ҵ�
void PacketHandler_CSCharLoadRes(void *pPacket)
{
	GetMsgBoxManager()->DeleteNetConnectBox();

	sCU_CHARACTER_LOAD_RES* pResult = (sCU_CHARACTER_LOAD_RES*)pPacket;

	if(pResult->wResultCode != CHARACTER_SUCCESS)
	{
		GetAlarmManager()->AlarmMessage( pResult->wResultCode, TRUE );

		if( GetLobbyManager()->IsFistAcceptServer() )
		{
			GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);
		}
		else
		{
			GetCharStageState()->ReservateState(CHAR_STATE_IMMEDIATLY_RETURN_LOGIN_ENTER);
			GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);
		}
	}	

	// ä�� ������Ʈ
	CDboEventGenerator::LobbyEvent(LMT_UPDATE_CHANNEL);

	// ĳ���� ���� UI ����
	CDboEventGenerator::LobbyEvent(LMT_UPDATE_CHARACTER_LIST);

	//if( GetLobbyManager()->GetCharacterCount_inSelectedServer() > 0 )
	//	GetLobbyManager()->SetSelectedCharIndex(0);

	GetLobbyManager()->SetWaitChannelInfo(FALSE);

	if(GetCharStageState()->IsState(CHAR_STATE_SELECT_INFOMATION_FROM_SERVER_LIST))
	{
		GetCharStageState()->ReservateState(CHAR_STATE_SELECT_ENTER);
		GetCharStageState()->ChangeState(CHAR_STATE_SERVER_EXIT);
	}
	else
		GetCharStageState()->ChangeState(CHAR_STATE_SELECT_ENTER);
}

void PacketHandler_CSCharAddRes(void *pPacket)
{
	sCU_CHARACTER_ADD_RES *pCharAddRes = (sCU_CHARACTER_ADD_RES*)pPacket;

	if(pCharAddRes->wResultCode != CHARACTER_SUCCESS)
	{
		GetCharStageState()->ChangeState(CHAR_STATE_MAKE_IDLE);
		GetAlarmManager()->AlarmMessage( pCharAddRes->wResultCode, TRUE );
		return;
	}

#ifdef dENTER_ONLY_NEW_CHARACTER
	listNewCharID.push_back(pCharAddRes->sPcDataSummary.charId);
#endif

	// ���� ���� ĳ���� ���� ����
	SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
	CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
	if( !pLobby )
	{
		DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
		return;
	}

	pLobby->AddCharacter(&pCharAddRes->sPcDataSummary);

	// ���� ���� ĳ���͸� �����Ѵ�
	pLobby->SetSelectedCharacterIndex( pLobby->GetCharacterCount() - 1 );

	// ĳ���� ���� UI ����
	CDboEventGenerator::LobbyEvent(LMT_UPDATE_CHARACTER_LIST);	

	GetCharStageState()->ChangeState(CHAR_STATE_MAKE_EXIT);
}

void PacketHandler_CSCharDelRes(void *pPacket)
{
	sCU_CHARACTER_DEL_RES *pCharDelRes = (sCU_CHARACTER_DEL_RES*)pPacket;

	GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);

	if(pCharDelRes->wResultCode != CHARACTER_SUCCESS)
	{	
		GetAlarmManager()->AlarmMessage( pCharDelRes->wResultCode, TRUE );
		return;
	}

	// ĳ���� ���� ���
	// ���� ���� ĳ���͸� �����Ѵ�
	SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
	CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
	if( !pLobby )
	{
		DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
		return;
	}

	pLobby->SetReserveDeleteCharacter(pCharDelRes->charId, ((RwReal)NTL_DELETE_CHAR_MAX_WAIT_TICK)/1000.f);

	// ĳ���Ͱ� �����ִٸ� ó�� ĳ���͸� �����Ѵ�
	if( pLobby->GetCharacterCount() > 0 )
	{
		pLobby->SetSelectedCharacterIndex( 0 );
		CDboEventGenerator::LobbyEvent(LMT_SELECT_CHARACTER, 0);
	}
	else
	{
		pLobby->SetSelectedCharacterIndex( INVALID_BYTE );
		CDboEventGenerator::LobbyEvent(LMT_SELECT_CHARACTER, INVALID_BYTE);
	}

	// ĳ���� ���� UI ����
	CDboEventGenerator::LobbyEvent(LMT_UPDATE_CHARACTER_LIST);	
}

void PacketHandler_CSCharConnectWaitCheckRes(void *pPacket)
{
	GetMsgBoxManager()->DeleteNetConnectBox();

	// ���� ������ �� �� �ִ� �ο� ���ѿ� �ɸ��� �ʾҴٸ�
	// ���� ������  ���� ��Ŷ���� �ְ� �ޱ� �����Ѵ�
	sCU_CONNECT_WAIT_CHECK_RES* pResult = (sCU_CONNECT_WAIT_CHECK_RES*)pPacket;

	if(pResult->wResultCode == CHARACTER_USER_SHOULD_WAIT_FOR_CONNECT)
	{
		// ���� ���� ���� ����ڰ� �ִ�. ���� CU_CONNECT_WAIT_COUNT_NFY ��Ŷ���� ��� ������ �˷��ش�
		GetCharStageState()->ChangeState(CHAR_STATE_WAIT_GAME_SERVER_CONNECT);
		return;
	}
	else if(pResult->wResultCode != CHARACTER_SUCCESS)
	{	
		GetAlarmManager()->AlarmMessage( pResult->wResultCode, TRUE );
		GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);
		return;
	}

	CDboEventGenerator::LobbyEvent(LMT_START_CONNECT_GAME_SERVER);
}

void PacketHandler_CSCharConnectWaitCancelRes(void *pPacket)
{
	GetMsgBoxManager()->DeleteNetConnectBox();
	GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);

	sCU_CONNECT_WAIT_CANCEL_RES* pResult = (sCU_CONNECT_WAIT_CANCEL_RES*)pPacket;

	if(pResult->wResultCode != CHARACTER_SUCCESS)
	{	
		GetAlarmManager()->AlarmMessage( pResult->wResultCode, TRUE );
		return;
	}
}

void PacketHandler_CSCharConnectWaitCountNfy(void *pPacket)
{
	sCU_CONNECT_WAIT_COUNT_NFY* pResult = (sCU_CONNECT_WAIT_COUNT_NFY*)pPacket;

	if( 0 == pResult->dwCountWaiting )
	{
		GetMsgBoxManager()->DeleteNetConnectBox();
		CDboEventGenerator::LobbyEvent(LMT_START_CONNECT_GAME_SERVER);
	}
	else
	{
		GetAlarmManager()->UpdateFormattedAlarmMessage(DST_LOBBY_WAIT_GAME_SERVER_CONNECT, FALSE, NULL, pResult->dwCountWaiting);
	}	
}

void PacketHandler_CSCharConnectWaitCancelNfy(void *pPacket)
{
	sCU_CONNECT_WAIT_CANCEL_NFY* pResult = (sCU_CONNECT_WAIT_CANCEL_NFY*)pPacket;

	switch(pResult->byServerType)
	{
	case NTL_SERVER_TYPE_CHARACTER:
		{
			GetMsgBoxManager()->DeleteNetConnectBox();
			GetAlarmManager()->AlarmMessage(DST_CHAR_SERVER_CONNECT_FAIL);
			break;
		}
	case NTL_SERVER_TYPE_GAME:
		{
			GetMsgBoxManager()->DeleteNetConnectBox();
			GetAlarmManager()->AlarmMessage(DST_CHECKING_GAME_SERVER);
			GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);
			break;
		}
	case NTL_SERVER_TYPE_COMMUNITY:
		{
			GetMsgBoxManager()->DeleteNetConnectBox();
			GetAlarmManager()->AlarmMessage(DST_CHECKING_COMMUNITY_SERVER);
			GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);
			break;
		}
	default:
		{
			DBO_FAIL("Not defined message");
		}
	}	
}

void PacketHandler_CSCharSelectRes(void *pPacket)
{
	GetMsgBoxManager()->DeleteNetConnectBox();

	sCU_CHARACTER_SELECT_RES *pCharSelectRes = (sCU_CHARACTER_SELECT_RES*)pPacket;
	if(pCharSelectRes->wResultCode == CHARACTER_SUCCESS)
	{
		// data setting
		SConnectData *pConnectData = GetDboGlobal()->GetConnectData(); 
		pConnectData->sGameCon.Init();

		memcpy(pConnectData->sGameCon.chAuthKey, pCharSelectRes->abyAuthKey, NTL_MAX_SIZE_AUTH_KEY);
		memcpy(pConnectData->sGameCon.chServerIP, pCharSelectRes->szGameServerIP, NTL_MAX_LENGTH_OF_IP + 1); 
		pConnectData->sGameCon.wServerPort = pCharSelectRes->wGameServerPortForClient;


		// game server ����.
		CNtlClientNet *pNet = GetDboGlobal()->GetNetwork(); 

		CGamePacketGenerator *pGamePacketGenerator = GetDboGlobal()->GetGamePacketGenerator(); 
		CNtlNetSender *pGameNetSender = pGamePacketGenerator->GetNetSender();

		CChatPacketGenerator *pChatPacketGenerator = GetDboGlobal()->GetChatPacketGenerator(); 
		CNtlNetSender *pChatNetSender = pChatPacketGenerator->GetNetSender();

		CNtlNetSender *pSLNetSender = API_GetSLPacketGenerator()->GetNetSender(); 

#ifdef NET_SERVER_SYNC

		RwBool bConnectSuccess = TRUE;

		if(!pNet->Connect((char*)pConnectData->sGameCon.chServerIP, pConnectData->sGameCon.wServerPort, &pConnectData->sGameCon.hSocket, false, NETCLIENT_ENCODE_NONE) )
			bConnectSuccess = FALSE;

		if(bConnectSuccess)
		{
			// socket connect state
			pConnectData->sGameCon.byConnState = SConnectAtomic::SOCKET_STATE_CONNECT;

			// game packet generator �� CNtlNetSender data setting.
			pGameNetSender->SetData(pConnectData->sGameCon.hSocket, pNet); 

			// simulation packet generator�� CNtlNetSender data setting
			pSLNetSender->SetData(pConnectData->sGameCon.hSocket, pNet);


			if( GetDboGlobal()->IsEnterTutorial() )
			{
				// Ʃ�丮�� ����
				GetCharStageState()->ReservateState(CHAR_STATE_READY_ENTER_TUTORIAL);
				GetCharStageState()->ChangeState(CHAR_STATE_SELECT_EXIT);
			}
			else
			{
				// ���� ����
				GetCharStageState()->ReservateState(CHAR_STATE_SUCCESS_ENTER_GAME);
				GetCharStageState()->ChangeState(CHAR_STATE_SELECT_EXIT);
			}
		}
		else
		{
			pGameNetSender->SetData(INVALID_HSERVER, NULL); 
			pChatNetSender->SetData(INVALID_HSERVER, NULL); 
			pSLNetSender->SetData(INVALID_HSERVER, NULL); 

			GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);
			GetAlarmManager()->AlarmMessage( DST_GAME_SERVER_CONNECT_FAIL );
		}
#else
		pGameNetSender->SetData(INVALID_HSERVER, NULL); 
		pChatNetSender->SetData(INVALID_HSERVER, NULL); 
		pGamePacketGenerator->SendGameEnterReq(pConnectData->uiAccountId, pCharSelectRes->charId, pConnectData->sGameCon.chAuthKey, false);
#endif

	}
	else
	{
		GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);
		GetAlarmManager()->AlarmMessage( pCharSelectRes->wResultCode, TRUE );
	}
}

void PacketHandler_CSCharExitRes(void *pPacket)
{
	SConnectData *pConnectData = GetDboGlobal()->GetConnectData();
	if(pConnectData->sCharCon.byConnState == SConnectAtomic::SOCKET_STATE_CONNECT)
	{
		CNtlClientNet *pNet = GetDboGlobal()->GetNetwork(); 
		pConnectData->sCharCon.byConnState = SConnectAtomic::SOCKET_STATE_DISCONNECT;
		pNet->Disconnect(pConnectData->sCharCon.hSocket);
		pConnectData->sCharCon.hSocket = INVALID_HSERVER;

		// Global ���� �ʱ�ȭ
		GetLobbyManager()->SetWaitServerInfo(TRUE);
		GetLobbyManager()->SetWaitChannelInfo(TRUE);	
	}
}

void PacketHandler_CSCharDelCancelRes(void *pPacket)
{
	sCU_CHARACTER_DEL_CANCEL_RES* pResult = (sCU_CHARACTER_DEL_CANCEL_RES*)pPacket;

	GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);

	if(pResult->wResultCode != CHARACTER_SUCCESS)
	{	
		GetAlarmManager()->AlarmMessage( pResult->wResultCode, TRUE );
		return;
	}


	SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
	CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
	if( !pLobby )
	{
		DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
		return;
	}

	pLobby->CancelDeleteCharacter(pResult->charId);

	CDboEventGenerator::LobbyEvent(LMT_UPDATE_CHARACTER_LIST);
}

void PacketHandler_CSCharDelNfy(void *pPacket)
{
	sCU_CHARACTER_DEL_NFY* pResult = (sCU_CHARACTER_DEL_NFY*)pPacket;

	SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
	CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
	if( !pLobby )
	{
		DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
		return;
	}


	RwUInt8	bySelectedCharacterIndex	= pLobby->GetSelectedCharacterIndex();
	RwUInt8	byDeletedCharIndex			= pLobby->RemoveCharacter(pResult->charId);

	if( byDeletedCharIndex == INVALID_BYTE )
		return;


	if( byDeletedCharIndex == bySelectedCharacterIndex )
	{
		if( pLobby->GetCharacterCount() > 0 )
		{
			pLobby->SetSelectedCharacterIndex(0);
		}
		else
		{
			pLobby->SetSelectedCharacterIndex(INVALID_BYTE);
		}
	}
	else if( byDeletedCharIndex < bySelectedCharacterIndex &&
			 INVALID_BYTE != bySelectedCharacterIndex )
	{
		pLobby->SetSelectedCharacterIndex(bySelectedCharacterIndex - 1);
	}

	CDboEventGenerator::LobbyEvent(LMT_UPDATE_CHARACTER_LIST);
	CDboEventGenerator::LobbyEvent(LMT_SELECT_CHARACTER, pLobby->GetSelectedCharacterIndex());
}

void PacketHandler_CSCharRenameRes(void *pPacket)
{
	GetMsgBoxManager()->DeleteNetConnectBox();
	GetCharStageState()->ChangeState(CHAR_STATE_SELECT_IDLE);

	sCU_CHARACTER_RENAME_RES* pResult = (sCU_CHARACTER_RENAME_RES*)pPacket;

	if(pResult->wResultCode != CHARACTER_SUCCESS)
	{	
		GetAlarmManager()->AlarmMessage( pResult->wResultCode, TRUE );
		return;
	}


	SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
	CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
	if( !pLobby )
	{
		DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
		return;
	}

	sLOBBY_CHARACTER* pLOBBY_CHARACTER = pLobby->GetCharacter(pResult->charId);
	if( !pLOBBY_CHARACTER )
	{
		DBO_FAIL("Not exist character of character ID : " << pResult->charId);
		return;
	}


	GetAlarmManager()->UpdateFormattedAlarmMessage(DST_LOBBY_COMPLETE_RENAME, FALSE, NULL,
												   pLOBBY_CHARACTER->tSummary.awchName,
												   GetLobbyManager()->GetReservedRename());

	pLobby->Rename(pResult->charId, GetLobbyManager()->GetReservedRename());
	GetLobbyManager()->SetReservedRename(L"");

	CDboEventGenerator::LobbyEvent(LMT_RENAME_CHARACTER, 0.f, pResult->charId);
}

void PacketHandler_CSCharDisconnectedNfy(void *pPacket)
{
	sCU_DISCONNECTED_NFY* pResult = (sCU_DISCONNECTED_NFY*)pPacket;

	GetAlarmManager()->AlarmMessage( pResult->wResultCode, TRUE );

	SConnectData* pConnectData = GetDboGlobal()->GetConnectData();
	pConnectData->sCharCon.bBanishment = true;
}

void PacketHandler_CSCharServerFarmInfoRefreshNfy(void *pPacket)
{
	sCU_SERVER_FARM_INFO_REFRESHED_NFY* pResult = (sCU_SERVER_FARM_INFO_REFRESHED_NFY*)pPacket;

	SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
	CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
	if( !pLobby )
		return;

	pLobby->RefreshServerInfo((eDBO_SERVER_STATUS)pResult->byServerStatus, pResult->dwLoad);
	CDboEventGenerator::LobbyEvent(LMT_REFRESH_SERVER, (RwReal)pResult->serverFarmId);
}

void PacketHandler_CSCharChannelFarmInfoRefreshNfy(void *pPacket)
{
	sCU_SERVER_CHANNEL_INFO_REFRESHED_NFY* pResult = (sCU_SERVER_CHANNEL_INFO_REFRESHED_NFY*)pPacket;

	SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
	CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
	if( !pLobby )
		return;


	for( RwInt8 i = 0 ; i < NTL_MAX_SERVER_CHANNEL_COUNT_IN_SERVER_FARM ; ++i )
	{
		sDBO_GAME_SERVER_CHANNEL_INFO* pDBO_GAME_SERVER_CHANNEL_INFO = pLobby->GetChannel(i);
		if( !pDBO_GAME_SERVER_CHANNEL_INFO )
			continue;

		if( pDBO_GAME_SERVER_CHANNEL_INFO->byServerChannelIndex == pResult->serverChannelInfo.byServerChannelIndex )
		{
			*pDBO_GAME_SERVER_CHANNEL_INFO = pResult->serverChannelInfo;
			CDboEventGenerator::LobbyEvent(LMT_REFRESH_CHANEL, (RwReal)pResult->serverChannelInfo.serverFarmId, pResult->serverChannelInfo.byServerChannelIndex);
			break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// game server packet handler

void PacketHandler_GSTutorial_WaitNfy(void *pPacket)
{
	GetMsgBoxManager()->DeleteNetConnectBox();

	sGU_TUTORIAL_WAIT_NFY* pTutorialNfy = (sGU_TUTORIAL_WAIT_NFY*)pPacket;	

	GetAlarmManager()->UpdateFormattedAlarmMessage(DST_TUTORIAL_WAITING_NUMBER, FALSE, NULL, pTutorialNfy->wWaitingCount);
	GetCharStageState()->ChangeState(CHAR_STATE_WAIT_FOR_TUTORIAL);
}

void PacketHandler_GSTutorial_WaitCancelRes(void *pPacket)
{
	GetMsgBoxManager()->DeleteNetConnectBox();

	sGU_TUTORIAL_WAIT_CANCEL_RES* pTutorialWaitCancelRes = (sGU_TUTORIAL_WAIT_CANCEL_RES*)pPacket;	

	if(pTutorialWaitCancelRes->wResultCode != GAME_SUCCESS)
		GetAlarmManager()->AlarmMessage( pTutorialWaitCancelRes->wResultCode, TRUE );
}

void PacketHandler_GSGameEnterRes(void *pPacket)
{
	GetMsgBoxManager()->DeleteNetConnectBox();

	sGU_GAME_ENTER_RES *pGameEnterRes = (sGU_GAME_ENTER_RES*)pPacket;
	if(pGameEnterRes->wResultCode != GAME_SUCCESS)
	{
		// string ���ڿ��� ��´�.
		// network data reading pause ��Ų��. 
		GetAlarmManager()->AlarmMessage( pGameEnterRes->wResultCode, TRUE );
		return;
	}

	SConnectData *pConnectData = GetDboGlobal()->GetConnectData();

	if( FALSE == pConnectData->bChangingGameServer )
	{
		// ĳ���� ���� ���� ����
		GetDboGlobal()->GetLobbyPacketGenerator()->SendCharDisconnectReq(true);

		// �κ��� �ƹ�Ÿ ������ �����
		CDboEventGenerator::LobbyEvent(LMT_DESTORY_LOBBY_AVATAR);
	}

	memcpy(pConnectData->sChatCon.chServerIP, pGameEnterRes->achCommunityServerIP, NTL_MAX_LENGTH_OF_IP);
	pConnectData->sChatCon.wServerPort = pGameEnterRes->wCommunityServerPort;

	// �ð� ������ �Է�.
	GetNtlSLGlobal()->SetServerTime( pGameEnterRes->timeDBOEnter );

	CNtlStage* pStage = CStageManager::GetInstance()->GetActiveStage();
	if( pStage )
	{
		std::string strStageName = pStage->GetName();

		if( strStageName == CHARACTER_STAGE_NAME )
		{
			// ������ ���忡 ������ ĳ���ʹ� Ʃ�丮���� �� �� ����
			SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
			CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
			if( !pLobby )
			{
				DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
				return;
			}

			sLOBBY_CHARACTER* pLOBBY_CHARACTER = pLobby->GetCharacter( pLobby->GetSelectedCharacterIndex() );
			if( !pLOBBY_CHARACTER )
			{
				DBO_FAIL("Not exist character of index : " << pLobby->GetSelectedCharacterIndex());
				NTL_RETURNVOID();
			}

			RwBool bTutorialFlag = pLOBBY_CHARACTER->tSummary.bTutorialFlag;

			// ���� ����!!
			GetDboGlobal()->SetDirectionFirstConnect( !bTutorialFlag );

			pLOBBY_CHARACTER->tSummary.bTutorialFlag = true;
			pLobby->SetSelectedChannelIndex( GetCharStageState()->GetSelectedChannelIndex() );

			// ������ �������� ������ ������ ���� ���
			pConnectData->byLastServerFarmID = pLobby->GetServerHandle();
		}
	}


#ifdef NET_SERVER_SYNC

	if(pConnectData->sChatCon.byConnState == SConnectAtomic::SOCKET_STATE_DISCONNECT)
	{
		RwBool bConnectSuccess = TRUE;

		CNtlClientNet *pNet = GetDboGlobal()->GetNetwork(); 
		if(!pNet->Connect((char*)pConnectData->sChatCon.chServerIP, pConnectData->sChatCon.wServerPort, &pConnectData->sChatCon.hSocket, false, NETCLIENT_ENCODE_NONE ) )
			bConnectSuccess = FALSE;

		CChatPacketGenerator *pChatPacketGenerator = GetDboGlobal()->GetChatPacketGenerator();
		CNtlNetSender *pChatNetSender = pChatPacketGenerator->GetNetSender();
		pChatNetSender->SetData(INVALID_HSERVER, NULL); 

		if(bConnectSuccess)
		{
			// socket connect state
			pConnectData->sChatCon.byConnState = SConnectAtomic::SOCKET_STATE_CONNECT;

			// chatting packet generator �� CNtlNetSender data setting.
			pChatNetSender->SetData(pConnectData->sChatCon.hSocket, pNet);			
		}
		else
		{
			pChatNetSender->SetData(INVALID_HSERVER, NULL); 
		}
	}	
#endif
}

void PacketHandler_GSAvatarCharInfo(void *pPacket)
{
	sGU_AVATAR_CHAR_INFO *pCharInfo = (sGU_AVATAR_CHAR_INFO*)pPacket;

	SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo(); 
	pAvatarInfo->uiSerialId = pCharInfo->handle;
	memcpy(&pAvatarInfo->sCharPf, &pCharInfo->sPcProfile, sizeof(sPC_PROFILE)); 
	memcpy(&pAvatarInfo->sCharState.sCharStateBase, &pCharInfo->sCharState.sCharStateBase, sizeof(sCHARSTATE_BASE)); 
	RwInt32 iDataLen = pCharInfo->wCharStateSize - sizeof(sCHARSTATE_BASE);
	if(iDataLen > 0)
		memcpy(&pAvatarInfo->sCharState.sCharStateDetail, &pCharInfo->sCharState.sCharStateDetail, iDataLen); 
}

void PacketHandler_GSAvatarItemInfo(void *pPacket)
{
	sGU_AVATAR_ITEM_INFO *pItemInfo = (sGU_AVATAR_ITEM_INFO*)pPacket;

	SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo();
	pAvatarInfo->byItemCount = pItemInfo->byBeginCount + pItemInfo->byItemCount;
	memcpy(&pAvatarInfo->sItemPf[pItemInfo->byBeginCount], pItemInfo->aItemProfile, pItemInfo->byItemCount*sizeof(sITEM_PROFILE));

}

void PacketHandler_GSAvatarSkillInfo(void *pPacket)
{
	sGU_AVATAR_SKILL_INFO *pSkillInfo = (sGU_AVATAR_SKILL_INFO*)pPacket;

	SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo(); 
	pAvatarInfo->bySkillCount = pSkillInfo->bySkillCount;

	memcpy(pAvatarInfo->sSkillPf, pSkillInfo->aSkillInfo, pAvatarInfo->bySkillCount*sizeof(sSKILL_INFO));

}

void PacketHandler_GSAvatarHTBInfo(void *pPacket)
{
	sGU_AVATAR_HTB_INFO *pHTBInfo = (sGU_AVATAR_HTB_INFO*)pPacket;

	SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo();
	pAvatarInfo->byHTBCount = pHTBInfo->byHTBSkillCount;

	memcpy(pAvatarInfo->sHTBPf, pHTBInfo->aHTBSkillnfo, pAvatarInfo->byHTBCount*sizeof(sHTB_SKILL_INFO));
}

void PacketHandler_GSAvatarBuffInfo(void *pPacket)
{
	sGU_AVATAR_BUFF_INFO *pBuffInfo = (sGU_AVATAR_BUFF_INFO*)pPacket;

	SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo();
	pAvatarInfo->byBuffCount = pBuffInfo->byBuffCount;

	memcpy(pAvatarInfo->sBuffPf, pBuffInfo->aBuffInfo, pAvatarInfo->byBuffCount*sizeof(sBUFF_INFO));
}

void PacketHandler_GSAvatarPassiveEffectInfo(void* pPacket)
{
	sGU_AVATAR_UPDATED_SKILL_DATA_INFO* pPassiveeFffectInfo = (sGU_AVATAR_UPDATED_SKILL_DATA_INFO*)pPacket;

	LIST_PASSIVE_SKILL_EFFECT* pListPassiveSkillEffect = GetNtlSLGlobal()->GetPassiveSkillEffectInfo();

	SPassiveSkillEffect stPassiveEffect;
	stPassiveEffect.SkillTblidx = pPassiveeFffectInfo->skillTblidx;
	stPassiveEffect.bySlotidx = pPassiveeFffectInfo->bySlotIndex;
	stPassiveEffect.sUpdatedSkillData = pPassiveeFffectInfo->sUpdatedSkillData;

	pListPassiveSkillEffect->push_back( stPassiveEffect );	
}

void PacketHandler_GSAvatarInfoEnd(void *pPacket)
{
	GetDboGlobal()->GetGamePacketGenerator()->SendEnterWorld();
}

void PacketHandler_GSAvatarWorldInfo(void *pPacket)
{
	CNtlStage *pStage = CStageManager::GetInstance()->GetActiveStage();
	std::string strStageName = pStage->GetName();

	if( strStageName == CHARACTER_STAGE_NAME )
	{
		// ������ ���� ��Ŷ�� ��ٸ��ٴ� �޼��� �ڽ��� �� �ִٸ� �ݴ´�
		GetMsgBoxManager()->DeleteNetConnectBox();
	}	

	sGU_AVATAR_WORLD_INFO *pWorldInfo = (sGU_AVATAR_WORLD_INFO*)pPacket;


	GetNtlWorldConcept()->DeleteGradeWorldPlayConcept(WORLD_CONCEPT_FIRST_GRADE);

	// world ���� �Է�.
	RwBool bWorldChange = FALSE;

	SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo(); 
	if(pAvatarInfo->sWorldInfo.worldID != pWorldInfo->worldInfo.worldID)
	{
		bWorldChange = TRUE;
	}

	pAvatarInfo->sCharState.sCharStateBase.vCurLoc = pWorldInfo->vCurLoc;
	pAvatarInfo->sCharState.sCharStateBase.vCurDir = pWorldInfo->vCurDir;

	memcpy(&pAvatarInfo->sOldWorldInfo, &pAvatarInfo->sWorldInfo, sizeof(sWORLD_INFO));
	memcpy(&pAvatarInfo->sWorldInfo, &pWorldInfo->worldInfo, sizeof(sWORLD_INFO));

	pAvatarInfo->byDojoBriefCount = pWorldInfo->byDojoCount;
	memcpy(&pAvatarInfo->sDojoBrief, &pWorldInfo->sDojoData, sizeof(sDBO_DOJO_BRIEF) * pWorldInfo->byDojoCount);

	{
		CNtlPLVisualManager* pVisualMng = (CNtlPLVisualManager*)GetSceneManager();

		pVisualMng->GetDojoContainer()->Clear();

		sEmblemFactor sEmFactor;

		if ( GAMERULE_DOJO == pWorldInfo->worldInfo.sRuleInfo.byRuleType )
		{
			NTL_ASSERT( pWorldInfo->byDojoCount == 1, "A dogo world must have one dojo." );

			for ( int i = 0; i < pWorldInfo->byDojoCount; ++i )
			{
				sEmFactor.byTypeA		= pWorldInfo->sDojoData[i].sMark.byMarkMain;
				sEmFactor.byTypeB		= pWorldInfo->sDojoData[i].sMark.byMarkInLine;
				sEmFactor.byTypeC		= pWorldInfo->sDojoData[i].sMark.byMarkOutLine;

				sEmFactor.byTypeAColor	= pWorldInfo->sDojoData[i].sMark.byMarkMainColor;
				sEmFactor.byTypeBColor	= pWorldInfo->sDojoData[i].sMark.byMarkInColor;
				sEmFactor.byTypeCColor	= pWorldInfo->sDojoData[i].sMark.byMarkOutColor;

				pVisualMng->GetDojoContainer()->SetDojoWorld( pWorldInfo->sDojoData[i].byLevel, &sEmFactor );
			}
		}
		else
		{
			for ( int i = 0; i < pWorldInfo->byDojoCount; ++i )
			{
				sEmFactor.byTypeA		= pWorldInfo->sDojoData[i].sMark.byMarkMain;
				sEmFactor.byTypeB		= pWorldInfo->sDojoData[i].sMark.byMarkInLine;
				sEmFactor.byTypeC		= pWorldInfo->sDojoData[i].sMark.byMarkOutLine;

				sEmFactor.byTypeAColor	= pWorldInfo->sDojoData[i].sMark.byMarkMainColor;
				sEmFactor.byTypeBColor	= pWorldInfo->sDojoData[i].sMark.byMarkInColor;
				sEmFactor.byTypeCColor	= pWorldInfo->sDojoData[i].sMark.byMarkOutColor;

				pVisualMng->GetDojoContainer()->SetDojo( pWorldInfo->sDojoData[i].dojoTblidx, pWorldInfo->sDojoData[i].byLevel, &sEmFactor );
			}
		}
	}

	RwUInt8 byRuleType = pWorldInfo->worldInfo.sRuleInfo.byRuleType;
	CWorldTable* pWorldTable = API_GetTableContainer()->GetWorldTable();

	// world concept ���� �Է�.
	if(byRuleType != GAMERULE_NORMAL)
	{
		if(byRuleType == GAMERULE_TUTORIAL)
		{
			GetNtlWorldConcept()->AddWorldPlayConcept(WORLD_PLAY_TUTORIAL);

			// �ӽ�
			CNtlWorldConceptTutorial* pConcept = reinterpret_cast<CNtlWorldConceptTutorial*>(GetNtlWorldConcept()->GetWorldConceptController(WORLD_PLAY_TUTORIAL));
			pConcept->SetTMQState(TIMEQUEST_GAME_STATE_BEGIN);
		}
		else if(byRuleType == GAMERULE_RANKBATTLE)
		{
			GetNtlWorldConcept()->AddWorldPlayConcept(WORLD_PLAY_RANK_BATTLE);
		}
		else if( byRuleType == GAMERULE_MINORMATCH || byRuleType == GAMERULE_MAJORMATCH || byRuleType == GAMERULE_FINALMATCH )
		{
			GetNtlWorldConcept()->AddWorldPlayConcept(WORLD_PLAY_T_BUDOKAI);
			CNtlWorldConceptTB* pBudokaiConcept = reinterpret_cast<CNtlWorldConceptTB*>( GetNtlWorldConcept()->GetWorldConceptController( WORLD_PLAY_T_BUDOKAI ) );
			NTL_ASSERT( pBudokaiConcept, "PacketHandler_GSAvatarWorldInfo : Invalid WorldConeptTB" );

			sWORLD_TBLDAT* pWorldData = reinterpret_cast<sWORLD_TBLDAT*>( pWorldTable->FindData( pWorldInfo->worldInfo.tblidx ) );
			NTL_ASSERT( pWorldInfo, "PacketHandler_GSAvatarWorldInfo :" << pWorldInfo->worldInfo.tblidx << "Invalid World Table Data" );
			pBudokaiConcept->SetMatchTbl( pWorldData->worldRuleTbldx );

			switch( byRuleType )
			{
			case GAMERULE_MINORMATCH: pBudokaiConcept->ChangeTBType( MATCHWORLD_TYPE_MINOR_MATCH ); break;
			case GAMERULE_MAJORMATCH: pBudokaiConcept->ChangeTBType( MATCHWORLD_TYPE_MAJOR_MATCH ); break;
			case GAMERULE_FINALMATCH: pBudokaiConcept->ChangeTBType( MATCHWORLD_TYPE_FINAL_MATCH ); break;
			}
			pBudokaiConcept->ChangeState( WORLD_STATE_IDLE );
		}
		else if(byRuleType == GAMERULE_TIMEQUEST)
		{
			CNtlSLEventGenerator::CameraControlDelete(CAMERA_CONTROL_TIMEMACHINE); // Ÿ�Ӹӽ� ���� ī�޶� �����Ѵ�.

			GetNtlWorldConcept()->AddWorldPlayConcept(WORLD_PLAY_TIME_MACHINE);

			CNtlWorldConceptTMQ* pTMQWorldConcept = reinterpret_cast<CNtlWorldConceptTMQ*>( GetNtlWorldConcept()->GetWorldConceptController(WORLD_PLAY_TIME_MACHINE) );
			pTMQWorldConcept->SetTMQState((eTIMEQUEST_GAME_STATE)pWorldInfo->worldInfo.sRuleInfo.sTimeQuestRuleInfo.sTimeQuestState.byGameState);
		}
		else if(byRuleType == GAMERULE_HUNT)
		{
			GetNtlWorldConcept()->AddWorldPlayConcept(WORLD_PLAY_PARTY_DUNGEON);
		}
		else if(byRuleType == GAMERULE_TLQ)
		{
			GetNtlWorldConcept()->AddWorldPlayConcept(WORLD_PLAY_TLQ);
		}
		else if(byRuleType == GAMERULE_DOJO)
		{
			CNtlSobAvatar* pAvatar = GetNtlSLGlobal()->GetSobAvatar();
			if( pAvatar )
			{
				CNtlDojo* pDojo = pAvatar->GetDojo();
				sSCRAMBLE_INFO* pSCRAMBLE_INFO = pDojo->GetScramble();

				if( eDBO_DOJO_STATUS_READY <= pSCRAMBLE_INFO->eDojoState && pSCRAMBLE_INFO->eDojoState <= eDBO_DOJO_STATUS_END )
				{
			GetNtlWorldConcept()->AddWorldPlayConcept(WORLD_PLAY_DOJO_SCRAMBLE);
				}
			}
		}
	}	
	else
	{
		// �Ϲ� ����� ������ �������� Ÿ�̸ӽ� ī�޶� �����ϴ� ���� �����Ѵ�
		CNtlSLEventGenerator::CameraControlDelete(CAMERA_CONTROL_TIMEMACHINE);
	}

	// ��Ʈ�� �����͸� ��� ������Ų��.
	CPacketProc::GetInstance()->ActivePop(FALSE);

	CNtlSoundEventGenerator::SendReleaseObjectGroup();


	// game stage�� ���� �ִ��� �ƴ����� �˻��Ѵ�.
	// game stage�� ���� �ִ� ��쿡�� teleport or rescus ���ؼ� ȣ��ȴ�.		
	if(strStageName != GAME_STAGE_NAME)
	{
		GetCharStageState()->ChangeState(CHAR_STATE_END);		

		// �κ� �������� ����
		CDboEventGenerator::StageDelete(CHARACTER_STAGE_NAME);

		SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
		CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
		if( !pLobby )
		{
			DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
			return;
		}

		sLOBBY_CHARACTER* pLOBBY_CHARACTER = pLobby->GetCharacter( pLobby->GetSelectedCharacterIndex() );
		if( !pLOBBY_CHARACTER )
		{
			DBO_FAIL("Not exist character of index : " << pLobby->GetSelectedCharacterIndex());
			NTL_RETURNVOID();
		}

		// �������� ������ ĳ������ �̸� ����
		GetNtlStorageManager()->SetData( dSTORAGE_ACCOUNT_LASTCHAR, pLOBBY_CHARACTER->tSummary.awchName );
		Logic_SaveAccountOption();

		// ���� �������� ����
		CDboEventGenerator::StageCreate(GAME_STAGE_NAME);
	}
	else
	{
		CNtlSLEventGenerator::WorldChange(bWorldChange);
	}
}

void Packethandler_GSAvatarWorldZoneInfo( void* pPacket )
{
	// Night ȿ�� �̺�Ʈ�� ������.
	sGU_AVATAR_ZONE_INFO* pZoneInfo = reinterpret_cast<sGU_AVATAR_ZONE_INFO*>(pPacket);
	if(pZoneInfo)
	{
		CNtlSLEventGenerator::NightEffect(pZoneInfo->zoneInfo.bIsDark);
	}
	return;
}

void PacketHandler_GSAvatarQuestCompleteInfo(void *pPacket)
{
	sGU_AVATAR_QUEST_COMPLETE_INFO * pData = (sGU_AVATAR_QUEST_COMPLETE_INFO*)pPacket;

	SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo(); 

	memcpy(&pAvatarInfo->sQuestCompleteInfo, &pData->completeInfo, sizeof(sQUEST_COMPLETE_INFO));
}

void PacketHandler_GSAvatarQuestProgressInfo(void *pPacket)
{
	sGU_AVATAR_QUEST_PROGRESS_INFO * pData = (sGU_AVATAR_QUEST_PROGRESS_INFO*)pPacket;

	SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo(); 

	pAvatarInfo->byQuestProgressInfoCount = pData->byProgressCount;
	memcpy(&pAvatarInfo->sQuestProgressInfo, &pData->progressInfo, pData->byProgressCount * sizeof(sQUEST_PROGRESS_INFO));
}

void PacketHandler_GSAvatarQuestInventoryInfo(void *pPacket)
{
	sGU_AVATAR_QUEST_INVENTORY_INFO * pData = (sGU_AVATAR_QUEST_INVENTORY_INFO*)pPacket;

	SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo(); 

	pAvatarInfo->sQuestInventoryInfo.byQuestItemCount = pData->byItemCount;
	memcpy(&pAvatarInfo->sQuestInventoryInfo.aQuestItemData, &pData->aInventoryInfo, sizeof(sQUEST_INVENTORY_INFO)*pData->byItemCount);
}

void PacketHandler_GSAVatarQuickSlotInfo(void *pPacket)
{
	sGU_QUICK_SLOT_INFO* pData = (sGU_QUICK_SLOT_INFO*)pPacket;

	SAvatarInfo* pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo();

	pAvatarInfo->bySetQuickSlotInfo = TRUE;
	pAvatarInfo->byQuickSlotInfoCount = pData->byQuickSlotCount;
	memcpy( &pAvatarInfo->sQuickSlotInfo, &pData->asQuickSlotData, sizeof( sQUICK_SLOT_PROFILE ) * pData->byQuickSlotCount );

	// Avatar���� �Ŀ� ��Ŷ�� ���� ��쿡�� ��ȿ��.
	if( GetNtlSLGlobal()->GetSobAvatar() )
		CDboEventGenerator::QuickSlotInfo();
}

void PacketHandler_GSAVatarWarFogInfo(void *pPacket)
{
	// �ƹ�Ÿ �ε��� ������ ������ �޴´�
	sGU_WAR_FOG_INFO* pResult = (sGU_WAR_FOG_INFO*)pPacket;

	SAvatarInfo* pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo();
	char* pWarFogFlag = pAvatarInfo->acWarFogFlag;

	memcpy(pWarFogFlag, pResult->abyWarFogInfo, NTL_MAX_SIZE_WAR_FOG);
}

void PacketHandler_GSEnterWorldComplete(void *pPacket)
{
	// game stage�� ���� �ִ��� �ƴ����� �˻��Ѵ�.
	// game stage�� ���� �ִ� ��쿡�� teleport or rescus or worldchange�� ���ؼ� ȣ��ȴ�.
	SGameData *pGameData = GetDboGlobal()->GetGameData();
	if(pGameData->bFirstWorldEnter)
	{
		GetDboGlobal()->GetGamePacketGenerator()->SendAuthKeyCommunityServerReq();	
	}

	pGameData->bFirstWorldEnter = FALSE;

	SConnectData *pConnectData = GetDboGlobal()->GetConnectData();
	if( pConnectData )
		pConnectData->bChangingGameServer = FALSE;
}

void PacketHandler_GSAuthKeyCommunityServerRes(void *pPacket)
{
	sGU_AUTH_KEY_FOR_COMMUNITY_SERVER_RES *pAuthKeyChat = (sGU_AUTH_KEY_FOR_COMMUNITY_SERVER_RES*)pPacket;
	if(pAuthKeyChat->wResultCode == GAME_SUCCESS)
	{
		SConnectData *pConnectData = GetDboGlobal()->GetConnectData();
		memcpy(pConnectData->sChatCon.chAuthKey, pAuthKeyChat->abyAuthKey, NTL_MAX_SIZE_AUTH_KEY);
		GetDboGlobal()->GetChatPacketGenerator()->SendChatEnterReq();
	}
}

void PacketHandler_GSAvatarPetitionInfo(void *pPacket)
{
	// ������ �������� ������ �־��� ����
	sGU_AVATAR_PETITION_INFO* pPetitionInfo = (sGU_AVATAR_PETITION_INFO*)pPacket;

	GetPetitionManager()->SetPetitionID(pPetitionInfo->petitionID);
	GetPetitionManager()->SetPetitionContent(pPetitionInfo->nCategory1, pPetitionInfo->nCategory2, pPetitionInfo->awchQuestionContent);
}

void PacketHandler_GSWarFog_UpdataRes(void *pPacket)
{
	// ����ũ ����ĭ�� Ŭ���Ͽ� �����׸� ������
	API_GetSLPacketLockManager()->Unlock( GU_WAR_FOG_UPDATE_RES );

	sGU_WAR_FOG_UPDATE_RES* pResult = (sGU_WAR_FOG_UPDATE_RES*)pPacket;

	if( pResult->wResultCode != GAME_SUCCESS )
	{
		GetAlarmManager()->AlarmMessage(pResult->wResultCode, TRUE);
		return;
	}	

	GetAlarmManager()->AlarmMessage(DST_UNCOVER_NAMEKAN_SIGN);
	CDboEventGenerator::MapEvent(MMT_WARFOG_UPDATE, pResult->handle);
}

void PacketHandler_GSCharServerExitRes(void *pPacket)
{
	GetMsgBoxManager()->DeleteNetConnectBox();

	sGU_CHAR_EXIT_RES *pCharServerRes = (sGU_CHAR_EXIT_RES*)pPacket;

	// login server ����.
	CNtlClientNet *pNet = GetDboGlobal()->GetNetwork(); 
	SConnectData *pConnectData = GetDboGlobal()->GetConnectData();

	pNet->Disconnect(pConnectData->sChatCon.hSocket);
	pNet->Disconnect(pConnectData->sGameCon.hSocket);

	pConnectData->sChatCon.byConnState = SConnectAtomic::SOCKET_STATE_DISCONNECT;
	pConnectData->sGameCon.byConnState = SConnectAtomic::SOCKET_STATE_DISCONNECT;

	pConnectData->sChatCon.hSocket = INVALID_HSERVER;
	pConnectData->sGameCon.hSocket = INVALID_HSERVER;

	CChatPacketGenerator *pChatPacketGenerator = GetDboGlobal()->GetChatPacketGenerator();
	CGamePacketGenerator *pGamePacketGenerator = GetDboGlobal()->GetGamePacketGenerator();

	CNtlNetSender *pNetSender;
	pNetSender = pChatPacketGenerator->GetNetSender();
	pNetSender->SetData(INVALID_HSERVER, NULL); 

	pNetSender = pGamePacketGenerator->GetNetSender();
	pNetSender->SetData(INVALID_HSERVER, NULL); 

	if(pCharServerRes->wResultCode == GAME_SUCCESS)
	{
		pConnectData->sCharCon.Init();
		pConnectData->ResetCharacterServer();

		// auth key
		memcpy(pConnectData->sCharCon.chAuthKey, pCharServerRes->achAuthKey, NTL_MAX_SIZE_AUTH_KEY);

		// character server ip ����.
		for(RwInt32 i = 0; i < (RwInt32)pCharServerRes->byServerInfoCount; i++)
		{
			pConnectData->AddCharacterServer(pCharServerRes->aServerInfo[i].szCharacterServerIP, 
				pCharServerRes->aServerInfo[i].wCharacterServerPortForClient,
				pCharServerRes->aServerInfo[i].dwLoad);
		}

		GetLobbyManager()->SetWaitChannelInfo(FALSE);
		GetGameStageState()->ChangeState(GAME_STATE_CHAR_SERVER_CONNECT);
	}
	else
	{
		GetAlarmManager()->AlarmMessage( DST_CHAR_SERVER_CONNECT_FAIL );
	}
}


void PacketHandler_GSGameServerChangeRes(void *pPacket)
{
	sGU_CHAR_SERVER_CHANGE_RES *pCharServerChangeRes = (sGU_CHAR_SERVER_CHANGE_RES*)pPacket;

	if(pCharServerChangeRes->wResultCode == GAME_SUCCESS)
	{
		SConnectData *pConnectData = GetDboGlobal()->GetConnectData();

		if(pConnectData->sGameCon.byConnState == SConnectAtomic::SOCKET_STATE_CONNECT)
		{
			CNtlClientNet *pNet = GetDboGlobal()->GetNetwork(); 
			pConnectData->sGameCon.byConnState = SConnectAtomic::SOCKET_STATE_DISCONNECT;
			pNet->Disconnect(pConnectData->sGameCon.hSocket);
			pConnectData->sGameCon.hSocket = INVALID_HSERVER;
		}

		pConnectData->sGameCon.Init();

		memcpy(pConnectData->sGameCon.chAuthKey, pCharServerChangeRes->achAuthKey, NTL_MAX_SIZE_AUTH_KEY);
		memcpy(pConnectData->sGameCon.chServerIP, pCharServerChangeRes->serverInfo.szCharacterServerIP, NTL_MAX_LENGTH_OF_IP); 
		pConnectData->sGameCon.wServerPort = pCharServerChangeRes->serverInfo.wCharacterServerPortForClient;



		CGamePacketGenerator *pGamePacketGenerator = GetDboGlobal()->GetGamePacketGenerator(); 
		CNtlNetSender *pGameNetSender = pGamePacketGenerator->GetNetSender();

		CNtlNetSender *pSLNetSender = API_GetSLPacketGenerator()->GetNetSender(); 

		RwBool bConnectSuccess = TRUE;
		CNtlClientNet *pNet = GetDboGlobal()->GetNetwork(); 
		if(!pNet->Connect((char*)pConnectData->sGameCon.chServerIP, pConnectData->sGameCon.wServerPort, &pConnectData->sGameCon.hSocket, false, NETCLIENT_ENCODE_NONE) )
			bConnectSuccess = FALSE;

		if(bConnectSuccess)
		{
			// socket connect state
			pConnectData->bChangingGameServer	= TRUE;
			pConnectData->sGameCon.byConnState	= SConnectAtomic::SOCKET_STATE_CONNECT;

			// game packet generator �� CNtlNetSender data setting.
			pGameNetSender->SetData(pConnectData->sGameCon.hSocket, pNet); 

			// simulation packet generator�� CNtlNetSender data setting
			pSLNetSender->SetData(pConnectData->sGameCon.hSocket, pNet);


			SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
			CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
			if( !pLobby )
			{
				DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
				return;
			}

			sLOBBY_CHARACTER* pLOBBY_CHARACTER = pLobby->GetCharacter( pLobby->GetSelectedCharacterIndex() );
			if( !pLOBBY_CHARACTER )
			{
				DBO_FAIL("Not exist character of index : " << pLobby->GetSelectedCharacterIndex());
				NTL_RETURNVOID();
			}

			CHARACTERID charID = pLOBBY_CHARACTER->tSummary.charId;

			// avatar recreate setting.
			SGameData *pGameData = GetDboGlobal()->GetGameData();
			pGameData->bAvatarReCreate = TRUE;
			SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo(); 
			RwV3d vAvatarPos = GetNtlSLGlobal()->GetSobAvatar()->GetPosition();
			pAvatarInfo->sCharState.sCharStateBase.vCurLoc.x = vAvatarPos.x;
			pAvatarInfo->sCharState.sCharStateBase.vCurLoc.y = vAvatarPos.y;
			pAvatarInfo->sCharState.sCharStateBase.vCurLoc.z = vAvatarPos.z;

			GetDboGlobal()->GetGamePacketGenerator()->SendGameEnterReq(pConnectData->uiAccountId, charID, pConnectData->sGameCon.chAuthKey, false);

			CChannelChangeGui* pGui = reinterpret_cast<CChannelChangeGui*>( GetDialogManager()->GetDialog(DIALOG_CHANNGEL_CHANGE) );
			pLobby->SetSelectedChannelIndex( pGui->GetSelectedChannelIndex() );

			CNtlSLEventGenerator::GameServerChannelChanged();
		}
		else
		{
			pGameNetSender->SetData(INVALID_HSERVER, NULL); 
			pSLNetSender->SetData(INVALID_HSERVER, NULL); 

			GetAlarmManager()->AlarmMessage( DST_GAME_SERVER_CONNECT_FAIL );
		}
	}
}

void PacketHandler_GSGameChannelChangeRes(void *pPacket)
{
	sGU_CHAR_CHANNEL_CHANGE_RES* pResult = (sGU_CHAR_CHANNEL_CHANGE_RES*)pPacket;

	if( pResult->wResultCode != GAME_SUCCESS )
	{
		GetAlarmManager()->AlarmMessage( pResult->wResultCode, TRUE );
		return;
	}


	SConnectData *pConnectData = GetDboGlobal()->GetConnectData();

	if(pConnectData->sGameCon.byConnState == SConnectAtomic::SOCKET_STATE_CONNECT)
	{
		CNtlClientNet *pNet = GetDboGlobal()->GetNetwork(); 
		pConnectData->sGameCon.byConnState = SConnectAtomic::SOCKET_STATE_DISCONNECT;
		pNet->Disconnect(pConnectData->sGameCon.hSocket);
		pConnectData->sGameCon.hSocket = INVALID_HSERVER;
	}

	pConnectData->sGameCon.Init();

	memcpy(pConnectData->sGameCon.chAuthKey, pResult->achAuthKey, NTL_MAX_SIZE_AUTH_KEY);
	memcpy(pConnectData->sGameCon.chServerIP, pResult->serverInfo.szCharacterServerIP, NTL_MAX_LENGTH_OF_IP); 
	pConnectData->sGameCon.wServerPort = pResult->serverInfo.wCharacterServerPortForClient;



	CGamePacketGenerator *pGamePacketGenerator = GetDboGlobal()->GetGamePacketGenerator(); 
	CNtlNetSender *pGameNetSender = pGamePacketGenerator->GetNetSender();

	CNtlNetSender *pSLNetSender = API_GetSLPacketGenerator()->GetNetSender(); 

	RwBool bConnectSuccess = TRUE;
	CNtlClientNet *pNet = GetDboGlobal()->GetNetwork(); 
	if(!pNet->Connect((char*)pConnectData->sGameCon.chServerIP, pConnectData->sGameCon.wServerPort, &pConnectData->sGameCon.hSocket, false, NETCLIENT_ENCODE_NONE) )
		bConnectSuccess = FALSE;

	if(bConnectSuccess)
	{
		// socket connect state
		pConnectData->bChangingGameServer	= TRUE;
		pConnectData->sGameCon.byConnState	= SConnectAtomic::SOCKET_STATE_CONNECT;

		// game packet generator �� CNtlNetSender data setting.
		pGameNetSender->SetData(pConnectData->sGameCon.hSocket, pNet); 

		// simulation packet generator�� CNtlNetSender data setting
		pSLNetSender->SetData(pConnectData->sGameCon.hSocket, pNet);


		SERVER_HANDLE	hServer	= GetLobbyManager()->GetSelectedServerHandle();
		CLobby*			pLobby	= GetLobbyManager()->GetLobby(hServer);
		if( !pLobby )
		{
			DBO_FAIL("Not exist lobby infomation of server handle : " << hServer);
			return;
		}

		sLOBBY_CHARACTER* pLOBBY_CHARACTER = pLobby->GetCharacter( pLobby->GetSelectedCharacterIndex() );
		if( !pLOBBY_CHARACTER )
		{
			DBO_FAIL("Not exist character of index : " << pLobby->GetSelectedCharacterIndex());
			NTL_RETURNVOID();
		}

		CHARACTERID charID = pLOBBY_CHARACTER->tSummary.charId;

		// avatar recreate setting.
		SGameData *pGameData = GetDboGlobal()->GetGameData();
		pGameData->bAvatarReCreate = TRUE;
		SAvatarInfo *pAvatarInfo = GetNtlSLGlobal()->GetAvatarInfo(); 
		RwV3d vAvatarPos = GetNtlSLGlobal()->GetSobAvatar()->GetPosition();
		pAvatarInfo->sCharState.sCharStateBase.vCurLoc.x = vAvatarPos.x;
		pAvatarInfo->sCharState.sCharStateBase.vCurLoc.y = vAvatarPos.y;
		pAvatarInfo->sCharState.sCharStateBase.vCurLoc.z = vAvatarPos.z;

		GetDboGlobal()->GetGamePacketGenerator()->SendGameEnterReq(pConnectData->uiAccountId, charID, pConnectData->sGameCon.chAuthKey, false);

		CChannelChangeGui* pGui = reinterpret_cast<CChannelChangeGui*>( GetDialogManager()->GetDialog(DIALOG_CHANNGEL_CHANGE) );
		pLobby->SetSelectedChannelIndex( pGui->GetSelectedChannelIndex() );

		CNtlSLEventGenerator::GameServerChannelChanged();
	}
	else
	{
		pGameNetSender->SetData(INVALID_HSERVER, NULL); 
		pSLNetSender->SetData(INVALID_HSERVER, NULL); 

		GetAlarmManager()->AlarmMessage( DST_GAME_SERVER_CONNECT_FAIL );
	}
}