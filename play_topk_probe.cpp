#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#define main landlord_bot_hidden_main
#include "test.cpp"
#undef main

using std::cout;
using std::fixed;
using std::getline;
using std::setprecision;
using std::string;
using std::vector;

static const char *kRankNames[] = {
    "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A", "2", "joker", "JOKER"
};

static GameState parseStateFromJson(const string &jsonText)
{
    std::istringstream input(jsonText);
    auto *oldBuf = std::cin.rdbuf(input.rdbuf());
    GameState state = readGameState();
    std::cin.rdbuf(oldBuf);
    return state;
}

static string cardsToIdString(const vector<Card> &cards)
{
    std::ostringstream out;
    for (size_t index = 0; index < cards.size(); ++index)
    {
        if (index)
            out << ',';
        out << cards[index];
    }
    return out.str();
}

static string handSummary(const vector<Card> &cards)
{
    if (cards.empty())
        return "PASS";

    int counts[MAX_LEVEL] = {};
    for (Card card : cards)
        ++counts[card2level(card)];

    std::ostringstream out;
    bool first = true;
    for (int level = 0; level < MAX_LEVEL; ++level)
    {
        if (counts[level] <= 0)
            continue;

        if (!first)
            out << ' ';
        first = false;
        out << kRankNames[level] << 'x' << counts[level];
    }
    return out.str();
}

static string comboSummary(const CardCombo &play)
{
    if (play.comboType == CardComboType::PASS)
        return "PASS";

    std::ostringstream out;
    out << cardComboStrings[static_cast<int>(play.comboType)]
        << " {" << handSummary(play.cards) << "}"
        << " [" << cardsToIdString(play.cards) << ']';
    return out.str();
}

static double effectiveSampleWeight(GameState &state, CardCombo &play)
{
    double sampleWeight = 0.4;

    if (state.lastValidCombo.comboType == CardComboType::PASS)
        sampleWeight = 0.1;

    if (isHardControlPlay(play) && !isDangerousSituation(state))
        sampleWeight = 0.0;

    return sampleWeight;
}

int main()
{
    std::srand(20260508);

    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    string jsonText = buffer.str();
    if (jsonText.empty())
    {
        std::cerr << "empty input" << std::endl;
        return 1;
    }

    GameState state = parseStateFromJson(jsonText);
    vector<CardCombo> validPlays = enumAllValidPlays(state.myCards, state.lastValidCombo);
    vector<ScoredPlay> topPlays = selectTopPlays(state, validPlays, 6);
    vector<InferredDeal> deals = buildRandomDeals(state, 20);

    double totalWeight = 0.0;
    for (const InferredDeal &deal : deals)
        totalWeight += deal.weight;

    cout << "seed=20260508\n";
    cout << "validPlays=" << validPlays.size() << '\n';
    cout << "sampleCount=" << deals.size() << '\n';
    cout << "totalDealWeight=" << fixed << setprecision(3) << totalWeight << '\n';
    cout << "freeTurn=" << (state.lastValidCombo.comboType == CardComboType::PASS) << '\n';
    cout << "dangerous=" << isDangerousSituation(state) << '\n';
    cout << "topK\n";
    cout << "rank\tplay\theuristicScore\tsampleScore\ttotalDealWeight\teffectiveSampleWeight\tfinalScore\n";

    for (size_t index = 0; index < topPlays.size(); ++index)
    {
        topPlays[index].sampleScore = evaluatePlayBySamples(state, deals, topPlays[index].play);
        double sampleWeight = effectiveSampleWeight(state, topPlays[index].play);
        double finalScore = topPlays[index].score + topPlays[index].sampleScore * sampleWeight;

        cout << (index + 1) << '\t'
             << comboSummary(topPlays[index].play) << '\t'
             << fixed << setprecision(3) << topPlays[index].score << '\t'
             << fixed << setprecision(3) << topPlays[index].sampleScore << '\t'
             << fixed << setprecision(3) << totalWeight << '\t'
             << fixed << setprecision(3) << sampleWeight << '\t'
             << fixed << setprecision(3) << finalScore << '\n';
    }

    CardCombo bestPlay = decidePlay(state, validPlays);
    cout << "bestPlay=" << comboSummary(bestPlay) << '\n';
    return 0;
}
