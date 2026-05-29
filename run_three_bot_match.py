import argparse
import itertools
import sys
from pathlib import Path

import run_reference_match as base


BOT_TARGETS = {
    "TEST": ("test.cpp", "match_test_tri.exe"),
    "TEST3": ("Test3.cpp", "match_test3_tri.exe"),
    "TEST3BEFORE": ("Test3Before.cpp", "match_test3before_tri.exe"),
    "REF": ("Reference.cpp", "match_reference_tri.exe"),
}


def parse_bot_names(bot_text):
    bot_names = tuple(part.strip().upper() for part in bot_text.split(",") if part.strip())
    if len(bot_names) != 3:
        raise RuntimeError("--bots must specify exactly 3 bot names.")
    if len(set(bot_names)) != len(bot_names):
        raise RuntimeError("--bots does not allow duplicates.")
    unknown = [name for name in bot_names if name not in BOT_TARGETS]
    if unknown:
        raise RuntimeError(f"Unknown bot names: {', '.join(unknown)}")
    return bot_names


def build_targets(workspace, compiler, bot_names):
    exes = {}
    for name in bot_names:
        source_name, exe_name = BOT_TARGETS[name]
        exe_path = workspace / exe_name
        base.run_command(
            [compiler, "-std=c++17", "-O2", source_name, "-o", str(exe_path)],
            workspace,
        )
        exes[name] = exe_path
    return exes


def make_bots(permutation, exes):
    return [{"name": name, "exe": exes[name]} for name in permutation]


def won_bot_names(bots, winner, landlord):
    if winner is None:
        return set()
    winner_team = base.team_name(winner, landlord)
    return {
        bots[player]["name"]
        for player in range(base.PLAYER_COUNT)
        if base.team_name(player, landlord) == winner_team
    }


def opposing_winner(player, landlord):
    if player == landlord:
        return (landlord + 1) % base.PLAYER_COUNT
    return landlord


def force_crash_loss(log, bots, stage, player, landlord, exc):
    forced_landlord = player if stage == "bid" else landlord
    forced_winner = opposing_winner(player, forced_landlord)
    error_line = str(exc).splitlines()[0] if str(exc) else repr(exc)
    log.append(
        f"{stage.upper()} CRASH P{player} {bots[player]['name']} landlord=P{forced_landlord} error={error_line}"
    )
    log.append(
        f"FORCED_RESULT winner=P{forced_winner} winnerBot={bots[forced_winner]['name']} "
        f"winnerTeam={base.team_name(forced_winner, forced_landlord)}"
    )
    illegal = (
        f"bot_crash stage={stage} P{player} bot={bots[player]['name']} "
        f"detail={error_line}"
    )
    return forced_landlord, forced_winner, illegal


