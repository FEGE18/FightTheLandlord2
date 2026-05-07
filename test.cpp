#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring> // 注意memset是cstring里的
#include <ctime>
#include <algorithm>
#include <vector>
#include "jsoncpp/json.h" // 在平台上，C++编译时默认包含此库

using std::set;
using std::sort;
using std::string;
using std::unique;
using std::vector;

constexpr int PLAYER_COUNT = 3;

enum class Stage
{
	BIDDING, // 叫分阶段
	PLAYING	 // 打牌阶段
};

enum class CardComboType
{
	PASS,		// 过
	SINGLE,		// 单张
	PAIR,		// 对子
	STRAIGHT,	// 顺子
	STRAIGHT2,	// 双顺
	TRIPLET,	// 三条
	TRIPLET1,	// 三带一
	TRIPLET2,	// 三带二
	BOMB,		// 炸弹
	QUADRUPLE2, // 四带二（只）
	QUADRUPLE4, // 四带二（对）
	PLANE,		// 飞机
	PLANE1,		// 飞机带小翼
	PLANE2,		// 飞机带大翼
	SSHUTTLE,	// 航天飞机
	SSHUTTLE2,	// 航天飞机带小翼
	SSHUTTLE4,	// 航天飞机带大翼
	ROCKET,		// 火箭
	INVALID		// 非法牌型
};

int cardComboScores[] = {
	0,	// 过
	1,	// 单张
	2,	// 对子
	6,	// 顺子
	6,	// 双顺
	4,	// 三条
	4,	// 三带一
	4,	// 三带二
	10, // 炸弹
	8,	// 四带二（只）
	8,	// 四带二（对）
	8,	// 飞机
	8,	// 飞机带小翼
	8,	// 飞机带大翼
	10, // 航天飞机（需要特判：二连为10分，多连为20分）
	10, // 航天飞机带小翼
	10, // 航天飞机带大翼
	16, // 火箭
	0	// 非法牌型
};

#ifndef _BOTZONE_ONLINE
string cardComboStrings[] = {
	"PASS",
	"SINGLE",
	"PAIR",
	"STRAIGHT",
	"STRAIGHT2",
	"TRIPLET",
	"TRIPLET1",
	"TRIPLET2",
	"BOMB",
	"QUADRUPLE2",
	"QUADRUPLE4",
	"PLANE",
	"PLANE1",
	"PLANE2",
	"SSHUTTLE",
	"SSHUTTLE2",
	"SSHUTTLE4",
	"ROCKET",
	"INVALID"};
#endif

// 用0~53这54个整数表示唯一的一张牌
using Card = short;
constexpr Card card_joker = 52;
constexpr Card card_JOKER = 53;

// 除了用0~53这54个整数表示唯一的牌，
// 这里还用另一种序号表示牌的大小（不管花色），以便比较，称作等级（Level）
// 对应关系如下：
// 3 4 5 6 7 8 9 10	J Q K	A	2	小王	大王
// 0 1 2 3 4 5 6 7	8 9 10	11	12	13	14
using Level = short;
constexpr Level MAX_LEVEL = 15;
constexpr Level MAX_STRAIGHT_LEVEL = 11;
constexpr Level level_joker = 13;
constexpr Level level_JOKER = 14;

/**
* 将Card变成Level
*/
// [示例程序提供，可直接复用] 把具体牌号转换成不区分花色的等级，供牌型识别和大小比较使用。
constexpr Level card2level(Card card)
{
	return card / 4 + card / 53;
}

// 牌的组合，用于计算牌型
struct CardCombo
{
	// 表示同等级的牌有多少张
	// 会按个数从大到小、等级从大到小排序
	struct CardPack
	{
		Level level;
		short count;

		// [示例程序提供，可直接复用] 定义牌种排序规则：先按张数降序，再按等级降序。
		bool operator<(const CardPack &b) const
		{
			if (count == b.count)
				return level > b.level;
			return count > b.count;
		}
	};
	vector<Card> cards;		 // 原始的牌，未排序
	vector<CardPack> packs;	 // 按数目和大小排序的牌种
	CardComboType comboType; // 算出的牌型
	Level comboLevel = 0;	 // 算出的大小序

	/**
						  * 检查个数最多的CardPack递减了几个
						  */
	// [示例程序提供，可直接复用] 统计主牌部分连续了多少组，用于判断顺子、飞机、航天飞机等连续牌型。
	int findMaxSeq() const
	{
		for (unsigned c = 1; c < packs.size(); c++)
			if (packs[c].count != packs[0].count ||
				packs[c].level != packs[c - 1].level - 1)
				return c;
		return packs.size();
	}

