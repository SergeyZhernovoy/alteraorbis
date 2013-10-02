/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef RESERVE_BANK_INCLUDED
#define RESERVE_BANK_INCLUDED

#include "gamelimits.h"
#include "../grinliz/glrandom.h"
#include "../grinliz/glutil.h"
#include "wallet.h"

class XStream;


class ReserveBank
{
public:
	ReserveBank();
	~ReserveBank();

	void Serialize( XStream* xs );

	int Gold() const					{ return bank.gold; }
	const Wallet& GetWallet() const		{ return bank; }

	int WithdrawDenizen();
	Wallet WithdrawMonster();

	int WithdrawVolcanoGold();
	Wallet WithdrawVolcano();

	// Withdraws 1 or 0 crystals. type is returned.
	int WithdrawRandomCrystal();
	int WithdrawGold( int g )		{ g = grinliz::Min( g, bank.gold ); bank.gold -= g; return g; }
	int WithdrawCrystal( int type ) {	if ( bank.crystal[type] > 0 ) { 
											bank.crystal[type] -= 1;
											return type;
										}
										return NUM_CRYSTAL_TYPES;
	}

	static ReserveBank* Instance() { return instance; }

private:
	static ReserveBank* instance;
	grinliz::Random random;

	Wallet bank;
};

#endif // RESERVE_BANK_INCLUDED
