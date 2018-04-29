#pragma once

#include "NetworkIDObject.h"
#include <iostream>

using namespace std;
using namespace RakNet;

class CRPGCharacter : public NetworkIDObject
{

public:
	enum CharacterType {
		CT_Mage,
		CT_Wizard,
		CT_Orc
	};

	int	PS_MaxHealth = 10;
	int PS_Health = 10;
	int PS_Damage = 1;
	char PS_Ability = 'D';

	CRPGCharacter();
	~CRPGCharacter();

	void SetIsMaster(bool isMaster) { m_isMaster = isMaster; }
	void SetCharacterType(CharacterType ctType) { m_Type = ctType; }
	CharacterType GetCharacterType() { return m_Type; }
	void Attack(CRPGCharacter* victem)
	{
		victem->PS_Health--; printf("You were attacked\n");
		if (victem->PS_Health < 1)
			printf("You were killed\n GAME OVER\n");
		else
			DisplayStats(victem);
	}
	void Ability(CRPGCharacter* g_character)
	{
		if (PS_Ability == 'D')
		{
			PS_MaxHealth++;
			cout << "You increased max health: " << PS_MaxHealth << "\n";
		}
		else if (PS_Ability == 'H') {
			PS_Health++;
			cout << "You healed. Health: " << PS_Health << "\n";
		}
		else if (PS_Ability == 'S') {
			PS_Damage++;
			cout << "You increased in strength. Damage: " << PS_Damage << "\n";
		}
		DisplayStats(g_character);
	}
	void SetStats(int health, int damage, char ability) { PS_MaxHealth = health; PS_Health = health; PS_Damage = damage; PS_Ability = ability; }
	void DisplayStats(CRPGCharacter* target) {
		cout << "\n\n" << target->PS_Health << "/" << target->PS_MaxHealth << " Health\n";
		cout << target->PS_Damage << " Damage\n";

		if (target->PS_Ability == 'D')
			cout << "Defense Ability\n\n";
		else if (target->PS_Ability == 'H')
			cout << "Healing Ability\n\n";
		else if (target->PS_Ability == 'S')
			cout << "Increase damage Ability\n\n";
	}


private:
	bool m_isMaster;
	CharacterType m_Type;
};

CRPGCharacter::CRPGCharacter()
{
}

CRPGCharacter::~CRPGCharacter()
{
}