	/**
	* 这个牌型最后算总分的时候的权重
	*/
	// [示例程序提供，可直接复用] 返回当前牌型的基础权重，主要用于后续扩展评估或调试。
	int getWeight() const
	{
		if (comboType == CardComboType::SSHUTTLE ||
			comboType == CardComboType::SSHUTTLE2 ||
			comboType == CardComboType::SSHUTTLE4)
			return cardComboScores[(int)comboType] + (findMaxSeq() > 2) * 10;
		return cardComboScores[(int)comboType];
	}

	// 创建一个空牌组
	// [示例程序提供，可直接复用] 构造一个 PASS 牌组，表示不出牌。
	CardCombo() : comboType(CardComboType::PASS) {}

	/**
	* 通过Card（即short）类型的迭代器创建一个牌型
	* 并计算出牌型和大小序等
	* 假设输入没有重复数字（即重复的Card）
	*/
	template <typename CARD_ITERATOR>
	// [示例程序提供，可直接复用] 根据一组具体牌自动识别牌型、主牌等级和内部牌种结构。
	CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end)
	{
		// 特判：空
		if (begin == end)
		{
			comboType = CardComboType::PASS;
			return;
		}

		// 每种牌有多少个
		short counts[MAX_LEVEL + 1] = {};

		// 同种牌的张数（有多少个单张、对子、三条、四条）
		short countOfCount[5] = {};

		cards = vector<Card>(begin, end);
		for (Card c : cards)
			counts[card2level(c)]++;
		for (Level l = 0; l <= MAX_LEVEL; l++)
			if (counts[l])
			{
				packs.push_back(CardPack{l, counts[l]});
				countOfCount[counts[l]]++;
			}
		sort(packs.begin(), packs.end());

		// 用最多的那种牌总是可以比较大小的
		comboLevel = packs[0].level;

		// 计算牌型
		// 按照 同种牌的张数 有几种 进行分类
		vector<int> kindOfCountOfCount;
		for (int i = 0; i <= 4; i++)
			if (countOfCount[i])
				kindOfCountOfCount.push_back(i);
		sort(kindOfCountOfCount.begin(), kindOfCountOfCount.end());

		int curr, lesser;

		switch (kindOfCountOfCount.size())
		{
		case 1: // 只有一类牌
			curr = countOfCount[kindOfCountOfCount[0]];
			switch (kindOfCountOfCount[0])
			{
			case 1:
				// 只有若干单张
				if (curr == 1)
				{
					comboType = CardComboType::SINGLE;
					return;
				}
				if (curr == 2 && packs[1].level == level_joker)
				{
					comboType = CardComboType::ROCKET;
					return;
				}
				if (curr >= 5 && findMaxSeq() == curr &&
					packs.begin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::STRAIGHT;
					return;
				}
				break;
			case 2:
				// 只有若干对子
				if (curr == 1)
				{
					comboType = CardComboType::PAIR;
					return;
				}
				if (curr >= 3 && findMaxSeq() == curr &&
					packs.begin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::STRAIGHT2;
					return;
				}
				break;
			case 3:
				// 只有若干三条
				if (curr == 1)
				{
					comboType = CardComboType::TRIPLET;
					return;
				}
				if (findMaxSeq() == curr &&
					packs.begin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::PLANE;
					return;
				}
				break;
			case 4:
				// 只有若干四条
				if (curr == 1)
				{
					comboType = CardComboType::BOMB;
					return;
				}
				if (findMaxSeq() == curr &&
					packs.begin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::SSHUTTLE;
					return;
				}
			}
			break;
		case 2: // 有两类牌
			curr = countOfCount[kindOfCountOfCount[1]];
			lesser = countOfCount[kindOfCountOfCount[0]];
			if (kindOfCountOfCount[1] == 3)
			{
				// 三条带？
				if (kindOfCountOfCount[0] == 1)
				{
					// 三带一
					if (curr == 1 && lesser == 1)
					{
						comboType = CardComboType::TRIPLET1;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr &&
						packs.begin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::PLANE1;
						return;
					}
				}
				if (kindOfCountOfCount[0] == 2)
				{
					// 三带二
					if (curr == 1 && lesser == 1)
					{
						comboType = CardComboType::TRIPLET2;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr &&
						packs.begin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::PLANE2;
						return;
					}
				}
			}
			if (kindOfCountOfCount[1] == 4)
			{
				// 四条带？
				if (kindOfCountOfCount[0] == 1)
				{
					// 四条带两只 * n
					if (curr == 1 && lesser == 2)
					{
						comboType = CardComboType::QUADRUPLE2;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr * 2 &&
						packs.begin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::SSHUTTLE2;
						return;
					}
				}
				if (kindOfCountOfCount[0] == 2)
				{
					// 四条带两对 * n
					if (curr == 1 && lesser == 2)
					{
						comboType = CardComboType::QUADRUPLE4;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr * 2 &&
						packs.begin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::SSHUTTLE4;
						return;
					}
				}
			}
		}

		comboType = CardComboType::INVALID;
	}

	/**
	* 判断指定牌组能否大过当前牌组（这个函数不考虑过牌的情况！）
	*/
	// [示例程序提供，可直接复用] 判断另一手牌 b 是否能合法压过当前牌组。
	bool canBeBeatenBy(const CardCombo &b) const
	{
		if (comboType == CardComboType::INVALID || b.comboType == CardComboType::INVALID)
			return false;
		if (b.comboType == CardComboType::ROCKET)
			return true;
		if (b.comboType == CardComboType::BOMB)
			switch (comboType)
			{
			case CardComboType::ROCKET:
				return false;
			case CardComboType::BOMB:
				return b.comboLevel > comboLevel;
			default:
				return true;
			}
		return b.comboType == comboType && b.cards.size() == cards.size() && b.comboLevel > comboLevel;
	}

	/**
	* 从指定手牌中寻找第一个能大过当前牌组的牌组
	* 如果随便出的话只出第一张
	* 如果不存在则返回一个PASS的牌组
	*/
	template <typename CARD_ITERATOR>
	// [示例程序提供，可作为过渡方案] 从手牌里贪心找第一手能出的牌；后续主策略会逐步被 enumAllValidPlays 替代。
	// todo
	CardCombo findFirstValid(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		if (comboType == CardComboType::PASS) // 如果不需要大过谁，只需要随便出
		{
			CARD_ITERATOR second = begin;
			second++;
			return CardCombo(begin, second); // 那么就出第一张牌……
		}

		// 然后先看一下是不是火箭，是的话就过
		if (comboType == CardComboType::ROCKET)
			return CardCombo();

		// 现在打算从手牌中凑出同牌型的牌
		auto deck = vector<Card>(begin, end); // 手牌
		short counts[MAX_LEVEL + 1] = {};

		unsigned short kindCount = 0;

		// 先数一下手牌里每种牌有多少个
		for (Card c : deck)
			counts[card2level(c)]++;

		// 手牌如果不够用，直接不用凑了，看看能不能炸吧
		if (deck.size() < cards.size())
			goto failure;

		// 再数一下手牌里有多少种牌
		for (short c : counts)
			if (c)
				kindCount++;

		// 否则不断增大当前牌组的主牌，看看能不能找到匹配的牌组
		{
			// 开始增大主牌
			int mainPackCount = findMaxSeq();
			bool isSequential =
				comboType == CardComboType::STRAIGHT ||
				comboType == CardComboType::STRAIGHT2 ||
				comboType == CardComboType::PLANE ||
				comboType == CardComboType::PLANE1 ||
				comboType == CardComboType::PLANE2 ||
				comboType == CardComboType::SSHUTTLE ||
				comboType == CardComboType::SSHUTTLE2 ||
				comboType == CardComboType::SSHUTTLE4;
			for (Level i = 1;; i++) // 增大多少
			{
				for (int j = 0; j < mainPackCount; j++)
				{
					int level = packs[j].level + i;

					// 各种连续牌型的主牌不能到2，非连续牌型的主牌不能到小王，单张的主牌不能超过大王
					if ((comboType == CardComboType::SINGLE && level > MAX_LEVEL) ||
						(isSequential && level > MAX_STRAIGHT_LEVEL) ||
						(comboType != CardComboType::SINGLE && !isSequential && level >= level_joker))
						goto failure;

					// 如果手牌中这种牌不够，就不用继续增了
					if (counts[level] < packs[j].count)
						goto next;
				}

				{
					// 找到了合适的主牌，那么从牌呢？
					// 如果手牌的种类数不够，那从牌的种类数就不够，也不行
					if (kindCount < packs.size())
						continue;

					// 好终于可以了
					// 计算每种牌的要求数目吧
					short requiredCounts[MAX_LEVEL + 1] = {};
					for (int j = 0; j < mainPackCount; j++)
						requiredCounts[packs[j].level + i] = packs[j].count;
					for (unsigned j = mainPackCount; j < packs.size(); j++)
					{
						Level k;
						for (k = 0; k <= MAX_LEVEL; k++)
						{
							if (requiredCounts[k] || counts[k] < packs[j].count)
								continue;
							requiredCounts[k] = packs[j].count;
							break;
						}
						if (k == MAX_LEVEL + 1) // 如果是都不符合要求……就不行了
							goto next;
					}

					// 开始产生解
					vector<Card> solve;
					for (Card c : deck)
					{
						Level level = card2level(c);
						if (requiredCounts[level])
						{
							solve.push_back(c);
							requiredCounts[level]--;
						}
					}
					return CardCombo(solve.begin(), solve.end());
				}

			next:; // 再增大
			}
		}

	failure:
		// 实在找不到啊
		// 最后看一下能不能炸吧

		for (Level i = 0; i < level_joker; i++)
			if (counts[i] == 4 && (comboType != CardComboType::BOMB || i > packs[0].level)) // 如果对方是炸弹，能炸的过才行
			{
				// 还真可以啊……
				Card bomb[] = {Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3)};
				return CardCombo(bomb, bomb + 4);
			}

		// 有没有火箭？
		if (counts[level_joker] + counts[level_JOKER] == 2)
		{
			Card rocket[] = {card_joker, card_JOKER};
			return CardCombo(rocket, rocket + 2);
		}

		// ……
		return CardCombo();
	}

	// [示例程序提供，可直接复用] 本地调试时打印当前牌组的牌型和大小信息。
	void debugPrint()
	{
#ifndef _BOTZONE_ONLINE
		std::cout << "【" << cardComboStrings[(int)comboType] << "共" << cards.size() << "张，大小序" << comboLevel << "】";
#endif
	}
};

