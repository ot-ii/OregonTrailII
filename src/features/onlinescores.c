#include "onlinescores.h"

#ifdef ONLINE_SCORES

#include <string.h>

#include "autopilot.h"
#include "../base/cstr.h"
#include "../base/date.h"
#include "../game/game.h"
#include "../game/member.h"
#include "onlinescoresnet.h"
#include "../game/party.h"
#include "../app/replay.h"
#include "../base/rng.h"
#include "../base/str.h"
#include "../app/version.h"

void OnlineScoresSubmitGame(struct game *game, const struct cstr *leader, int32_t score)
{
	struct os_score s;

	if (!game || !game->party || !game->date || !leader)
		return;

	if (RpMode() == RP_REPLAY)
		return;

#ifdef OTII_AUTO_MODE
	if (AutoPilotRunning() || AutoBatchActive())
		return;
#endif

	memset(&s, 0, sizeof(s));
	const char *name = CStrGet(leader);
	StrCopy(s.name, name ? name : "");
	s.score = score;
	s.level = game->player_level;
	s.occupation = game->party->occupation;
	s.destination = game->destination;
	s.year = game->date->year;
	s.month = game->date->month;
	s.day = game->date->day;
	s.days = game->days_traveled;
	s.party = game->party->member_count;

	for (int32_t i = 0; i < game->party->member_count && i < 6; ++i)
		if (game->party->members[i] && game->party->members[i]->state == MEMBER_ALIVE)
			++s.survivors;

	s.seed = RngSeed();
	StrCopy(s.version, GameVersionString());
	OsSubmitAsync(&s);
}

int32_t OnlineScoresTop10(char names[10][32], int32_t scores[10])
{
	return OsFetchTop(names, scores, 3000);
}

#endif
