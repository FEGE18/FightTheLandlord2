#!/usr/bin/env python3
import argparse
import json
import os
import random
import subprocess
import sys
import time
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


PLAYER_COUNT = 3
CARD_JOKER = 52
CARD_JOKER_BIG = 53
MAX_LEVEL = 15
MAX_STRAIGHT_LEVEL = 11
LEVEL_JOKER = 13
LEVEL_JOKER_BIG = 14

RANK_NAMES = [
    "3", "4", "5", "6", "7", "8", "9", "10",
    "J", "Q", "K", "A", "2", "joker", "JOKER",
]


class ComboType:
    PASS = "PASS"
    SINGLE = "SINGLE"
    PAIR = "PAIR"
    STRAIGHT = "STRAIGHT"
    STRAIGHT2 = "STRAIGHT2"
    TRIPLET = "TRIPLET"
    TRIPLET1 = "TRIPLET1"
    TRIPLET2 = "TRIPLET2"
    BOMB = "BOMB"
    QUADRUPLE2 = "QUADRUPLE2"
    QUADRUPLE4 = "QUADRUPLE4"
    PLANE = "PLANE"
    PLANE1 = "PLANE1"
    PLANE2 = "PLANE2"
    SSHUTTLE = "SSHUTTLE"
    SSHUTTLE2 = "SSHUTTLE2"
    SSHUTTLE4 = "SSHUTTLE4"
    ROCKET = "ROCKET"
    INVALID = "INVALID"


@dataclass
class Combo:
    cards: list
    combo_type: str
    combo_level: int = 0
    packs: list = None

    def find_max_seq(self):
        if not self.packs:
            return 0
        for index in range(1, len(self.packs)):
            if self.packs[index][1] != self.packs[0][1]:
                return index
            if self.packs[index][0] != self.packs[index - 1][0] - 1:
                return index
        return len(self.packs)


def card_level(card):
    return card // 4 + (1 if card == CARD_JOKER_BIG else 0)


def card_sort_key(card):
    return (card_level(card), card)


def sorted_cards(cards):
    return sorted(cards, key=card_sort_key)


def card_names(cards):
    if not cards:
        return "PASS"
    counts = Counter(card_level(card) for card in cards)
    parts = []
    for level in range(MAX_LEVEL):
        count = counts.get(level, 0)
        if count:
            parts.append(f"{RANK_NAMES[level]}x{count}")
    return " ".join(parts)


def cards_id_text(cards):
    return "[" + ",".join(str(card) for card in cards) + "]"


def detect_combo(cards):
    cards = list(cards)
    if not cards:
        return Combo(cards=[], combo_type=ComboType.PASS, combo_level=0, packs=[])

    counts = Counter(card_level(card) for card in cards)
    packs = sorted(
        [(level, count) for level, count in counts.items()],
        key=lambda item: (-item[1], -item[0]),
    )
    combo_level = packs[0][0]
    count_of_count = Counter(counts.values())
    kinds = sorted(count_of_count.keys())

    combo = Combo(cards=cards, combo_type=ComboType.INVALID, combo_level=combo_level, packs=packs)

    if len(kinds) == 1:
        count_kind = kinds[0]
        curr = count_of_count[count_kind]
        if count_kind == 1:
            if curr == 1:
                combo.combo_type = ComboType.SINGLE
            elif curr == 2 and packs[1][0] == LEVEL_JOKER:
                combo.combo_type = ComboType.ROCKET
            elif curr >= 5 and combo.find_max_seq() == curr and packs[0][0] <= MAX_STRAIGHT_LEVEL:
                combo.combo_type = ComboType.STRAIGHT
        elif count_kind == 2:
            if curr == 1:
                combo.combo_type = ComboType.PAIR
            elif curr >= 3 and combo.find_max_seq() == curr and packs[0][0] <= MAX_STRAIGHT_LEVEL:
                combo.combo_type = ComboType.STRAIGHT2
        elif count_kind == 3:
            if curr == 1:
                combo.combo_type = ComboType.TRIPLET
            elif combo.find_max_seq() == curr and packs[0][0] <= MAX_STRAIGHT_LEVEL:
                combo.combo_type = ComboType.PLANE
        elif count_kind == 4:
            if curr == 1:
                combo.combo_type = ComboType.BOMB
            elif combo.find_max_seq() == curr and packs[0][0] <= MAX_STRAIGHT_LEVEL:
                combo.combo_type = ComboType.SSHUTTLE
        return combo

    if len(kinds) == 2:
        small_kind = kinds[0]
        large_kind = kinds[1]
        curr = count_of_count[large_kind]
        lesser = count_of_count[small_kind]

        if large_kind == 3:
            if small_kind == 1:
                if curr == 1 and lesser == 1:
                    combo.combo_type = ComboType.TRIPLET1
                elif combo.find_max_seq() == curr and lesser == curr and packs[0][0] <= MAX_STRAIGHT_LEVEL:
                    combo.combo_type = ComboType.PLANE1
            elif small_kind == 2:
                if curr == 1 and lesser == 1:
                    combo.combo_type = ComboType.TRIPLET2
                elif combo.find_max_seq() == curr and lesser == curr and packs[0][0] <= MAX_STRAIGHT_LEVEL:
                    combo.combo_type = ComboType.PLANE2

        elif large_kind == 4:
            if small_kind == 1:
                if curr == 1 and lesser == 2:
                    combo.combo_type = ComboType.QUADRUPLE2
                elif combo.find_max_seq() == curr and lesser == curr * 2 and packs[0][0] <= MAX_STRAIGHT_LEVEL:
                    combo.combo_type = ComboType.SSHUTTLE2
            elif small_kind == 2:
                if curr == 1 and lesser == 2:
                    combo.combo_type = ComboType.QUADRUPLE4
                elif combo.find_max_seq() == curr and lesser == curr * 2 and packs[0][0] <= MAX_STRAIGHT_LEVEL:
                    combo.combo_type = ComboType.SSHUTTLE4
        return combo

    return combo