// ==================================================
// 单文件骨架：状态结构
// ==================================================

struct HandPlan
{
	vector<CardCombo> groups;
	int handCount = 0;
};

struct GameState
{
	Stage stage = Stage::BIDDING;
	int myPosition = 0;
	int landlordPosition = -1;
	int finalBid = -1;
	vector<int> bidHistory;
	vector<Card> myCards;
	vector<Card> publicCards;
	CardCombo lastValidCombo;
	int cardRemaining[PLAYER_COUNT] = {17, 17, 17};
	vector<vector<Card>> playHistory[PLAYER_COUNT];
	bool cardPlayed[54] = {};
	short levelRemaining[MAX_LEVEL] = {};

	// [我们实现] 判断当前 Bot 是否为地主，供策略层快速区分角色使用。
	bool isLandlord() const
	{
		return myPosition == landlordPosition;
	}

	// [我们实现] 判断指定位置的玩家是否是我的队友，目前主要用于农民配合逻辑。
	bool isTeammate(int pos) const
	{
		return !isLandlord() && pos != myPosition && pos != landlordPosition;
	}

	// [我们实现] 返回我的队友位置；如果我是地主则返回 -1。
	int getTeammatePos() const
	{
		if (isLandlord())
			return -1;
		for (int pos = 0; pos < PLAYER_COUNT; ++pos)
			if (pos != myPosition && pos != landlordPosition)
				return pos;
		return -1;
	}

