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

//一次出牌的事件上下文
struct PlayEvent
{
	//当前行动的玩家编号
	int player = -1;
	//当前玩家实际打出的牌
	vector<Card> cards;
	//当前玩家实际打出的牌型
	CardCombo combo;
	//当前玩家行动前，需要压过的牌
	CardCombo requiredCombo;
	//requiredCombo是谁出的
	int requiredPlayer = -1;
	//当前玩家是否选择PASS
	bool isPass = false;
};

//一条PASS约束
struct PassConstraint
{
	//选择PASS的玩家
	int player = -1;
	//他当时要压过的牌
	CardCombo requirCombo;
	//这牌是谁出的
	int requirPlayer = -1;
	
	//约束强度，数值越小证据越强
	//例如危险残局里的 PASS 可以更强，普通跟牌 PASS 可以更弱
	double strength = 1.0;
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

	//按时间顺序记录每一次出牌事件
	vector<PlayEvent> playEvents;
	//从PASS中提取的约束
	vector<PassConstraint> passConstraints;

	//记牌器基础数据
	bool cardPlayed[54] = {};
	short levelRemaining[MAX_LEVEL] = {};

	//这张牌当前是否属于未知牌区
	bool cardUnknown[54] = {};
	//每个玩家当前确定还持有的明牌
	vector<Card> konwCardOfPlayer[PLAYER_COUNT];

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

//一次可能的完整发牌结果
struct InferredDeal
{
	//每个玩家在这个样本中的手牌
	vector<Card> hands[PLAYER_COUNT];
	//这个样本的可信权重
	double weight = 1.0;
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

void initRandomSeed()
{
	// time(nullptr) 提供秒级时间，clock() 提供当前进程运行时间。
    // 两者异或后作为种子，避免每次进程启动都使用默认固定种子。
	std::srand(static_cast<unsigned>(std::time(nullptr)) ^ static_cast<unsigned>(clock()));
}


// ==================================================
// 单文件骨架：IO / 状态恢复
// ==================================================

void RebuildCard(GameState &state);
void recordPlayEvent(GameState &state, int player, vector<Card> &playedCard);
bool isSameSidePlayer(GameState &state, int palyer);
bool canBeatComboFast(vector<Card> &hand, CardCombo &requiredCombo);
bool canPlayerBeatInDeal(InferredDeal &deal, int player, CardCombo &requiredCombo);
 
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

			// 先记录事件，再更新桌面状态。
			// 这样 event 里拿到的是“行动前”的 requiredCombo 和 requiredPlayer。
			recordPlayEvent(state, player, playedCards);

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

			// 记录我自己的历史行动。
			// 这里同样要在更新任何会影响后续判断的状态之前记录。
			recordPlayEvent(state, state.myPosition, playedCards);

			// 如果我当时出了有效牌，这手牌也会成为后续玩家需要压过的桌面牌。
			if (!playedCards.empty())
			{
				// 更新当前桌面最后一手有效牌。
				state.lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());

				// 记录这手有效牌是我出的。
				state.lastValidPlayer = state.myPosition;
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
	{
		for (Card card : state.publicCards)
		{
			//只有还没被打出的地主明牌，才需要从未知牌里排除
			if(!state.cardPlayed[card])
				--state.levelRemaining[card2level(card)];
		}
	}