def combo_text(combo):
    if combo.combo_type == ComboType.PASS:
        return "PASS"
    return f"{combo.combo_type} {card_names(combo.cards)} {cards_id_text(combo.cards)} level={combo.combo_level}"


def can_beat(required, candidate):
    if required.combo_type == ComboType.INVALID or candidate.combo_type == ComboType.INVALID:
        return False
    if candidate.combo_type == ComboType.ROCKET:
        return True
    if candidate.combo_type == ComboType.BOMB:
        if required.combo_type == ComboType.ROCKET:
            return False
        if required.combo_type == ComboType.BOMB:
            return candidate.combo_level > required.combo_level
        return True
    return (
        candidate.combo_type == required.combo_type
        and len(candidate.cards) == len(required.cards)
        and candidate.combo_level > required.combo_level
    )


def is_legal_play(hand, action, last_valid_combo):
    hand_counts = Counter(hand)
    action_counts = Counter(action)
    for card, count in action_counts.items():
        if hand_counts[card] < count:
            return False, f"card_not_in_hand card={card}"

    action_combo = detect_combo(action)
    if last_valid_combo.combo_type == ComboType.PASS:
        if action_combo.combo_type == ComboType.PASS:
            return False, "cannot_pass_on_free_turn"
        if action_combo.combo_type == ComboType.INVALID:
            return False, "invalid_combo_on_free_turn"
        return True, ""

    if action_combo.combo_type == ComboType.PASS:
        return True, ""
    if action_combo.combo_type == ComboType.INVALID:
        return False, "invalid_combo"
    if not can_beat(last_valid_combo, action_combo):
        return False, f"cannot_beat required={combo_text(last_valid_combo)} action={combo_text(action_combo)}"
    return True, ""


def remove_cards(hand, action):
    remaining = list(hand)
    for card in action:
        remaining.remove(card)
    return sorted_cards(remaining)