	// [我们实现] 统计当前未知区域还剩多少张牌，后续可用于记牌和概率推断。
	int getUnknownCardCount() const
	{
		int total = 0;
		for (short count : levelRemaining)
			total += count;
		return total;
	}
};

// ==================================================
// 单文件骨架：基础工具
// ==================================================

// [我们实现] 按等级优先、牌号次之对手牌排序，统一后续枚举、输出和调试行为
// todo_done
void sortCards(vector<Card> &cards)
{
	sort(cards.begin(), cards.end(), [](Card left, Card right) {
		Level leftLevel = card2level(left);
		Level rightLevel = card2level(right);
		if (leftLevel == rightLevel)
			return left < right;	//升序排列
		return leftLevel < rightLevel;
	});
}

// [我们实现] 从一手牌里删掉已经打出的具体牌，返回删除后的新手牌副本。
// todo_done
vector<Card> removeCardsFromHand(vector<Card> hand, const vector<Card> &played)
{
	for (Card card : played)
	{
		auto position = std::find(hand.begin(), hand.end(), card);
		if (position != hand.end())
			hand.erase(position);
	}
	sortCards(hand);
	return hand;
}

// [我们实现] 按等级把手牌分组，方便做对子、三条、炸弹等统计和枚举。
// todo_done
vector<vector<Card> > groupCardsByLevel(const vector<Card> &hand)
{
	vector<vector<Card> > grouped(MAX_LEVEL);
	for (Card card : hand)
		grouped[card2level(card)].push_back(card);
	return grouped;
}

// ==================================================
// 单文件骨架：IO / 状态恢复
// ==================================================

