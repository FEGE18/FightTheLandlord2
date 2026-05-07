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
///

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
	//当前桌面最后一手有效牌是谁出的,-1表示自由出牌，
	int lastValidPlayer = -1;
	//三个玩家各自还剩几张牌
	int cardRemaining[PLAYER_COUNT] = {17, 17, 17};
	//每个人历史上出过什么
	vector<vector<Card>> playHistory[PLAYER_COUNT];
	//记牌器基础数据
	bool cardPlayed[54] = {};
	short levelRemaining[MAX_LEVEL] = {};

	// [我们实现] 判断我是否为地主，供策略层快速区分角色使用。
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
	sort(cards.begin(), cards.end(), [](Card left, Card right) 
	{
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

			// 如果这个玩家选择 PASS，只记录连续 PASS 数量。
			if (playedCards.empty())
			{
    			++howManyPass;
			}
				// 如果这个玩家出了有效牌，就更新当前需要压过的牌。
				else
			{
    			// 记录当前桌面最后一手有效牌是什么。
    			state.lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
				//记录这是谁出的
    			state.lastValidPlayer = player;
			}

		}

		// 如果前面两家都 PASS，说明上一轮牌权已经回到当前玩家。
		// 当前玩家可以自由出牌，不需要再压任何人。
		if (howManyPass == 2)
		{
    		// PASS 牌型表示自由出牌。
    		state.lastValidCombo = CardCombo();

    		// -1 表示当前没有需要压过的出牌者。
    		state.lastValidPlayer = -1;
		}

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

//判断指定玩家是否和我属于同一阵营
bool isSameSidePlayer(GameState &state, int palyer)
{
	//-1表示当前没有出牌者
	if(palyer<0)
		return false;

	//我是地主
	if(state.isLandlord())
		return palyer == state.landlordPosition;

	//我是农民
	return palyer != state.landlordPosition;
}

//判断某一手候选牌是否能直接把自己的手牌出完,最高优先级规则：如果能直接赢，通常不需要再评分
bool isWinningPlay(GameState &state,CardCombo &play)
{
	//PASS不能赢
	if(play.comboType == CardComboType::PASS)
		return false;

	//非法牌不能赢
	if(play.comboType==CardComboType::INVALID)
		return false;

	//如果这一手出牌数等于我当前手牌数，说明我能直接出完
	return play.cards.size() == state.myCards.size();
}

//判断当前是否存在必须抢牌权的危险局面
bool isDangerousSituation(GameState &state)
{
	//我是地主
	if(state.isLandlord())
	{
		//找到所有农名
		for (int i = 0; i < PLAYER_COUNT;i++)
		{
			if(i==state.landlordPosition)
				continue;

			//任意一个农民只剩1-2张牌，就算危险
			if(state.cardRemaining[i]<=2)
				return true;
		}
		return false;
	}

	//我是农民
	return state.cardRemaining[state.landlordPosition] <= 2;
}

// 判断一手牌是否是硬控牌。
// 炸弹和火箭可以压大多数牌，但会导致底分翻倍，也会消耗关键控制资源。
bool isHardControlPlay(CardCombo &play)
{
    // 炸弹是硬控牌。
    if (play.comboType == CardComboType::BOMB)
        return true;

    // 火箭是最高硬控牌。
    if (play.comboType == CardComboType::ROCKET)
        return true;

    // 其他牌型不算硬控牌。
    return false;
}

//判断是否应该主动争夺牌权
bool shouldFightControl(GameState &state)
{
	//估计我当前手牌还需要几手
	int myHandCount = getMinHandCount(state.myCards);

	//如果我当前手牌还需要几手出完
	if(myHandCount<=3)
		return true;

	//地主手牌较顺，主动控局
	if(state.isLandlord() && myHandCount<=5)
		return true;
	
		//农民看地主
		if(!state.isLandlord() && state.cardRemaining[state.landlordPosition]<=5)
			return true;

		return false;
}

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
double evaluatePlayGain(vector<Card> &handBefore, CardCombo &play, GameState &state)
{
	//PASS 不会改善自己的手牌，收益为0
	if(play.comboType == CardComboType::PASS)
		return 0.0;

	//计算出牌后的剩余手牌
	vector<Card> handAfter = removeCardsFromHand(handBefore, play.cards);

	//如果这一手直接出完，给极高分
	if(handAfter.empty())
		return 10000.0;

	//出牌前估计还需要几手
	int beforeCount = getMinHandCount(handBefore);

	//出牌后估计还需几手
	int afterCount = getMinHandCount(handAfter);

	//从0开始累计收益分
	double gain = 0.0;

	// 最核心指标：这一手是否减少了剩余手数
    // 减少 1 手比单纯多出几张牌更重要
	gain += (beforeCount - afterCount) * 5.0;

	//次要指标：一次打出更多牌通常需要更接近胜利
	gain += (play.cards.size()) * 0.3;
	// 普通局面下，炸弹和火箭是珍贵控制资源，先扣分保守使用。
    if (play.comboType == CardComboType::BOMB)
        gain -= 6.0;

    if (play.comboType == CardComboType::ROCKET)
        gain -= 8.0;

    // 当前如果是危险局面，对方快出完了，硬控牌惩罚降低。
    // 注意：这里不是鼓励乱炸，而是避免危险时仍然过度保守。
    if (isDangerousSituation(state) && isHardControlPlay(play))
        gain += 4.0;

    return gain;
}

// ==================================================
// 单文件骨架：策略层
// ==================================================

// [我们要自己实现的核心函数] 根据手牌强度和前序叫分结果决定本轮是否叫分、叫几分。
//这个函数不是自己瞎算所有东西，它应该建立在 evaluateHandStrength 之上
int decideBid(vector<Card> &hand, vector<int> &bidHistory)
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
	//统计高对子的数量
	int highPairCount = 0;
	if(grouped[10].size()>=2)
		++highPairCount;
	if(grouped[11].size()>=2)
		++highPairCount;
	if(grouped[12].size()>=2)
		++highPairCount;
	

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
	//判断牌型是否可以接受
	bool handShape = handCount <= 11;

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
    	bidScore -= 0.3;

		// 如果已经有人叫到 2，我要赢叫分只能叫 3，所以需要更谨慎。
	else if (maxBid == 2)
    	bidScore -= 2.0;

	// A 的控制力弱于 2 和王，但一对以上仍然有价值。
	if (aceCount >= 2)
    	bidScore += 0.8;

	//只在牌型可以接受的情况下激进
	if(handShape)
	{
		//中档控制加分
		if(jokerCount>=1 && twoCount>=2 && aceCount>=2)
			bidScore += 0.6;
		if(bombCount>=1 && twoCount>=2 && aceCount>=2)
			bidScore += 0.6;
		if(highPairCount>=2)
			bidScore += 0.3;
		if(highPairCount>=3)
			bidScore += 0.4;
	}

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
CardCombo decidePlay(GameState &state, vector<CardCombo> &validPlays)
{
	//如果没有任何合法候选，返回PASS
	if(validPlays.empty())
		return CardCombo();

	//第一优先级：如果能直接出完牌，就立刻出
	for(CardCombo &play:validPlays)
	{
		//直接出完意味着本方获胜不需要考虑
		if(isWinningPlay(state,play))
			return play;
	}

	//判断当前是不是自由出牌
	bool freeTurn = state.lastValidCombo.comboType == CardComboType::PASS;
	//判断当前是不是危险局面
	bool dangerous = isDangerousSituation(state);
	//判断当前需要压的牌是不是队友出的
	bool followingTeammate = !freeTurn && state.lastValidPlayer >= 0 && isSameSidePlayer(state, state.lastValidPlayer);
	// 当前需要压的牌是不是对手出的。
	bool followingOpponent =!freeTurn &&state.lastValidPlayer >= 0 &&!isSameSidePlayer(state, state.lastValidPlayer);
	// 当前出牌者是不是危险对手，只有“对手出的牌”并且“这个对手只剩 1~2 张”时，才是必须积极压制的局面。
	bool followingDangerousOpponent =followingOpponent && state.cardRemaining[state.lastValidPlayer] <= 2;
	//看是否要主动出击
	bool fightForControl = shouldFightControl(state);

	//===评分系统
	//初始化为PASS
	CardCombo bestplay;
	//当前最高分设为一个很小的数
	double bestScore = -1e18;

	//对每个候选牌进行评估打分
	for(CardCombo &play:validPlays)
	{
		//自由出牌时不能PASS
		if(freeTurn && play.comboType == CardComboType::PASS)
			continue;

		//基础分来自评估层，只评价这手牌本身的收益
		double score = evaluatePlayGain(state.myCards, play, state);

		//如果当前压的是队友的牌，正常不应该抢
		if(followingTeammate)
		{
			//PASS是好的选择
			if(play.comboType==CardComboType::PASS)
				score += 1.0;

			//非危险局面压队友
			else
			{
				score -= 1.0;
				
				//如果用炸弹或火箭压队友，除非危险局面，否则非常不划算
				if(isHardControlPlay(play) && !dangerous)
					score -= 10.0;
				
				//如果用2或王压队友，普通局面扣分
				for(Card card:play.cards)
				{
					Level level = card2level(card);
					if(level>=12&&!dangerous)
						score -= 2.0;
				}
			}
		}

		// 如果当前要压的是危险对手出的牌，PASS 风险极高。
		if (followingDangerousOpponent)
		{
    		// 不压危险对手，可能直接让对方继续走完，重罚。
    		if (play.comboType == CardComboType::PASS)
    		{
        		score -= 25.0;
    		}
    		else
    		{
        		// 愿意出牌压制危险对手，给明显奖励。
        		score += 8.0;

        		// 危险对手快跑时，炸弹和火箭可以接受。
        		if (isHardControlPlay(play))
            		score += 6.0;
    		}
		}	

		//处理过牌
		if(play.comboType == CardComboType::PASS)
		{
			//如果压队友，配合给奖励
			if(followingTeammate)
				score += 1.5;
			else if(followingOpponent)
			{
				//如果要主动出击，PASS代价更高
				if(fightForControl)
					score -= 5.0;
				else
					score -= 2.0;
			}
		}

		//硬控牌
		if(isHardControlPlay(play))
		{
			//如果不是在压危险对手，扣分
			if(!followingDangerousOpponent)
				score -= 6.0;
			
			//如果在压危险对手，加分
			else
				score += 2.0;
		}

		//普通局面下保护2和王
		if(!followingDangerousOpponent && play.comboType!=CardComboType::PASS)
		{
			//强牌权时，惩罚低
			double rate = fightForControl ? 0.4 : 1.0;
			for(Card card:play.cards)
			{
				Level level = card2level(card);
				if(level==12)
					score -= 1.0 * rate;
				if(level==level_joker)
					score -= 1.5 * rate;
				if(level==level_JOKER)
					score -= 1.8 * rate;
			}
		}

		//压对手
		if (followingOpponent && play.comboType != CardComboType::PASS)
		{
    		// 普通对手：小幅奖励，鼓励合理压制。
    		score += 1.0;

			if(fightForControl)
				score += 3.0;
		}

		//自由出牌
		// 自由出牌时，我拥有主动权，应优先打出能整理手牌的组合。
		if (freeTurn && play.comboType != CardComboType::PASS)
		{
    		// 多张牌组合通常比单张更能推进出完进度。
    		if (play.cards.size() >= 5)
        		score += 2.0;

    		// 三带、顺子、连对、飞机等组合牌优先级更高。
    		if (play.comboType == CardComboType::STRAIGHT ||
        		play.comboType == CardComboType::STRAIGHT2 ||
        		play.comboType == CardComboType::TRIPLET1 ||
        		play.comboType == CardComboType::TRIPLET2 ||
        		play.comboType == CardComboType::PLANE ||
        		play.comboType == CardComboType::PLANE1 ||
        		play.comboType == CardComboType::PLANE2)
    		{
        		score += 2.0;
    		}

    		// 自由出牌时不鼓励先出炸弹或火箭
    		if (isHardControlPlay(play))
        		score -= 8.0;
		}

		// 只剩一手牌
		if (play.comboType != CardComboType::PASS)
		{
    		// 计算出牌后的剩余手牌。
    		vector<Card> handAfter = removeCardsFromHand(state.myCards, play.cards);

    		// 如果剩余手牌只需要一手出完，给较高奖励。
    		if (!handAfter.empty() && getMinHandCount(handAfter) == 1)
        		score += 8.0;

			if(fightForControl)
			{
				int before = getMinHandCount(state.myCards);
				int after = getMinHandCount(handAfter);

				//如果这一手减少，那么很有必要
				if(before>after)
					score += 3.0 * (before - after);
			}
		}

		// 如果当前候选分数更高，就更新最优选择
		if(score > bestScore)
		{
			bestScore = score;
			bestplay = play;
		}
	}

	//如果所有候选都被跳过，返回PASS
	if(bestScore==-1e18)
		return CardCombo();

	return bestplay;
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