def find_compiler():
    candidates = [
        os.environ.get("CXX"),
        "E:/mingw64/bin/g++.exe",
        "g++",
    ]
    for candidate in candidates:
        if not candidate:
            continue
        try:
            result = subprocess.run(
                [candidate, "--version"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=5,
            )
            if result.returncode == 0:
                return candidate
        except Exception:
            pass
    raise RuntimeError("No usable C++ compiler found. Set CXX or install g++.")


def run_command(args, cwd, timeout=120):
    result = subprocess.run(
        args,
        cwd=str(cwd),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
    )
    if result.returncode != 0:
        raise RuntimeError(
            "Command failed:\n"
            + " ".join(str(arg) for arg in args)
            + "\nSTDOUT:\n"
            + result.stdout
            + "\nSTDERR:\n"
            + result.stderr
        )
    return result


def build_targets(workspace, compiler, build_probe):
    our_exe = workspace / "match_our.exe"
    ref_exe = workspace / "match_reference.exe"
    probe_exe = workspace / "match_play_topk_probe.exe"

    run_command(
        [compiler, "-std=c++17", "-O2", "test.cpp", "-o", str(our_exe)],
        workspace,
    )
    run_command(
        [compiler, "-std=c++17", "-O2", "Reference.cpp", "-o", str(ref_exe)],
        workspace,
    )
    if build_probe:
        run_command(
            [compiler, "-std=c++17", "-O2", "play_topk_probe.cpp", "-o", str(probe_exe)],
            workspace,
        )

    return our_exe, ref_exe, probe_exe


def compact_json(value):
    return json.dumps(value, separators=(",", ":"), ensure_ascii=False)


def parse_bot_output(stdout):
    lines = [line.strip() for line in stdout.splitlines() if line.strip()]
    last_error = None
    for line in reversed(lines):
        try:
            return json.loads(line)
        except Exception as exc:
            last_error = exc
    raise ValueError(f"Bot did not print JSON. stdout={stdout!r}, last_error={last_error}")


def call_bot(exe_path, payload, timeout):
    input_text = compact_json(payload)
    started = time.perf_counter()
    result = subprocess.run(
        [str(exe_path)],
        input=input_text,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
    )
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    if result.returncode != 0:
        raise RuntimeError(
            f"Bot exited with code {result.returncode}\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}\nINPUT:\n{input_text}"
        )
    parsed = parse_bot_output(result.stdout)
    return parsed, result.stdout.strip(), result.stderr.strip(), elapsed_ms, input_text


def call_probe(probe_exe, payload, timeout):
    if not probe_exe:
        return ""
    result = subprocess.run(
        [str(probe_exe)],
        input=compact_json(payload),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
    )
    if result.returncode != 0:
        return "PROBE_FAILED\nSTDOUT:\n" + result.stdout + "\nSTDERR:\n" + result.stderr
    return result.stdout.strip()


def bid_payload(hand, bids_so_far):
    return {
        "requests": [
            {
                "own": list(hand),
                "bid": list(bids_so_far),
            }
        ],
        "responses": [],
    }


def play_request(player, dealt_hand, public_cards, landlord, final_bid, history_pair, first_play_request):
    request = {
        "history": [list(history_pair[0]), list(history_pair[1])],
    }
    if first_play_request:
        request.update(
            {
                "publiccard": list(public_cards),
                "own": list(dealt_hand),
                "landlord": landlord,
                "pos": player,
                "finalbid": final_bid,
            }
        )
    return request


def hand_with_public(player, dealt_hands, public_cards, landlord):
    hand = list(dealt_hands[player])
    if player == landlord:
        hand += list(public_cards)
    return sorted_cards(hand)


def team_name(player, landlord):
    return "landlord" if player == landlord else "farmers"


def make_bots(our_seat, our_exe, ref_exe):
    bots = []
    for player in range(PLAYER_COUNT):
        if player == our_seat:
            bots.append({"name": "OUR", "exe": our_exe})
        else:
            bots.append({"name": "REF", "exe": ref_exe})
    return bots


def validate_bid(raw_bid, current_max):
    try:
        bid = int(raw_bid)
    except Exception:
        return 0, False, f"bid_not_int value={raw_bid!r}"
    if bid == 0:
        return bid, True, ""
    if current_max < bid <= 3:
        return bid, True, ""
    return bid, False, f"invalid_bid bid={bid} currentMax={current_max}"


def run_one_game(args, workspace, our_exe, ref_exe, probe_exe, game_index, seed, our_seat):
    rng = random.Random(seed)
    deck = list(range(54))
    rng.shuffle(deck)
    dealt_hands = [sorted_cards(deck[i * 17:(i + 1) * 17]) for i in range(PLAYER_COUNT)]
    public_cards = sorted_cards(deck[51:54])
    bots = make_bots(our_seat, our_exe, ref_exe)

    log = []
    log.append(f"================ GAME {game_index} seed={seed} ourSeat={our_seat} ================")
    log.append("bots: " + ", ".join(f"P{idx}={bot['name']}" for idx, bot in enumerate(bots)))
    for player in range(PLAYER_COUNT):
        log.append(f"initial P{player} {bots[player]['name']} hand={card_names(dealt_hands[player])} {cards_id_text(dealt_hands[player])}")
    log.append(f"public={card_names(public_cards)} {cards_id_text(public_cards)}")

    conversations = [{"requests": [], "responses": []} for _ in range(PLAYER_COUNT)]
    bids = []
    bid_records = []
    max_bid = 0
    landlord = 0
    final_bid = 0

    for player in range(PLAYER_COUNT):
        payload = bid_payload(dealt_hands[player], bids)
        parsed, stdout, stderr, elapsed_ms, input_text = call_bot(
            bots[player]["exe"],
            payload,
            args.timeout,
        )
        bid, ok, reason = validate_bid(parsed.get("response"), max_bid)

        conversations[player]["requests"].append(payload["requests"][0])
        conversations[player]["responses"].append(bid)
        bids.append(bid)
        bid_records.append((player, bid, ok, reason, elapsed_ms))

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

    if max_bid == 0:
        landlord = 0
        final_bid = 0

    hands = [
        hand_with_public(player, dealt_hands, public_cards, landlord)
        for player in range(PLAYER_COUNT)
    ]
    log.append(f"landlord=P{landlord} finalBid={final_bid}")
    for player in range(PLAYER_COUNT):
        log.append(f"playStart P{player} count={len(hands[player])} hand={card_names(hands[player])} {cards_id_text(hands[player])}")

    first_play_request = [True, True, True]
    last_action = [[], [], []]
    last_valid_combo = Combo([], ComboType.PASS, 0, [])
    last_valid_player = -1
    pass_count = 0
    current_player = landlord
    winner = None
    illegal = None
    turn_records = []
    multiplier = 1
    landlord_non_opening_plays = 0
    farmer_played_any = False
    landlord_play_count = 0

    max_turns = args.max_turns
    for turn in range(1, max_turns + 1):
        player = current_player
        bot = bots[player]
        history_pair = [
            list(last_action[(player - 2) % PLAYER_COUNT]),
            list(last_action[(player - 1) % PLAYER_COUNT]),
        ]
        request = play_request(
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

        probe_text = ""
        if args.probe_ours and bot["name"] == "OUR":
            probe_text = call_probe(probe_exe, payload, args.timeout)

        parsed, stdout, stderr, elapsed_ms, input_text = call_bot(
            bot["exe"],
            payload,
            args.timeout,
        )
        action = parsed.get("response", [])
        if not isinstance(action, list):
            illegal = f"response_not_list P{player} response={action!r}"
            break
        action = [int(card) for card in action]

        action_combo = detect_combo(action)
        legal, reason = is_legal_play(hands[player], action, last_valid_combo)

        before_hand = list(hands[player])
        conversations[player]["responses"].append(list(action))

        log.append("")
        log.append(
            f"TURN {turn} P{player} {bot['name']} team={team_name(player, landlord)} "
            f"need={combo_text(last_valid_combo)} lastValidPlayer={last_valid_player} "
            f"passCount={pass_count} elapsedMs={elapsed_ms:.1f}"
        )
        log.append(f"  handBefore count={len(before_hand)} {card_names(before_hand)} {cards_id_text(before_hand)}")
        log.append(f"  historyForBot={compact_json(history_pair)}")
        log.append(f"  action={combo_text(action_combo)} legal={legal} reason={reason}")
        log.append(f"  rawOutput={stdout}")
        if stderr:
            log.append(f"  stderr={stderr}")
        if args.log_json:
            log.append(f"  inputJson={input_text}")
        if probe_text:
            log.append("  OUR_TOPK_BEGIN")
            for line in probe_text.splitlines():
                log.append("    " + line)
            log.append("  OUR_TOPK_END")

        if not legal:
            illegal = f"illegal_play turn={turn} P{player} reason={reason}"
            log.append("  ILLEGAL_STOP " + illegal)
            break

        if action_combo.combo_type in (ComboType.BOMB, ComboType.ROCKET):
            multiplier *= 2

        hands[player] = remove_cards(hands[player], action)
        last_action[player] = list(action)

        if action_combo.combo_type == ComboType.PASS:
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
            last_valid_combo = Combo([], ComboType.PASS, 0, [])
            last_valid_player = -1
            pass_count = 0

        log.append(
            "  remain="
            + ", ".join(f"P{idx}:{len(hands[idx])}" for idx in range(PLAYER_COUNT))
        )

        turn_records.append(
            {
                "turn": turn,
                "player": player,
                "bot": bot["name"],
                "action": list(action),
                "combo": action_combo.combo_type,
                "level": action_combo.combo_level,
                "remain": [len(hand) for hand in hands],
            }
        )

        if not hands[player]:
            winner = player
            log.append(f"WINNER P{player} {bot['name']} team={team_name(player, landlord)}")
            break

        current_player = (current_player + 1) % PLAYER_COUNT

    if winner is None and illegal is None:
        illegal = f"max_turns_reached maxTurns={max_turns}"
        log.append("STOP " + illegal)

    if winner is not None:
        winner_team = team_name(winner, landlord)
        spring = False
        anti_spring = False
        if winner_team == "landlord" and not farmer_played_any:
            spring = True
            multiplier *= 2
        if winner_team == "farmers" and landlord_non_opening_plays == 0:
            anti_spring = True
            multiplier *= 2
        log.append(
            f"SUMMARY winner=P{winner} winnerTeam={winner_team} ourSeat={our_seat} "
            f"ourTeam={team_name(our_seat, landlord)} ourWon={team_name(our_seat, landlord) == winner_team} "
            f"landlord=P{landlord} finalBid={final_bid} multiplier={multiplier} "
            f"spring={spring} antiSpring={anti_spring}"
        )
    else:
        log.append(f"SUMMARY illegal={illegal} ourSeat={our_seat} landlord=P{landlord}")

    if winner is not None and team_name(our_seat, landlord) != team_name(winner, landlord):
        log.append("OUR_LOSS_TURNS")
        for record in turn_records:
            if record["player"] == our_seat:
                log.append(
                    f"  turn={record['turn']} action={record['combo']} "
                    f"level={record['level']} cards={cards_id_text(record['action'])} "
                    f"remain={record['remain']}"
                )

    return {
        "log": "\n".join(log),
        "winner": winner,
        "landlord": landlord,
        "our_won": winner is not None and team_name(our_seat, landlord) == team_name(winner, landlord),
        "illegal": illegal,
    }


def parse_args():
    parser = argparse.ArgumentParser(description="Run local FightTheLandlord2 matches: OUR test.cpp vs Reference.cpp.")
    parser.add_argument("--games", type=int, default=1, help="Number of games to run.")
    parser.add_argument("--seed", type=int, default=20260510, help="Base random seed.")
    parser.add_argument("--our-seat", type=int, default=0, choices=[0, 1, 2], help="Seat used by our bot.")
    parser.add_argument("--rotate-seat", action="store_true", help="Rotate our seat across games.")
    parser.add_argument("--timeout", type=float, default=6.0, help="Per bot call timeout in seconds.")
    parser.add_argument("--max-turns", type=int, default=300, help="Stop a game after this many turns.")
    parser.add_argument("--no-build", action="store_true", help="Use existing match_*.exe files.")
    parser.add_argument("--no-probe-ours", dest="probe_ours", action="store_false", help="Do not dump TopK for our turns.")
    parser.add_argument("--log-json", action="store_true", help="Write full JSON input for every turn.")
    parser.add_argument("--log", default="reference_match_log.txt", help="Output log path.")
    return parser.parse_args()


def main():
    args = parse_args()
    workspace = Path(__file__).resolve().parent

    if args.no_build:
        our_exe = workspace / "match_our.exe"
        ref_exe = workspace / "match_reference.exe"
        probe_exe = workspace / "match_play_topk_probe.exe"
    else:
        compiler = find_compiler()
        our_exe, ref_exe, probe_exe = build_targets(workspace, compiler, args.probe_ours)

    if not our_exe.exists():
        raise RuntimeError(f"Missing our executable: {our_exe}")
    if not ref_exe.exists():
        raise RuntimeError(f"Missing reference executable: {ref_exe}")
    if args.probe_ours and not probe_exe.exists():
        raise RuntimeError(f"Missing probe executable: {probe_exe}")

    all_logs = []
    our_wins = 0
    illegal_count = 0
    for game_index in range(args.games):
        our_seat = (args.our_seat + game_index) % PLAYER_COUNT if args.rotate_seat else args.our_seat
        seed = args.seed + game_index
        result = run_one_game(args, workspace, our_exe, ref_exe, probe_exe, game_index + 1, seed, our_seat)
        all_logs.append(result["log"])
        if result["our_won"]:
            our_wins += 1
        if result["illegal"]:
            illegal_count += 1
        print(
            f"game={game_index + 1} seed={seed} ourSeat={our_seat} "
            f"ourWon={result['our_won']} winner=P{result['winner']} illegal={result['illegal']}"
        )

    summary = (
        f"================ MATCH SUMMARY ================\n"
        f"games={args.games} ourWins={our_wins} losses={args.games - our_wins - illegal_count} "
        f"illegalOrStopped={illegal_count} seedBase={args.seed}\n"
    )
    output_text = summary + "\n\n" + "\n\n".join(all_logs) + "\n"
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
