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
//某一次打出去的一组牌：一张 7、一个顺子 34567
struct CardCombo
{
	// 表示同等级的牌有多少张
	// 会按个数从大到小、等级从大到小排序
	//===把“同一个点数的牌”打包成一条统计记录===
	struct CardPack
	{
		//这个集合是什么点数，比如 3、4、A、2、王
		Level level;
		// 这个点数一共有几张
		short count;

		// [示例程序提供，可直接复用] 定义牌种排序规则：先按张数降序，再按等级降序。
		//===张数多的排前面，如果张数一样，大点数排前面===
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
	//构造函数，输入一组牌，自动识别牌型
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

/// 当前整局局面
//这个结构表示的是，当前整盘游戏轮到我时的完整局面
struct GameState
{
	Stage stage = Stage::BIDDING;
	//我是 0/1/2 号位里的谁
	int myPosition = 0;
	//地主是谁
	int landlordPosition = -1;
	int finalBid = -1;
	//前面玩家是怎么叫分的
	vector<int> bidHistory;
	//我现在手里还剩哪些牌
	vector<Card> myCards;
	//地主的三张明牌
	vector<Card> publicCards;
	//当前桌面上需要压过的牌
	CardCombo lastValidCombo;
	//三个玩家各自还剩几张牌
	int cardRemaining[PLAYER_COUNT] = {17, 17, 17};
	//每个人历史上出过什么
	vector<vector<Card>> playHistory[PLAYER_COUNT];
	//记牌器基础数据
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

// [我们实现] 按等级优先、牌号次之对手牌排序，统一后续枚举、输出和调试行为。

void sortCards(vector<Card> &cards)
{
	sort(cards.begin(), cards.end(), [](Card left, Card right) 
	{
		Level leftLevel = card2level(left);
		Level rightLevel = card2level(right);
		if (leftLevel == rightLevel)
			return left < right;
		return leftLevel < rightLevel;
	});
}

// [我们实现] 从一手牌里删掉已经打出的具体牌，返回删除后的新手牌副本。
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
vector<vector<Card>> groupCardsByLevel(const vector<Card> &hand)
{
	vector<vector<Card>> grouped(MAX_LEVEL);
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

// [我们要自己实现的核心函数] 枚举当前局面下所有合法出牌，后续会逐步替换掉 findFirstValid 的贪心行为。
//把“我现在合法能出的牌”列出来：如果上家出一对 9，我手里有：一对 J，一对 K，炸弹 AAAA
vector<CardCombo> enumAllValidPlays(const vector<Card> &hand, const CardCombo &lastCombo)
{
	vector<CardCombo> plays;
	if (hand.empty())
		return plays;

	if (lastCombo.comboType == CardComboType::PASS)
	{
		for (Card card : hand)
			plays.emplace_back(&card, &card + 1);

		auto grouped = groupCardsByLevel(hand);
		for (const auto &cardsAtLevel : grouped)
		{
			if (cardsAtLevel.size() >= 2)
				plays.emplace_back(cardsAtLevel.begin(), cardsAtLevel.begin() + 2);
			if (cardsAtLevel.size() >= 3)
				plays.emplace_back(cardsAtLevel.begin(), cardsAtLevel.begin() + 3);
			if (cardsAtLevel.size() == 4)
				plays.emplace_back(cardsAtLevel.begin(), cardsAtLevel.end());
		}

		auto groupedWithJokers = groupCardsByLevel(hand);
		if (!groupedWithJokers[level_joker].empty() && !groupedWithJokers[level_JOKER].empty())
		{
			Card rocket[] = {groupedWithJokers[level_joker].front(), groupedWithJokers[level_JOKER].front()};
			plays.emplace_back(rocket, rocket + 2);
		}

		return plays;
	}

	plays.push_back(CardCombo());
	CardCombo candidate = lastCombo.findFirstValid(hand.begin(), hand.end());
	if (candidate.comboType != CardComboType::PASS && candidate.comboType != CardComboType::INVALID)
		plays.push_back(candidate);
	return plays;
}

// [我们要自己实现的扩展函数] 当主体牌型确定后，为三带一、飞机带翼等牌型补全最合适的带牌。
CardCombo selectAttachment(const vector<Card> &, const CardCombo &mainCombo, CardComboType)
{
	return mainCombo;
}


// ==================================================
// 单文件骨架：拆分层
// ==================================================

// [我们要自己实现的核心函数] 把手牌拆成若干组合法牌型，供策略层评估“最少还要几手出完”。
vector<HandPlan> decomposeHand(const vector<Card> &hand, int topK = 1)
{
	HandPlan plan;
	auto grouped = groupCardsByLevel(hand);
	for (const auto &cardsAtLevel : grouped)
	{
		if (cardsAtLevel.empty())
			continue;
		if (cardsAtLevel.size() == 4)
			plan.groups.emplace_back(cardsAtLevel.begin(), cardsAtLevel.end());
		else if (cardsAtLevel.size() >= 3)
			plan.groups.emplace_back(cardsAtLevel.begin(), cardsAtLevel.begin() + 3);
		else if (cardsAtLevel.size() >= 2)
			plan.groups.emplace_back(cardsAtLevel.begin(), cardsAtLevel.begin() + 2);
		else
			plan.groups.emplace_back(cardsAtLevel.begin(), cardsAtLevel.begin() + 1);
	}
	plan.handCount = static_cast<int>(plan.groups.size());
	vector<HandPlan> plans;
	if (topK > 0)
		plans.push_back(plan);
	return plans;
}

// [我们要自己实现的核心函数] 快速返回当前手牌最少还需要几手，供评估层频繁调用。
int getMinHandCount(const vector<Card> &hand)
{
	auto plans = decomposeHand(hand, 1);
	return plans.empty() ? 0 : plans.front().handCount;
}

// ==================================================
// 单文件骨架：评估层
// ==================================================

// [我们要自己实现的核心函数] 评估整手牌强度，主要用于叫分决策和后续参数调优。
//如果只看我自己这手牌，不看当前桌面动作，这手牌到底强不强
double evaluateHandStrength(const vector<Card> &hand)
{
//===为手牌评分：总分=大牌分+炸弹分+结构分-碎牌惩罚-手牌惩罚===
	double score = 0.0;
	auto grouped = groupCardsByLevel(hand);

	// 大牌分：我手里有没有抢牌权的能力
	//为等级赋分，记录成一个数组
	static const double highCardBonus[MAX_LEVEL] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 3~Q
		0.8,						  // K
		1.5,						  // A
		2.5,						  // 2
		3.5,						  //小鬼
		4.0};						  //大鬼

	for (Level i = 0; i < MAX_LEVEL;i++)
	{
		//grouped[i] 返回的是手牌被拆分成不同的数组之后不同等级的牌数。grouped[10] = 2 表示K有2张
		score += highCardBonus[i] * grouped[i].size();
	}
	// 炸弹分：我有没有硬压能力
	//每个炸弹 +6
	for (Level i = 0; i < level_joker;i++)
	{
		if(grouped[i].size()==4)
			score += 6.0;
	}

	//火箭在大牌分的基础上 +4
	if(!grouped[level_joker].empty() && !grouped[level_JOKER].empty())
		score += 4.0;

	// 结构分：这副牌整不整齐，容易不容易组织成高质量出牌
	//三条+2.0，对子+1.0
	for (Level i = 0; i < level_joker;i++)
	{
		size_t cnt = grouped[i].size();
		if(cnt==3)
			score += 2.0;
		else if(cnt == 2)
			score += 1.0;
	}

	//连续牌型潜力：调用 decomposeHand 接口 ，这个1是指"返回前 K 种最优拆法"
	auto plans = decomposeHand(hand, 1);
	if(!plans.empty())
	{
		for(CardCombo &combo :plans.front().groups)
		{
			switch (combo.comboType)
			{
			case CardComboType::STRAIGHT:
				score += 1.5; break;
			case CardComboType::STRAIGHT2:
				score += 2.0; break;
			case CardComboType::PLANE:
			case CardComboType::PLANE1:
			case CardComboType::PLANE2:
				score += 2.5; break;
			case CardComboType::SSHUTTLE:
			case CardComboType::SSHUTTLE2:
			case CardComboType::SSHUTTLE4: 
				score += 4.0; break;

			default:
				break;
			}
		}
	}
	// 碎牌惩罚：这手牌是不是太碎，后面很难处理
	for (Level i = 0; i < level_joker;i++)
	{
		if(grouped[i].size() == 1)
		{
			//3~9
			if(i<=6)
				score -= 0.8;
			//10,J,Q
			else if(i<=9)
				score -= 0.3;
		}
	}
	// 手数惩罚：这副牌总体还要出多少手才能打完
	// 依赖 getMinHandCount
	score -= 1.2 * getMinHandCount(hand);

	return score;
}

// [我们要自己实现的核心函数] 评估某一手候选出牌对局面的收益，供出牌策略比较多个选项。
//看“出这一手之后”局面有没有变好
//输入的是 出牌前手牌，你打算出的这一手，当前局面
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
//这个函数不是自己瞎算所有东西，它应该建立在 evaluateHandStrength 之上
int decideBid(const vector<Card> &hand, const vector<int> &bidHistory)
{
	//找到历史叫分中的最高分
	int maxBid = bidHistory.empty() ? 0 : *std ::max_element(bidHistory.begin(), bidHistory.end());
	//最高分大于3，直接跳过
	if(maxBid>=3)	return 0;

	//===快速统计关键牌===
	// 按牌面等级分组，grouped[12] 表示所有 2，grouped[13] 表示小王
	auto grouped = groupCardsByLevel(hand);
	// 判断是否有火箭
	bool hasRocket = !grouped[level_joker].empty() && !grouped[level_JOKER].empty();
	//统计炸弹数量
	int bombCount = 0;
	for (Level i = 0; i < level_joker;++i)
	{
		if(grouped[i].size() == 4)
			++bombCount;
	}
	// 统计 2 的数量
	int twoCount = grouped[12].size();
	// 统计 A 的数量
	int aceCount = grouped[11].size();
	// 统计王的数量
	int jokerCount = grouped[level_joker].size() + grouped[level_JOKER].size();

	// 我这手牌最多愿意叫到几分
	int targetBid = 0;

	// 第一层：硬条件控制叫分上限。先默认最多可以叫 3，后面再逐步收紧。
	int bidCap = 3;

	//是否具有基础控牌能力
	bool hasBasicControl = hasRocket || bombCount > 0 || jokerCount > 0 || twoCount > 0 || aceCount >= 2;

	//是否具有强控牌能力
	bool hasStrongControl = hasRocket || bombCount > 0 || twoCount >= 2 || (jokerCount >= 1 && twoCount >= 1) || (twoCount >= 1 && aceCount >= 2);

	if(!hasBasicControl && bidCap>1)
		bidCap = 1;
	if(!hasStrongControl&&bidCap>2)
		bidCap = 2;

	//计算当前手牌至少大概要分几手出完，手数越多，当地主风险越大。
	int handCount = getMinHandCount(hand);

	//如果前面已经有人叫到2，风险变大
	bool mustBidThree = (maxBid == 2);
	
	//叫3的强牌门槛，必须有非常明确的硬控牌能力
	bool hasThreePointControl = hasRocket || bombCount >= 1 || twoCount >= 3 || (jokerCount >= 1 && twoCount >= 2);

	//如果手数非常多，说明牌很碎，即使有一定打牌，也不适合叫地主
	if(handCount>=14 && !hasThreePointControl &&bidCap>1)
		bidCap = 1;
	//如果手数偏多，不要冒险叫3
	else if(handCount>=12 && !hasThreePointControl &&bidCap>2)
		bidCap = 2;


	//如果有人叫2，而我没有叫3的条件，就强制不允许叫到3
	if(mustBidThree&&!hasThreePointControl&&bidCap>2)
		bidCap = 2;

	
	// 低散牌数量：统计 3~9 中只有一张的牌，这些牌当地主时很难主动处理。


	// 第二层：细粒度评分。先直接复用评估层，不在策略层重复算整手牌强度。
	double bidScore = evaluateHandStrength(hand);

	//火箭对叫地主价值很高，额外加分
	if(hasRocket)
		bidScore += 3.0;

	// 每个炸弹都能强行拿回牌权，额外加分
	bidScore += bombCount * 2.5;

	// 2是叫地主时最关键的常规控制牌，按数量加分
	bidScore += twoCount * 0.8;

	// 王也是非常强的单牌控制资源，按数量加分
	bidScore += jokerCount * 1.0;

	// 前面最高叫分越高，继续加叫的风险越大。
	if (maxBid == 1)
    	bidScore -= 1.0;

		// 如果已经有人叫到 2，我要赢叫分只能叫 3，所以需要更谨慎。
	else if (maxBid == 2)
    	bidScore -= 2.5;

	// A 的控制力弱于 2 和王，但一对以上仍然有价值。
	if (aceCount >= 2)
    	bidScore += 0.8;

	if (bidScore >= 16.0)
    	targetBid = 3;
	else if (bidScore >= 11.0)
    	targetBid = 2;
	else if (bidScore >= 7.0)
    	targetBid = 1;
	else
		targetBid = 0;

	// 硬条件限制最终叫分上限
	if (targetBid > bidCap)
    	targetBid = bidCap;

	//如果不如最高分，不叫
	if(targetBid<=maxBid)
		return 0;

	return targetBid;
}

// [我们要自己实现的核心函数] 在所有合法出牌中选出当前最优的一手，是后续策略升级的主入口。
//依赖于evaluatePlayGain
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
