/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header: /data/gemrb/cvs2svn/gemrb/gemrb/gemrb/plugins/Core/Dialog.cpp,v 1.10 2005/03/05 10:31:20 avenger_teambg Exp $
 *
 */

#include "../../includes/win32def.h"
#include "Dialog.h"
#include "GameScript.h"

Dialog::Dialog(void)
{
}

Dialog::~Dialog(void)
{
	for (unsigned int i = 0; i < initialStates.size(); i++) {
		if (initialStates[i]) {
			FreeDialogState( initialStates[i] );
		}
	}
}

void Dialog::AddState(DialogState* ds)
{
	initialStates.push_back( ds );
}

DialogState* Dialog::GetState(unsigned int index)
{
	if (index >= initialStates.size()) {
		return NULL;
	}
	return initialStates.at( index );
}

void Dialog::FreeDialogState(DialogState* ds)
{
	for (unsigned int i = 0; i < ds->transitionsCount; i++) {
		if (ds->transitions[i]->action)
			FreeDialogString( ds->transitions[i]->action );
		if (ds->transitions[i]->trigger)
			FreeDialogString( ds->transitions[i]->trigger );
		delete( ds->transitions[i] );
	}
	free( ds->transitions );
	if (ds->trigger) {
		FreeDialogString( ds->trigger );
	}
	delete( ds );
}

void Dialog::FreeDialogString(DialogString* ds)
{
	for (unsigned int i = 0; i < ds->count; i++) {
		if (ds->strings[i]) {
			free( ds->strings[i] );
		}
	}
	free( ds->strings );
	delete( ds );
}

int Dialog::FindFirstState(Scriptable* target)
{
	for (unsigned int i = 0; i < initialStates.size(); i++) {
		if (EvaluateDialogTrigger( target, GetState( i )->trigger )) {
			return i;
		}
	}
	return -1;
}

int Dialog::FindRandomState(Scriptable* target)
{
	unsigned int i;
	unsigned int max = initialStates.size();
	if (!max) return -1;
	unsigned int pick = rand()%max;
	for (i=pick; i < max; i++) {
		if (EvaluateDialogTrigger( target, GetState( i )->trigger )) {
			return i;
		}
	}
	for (i=0; i < pick; i++) {
		if (EvaluateDialogTrigger( target, GetState( i )->trigger )) {
			return i;
		}
	}
	return -1;
}

bool Dialog::EvaluateDialogTrigger(Scriptable* target, DialogString* trigger)
{
	int ORcount = 0;
	int result;
	bool subresult = true;

	if (!trigger) {
		return false;
	}
	for (unsigned int t = 0; t < trigger->count; t++) {
		result = GameScript::EvaluateString( target, trigger->strings[t] );
		if (result > 1) {
			if (ORcount)
				printf( "[Dialog]: Unfinished OR block encountered!\n" );
			ORcount = result;
			subresult = false;
			continue;
		}
		if (ORcount) {
			subresult |= ( result != 0 );
			if (--ORcount)
				continue;
			result = subresult ? 1 : 0;
		}
		if (!result)
			return 0;
	}
	if (ORcount) {
		printf( "[Dialog]: Unfinished OR block encountered!\n" );
	}
	return 1;
}
