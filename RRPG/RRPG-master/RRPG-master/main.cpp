#include "MessageIdentifiers.h"
#include "RakPeer.h"
#include "RakPeerInterface.h"
#include "ReadyEvent.h"
#include "FullyConnectedMesh2.h"
#include "ConnectionGraph2.h"
#include "Gets.h"
#include <thread>
#include "RPGCharacters.h"
#include "NetworkIDManager.h"
#include "NetworkIDObject.h"

using namespace RakNet;

RakPeerInterface* g_rakPeerInterface;

const unsigned int MAX_CONNECTIONS = 4;
const unsigned short HOST_GAME_PORT = 6500;
const unsigned int MIN_NUM_PLAYERS = 1;
const unsigned int MAX_TURNS = 5;

const char* LOCAL_IP = "127.0.0.1";


bool g_isHosting = false;
unsigned int m_currentTurnIndex = 0;
CRPGCharacter* g_character;

ReadyEvent g_readyEventPlugin;

// These two plugins are just to automatically create a fully connected mesh so I don't have to call connect more than once
FullyConnectedMesh2 g_fcm2;
ConnectionGraph2 g_cg2;

NetworkIDManager g_networkIDManager;

enum {
	ID_GB3_CREATE_CHARACTER = ID_USER_PACKET_ENUM,
	ID_GB3_ATTACK,
	ID_GB3_ABILITY
};

enum ReadyEventIDs
{
	ID_RE_CHARACTER_SELECT = 0,
	ID_RE_TURN_COMPLETE,
};

enum GameState
{
	GS_Waiting = 0,
	GS_WaitingForPlayers,
	GS_CharacterSelect,
	GS_ReadyToPlay,
	GS_GameLoop,
	GS_GameOver,
};

GameState g_gameState = GS_WaitingForPlayers;

int CreateSocket()
{
	SocketDescriptor sd;
	if (g_isHosting)
	{
		sd.port = HOST_GAME_PORT;
	}
	else
	{
		//looking for an open port
		unsigned short startingPort = HOST_GAME_PORT + 1;
		while (IRNS2_Berkley::IsPortInUse(startingPort, LOCAL_IP, AF_INET, SOCK_DGRAM) == true)
		{
			startingPort++;
		}
		sd.port = startingPort;
	}

	strcpy_s(sd.hostAddress, LOCAL_IP);
	g_rakPeerInterface->SetMaximumIncomingConnections(MAX_CONNECTIONS);
	return g_rakPeerInterface->Startup(MAX_CONNECTIONS, &sd, 1);
}

bool SetupReadyEvent(int event, RakNetGUID player)
{
	return g_readyEventPlugin.AddToWaitList(event, player);
}

bool SetReadyEvent(int event, bool val)
{
	return g_readyEventPlugin.SetEvent(event, val);
}

void PacketListener()
{
	while (1)
	{
		Packet* packet;
		for (packet = g_rakPeerInterface->Receive(); packet != nullptr; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive())
		{
			unsigned short packetIdentifier = packet->data[0];
			switch (packetIdentifier)
			{
			case ID_NEW_INCOMING_CONNECTION:
			case ID_REMOTE_NEW_INCOMING_CONNECTION:
				printf("Another client has connected.\n");
				SetupReadyEvent(ID_RE_CHARACTER_SELECT, packet->guid);
				break;
			case ID_CONNECTION_REQUEST_ACCEPTED:
				printf("Our connection request has been accepted.\n");
				SetupReadyEvent(ID_RE_CHARACTER_SELECT, packet->guid);
				break;
			case ID_UNCONNECTED_PONG:
			{
				RakNet::TimeMS time;
				RakNet::BitStream bsIn(packet->data, packet->length, false);
				bsIn.IgnoreBytes(1);
				bsIn.Read(time);
				printf("System Address: %s.", packet->systemAddress.ToString());
				printf(" Time: " + (RakNet::GetTime() - time));
				printf(". \n");
				ConnectionAttemptResult car = g_rakPeerInterface->Connect(packet->systemAddress.ToString(false), packet->systemAddress.GetPort(), nullptr, 0);
				if (car == CONNECTION_ATTEMPT_STARTED)
				{
					printf("Attempting to connect to host. \n");
				}
				else
				{
					printf("Cannot Connect. \n");
				}
			}
			break;

			case ID_READY_EVENT_ALL_SET:
			{
				BitStream bs(packet->data, packet->length, false);
				bs.IgnoreBytes(sizeof(MessageID));
				int readyEvent;
				bs.Read(readyEvent);
				if (readyEvent == ID_RE_CHARACTER_SELECT)
				{
					if (g_gameState != GS_GameLoop)
					{
						system("cls");
						g_gameState = GS_ReadyToPlay;
						printf("Game is ready to start! \n");
					}

					SetupReadyEvent(ID_RE_TURN_COMPLETE, packet->guid);
				}
				else if (readyEvent == ID_RE_TURN_COMPLETE)
				{
					SetReadyEvent(ID_RE_TURN_COMPLETE, false);
					//printf("Everyone has finished!\n");
					if (g_gameState != GS_GameLoop)
					{
						++m_currentTurnIndex;
					}
					g_gameState = m_currentTurnIndex >= MAX_TURNS ? GS_GameOver : GS_GameLoop;
				}
			}
			break;
			case ID_READY_EVENT_SET:

				break;
			case ID_GB3_CREATE_CHARACTER:
			{
				printf("Creating... \n");
				BitStream bs(packet->data, packet->length, false);
				bs.IgnoreBytes(sizeof(MessageID));
				RakNet::NetworkID netID;
				bs.Read(netID);
				int characterType;
				bs.Read(characterType);

				CRPGCharacter* character = new CRPGCharacter();
				character->SetIsMaster(false);
				character->SetNetworkIDManager(&g_networkIDManager);
				character->SetNetworkID(netID);
				character->SetCharacterType((CRPGCharacter::CharacterType)characterType);

			}
			break;
			case ID_GB3_ATTACK:
			{
				BitStream bs(packet->data, packet->length, false);
				bs.IgnoreBytes(sizeof(MessageID));
				RakNet::NetworkID netID;
				bs.Read(netID);

				CRPGCharacter* attacker = g_networkIDManager.GET_OBJECT_FROM_ID<CRPGCharacter*>(netID);
				if (attacker)
				{
					attacker->Attack(g_character);
				}

			}
			break;
			default:
				//printf("pi: %i\n", packetIdentifier);
				break;
			}
		}
	}
}