def run_one_game(args, bots, game_index, seed):
    rng = base.random.Random(seed)
    deck = list(range(54))
    rng.shuffle(deck)
    dealt_hands = [base.sorted_cards(deck[i * 17:(i + 1) * 17]) for i in range(base.PLAYER_COUNT)]
    public_cards = base.sorted_cards(deck[51:54])

    log = []
    log.append(f"================ GAME {game_index} seed={seed} ================")
    log.append("bots: " + ", ".join(f"P{idx}={bot['name']}" for idx, bot in enumerate(bots)))
    for player in range(base.PLAYER_COUNT):
        log.append(
            f"initial P{player} {bots[player]['name']} hand={base.card_names(dealt_hands[player])} {base.cards_id_text(dealt_hands[player])}"
        )
    log.append(f"public={base.card_names(public_cards)} {base.cards_id_text(public_cards)}")

    conversations = [{"requests": [], "responses": []} for _ in range(base.PLAYER_COUNT)]
    bids = []
    max_bid = 0
    landlord = 0
    final_bid = 0
    winner = None
    illegal = None
    forced_by_crash = False

    for player in range(base.PLAYER_COUNT):
        payload = base.bid_payload(dealt_hands[player], bids)
        try:
            parsed, stdout, stderr, elapsed_ms, input_text = base.call_bot(
                bots[player]["exe"],
                payload,
                args.timeout,
            )
        except Exception as exc:
            landlord, winner, illegal = force_crash_loss(log, bots, "bid", player, landlord, exc)
            forced_by_crash = True
            break
        bid, ok, reason = base.validate_bid(parsed.get("response"), max_bid)

        conversations[player]["requests"].append(payload["requests"][0])
        conversations[player]["responses"].append(bid)
        bids.append(bid)

        if ok and bid > max_bid:
            max_bid = bid
            landlord = player
            final_bid = bid

        log.append(
            f"BID P{player} {bots[player]['name']} bid={bid} ok={ok} "
            f"elapsedMs={elapsed_ms:.1f} reason={reason}"
        )
        if stderr:
            log.append(f"BID STDERR P{player}: {stderr}")
        if not ok:
            raise RuntimeError("\n".join(log + [f"Invalid bid input={input_text}", f"stdout={stdout}"]))

        if bid == 3:
            break

    if not forced_by_crash and max_bid == 0:
        landlord = 0
        final_bid = 0

    hands = [
        base.hand_with_public(player, dealt_hands, public_cards, landlord)
        for player in range(base.PLAYER_COUNT)
    ]
    log.append(f"landlord=P{landlord} landlordBot={bots[landlord]['name']} finalBid={final_bid}")
    for player in range(base.PLAYER_COUNT):
        log.append(
            f"playStart P{player} count={len(hands[player])} hand={base.card_names(hands[player])} {base.cards_id_text(hands[player])}"
        )

    first_play_request = [True, True, True]
    last_action = [[], [], []]
    last_valid_combo = base.Combo([], base.ComboType.PASS, 0, [])
    last_valid_player = -1
    pass_count = 0
    current_player = landlord
    multiplier = 1
    landlord_non_opening_plays = 0
    farmer_played_any = False
    landlord_play_count = 0

    if not forced_by_crash:
        for turn in range(1, args.max_turns + 1):
            player = current_player
            bot = bots[player]
            history_pair = [
                list(last_action[(player - 2) % base.PLAYER_COUNT]),
                list(last_action[(player - 1) % base.PLAYER_COUNT]),
            ]
            request = base.play_request(
                player,
                dealt_hands[player],
                public_cards,
                landlord,
                final_bid,
                history_pair,
                first_play_request[player],
            )
            first_play_request[player] = False
            conversations[player]["requests"].append(request)

            payload = {
                "requests": conversations[player]["requests"],
                "responses": conversations[player]["responses"],
            }

            try:
                parsed, stdout, stderr, elapsed_ms, input_text = base.call_bot(
                    bot["exe"],
                    payload,
                    args.timeout,
                )
            except Exception as exc:
                landlord, winner, illegal = force_crash_loss(log, bots, "play", player, landlord, exc)
                forced_by_crash = True
                break

            action = parsed.get("response", [])
            if not isinstance(action, list):
                illegal = f"response_not_list P{player} response={action!r}"
                break
            action = [int(card) for card in action]

            action_combo = base.detect_combo(action)
            legal, reason = base.is_legal_play(hands[player], action, last_valid_combo)

            before_hand = list(hands[player])
            conversations[player]["responses"].append(list(action))

            log.append("")
            log.append(
                f"TURN {turn} P{player} {bot['name']} team={base.team_name(player, landlord)} "
                f"need={base.combo_text(last_valid_combo)} lastValidPlayer={last_valid_player} "
                f"passCount={pass_count} elapsedMs={elapsed_ms:.1f}"
            )
            log.append(f"  handBefore count={len(before_hand)} {base.card_names(before_hand)} {base.cards_id_text(before_hand)}")
            log.append(f"  historyForBot={base.compact_json(history_pair)}")
            log.append(f"  action={base.combo_text(action_combo)} legal={legal} reason={reason}")
            log.append(f"  rawOutput={stdout}")
            if stderr:
                log.append(f"  stderr={stderr}")
            if args.log_json:
                log.append(f"  inputJson={input_text}")

            if not legal:
                illegal = f"illegal_play turn={turn} P{player} reason={reason}"
                log.append("  ILLEGAL_STOP " + illegal)
                break

            if action_combo.combo_type in (base.ComboType.BOMB, base.ComboType.ROCKET):
                multiplier *= 2

            hands[player] = base.remove_cards(hands[player], action)
            last_action[player] = list(action)

            if action_combo.combo_type == base.ComboType.PASS:
                pass_count += 1
            else:
                pass_count = 0
                last_valid_combo = action_combo
                last_valid_player = player
                if player == landlord:
                    landlord_play_count += 1
                    if landlord_play_count > 1:
                        landlord_non_opening_plays += 1
                else:
                    farmer_played_any = True

            if pass_count >= 2:
                last_valid_combo = base.Combo([], base.ComboType.PASS, 0, [])
                last_valid_player = -1
                pass_count = 0

            log.append(
                "  remain=" + ", ".join(f"P{idx}:{len(hands[idx])}" for idx in range(base.PLAYER_COUNT))
            )

            if not hands[player]:
                winner = player
                log.append(f"WINNER P{player} {bot['name']} team={base.team_name(player, landlord)}")
                break

            current_player = (current_player + 1) % base.PLAYER_COUNT

    if winner is None and illegal is None:
        illegal = f"max_turns_reached maxTurns={args.max_turns}"
        log.append("STOP " + illegal)

    winner_bot = bots[winner]["name"] if winner is not None else "NONE"
    bot_wins = {bot["name"]: False for bot in bots}
    if winner is not None:
        winner_team = base.team_name(winner, landlord)
        spring = False
        anti_spring = False
        if not forced_by_crash:
            if winner_team == "landlord" and not farmer_played_any:
                spring = True
                multiplier *= 2
            if winner_team == "farmers" and landlord_non_opening_plays == 0:
                anti_spring = True
                multiplier *= 2

        winners = won_bot_names(bots, winner, landlord)
        bot_wins = {name: name in winners for name in bot_wins}
        bot_win_text = " ".join(f"{name.lower()}Won={bot_wins[name]}" for name in bot_wins)
        log.append(
            f"SUMMARY winner=P{winner} winnerBot={winner_bot} winnerTeam={winner_team} "
            f"landlord=P{landlord} landlordBot={bots[landlord]['name']} finalBid={final_bid} "
            f"multiplier={multiplier} spring={spring} antiSpring={anti_spring} forcedByCrash={forced_by_crash} "
            f"{bot_win_text}"
        )
    else:
        log.append(
            f"SUMMARY illegal={illegal} landlord=P{landlord} landlordBot={bots[landlord]['name']} finalBid={final_bid}"
        )

    return {
        "log": "\n".join(log),
        "winner": winner,
        "winner_bot": winner_bot,
        "landlord": landlord,
        "illegal": illegal,
        "bot_wins": bot_wins,
    }


