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
    int timeLimit = 500;
    clock_t searchTime = clock() + timeLimit * CLOCKS_PER_SEC / 1000;
    int round = 0;
    int sampleCount = 0;
    double totalDealWeight = 0.0;

    while (clock() < searchTime)
    {
        vector<InferredDeal> deals = buildRandomDeals(state, 1);
        if (deals.empty())
            continue;

        InferredDeal &deal = deals[0];
        ++sampleCount;
        totalDealWeight += deal.weight;

        for (int i = 0; i < static_cast<int>(topPlays.size()); ++i)
        {
            if (clock() >= searchTime)
                break;

            int index = (round + i) % static_cast<int>(topPlays.size());
            ScoredPlay &scored = topPlays[index];
            RResult rolloutResult = simulateDealAfterPlay(state, deal, scored.play, searchTime);

            scored.sampleScore += rolloutResult.score * deal.weight;
            scored.weight += deal.weight;
            scored.visits++;
        }

        if (!topPlays.empty())
            round = (round + 1) % static_cast<int>(topPlays.size());
    }

    struct ProbeRow
    {
        int index = 0;
        double avgSampleScore = 0.0;
        double trust = 0.0;
        double sampleWeight = 0.0;
        double finalScore = 0.0;
    };

    vector<ProbeRow> rows;
    int bestIndex = 0;
    double bestScore = -1e18;

    for (int i = 0; i < static_cast<int>(topPlays.size()); ++i)
    {
        ScoredPlay &scored = topPlays[i];

        double avgSampleScore = 0.0;
        if (scored.weight > 0.0)
            avgSampleScore = scored.sampleScore / scored.weight;

        double trust = 0.0;
        if (scored.visits > 0)
            trust = scored.visits / (scored.visits + 6.0);

        double sampleWeight = getSampleWeight(state, scored.play) * trust;
        double finalScore = scored.score + avgSampleScore * sampleWeight;

        rows.push_back({i, avgSampleScore, trust, sampleWeight, finalScore});

        if (finalScore > bestScore)
        {
            bestScore = finalScore;
            bestIndex = i;
        }
    }

    std::sort(rows.begin(), rows.end(), [](const ProbeRow &a, const ProbeRow &b)
    {
        if (a.finalScore != b.finalScore)
            return a.finalScore > b.finalScore;
        return a.index < b.index;
    });

    cout << "seed=20260508\n";
    cout << "validPlays=" << validPlays.size() << '\n';
    cout << "sampleCount=" << sampleCount << '\n';
    cout << "totalDealWeight=" << fixed << setprecision(3) << totalDealWeight << '\n';
    cout << "freeTurn=" << (state.lastValidCombo.comboType == CardComboType::PASS) << '\n';
    cout << "dangerous=" << isDangerousSituation(state) << '\n';
    cout << "topK\n";
    cout << "rank\tplay\theuristicScore\tavgSampleScore\tvisits\ttrust\ttotalDealWeight\teffectiveSampleWeight\tfinalScore\n";

    for (size_t rank = 0; rank < rows.size(); ++rank)
    {
        ProbeRow &row = rows[rank];
        ScoredPlay &scored = topPlays[row.index];

        cout << (rank + 1) << '\t'
             << comboSummary(scored.play) << '\t'
             << fixed << setprecision(3) << scored.score << '\t'
             << fixed << setprecision(3) << row.avgSampleScore << '\t'
             << scored.visits << '\t'
             << fixed << setprecision(3) << row.trust << '\t'
             << fixed << setprecision(3) << totalDealWeight << '\t'
             << fixed << setprecision(3) << row.sampleWeight << '\t'
             << fixed << setprecision(3) << row.finalScore << '\n';
    }

    CardCombo bestPlay;
    if (!topPlays.empty())
        bestPlay = topPlays[bestIndex].play;
    cout << "bestPlay=" << comboSummary(bestPlay) << '\n';
    return 0;
}