void GameThread()
{
	char userInput[255];
	while (1)
	{
		unsigned short numConnections = g_rakPeerInterface->NumberOfConnections();
		if (g_gameState == GS_WaitingForPlayers && numConnections >= MIN_NUM_PLAYERS)
		{
			g_gameState = GS_CharacterSelect;
			SetupReadyEvent(ID_RE_CHARACTER_SELECT, g_rakPeerInterface->GetMyGUID());
		}

		if (g_gameState == GS_CharacterSelect)
		{
			system("cls");
			printf("Welcome to RRPG, select your character! \n");
			printf("1 for Wizard, 2 for Mage, 3 for Orc. \n");
			Gets(userInput, sizeof(userInput));
			SetReadyEvent(ID_RE_CHARACTER_SELECT, true);
			g_gameState = GS_Waiting;

			//create character
			g_character = new CRPGCharacter();
			int type = atoi(userInput);

			CRPGCharacter::CharacterType ctype = CRPGCharacter::CT_Orc;
			if (type == 1)
			{
				ctype = CRPGCharacter::CT_Wizard;

				printf("Wizard: \n");
				g_character->SetStats(8, 2, 'D');

			}
			else if (type == 2)
			{
				ctype = CRPGCharacter::CT_Mage;
				g_character->SetStats(6, 3, 'H');
				printf("Mage: \n");
			}
			else if (type == 3)
			{
				ctype = CRPGCharacter::CT_Orc;
				g_character->SetStats(12, 1, 'S');
				printf("Orc: \n");
			}

			g_character->DisplayStats(g_character);
			g_character->SetCharacterType(ctype);
			g_character->SetNetworkIDManager(&g_networkIDManager);
			g_character->SetIsMaster(true);
			printf("waiting for others.\n");
		}

		if (g_gameState == GS_ReadyToPlay)
		{
			BitStream bs;
			bs.Write((unsigned char)ID_GB3_CREATE_CHARACTER);
			bs.Write(g_character->GetNetworkID());
			int ctype = (int)g_character->GetCharacterType();
			bs.Write(ctype);
			g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);

			g_gameState = GS_GameLoop;
		}

		if (g_gameState == GS_GameLoop)
		{
			system("cls");
			printf("choose your move\n");
			printf("turn # %i\n", m_currentTurnIndex);
			printf("1 attack, 2 ability\n");
			Gets(userInput, sizeof(userInput));
			int turnType = atoi(userInput);
			if (turnType == 1)
			{
				BitStream bs;
				bs.Write((unsigned char)ID_GB3_ATTACK);
				bs.Write(g_character->GetNetworkID());
				g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
			}
			else if (turnType == 2)
			{
				g_character->Ability(g_character);
			}

			g_gameState = GS_Waiting;
			printf("turn complete, waiting for opponents. \n");
			g_character->DisplayStats(g_character);
			SetReadyEvent(ID_RE_TURN_COMPLETE, true);
		}
	}
}

int main()
{
	g_rakPeerInterface = RakPeerInterface::GetInstance();

	g_rakPeerInterface->AttachPlugin(&g_readyEventPlugin);
	g_rakPeerInterface->AttachPlugin(&g_fcm2);
	g_rakPeerInterface->AttachPlugin(&g_cg2);

	g_fcm2.SetAutoparticipateConnections(true);
	g_fcm2.SetConnectOnNewRemoteConnection(true, "");
	g_cg2.SetAutoProcessNewConnections(true);

	char userInput[255];
	printf("Welcome to RRPG, you like to host? (y)es or (n)o? \n");
	Gets(userInput, sizeof(userInput));
	g_isHosting = userInput[0] == 'y' || userInput[0] == 'Y' ? true : false;
	if (CreateSocket() != RAKNET_STARTED)
	{
		printf("ERROR: Raknet could not start. \n");
	}

	//socket creation is a success, time to listen for packets
	std::thread packetListener(PacketListener);

	if (g_isHosting)
	{
		printf("Other players connecting..\n");
	}
	else
	{
		printf("scanning for hosts...\n");
		g_rakPeerInterface->Ping("255.255.255.255", HOST_GAME_PORT, true);
	}

	std::thread gameThread(GameThread);

	gameThread.join();
	packetListener.join();

	return 0;
}