def parse_args():
    parser = argparse.ArgumentParser(description="Run local FightTheLandlord2 matches: TEST vs TEST3 vs REF.")
    parser.add_argument("--bots", default="TEST,TEST3,REF", help="Comma-separated bot names. Supported: TEST, TEST3, TEST3BEFORE, REF.")
    parser.add_argument("--games", type=int, default=30, help="Number of games to run.")
    parser.add_argument("--seed", type=int, default=20260830, help="Base random seed.")
    parser.add_argument("--timeout", type=float, default=30.0, help="Per bot call timeout in seconds.")
    parser.add_argument("--max-turns", type=int, default=300, help="Stop a game after this many turns.")
    parser.add_argument("--paired-seeds", action="store_true", help="Reuse each seed across all seat permutations before moving to the next seed.")
    parser.add_argument("--no-build", action="store_true", help="Use existing match_*_tri.exe files.")
    parser.add_argument("--log-json", action="store_true", help="Write full JSON input for every turn.")
    parser.add_argument("--log", default="reference_match_log_test_vs_test3_vs_ref.txt", help="Output log path.")
    return parser.parse_args()


def main():
    args = parse_args()
    workspace = Path(__file__).resolve().parent
    bot_names = parse_bot_names(args.bots)

    if args.no_build:
        exes = {
            name: workspace / BOT_TARGETS[name][1]
            for name in bot_names
        }
    else:
        compiler = base.find_compiler()
        exes = build_targets(workspace, compiler, bot_names)

    for name, exe_path in exes.items():
        if not exe_path.exists():
            raise RuntimeError(f"Missing executable for {name}: {exe_path}")

    permutations = list(itertools.permutations(bot_names))
    all_logs = []
    win_counts = {name: 0 for name in bot_names}
    illegal_count = 0
    role_counts = {
        name: {
            "landlord_games": 0,
            "landlord_wins": 0,
            "farmer_games": 0,
            "farmer_wins": 0,
        }
        for name in bot_names
    }

    if args.paired_seeds and args.games % len(permutations) != 0:
        raise RuntimeError("--paired-seeds requires games to be a multiple of 6.")

    if args.paired_seeds:
        schedule = []
        group_count = args.games // len(permutations)
        game_no = 0
        for seed_offset in range(group_count):
            seed = args.seed + seed_offset
            for permutation in permutations:
                game_no += 1
                schedule.append((game_no, seed, permutation))
    else:
        schedule = []
        for game_index in range(args.games):
            schedule.append((game_index + 1, args.seed + game_index, permutations[game_index % len(permutations)]))

    for game_no, seed, permutation in schedule:
        bots = make_bots(permutation, exes)
        result = run_one_game(args, bots, game_no, seed)
        all_logs.append(result["log"])
        if result["illegal"]:
            illegal_count += 1

        landlord = result["landlord"]
        for player, bot in enumerate(bots):
            name = bot["name"]
            is_landlord = player == landlord
            if is_landlord:
                role_counts[name]["landlord_games"] += 1
                if result["bot_wins"][name]:
                    role_counts[name]["landlord_wins"] += 1
            else:
                role_counts[name]["farmer_games"] += 1
                if result["bot_wins"][name]:
                    role_counts[name]["farmer_wins"] += 1
            if result["bot_wins"][name]:
                win_counts[name] += 1

        bot_win_text = " ".join(f"{name.lower()}Won={result['bot_wins'][name]}" for name in bot_names)
        print(
            f"game={game_no} seed={seed} seats="
            + ",".join(f"P{idx}:{bot['name']}" for idx, bot in enumerate(bots))
            + f" winner=P{result['winner']} winnerBot={result['winner_bot']} illegal={result['illegal']}"
            + f" {bot_win_text}"
        )

    summary_lines = [
        "================ TRI MATCH SUMMARY ================",
        f"games={args.games} illegalOrStopped={illegal_count} seedBase={args.seed} bots={','.join(bot_names)}",
    ]
    for name in bot_names:
        overall_rate = 100.0 * win_counts[name] / args.games if args.games else 0.0
        landlord_games = role_counts[name]["landlord_games"]
        farmer_games = role_counts[name]["farmer_games"]
        landlord_rate = 100.0 * role_counts[name]["landlord_wins"] / landlord_games if landlord_games else 0.0
        farmer_rate = 100.0 * role_counts[name]["farmer_wins"] / farmer_games if farmer_games else 0.0
        summary_lines.append(
            f"{name} wins={win_counts[name]}/{args.games} winRate={overall_rate:.1f}% "
            f"landlord={role_counts[name]['landlord_wins']}/{landlord_games} landlordRate={landlord_rate:.1f}% "
            f"farmers={role_counts[name]['farmer_wins']}/{farmer_games} farmerRate={farmer_rate:.1f}%"
        )

    output_text = "\n".join(summary_lines) + "\n\n" + "\n\n".join(all_logs) + "\n"
    log_path = Path(args.log)
    if not log_path.is_absolute():
        log_path = workspace / log_path
    log_path.write_text(output_text, encoding="utf-8")
    print(f"log={log_path}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        raise
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)