	//在所有历史都重放完成之后，统一重建确定牌和未知牌区
	RebuildCard(state);

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


//根据一次PASS事件的上下文，决定它作为没有压制牌的证据有多强
//越小说明证据越强,1.0说明几乎不做约束
double getPassStrength(GameState &state,int player,CardCombo &requiredCombo,int requiredPlayer)
{
	//如果是自由派，PASS不会出现
	if(requiredCombo.comboType==CardComboType::PASS || requiredPlayer<0)
		return 1.0;

	//如果和上一个出牌的人同一阵营，强度较弱
	if(isSameSidePlayer(state,player) && isSameSidePlayer(state,requiredPlayer))
		return 0.9;

	//我是地主
	if(state.isLandlord())
	{
		//快没牌了还PASS，大概率压不了
		if(state.cardRemaining[player]<=3)
			return 0.5;
		//普通局面
		return 0.7;
	}

	//我是农民
	if(requiredPlayer==state.landlordPosition)
	{
		//地主快跑完了，农民压不了
		if(state.cardRemaining[state.landlordPosition]<3)
			return 0.3;

		return 0.7;
	}
	//我是农民，出牌的压的是我的牌
	return 0.9; 
}
//记录一次出牌事件
void recordPlayEvent(GameState &state,int player,vector<Card> &playedCard)
{
	PlayEvent event;
	//记录是谁在行动,出了什么牌
	event.player = player;
	event.cards = playedCard;

	//记录当前玩家行动前桌面上需要压过的牌及是谁出的
	event.requiredCombo = state.lastValidCombo;
	event.requiredPlayer = state.lastValidPlayer;

	//判断是否PASS
	event.isPass = playedCard.empty();
	//记录之前出的什么牌
	if(event.isPass)
		event.combo = CardCombo();
	else
		event.combo = CardCombo(playedCard.begin(), playedCard.end());

	if(player != state.myPosition && event.isPass && event.requiredCombo.comboType != CardComboType::PASS && event.requiredPlayer >= 0)
	{
		//创建一条PASS约束
		PassConstraint constraint;
		//记录谁PASS
		constraint.player = player;
		constraint.requirCombo = event.requiredCombo;
		constraint.requirPlayer = event.requiredPlayer;
		constraint.strength = getPassStrength(state, player, event.requiredCombo, event.requiredPlayer);

		//记录约束
		state.passConstraints.push_back(constraint);
	}
	//按时间顺序保存事件
	state.playEvents.push_back(event);
}

//根据当前已经恢复出来的局面，重建已知牌和未知牌区
void RebuildCard(GameState &state)
{
	//先默认54张牌都属于未知牌
	for (Card i = 0; i < 54;++i)
		state.cardUnknown[i] = true;

	//清空每个玩家的确定持牌记录
	for (int i = 0; i < PLAYER_COUNT;++i)
		state.konwCardOfPlayer[i].clear();

	//我的手牌是确定信息
	for(Card i:state.myCards)
		state.cardUnknown[i] = false;

	//已经打出去的牌也是确定信息
	for (Card i = 0; i < 54;++i)
	{
		if(state.cardPlayed[i])
			state.cardUnknown[i] = false;
	}

	//地主明牌
	if(state.landlordPosition >=0&&state.landlordPosition!=state.myPosition)
	{
		for(Card i:state.publicCards)
		{
			if(!state.cardPlayed[i])
			{
				//如果地主没打出，这张牌属于地主
				state.konwCardOfPlayer[state.landlordPosition].push_back(i);
				//移出未知牌区
				state.cardUnknown[i] = false;
			}
		}
	}
}

//收集当前所有未知牌
vector<Card> collectUnknowCard(GameState &state)
{
	//保存所有任然未知的具体牌
	vector<Card> unknownCard;

	for (Card i = 0; i < 54;++i)
	{
		if(state.cardUnknown[i])
			unknownCard.push_back(i);
	}

	sortCards(unknownCard);

	return unknownCard;
}

// 检查未知牌数量是否和玩家剩余手牌数一致
bool checkUnknownCard(GameState &state)
{
	//收集所有未知牌
	vector<Card> unknownCard = collectUnknowCard(state);

	//统计其他玩家还剩多少张牌是无法确定的
	int expectedUnknownCount = 0;

	for (int i = 0; i < PLAYER_COUNT;++i)
	{
		//我的手牌已知，不属于未知牌
		if(i==state.myPosition)
		continue;
		
		//这个玩家剩余牌中的明牌
		int knownCount = state.konwCardOfPlayer[i].size();

		//剩下那部分才是未知牌
		expectedUnknownCount += state.cardRemaining[i] - knownCount;
	}
	//如果数量一致，说明合理
	return unknownCard.size() == expectedUnknownCount;
}

//构造一次随机补全的完整局面
InferredDeal bulidOneRandomDeal(GameState &state)
{
	//创建一个样本
	InferredDeal deal;

	//我的手牌是确定的，直接复制进去
	deal.hands[state.myPosition] = state.myCards;

	//其他玩家的确定手牌也放进去
	//地主未打出的底牌
	for (int i = 0; i < PLAYER_COUNT;++i)
	{
		if(i==state.myPosition)
			continue;
		deal.hands[i] = state.konwCardOfPlayer[i];
	}

	//收集所有未知牌
	vector<Card> unknownCard = collectUnknowCard(state);

	//随机打乱未知牌
	std::random_shuffle(unknownCard.begin(), unknownCard.end());

	// 计算理论上需要分配出去的未知牌数量。
	int totalNeedCount = 0;

	// 遍历其他两个玩家，统计他们还缺多少张未知牌。
	for (int player = 0; player < PLAYER_COUNT; ++player)
	{
    	// 我的手牌已经确定，不需要从未知牌中补。
    	if (player == state.myPosition)
        	continue;

    	// 这个玩家需要补的未知牌数量 = 剩余牌数 - 已知确定牌数。
    	totalNeedCount += state.cardRemaining[player] - static_cast<int>(deal.hands[player].size());
	}

	// 如果未知牌数量和需要补的数量不一致，说明状态恢复或未知牌重建有问题。
	if (totalNeedCount != unknownCard.size())
	{
    	// 返回一个空权重样本，表示这次补全不可用。
    	deal.weight = 0.0;
    	return deal;
	}

	//当前已经发到unknownCards 的哪个位置
	int cnt = 0;

	//给其他玩家补足手牌
	for (int i = 0; i < PLAYER_COUNT;++i)
	{
		if(i==state.myPosition)
		continue;

		//这个玩家要补多少张
		int needCount = state.cardRemaining[i] - deal.hands[i].size();
		//从未知牌中取给这个玩家
		for (int j = 0; j < needCount;++j)
		{
			deal.hands[i].push_back(unknownCard[cnt]);
			++cnt;
		}

		// 每个玩家手牌排序
    	sortCards(deal.hands[i]);
	}
	//样本权重设为1
	deal.weight = 1.0;

	return deal;
}

// 检查一次随机补全样本的手牌张数是否正确。
bool checkInferredDeal(GameState &state, InferredDeal &deal)
{
    // 如果样本本身已经标记为无效，直接返回 false。
    if (deal.weight <= 0.0)
        return false;

    // 遍历三个玩家。
    for (int player = 0; player < PLAYER_COUNT; ++player)
    {
        // 每个玩家样本手牌数量必须等于当前局面记录的剩余牌数。
        if (static_cast<int>(deal.hands[player].size()) != state.cardRemaining[player])
            return false;
    }

    // 所有玩家张数都对，说明这个样本在数量层面合法。
    return true;
}

// 检查一次随机补全样本中是否存在重复牌。
bool checkInferredDealNoDuplicate(InferredDeal &deal)
{
    // 标记每张牌是否已经在样本手牌中出现过。
    bool seen[54] = {};

    // 遍历三个玩家。
    for (int player = 0; player < PLAYER_COUNT; ++player)
    {
        // 遍历这个玩家样本手牌里的每张牌。
        for (Card card : deal.hands[player])
        {
            // 如果这张牌之前已经出现过，说明重复了。
            if (seen[card])
                return false;

            // 标记这张牌已经出现。
            seen[card] = true;
        }
    }

    // 没有发现重复牌，说明样本在唯一性层面合法。
    return true;
}

//判断在某个随机补全样本中，指定玩家是否有能力压过 requiredCombo
bool canPlayerBeatInDeal(InferredDeal &deal,int player,CardCombo &requiredCombo)
{
	if(requiredCombo.comboType ==CardComboType::PASS)
		return false;

	//枚举这个玩家在样本手牌中所有能出的合法响应
	vector<CardCombo> vaildPlays = enumAllValidPlays(deal.hands[player], requiredCombo);

	for(CardCombo &play:vaildPlays)
	{
		if(play.comboType !=CardComboType::PASS && play.comboType !=CardComboType::INVALID)
			return true;
	}

	return false;
}

//根据历史PASS约束，评估一个随机补全样本的可信度
//如果某个玩家历史上 PASS 了，但这个样本里他其实能压过当时那手牌，就降低这个样本的权重
double evaluateDealByPass(GameState &state,InferredDeal &deal)
{
	//初始权重为1，表示完全可信
	double weight = 1.0;

	//遍历所有从PASS中提取出来的约束
	for(PassConstraint &constraint : state.passConstraints)
	{
		if(constraint.player<0 || constraint.player>=PLAYER_COUNT)
			continue;
		
		if(canPlayerBeatInDeal(deal,constraint.player,constraint.requirCombo))
		{
			weight *= constraint.strength;
		}
	}
	return weight;
}

//批量生成若干个随机补全样本
//sampleCount 表示希望生成多少种可能局面
vector<InferredDeal> buildRandomDeals(GameState &state, int sampleCount)
{
	//保存所有有效样本
	vector<InferredDeal> deals;

	for (int i = 0; i < sampleCount;++i)
	{
		//生成一个随机补全样本
		InferredDeal deal = bulidOneRandomDeal(state);

		//如果样本权重为0，说明失败，跳过
		if(deal.weight<=0.0)
		continue;

		//根据历史PASS约束修正
		deal.weight *= evaluateDealByPass(state, deal);

		if(deal.weight<=0.0)
		continue;

		//如果样本手牌张数不对，排除
		if(!checkInferredDeal(state,deal))
		continue;

		//如果有重复牌，排除
		if(!checkInferredDealNoDuplicate(deal))
			continue;

		//保存有效样本
		deals.push_back(deal);
	}
	return deals;
}

// 调试用：检查随机补全模块是否能生成合法样本。
// 注意：这个函数不应该在 Botzone 正式输出前打印内容，否则会污染 JSON 输出。
void debugRandomDeals(GameState &state)
{
    // 先检查确定未知牌数量是否一致。
    bool unknownOk = checkUnknownCard(state);

    // 生成 20 个随机补全样本。
    vector<InferredDeal> deals = buildRandomDeals(state, 20);

    // 输出调试信息到 cerr，不影响正常 JSON 输出。
    std::cerr << "[debug] unknownOk=" << unknownOk
              << " sampleCount=" << deals.size()
              << " unknownCards=" << collectUnknowCard(state).size()
              << std::endl;

    // 最多打印前三个样本的三个玩家手牌数量，避免输出太多。
    for (int i = 0; i < static_cast<int>(deals.size()) && i < 3; ++i)
    {
        std::cerr << "[debug] deal " << i
                  << " p0=" << deals[i].hands[0].size()
                  << " p1=" << deals[i].hands[1].size()
                  << " p2=" << deals[i].hands[2].size()
                  << std::endl;
    }
}
// 调试用：检查 PASS 约束是否正确生成，并观察随机样本权重。
// 注意：正式 Botzone 输出前不要调用，避免调试信息干扰。
void debugPassConstraints(GameState &state)
{
    // 输出历史事件数量和 PASS 约束数量。
    std::cerr << "[debug-pass] events=" << state.playEvents.size()
              << " constraints=" << state.passConstraints.size()
              << std::endl;

    // 生成一批样本，观察有多少样本被 PASS 约束降权。
    vector<InferredDeal> deals = buildRandomDeals(state, 50);

    int penalizedCount = 0;
    double minWeight = deals.empty() ? 0.0 : deals[0].weight;
    double maxWeight = deals.empty() ? 0.0 : deals[0].weight;
    double totalWeight = 0.0;

    for (InferredDeal &deal : deals)
    {
        // 权重小于 1，说明至少触发过一次 PASS 降权。
        if (deal.weight < 1.0)
            ++penalizedCount;

        if (deal.weight < minWeight)
            minWeight = deal.weight;

        if (deal.weight > maxWeight)
            maxWeight = deal.weight;

        totalWeight += deal.weight;
    }

    // 打印前几条 PASS 约束，避免输出太多。
    for (int i = 0; i < static_cast<int>(state.passConstraints.size()) && i < 8; ++i)
    {
        PassConstraint &constraint = state.passConstraints[i];
        int matchedDeals = 0;

        // 统计这条约束在多少个样本中被触发。
        // 如果样本中该玩家能压过当时那手牌，却历史上选择 PASS，这个样本就会被降权。
        for (InferredDeal &deal : deals)
        {
            if (canPlayerBeatInDeal(deal, constraint.player, constraint.requirCombo))
                ++matchedDeals;
        }

        std::cerr << "[debug-pass] constraint " << i
                  << " player=" << constraint.player
                  << " requiredPlayer=" << constraint.requirPlayer
                  << " requiredType=" << cardComboStrings[static_cast<int>(constraint.requirCombo.comboType)]
                  << " requiredLevel=" << constraint.requirCombo.comboLevel
                  << " requiredSize=" << constraint.requirCombo.cards.size()
                  << " strength=" << constraint.strength
                  << " matchedDeals=" << matchedDeals
                  << std::endl;
    }

    double avgWeight = deals.empty() ? 0.0 : totalWeight / deals.size();

    std::cerr << "[debug-pass] deals=" << deals.size()
              << " penalized=" << penalizedCount
              << " minWeight=" << minWeight
              << " maxWeight=" << maxWeight
              << " avgWeight=" << avgWeight
              << std::endl;
}


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

//带分数的候选出牌
struct ScoredPlay
{
	//候选出牌本身
	CardCombo play;
	//这手牌当前的启发式评分
	double score = 0;
	//在随机补全样本中的平均评估分
	double sampleScore = 0;
};


//评估一手候选牌在当前真实局面下的启发式分数
double evaluatePlayHScore(GameState &state ,CardCombo &play)
{
	// 判断当前是不是自由出牌
	bool freeTurn = state.lastValidCombo.comboType == CardComboType::PASS;

	// 判断当前是不是危险局面
	bool dangerous = isDangerousSituation(state);

	// 判断当前需要压的牌是不是队友出的
	bool followingTeammate = !freeTurn && state.lastValidPlayer >= 0 && isSameSidePlayer(state, state.lastValidPlayer);

	// 判断当前需要压的牌是不是对手出的
	bool followingOpponent = !freeTurn && state.lastValidPlayer >= 0 && !isSameSidePlayer(state, state.lastValidPlayer);

	// 判断当前是否是在压危险对手
	bool followingDangerousOpponent = followingOpponent && state.cardRemaining[state.lastValidPlayer] <= 2;

	// 判断当前是否应该主动争夺牌权
	bool fightForControl = shouldFightControl(state);

	//复用已有的基础收益评分,只看出牌后，我的手牌有没有变好
	double score = evaluatePlayGain(state.myCards, play, state);

	//压队友的情况
	if(followingTeammate)
	{
		if(play.comboType == CardComboType::PASS)
			//让队友继续控牌
			score += 1.0;
		else
		{
			//普通局面压队友扣分
			score -= 1.0;
			
			//不应该用炸弹
			if(isHardControlPlay(play) && !dangerous)
				score -= 10.0;

			//用 2 或王压队友也比较亏，普通局面要扣分
			for(Card card:play.cards)
			{
				Level level = card2level(card);
				if(level>=12 && !dangerous)
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

			//自由出牌时，同牌型更倾向先走低牌
			//这样可以避免 333 和 AAA 启发式打平后，被样本分推去先出 AAA
			if(play.comboType == CardComboType::SINGLE ||
			   play.comboType == CardComboType::PAIR ||
			   play.comboType == CardComboType::TRIPLET)
			{
				score -= play.comboLevel * 0.08;
			}

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


	return score;
}

//在一个随机补全样本中，评估我打出Play之后的局面收益
double evaluatePlayInDeal(GameState &state,InferredDeal &deal,CardCombo &play)
{
	//PASS无收益
	if(play.comboType == CardComboType::PASS)
		return 0;

	//取出我的手牌
	vector<Card> myHandBefore = deal.hands[state.myPosition];

	//假设打出play，出牌后的手牌
	vector<Card> myHandAfter = removeCardsFromHand(myHandBefore, play.cards);

	//如果出完，直接出
	if(myHandAfter.empty())
		return 10000;
	
	//比较手数
	int beforeCount = getMinHandCount(myHandBefore);
	int afterCount = getMinHandCount(myHandAfter);

	//样本收益
	double score = 0;

	//减少出手数，加分
	score += (beforeCount - afterCount) * 5.0;
	//出得多，接近出完
	score += play.cards.size() * 0.3;

	//===接入随机补全
	int nextPlayer = (state.myPosition + 1) % PLAYER_COUNT;
	bool nextPlaySameSide = isSameSidePlayer(state, nextPlayer);

	if(canPlayerBeatInDeal(deal,nextPlayer,play))
	{
		//下家是对手
		if(!nextPlaySameSide)
		{
			//普通情况
			score -= 4.0;

			//如果只剩3张牌，风险大
			if(state.cardRemaining[nextPlayer]<=3)
				score -= 8.0;
		}
		//下家是队友
		else
		{
			score -= 1.0;
		}
	}
	else
	{
		//下家压不了我
		score += 2.0;
	}

	return score;
}

//用一批随机补全样本，评估某一候选牌的平均局面价值
double evaluatePlayBySamples(GameState &state,vector<InferredDeal> &deals,CardCombo &play)
{
	if(deals.empty())
		return 0;

	//加权总分
	double totalScore = 0;
	//样本总权重
	double totalWeight = 0;

	for(InferredDeal &deal:deals)
	{
		if(deal.weight<=0)
		continue;

		//用样本评估候选牌
		double oneScore = evaluatePlayInDeal(state, deal, play);

		//按样本可信度加权
		totalScore += oneScore * deal.weight;
		//累计样本加权
		totalWeight += deal.weight;
	}
	if(totalWeight<=0)
		return 0;

	return totalScore / totalWeight;
}

//从所有合法出牌中选出启发式评分最高的前 topK个候选
vector<ScoredPlay> selectTopPlays(GameState &state,vector<CardCombo> &validPlays,int topK)
{
	//保存所有带分数的候选
	vector<ScoredPlay> scoredPlays;

	//判断当前是不是自由出牌
	bool freeTurn = state.lastValidCombo.comboType == CardComboType::PASS;

	for(CardCombo &play : validPlays)
	{
		if(freeTurn && play.comboType ==CardComboType::PASS)
			continue;

		//创建一条带分数的候选
		ScoredPlay scored;
		scored.play = play;
		//计算这手牌的启发式评分
		scored.score = evaluatePlayHScore(state, play);

		//放入候选
		scoredPlays.push_back(scored);
	}

	//按分数排序
	std::sort(scoredPlays.begin(), scoredPlays.end(),
			  [](ScoredPlay &a, ScoredPlay &b)
			  {
				  return a.score > b.score;
			  });
	
	//只保留前topK个
	if(topK>0 && scoredPlays.size()>topK)
		scoredPlays.resize(topK);

	return scoredPlays;
}

// 调试用：输出 TopK 候选的启发式分、样本分和最终融合分。
// 注意：这个函数只写 cerr，不应该在 Botzone 正式输出前默认调用。
void debugTopPlays(GameState &state,vector<CardCombo> &validPlays)
{
	// 先用和 decidePlay 一致的方式选出 TopK。
	vector<ScoredPlay> topPlays = selectTopPlays(state, validPlays, 6);

	// 只生成一次随机补全样本，保证每个候选使用同一批样本比较。
	vector<InferredDeal> deals = buildRandomDeals(state, 20);

	std::cerr << "[debug-top] topCount=" << topPlays.size()
			  << " dealCount=" << deals.size()
			  << " freeTurn=" << (state.lastValidCombo.comboType == CardComboType::PASS)
			  << " dangerous=" << isDangerousSituation(state)
			  << std::endl;

	double bestScore = -1e18;
	int bestIndex = -1;

	for(int i = 0; i < static_cast<int>(topPlays.size()); ++i)
	{
		ScoredPlay &scored = topPlays[i];

		// 计算样本平均分。
		scored.sampleScore = evaluatePlayBySamples(state, deals, scored.play);

		// 使用和 decidePlay 一致的样本权重规则。
		double sampleWeight = 0.4;
		if(state.lastValidCombo.comboType == CardComboType::PASS)
			sampleWeight = 0.1;
		if(isHardControlPlay(scored.play) && !isDangerousSituation(state))
			sampleWeight = 0;

		// 融合成最终分。
		double finalScore = scored.score + scored.sampleScore * sampleWeight;

		if(finalScore > bestScore)
		{
			bestScore = finalScore;
			bestIndex = i;
		}

		std::cerr << "[debug-top] index=" << i
				  << " type=" << static_cast<int>(scored.play.comboType)
				  << " level=" << scored.play.comboLevel
				  << " size=" << scored.play.cards.size()
				  << " cards=";

		for(Card card : scored.play.cards)
			std::cerr << card << ",";

		std::cerr << " heuristic=" << scored.score
				  << " sample=" << scored.sampleScore
				  << " sampleWeight=" << sampleWeight
				  << " final=" << finalScore
				  << std::endl;
	}

	std::cerr << "[debug-top] bestIndex=" << bestIndex
			  << " bestScore=" << bestScore
			  << std::endl;
}

// [我们要自己实现的核心函数] 在所有合法出牌中选出当前最优的一手，是后续策略升级的主入口。
//依赖于evaluatePlayGain
CardCombo decidePlay(GameState &state, vector<CardCombo> &validPlays)
{
	//如果没有任何合法候选，返回PASS
	if(validPlays.empty())
		return CardCombo();

	
	//===评分系统
	//初始化为PASS
	CardCombo bestplay;
	//当前最高分设为一个很小的数
	double bestScore = -1e18;

	//第一优先级：如果能直接出完牌，就立刻出
	for(CardCombo &play : validPlays)
	{
		if(isWinningPlay(state,play))
			return play;
	}

	//先用启发式评分选出前几个候选，后续 PIMC 只在这些候选里模拟
	vector<ScoredPlay> topPlays = selectTopPlays(state, validPlays, 6);

	//只生成一次随机补全样本
	vector<InferredDeal> deals = buildRandomDeals(state, 20);
	//为每个TopK候选计算样本平均分
	for(ScoredPlay &scored:topPlays)
	{
		scored.sampleScore = evaluatePlayBySamples(state, deals, scored.play);
	}


	//选启发式评分最高的一手
	for(ScoredPlay &scored:topPlays)
	{
		//设定随机样本的权重
		double sampleWeight = 0.4;
		if(state.lastValidCombo.comboType == CardComboType::PASS)
			sampleWeight = 0.1;
		if(isHardControlPlay(scored.play) && !isDangerousSituation(state))
			sampleWeight = 0;

		//启发式评分和样本评分
		double finalScore = scored.score + scored.sampleScore * sampleWeight;
		if(finalScore > bestScore)
		{
			bestScore = finalScore;
			bestplay = scored.play;
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
	//初始化随机数
	initRandomSeed();

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