// [基于示例程序逻辑改造，建议优先保留] 读取 Botzone 输入并重建当前局面，返回本轮决策所需的完整状态。
GameState readGameState()
{
	GameState state;

	string line;
	getline(std::cin, line);
	Json::Value input;
	Json::Reader reader;
	reader.parse(line, input);

	{
		auto firstRequest = input["requests"][0u];
		auto own = firstRequest["own"];
		for (unsigned i = 0; i < own.size(); ++i)
			state.myCards.push_back(own[i].asInt());

		if (!firstRequest["bid"].isNull())
		{
			auto bidArray = firstRequest["bid"];
			state.myPosition = static_cast<int>(bidArray.size());
			for (unsigned i = 0; i < bidArray.size(); ++i)
				state.bidHistory.push_back(bidArray[i].asInt());
		}
	}

	int whoInHistory[] = {
		(state.myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT,
		(state.myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT};

	int turn = input["requests"].size();
	for (int i = 0; i < turn; ++i)
	{
		auto request = input["requests"][i];
		auto llpublic = request["publiccard"];
		if (!llpublic.isNull())
		{
			state.landlordPosition = request["landlord"].asInt();
			state.finalBid = request["finalbid"].asInt();
			state.myPosition = request["pos"].asInt();
			whoInHistory[0] = (state.myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT;
			whoInHistory[1] = (state.myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT;
			state.cardRemaining[state.landlordPosition] += llpublic.size();
			for (unsigned index = 0; index < llpublic.size(); ++index)
			{
				Card card = llpublic[index].asInt();
				state.publicCards.push_back(card);
				if (state.landlordPosition == state.myPosition)
					state.myCards.push_back(card);
			}
		}

		auto history = request["history"];
		if (history.isNull())
			continue;

		state.stage = Stage::PLAYING;
		int howManyPass = 0;
		for (int offset = 0; offset < 2; ++offset)
		{
			int player = whoInHistory[offset];
			auto playerAction = history[offset];
			vector<Card> playedCards;
			for (unsigned cardIndex = 0; cardIndex < playerAction.size(); ++cardIndex)
			{
				Card card = playerAction[cardIndex].asInt();
				playedCards.push_back(card);
				state.cardPlayed[card] = true;
			}
			state.playHistory[player].push_back(playedCards);
			state.cardRemaining[player] -= static_cast<int>(playerAction.size());

			if (playedCards.empty())
				++howManyPass;
			else
				state.lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
		}

		if (howManyPass == 2)
			state.lastValidCombo = CardCombo();

		if (i < turn - 1)
		{
			auto playerAction = input["responses"][i];
			vector<Card> playedCards;
			for (unsigned cardIndex = 0; cardIndex < playerAction.size(); ++cardIndex)
			{
				Card card = playerAction[cardIndex].asInt();
				playedCards.push_back(card);
				state.cardPlayed[card] = true;
			}
			state.myCards = removeCardsFromHand(state.myCards, playedCards);
			state.playHistory[state.myPosition].push_back(playedCards);
			state.cardRemaining[state.myPosition] -= static_cast<int>(playerAction.size());
		}
	}

	sortCards(state.myCards);
	for (Card card = 0; card <= card_JOKER; ++card)
		++state.levelRemaining[card2level(card)];
	for (Card card : state.myCards)
		--state.levelRemaining[card2level(card)];
	for (int card = 0; card <= card_JOKER; ++card)
		if (state.cardPlayed[card])
			--state.levelRemaining[card2level(card)];
	if (!state.isLandlord())
		for (Card card : state.publicCards)
			--state.levelRemaining[card2level(card)];

	return state;
}

// [基于示例程序逻辑改造，建议优先保留] 按平台要求输出叫分决策 JSON。
void outputBid(int value)
{
	Json::Value result;
	result["response"] = value;
	Json::FastWriter writer;
	std::cout << writer.write(result) << std::endl;
}

// [基于示例程序逻辑改造，建议优先保留] 按平台要求输出出牌决策 JSON，空数组表示 PASS。
void outputPlay(const vector<Card> &cards)
{
	Json::Value result, response(Json::arrayValue);
	for (Card card : cards)
		response.append(card);
	result["response"] = response;
	Json::FastWriter writer;
	std::cout << writer.write(result) << std::endl;
}

// ==================================================
// 单文件骨架：枚举层
// ==================================================

// [我们要自己实现的核心函数] 枚举当前局面下所有合法出牌，供策略层评估和比较；如果没有合法出牌则返回一个只包含 PASS 的列表
// todo
/* 
实现逻辑：
先扫描一遍当前手牌，记录每个level各有几张牌，可以快速筛选可能的牌型；
然后定向枚举挑牌，比如单张、对子、三带一等等；每找到一种合法组合，就将其放入candidate中；
接着把candidate传入构造函数CardCombo(start,end)中，得到comboType和comboLevel；
最后判断这组candidate是否合法
*/
vector<CardCombo> enumAllValidPlays(const vector<Card> &hand, const CardCombo &lastCombo){
	vector<CardCombo> validPlays; // 可能的出牌列表
	validPlays.push_back(CardCombo());	// pass

	// 上家出火箭，直接pass
	if(lastCombo.comboType==CardComboType::ROCKET)	
		return validPlays;
	
	int counts[MAX_LEVEL+1]={0};	// 统计手牌中各个level的牌有多少张
	vector<Card> cardsByLevel[MAX_LEVEL+1];
	for(Card c:hand){
		Level l=card2level(c);
		counts[l]++;
		cardsByLevel[l].push_back(c);
	}

	set<string> uniqueFP;//！！！用于去重（同点数不同花色）！！！

	// 检查手牌，并将合法组合加入合法出牌序列（独立于上面所说的实现逻辑！）
	auto addPlay=[&](const vector<Card> candidates){
		CardCombo choice(candidates.begin(),candidates.end());
		// 上家未出牌，或自己的牌能大过上家，就将choice放入有效出牌序列中
		if(lastCombo.comboType==CardComboType::PASS||lastCombo.canBeBeatenBy(choice)){
			// 把“等级”&“点数”组合成指纹fp
			string fp="";
			for(auto pack:choice.packs){
				fp+=std::to_string(pack.level)+"&"+std::to_string(pack.count);	// 这个count是“等级为level的牌的张数”
			}

			if(uniqueFP.insert(fp).second){	// set不允许存储重复元素，仅当fp是首次被检测才将其存入
				//// if里的条件简析：
				//// 调用uniqueFP.insert(fp)时，它在把元素塞进去的同时返回一个std::pair<iterator, bool>类型的结果（一个包含两个元素的键值对）
				//// .first(一个iterator)是集合中实际存放这个fp的位置；
				//// .second(bool)标志这次插入是否成功
				validPlays.push_back(choice);
			}
		}
	};

	// 得到等级为l的前count张牌
	auto getCards=[&](Level l,int count){
		vector<Card> res;
		for(int i=0;i<count;i++)res.push_back(cardsByLevel[l][i]);
		return res;
	};

	// 当主体牌型确定后，为三带一、飞机带翼等牌型补全最合适的带牌
	// curCombo为主牌，need为需要带几组副牌（主要用于飞机和航天飞机，航天飞机的副牌组数是主牌组数的两倍，因为只能是四带二/四带两对）
	// type为带牌的种类（单张or对子），ex为被主牌占用的level（即副牌中不能出现的level种类）
	// 内部使用引用变量，直接在函数内对可行牌组进行插入
	auto selectAttachment=[&](vector<Card> curCombo,int need,int type,const set<Level>& ex){	
		vector<vector<Card> > res;	// 记录可行结果
		// 要在lambda内部调用自身，只能用self参数！！（因为在函数内部dfs自身仍未定义）
		// startL为初始的牌的等级，remain为还需要的副牌的张数，path为暂存结果
		auto dfs=[&](auto& self,int startL,int remain,vector<Card> path){
            if(remain==0){
                res.push_back(path);
                return;
            }
            for(Level i=startL;i<=MAX_LEVEL;i++){
				// 如果i不在ex（即主牌）中
                if(ex.count(i)==0&&counts[i]>=type){
                    vector<Card> nextPath=path;
                    for(int j=0;j<type;j++)nextPath.push_back(cardsByLevel[i][j]);
                    self(self,i+1,remain-1,nextPath);
                }
            }
        };
        dfs(dfs,0,need,curCombo);
        for(auto& aRes:res)addPlay(aRes);	// 使用&可优化性能？
	};

	// 定向枚举
	//// 炸弹+火箭
	auto getBombAndRocket=[&](){
		// bomb
		for(Level i=0;i<level_joker;i++){
			if(counts[i]==4)addPlay(getCards(i,4));
		}

		// rocket
		if(counts[level_JOKER]==1&&counts[level_joker]==1){
			vector<Card> rocket;
			rocket.push_back(cardsByLevel[level_JOKER][0]);
			rocket.push_back(cardsByLevel[level_joker][0]);
			addPlay(rocket);
		}
	};

	//// 单张
	auto getSingle=[&](){
		for(Level i=0;i<=MAX_LEVEL;i++){
			if(counts[i]>=1)addPlay(getCards(i,1));
		}
	};

	//// 对子
	auto getPair=[&](){
		for(Level i=0;i<level_joker;i++){
			if(counts[i]>=2)addPlay(getCards(i,2));
		}
	};

	//// 三/带一/对
	//// with:0=带零，1=带一，2=带一对
	auto getTriplet=[&](int with){
		for(Level i=0;i<level_joker;i++){
			if(counts[i]>=3){
				vector<Card> body=getCards(i,3);
				set<Level> ex={i};	//excluded-即带牌中不应出现的牌，也就是主牌的level
				if (with==0) addPlay(body);
                if (with==1) selectAttachment(body, 1, 1, ex);
                if (with==2) selectAttachment(body, 1, 2, ex);
			}
		}
	};

	//// “连续序列”型；type：1=单顺，2=双顺，3=飞机，4=航天飞机，同时type也代表所属牌型的主牌中各个card的张数
	//// 四种类型的主牌都不能有2(level<=MAX_STRAIGHT_LEVEL)
	//// minLen、maxLen分别标识对应牌型的主牌level长度限制
	auto getStraightAndPlane=[&](int minLen,int maxLen,int type){
		for(int l=minLen;l<=maxLen;l++){
			// 滑动窗口遍历，start标识窗口起点
			for(Level start=0;start<=MAX_STRAIGHT_LEVEL-l+1;start++){
				bool valid=true;
				for(int k=0;k<l;k++){
					// 判断起点是否有效
					if(counts[start+k]<type){
						valid=false;
						break;
					}
				}
				
				if(valid){
					vector<Card> body;
					set<Level> ex;
					// 如果连续l个点数的张数都达标，则分别调用getCards提取type张牌，加入body数组中
					for(int k=0;k<l;k++){
						vector<Card> cards=getCards(start+k,type);
						body.insert(body.end(),cards.begin(),cards.end());
						ex.insert(start+k);
					}

					// 单顺、双顺
					if(type==1||type==2){
						addPlay(body);
					}

					// 飞机
					if(type==3){
						addPlay(body);
						selectAttachment(body,l,1,ex);
						selectAttachment(body,l,2,ex);
					}

					// 航天飞机
					if(type==4){
						addPlay(body);
						selectAttachment(body,l*2,1,ex);
						selectAttachment(body,l*2,2,ex);
					}
				}
			}
		}
	};

	//// 四带二
	auto getQuadruple=[&]{
		for(Level i=0;i<level_joker;i++){
			if(counts[i]==4){
				vector<Card> body=getCards(i,4);
				set<Level> ex={i};
				addPlay(body);
				selectAttachment(body,2,1,ex);
				selectAttachment(body,2,2,ex);
			}
		}
	};

	// 调用lambda来枚举
	getBombAndRocket();
	
	if(lastCombo.comboType==CardComboType::PASS){
		getSingle();
		getPair();
		getTriplet(0);
		getTriplet(1);
		getTriplet(2);
		getStraightAndPlane(5,12,1);	// 单顺，主牌种数介于5～12
		getStraightAndPlane(3,10,2);	// 双顺，主牌的种数介于3～10（一个玩家最多只能有20张牌，也就是地主）
		getStraightAndPlane(2,6,3);		// 飞机，主牌的种数介于2～6
		getStraightAndPlane(2,5,4);		// 航天飞机
		getQuadruple();
	}else{
		switch(lastCombo.comboType){
			case CardComboType::SINGLE:		getSingle();break;

			case CardComboType::PAIR:		getPair();break;

			case CardComboType::TRIPLET:	getTriplet(0);break;
			case CardComboType::TRIPLET1:	getTriplet(1);break;
			case CardComboType::TRIPLET2:	getTriplet(2);break;

			// 顺子类的，长度和上家一样
            case CardComboType::STRAIGHT:    getStraightAndPlane(lastCombo.packs.size(), lastCombo.packs.size(), 1); break;
            case CardComboType::STRAIGHT2:   getStraightAndPlane(lastCombo.packs.size(), lastCombo.packs.size(), 2); break;
            
            case CardComboType::PLANE:
            case CardComboType::PLANE1:
            case CardComboType::PLANE2: 
            case CardComboType::SSHUTTLE:
            case CardComboType::SSHUTTLE2:
            case CardComboType::SSHUTTLE4:{
				// 飞机长度
				int seqLen=lastCombo.findMaxSeq();
				getStraightAndPlane(seqLen, seqLen, 3);
                getStraightAndPlane(seqLen, seqLen, 4);
                break;
			}

			case CardComboType::QUADRUPLE2:
            case CardComboType::QUADRUPLE4:  getQuadruple(); break;
            default: break;
		}
	}
	return validPlays;
}

// ==================================================
// 单文件骨架：拆分层
// ==================================================

// [我们要自己实现的核心函数] 把手牌拆成若干组合法牌型，供策略层评估“最少还要几手出完”。
// 使用 MCTS，通过模拟来选取最优的若干组牌
// todo
// 返回最优的前 topK 组拆法
vector<HandPlan> decomposeHand(const vector<Card> &hand, int topK = 1){
	
} 

// [我们要自己实现的核心函数] 快速返回当前手牌出完最少还需要几手，供评估层频繁调用
// 不能调用 decomposeHand()，否则性能过差
// todo
int getMinHandCount(const vector<Card> &hand)
{
	// 这里应该实现一个更高效的算法来快速计算最少手数，而不是调用 decomposeHand()
	return 0; // 占位符，需要根据实际逻辑实现
}

// ==================================================
// 单文件骨架：评估层
// ==================================================

// [我们要自己实现的核心函数] 评估整手牌强度，主要用于叫分决策和后续参数调优。
double evaluateHandStrength(const vector<Card> &hand)
{
	double score = 0.0;
	auto grouped = groupCardsByLevel(hand);
	for (Level level = 0; level < MAX_LEVEL; ++level)
	{
		score += grouped[level].size() * 0.5;
		if (level >= 10)
			score += grouped[level].size() * 1.2;
		if (grouped[level].size() == 4)
			score += 6.0;
	}
	if (!grouped[level_joker].empty() && !grouped[level_JOKER].empty())
		score += 8.0;
	score -= getMinHandCount(hand) * 0.8;
	return score;
}

// [我们要自己实现的核心函数] 评估某一手候选出牌对局面的收益，供出牌策略比较多个选项。
double evaluatePlayGain(const vector<Card> &handBefore, const CardCombo &play, const GameState &)
{
	if (play.comboType == CardComboType::PASS)
		return 0.0;
	vector<Card> handAfter = removeCardsFromHand(handBefore, play.cards);
	int beforeCount = getMinHandCount(handBefore);
	int afterCount = getMinHandCount(handAfter);
	double gain = static_cast<double>(beforeCount - afterCount);
	if (play.comboType == CardComboType::BOMB)
		gain -= 1.5;
	if (play.comboType == CardComboType::ROCKET)
		gain -= 2.0;
	return gain;
}

// ==================================================
// 单文件骨架：策略层
// ==================================================

// [我们要自己实现的核心函数] 根据手牌强度和前序叫分结果决定本轮是否叫分、叫几分。
int decideBid(const vector<Card> &hand, const vector<int> &bidHistory)
{
	int maxBid = bidHistory.empty() ? -1 : *std::max_element(bidHistory.begin(), bidHistory.end());
	double strength = evaluateHandStrength(hand);
	int desiredBid = 0;
	if (strength >= 18.0)
		desiredBid = 3;
	else if (strength >= 14.0)
		desiredBid = 2;
	else if (strength >= 10.0)
		desiredBid = 1;

	if (desiredBid <= maxBid)
		return 0;
	return desiredBid;
}

// [我们要自己实现的核心函数] 在所有合法出牌中选出当前最优的一手，是后续策略升级的主入口。
CardCombo decidePlay(const GameState &state, const vector<CardCombo> &validPlays)
{
	if (validPlays.empty())
		return CardCombo();

	const CardCombo *bestPlay = &validPlays.front();
	double bestScore = -1e18;
	for (const CardCombo &play : validPlays)
	{
		double score = evaluatePlayGain(state.myCards, play, state);
		if (play.comboType == CardComboType::PASS)
			score -= 0.2;
		if (score > bestScore)
		{
			bestScore = score;
			bestPlay = &play;
		}
	}
	return *bestPlay;
}

// ==================================================
// 单文件骨架：主入口
// ==================================================

// [我们实现的程序入口] 负责串联“读状态 -> 调策略 -> 输出结果”，尽量不承载具体业务细节。
int main()
{
	GameState state = readGameState();

	if (state.stage == Stage::BIDDING)
	{
		int bidValue = decideBid(state.myCards, state.bidHistory);
		outputBid(bidValue);
		return 0;
	}

	auto validPlays = enumAllValidPlays(state.myCards, state.lastValidCombo);
	CardCombo action = decidePlay(state, validPlays);

	assert(action.comboType != CardComboType::INVALID);
	assert(
		(state.lastValidCombo.comboType != CardComboType::PASS && action.comboType == CardComboType::PASS) ||
		(state.lastValidCombo.comboType != CardComboType::PASS && state.lastValidCombo.canBeBeatenBy(action)) ||
		(state.lastValidCombo.comboType == CardComboType::PASS && action.comboType != CardComboType::INVALID));

	outputPlay(action.cards);
	return 0;
}