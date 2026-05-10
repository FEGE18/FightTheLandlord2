#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring> // 注意memset是cstring里的
#include <ctime>
#include <algorithm>
#include <vector>
#include <unordered_map>
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
	Level comboLevel = 0;	 // 算出的大小序（主牌的等级，如果是顺子型的以最高等级为准）

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

// 表示手牌可以拆分成的牌型组合：每一个groups[]都是一种牌型，所有groups[]加和就是当前的手牌集合）
// handCount表示同一手牌的不同出法的个数
struct HandPlan
{
	vector<CardCombo> groups;
	int handCount=0;
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
#pragma region enum
// [我们要自己实现的核心函数] 枚举当前局面下所有合法出牌，供策略层评估和比较；如果没有合法出牌则返回一个只包含 PASS 的列表
// todo
/* 
实现逻辑：
先扫描一遍当前手牌，记录每个level各有几张牌，可以快速筛选可能的牌型；
然后定向枚举挑牌，比如单张、对子、三带一等等；每找到一种合法组合，就将其放入candidate中；
接着把candidate传入构造函数CardCombo(start,end)中，得到comboType和comboLevel；
最后判断这组candidate是否合法
*/
vector<CardCombo> enumAllValidPlays(vector<Card>& hand,CardCombo& lastCombo){
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
	auto addPlay=[&](vector<Card> candidates){
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
	auto selectAttachment=[&](vector<Card> curCombo,int need,int type,set<Level>& ex){	
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
        for(auto& aRes:res)addPlay(aRes);	// -----使用&可优化性能？-----
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

// 威胁信息
struct ResponseThreatInfo{
    bool canBeat=false;          // 是否存在合法响应
    bool canWinNow=false;        // 是否存在一手直接压完
    bool canLeaveOneHand=false;  // 是否存在压完后只剩一手
    int minRemainCards=100;      // 所有合法响应中，压完后最少剩余牌数
    int minRemainHands=100;      // 所有合法响应中，压完后最少还需几手
    CardCombo bestResponse;      // 最危险的一手响应
};

int getMinHandCount(vector<Card> &hand);

/*这个接口内部可以复用 enumAllValidPlays；
不要重新写牌型判断；
PASS 和 INVALID 不算有效响应；
bestResponse 建议选 minRemainHands 最小的响应，如果并列可以选 remainCards 更少的；
requiredCombo == PASS 时可以直接返回 canBeat = false，因为自由出牌不是“压牌威胁分析”的场景*/

ResponseThreatInfo analyzeResponseThreat(vector<Card>& hand,CardCombo& requiredCombo){
    ResponseThreatInfo info;
    vector<CardCombo> responses=enumAllValidPlays(hand, requiredCombo);

    for(CardCombo &response:responses){
        if(response.comboType==CardComboType::PASS||response.comboType==CardComboType::INVALID)
            continue;

        info.canBeat=true;
        vector<Card> handAfter=removeCardsFromHand(hand,response.cards);	// 打出当前牌组后剩余的手牌
        int remainCards=handAfter.size();

		if(handAfter.empty()){
			info.minRemainHands=0;
			info.minRemainCards=0;
			info.bestResponse=response;
			break;
		}

		int remainHands=getMinHandCount(handAfter);
        if(remainHands<info.minRemainHands||
		(remainHands==info.minRemainHands&&remainCards<info.minRemainCards)){
			info.minRemainHands=remainHands;
			info.minRemainCards=remainCards;
			info.bestResponse=response;
		}
    }

	if(info.canBeat){
		info.canWinNow=(info.minRemainHands==0);
		info.canLeaveOneHand=(info.minRemainHands==1);
	}

    return info;
}


#pragma endregion
// ==================================================
// 单文件骨架：拆分层
// ==================================================
#pragma region decompose
int getMinHandCount(vector<Card> &hand);
void searchDecompose(vector<Card> curHand,HandPlan& curPlan,vector<HandPlan>& res,int maxD);
// [我们要自己实现的核心函数] 把手牌拆成若干组合法牌型，供策略层评估“最少还要几手出完”。
// 使用 MCTS，通过模拟来选取最优的若干组牌 <- 这句话是给策略层看的
// todo
// 返回最优的前topK组拆法，用beam search
vector<HandPlan> decomposeHand(vector<Card> &hand, int topK = 1){
	vector<HandPlan> allPlans;
	HandPlan ini;

	if(hand.empty()){
		allPlans.push_back(ini);
		return allPlans;
	}

	int dyLimit=getMinHandCount(hand)+3;	// 搜索深度由最少手数加一个值来限制，这个3后续可再调整
	searchDecompose(hand,ini,allPlans,dyLimit);

	// 按手数升序排序allPlans
	// 手数相同时优先保留炸弹、火箭等高价值牌型（通过累加权重作Tie-breaker）
	std::sort(allPlans.begin(),allPlans.end(),[](HandPlan& a,HandPlan& b){
		if(a.handCount!=b.handCount)
			return a.handCount<b.handCount;
			
		int weightA=0;
		for(CardCombo& c:a.groups)weightA+=c.getWeight();
		int weightB=0;
		for(CardCombo& c:b.groups)weightB+=c.getWeight();
		
		return weightA > weightB;
	});

	if(allPlans.size()>topK){
		allPlans.resize(topK);
	}

	// if(allPlans.empty()){}

	return allPlans;
}

// 回溯
// curHand是当前仍未匹配的牌，curPlan是遍历路径，res保存结果，maxDep剪枝
void searchDecompose(vector<Card> curHand,HandPlan& curPlan,vector<HandPlan>& res,int maxD){
	if(curHand.empty()){
		res.push_back(curPlan);
		return;
	}
	if(curPlan.groups.size()>=maxD)return;

	CardCombo empty;	// PASS牌型
	auto play=enumAllValidPlays(curHand,empty);	// 所有可能的牌型

	// 先对牌组排序，张数消耗得越多的牌组优先级越高
	std::sort(play.begin(),play.end(),[](CardCombo& a,CardCombo& b){
		// 如果牌组的张数相同，连牌优先（顺子、飞机、航天飞机）
		if(a.cards.size()==b.cards.size()){
			return a.comboType>b.comboType;
		}
		return a.cards.size()>b.cards.size();
	});

	int limit=5;	// 每个节点最多探索排名前limit的组法
	int curBranch=0;// 当前分支数
	// 回溯主体
	for(const CardCombo& p:play){
		if(p.comboType==CardComboType::PASS||p.comboType==CardComboType::INVALID)continue;
		if(curBranch>=limit)break;
		curBranch++;

		vector<Card> nextHand=removeCardsFromHand(curHand,p.cards);
		curPlan.groups.push_back(p);
		curPlan.handCount++;

		searchDecompose(nextHand,curPlan,res,maxD);

		curPlan.groups.pop_back();
		curPlan.handCount--;
	}
}

// 使用状态压缩dp来优化性能，总体思路是回溯法+贪心
static std::unordered_map<uint64_t,int> memo;	// 用于缓存getMinHandCount的结果，实现剪枝
											 	// 键：某一时刻手牌的牌型状态state；值：在当前存档（键）的状态下，出完所有牌的最少手数

// counts[]表示当前每一级牌还有几张
// wings表示额外的带牌空位：比如拿出了一个飞机（333444），三条可以带单张或对子，所以飞机主干被拿走后留下了2个翅膀空位，即wing=2；
//// 当进入到了递归的底层（此时只剩下散牌）时，就可以将wing组散牌装进飞机中，也就进一步减少了hands
int dfs(short counts[15],int wings){
	// 将counts转化为单一状态state
	// 压缩当前剩余手数情况及额外带牌名额，作为state键
	// 用uint64_t是因为其拷贝等操作快于int,string等
	uint64_t state=0;
	for(int i=0;i<15;i++){
		state=(state<<3)|counts[i];	// 将state左移三位，然后把counts[i]补在state的低三位上
	}
	state=(state<<6)|(wings&0b111111);	// 把wings加在state末六位，因为wing也是手牌拆分状态的组成部分
										// 这里的（wings&0b111111）是为了只保留wings的末六位，防止wings值过大导致污染前面的counts数据

	if(memo.count(state))return memo[state];	// 备忘录中已有state状态，直接返回

	// 统计四、三、二、单牌的具体数目，c[i]表示i牌有c[i]组
	// hands初始化为最笨蛋的出法
	int c[5]={0};
	for(int i=0;i<15;i++)c[counts[i]]++;
	int hands=c[1]+c[2]+c[3]+c[4];	//

	// 筛选火箭
	if(counts[level_JOKER]==1&&counts[level_joker]==1){
		hands-=1;
		c[1]-=2;
	}

	// 筛选翅膀
	int posWing=c[1]+c[2];	// 可能的翅膀数
	int needWing=c[3]+c[4]*2+wings;	// 手牌中实际需要的翅膀数，wings是从上一层继承下来的，也要加进来
	int matched=std::min(posWing,needWing);		// 实际用到的翅膀数，为二者较小者
	hands-=matched;

	int ans=hands;
	// 单顺，下面的筛选都使用滑动窗口
	for(int l=5;l<=12;l++){	// 分别筛选长度为5～12的单顺
		for(int start=0;start<=MAX_STRAIGHT_LEVEL-l+1;start++){
			bool valid=true;
			for(int i=0;i<l;i++){	// l是窗口长度，i遍历窗口查看是否可行
				if(counts[start+i]==0){	// 顺子里缺牌了，不行
										// 直接跳出内层循环，回到start所在循环，start++，从下一等级的牌开始遍历
					valid=false;
					break;
				}
			}
			if(valid){
				for(int i=0;i<l;i++)
					// 有合法的单顺，就把单顺中的牌从手牌计数器中删掉，然后拿着新的手牌计数器去dfs
					counts[start+i]-=1;
				ans=std::min(ans,dfs(counts,wings)+1);
				for(int i=0;i<l;i++)
					// 回溯，继续start的循环
					counts[start+i]+=1;
			}
		}
	}

	// 双顺
	for(int l=3;l<=10;l++){
		for(int start=0;start<=MAX_STRAIGHT_LEVEL-l+1;start++){
			bool valid=true;
			for(int i=0;i<l;i++){
				if(counts[start+i]<2){
					valid=false;
					break;
				}
			}
			if(valid){
				for(int i=0;i<l;i++)
					counts[start+i]-=2;
				ans=std::min(ans,dfs(counts,wings)+1);
				for(int i=0;i<l;i++)
					counts[start+i]+=2;
			}
		}
	}

	// 飞机
	for(int l=2;l<=6;l++){
		for(int start=0;start<=MAX_STRAIGHT_LEVEL-l+1;start++){
			bool valid=true;
			for(int i=0;i<l;i++){
				if(counts[start+i]<3){
					valid=false;
					break;
				}
			}
			if(valid){
				for(int i=0;i<l;i++)
					counts[start+i]-=3;
				// wings更新为wings+l（每组三排带一个翅膀）
				ans=std::min(ans,dfs(counts,wings+l)+1);
				for(int i=0;i<l;i++)
					counts[start+i]+=3;
			}
		}
	}
	memo[state]=ans;
	return memo[state];
}

// [我们要自己实现的核心函数] 快速返回当前手牌出完最少还需要几手，供评估层频繁调用
// todo

int getMinHandCount(vector<Card> &hand){
	if(hand.empty())return 0;
	short counts[15]={0};
	for(Card c:hand)
		counts[card2level(c)]++;
	return dfs(counts,0);
}

// 快速判断 hand 是否存在任意一手牌可以压过 lastCombo
// 这个函数只回答“能不能压”，不需要返回具体出哪几张牌
// 用counts频率表加快查找
bool canBeatComboFast(vector<Card>& hand,CardCombo& lastCombo){
	if(lastCombo.comboType==CardComboType::PASS)return true;
	if(hand.empty()||lastCombo.comboType==CardComboType::ROCKET)return false;	// hand判空好像有点没必要，但还是写一下

	short counts[15]={0};
	for(Card c:hand){
		counts[card2level(c)]++;
	}

	// 有火箭，必能压过
	if(counts[level_JOKER]==1&&counts[level_joker]==1)return true;

	// 有炸弹
	//// 如果上家是炸弹
	if(lastCombo.comboType==CardComboType::BOMB){
		for(Level i=lastCombo.comboLevel;i<level_joker;i++){
			// 手牌中有能压过上家的炸弹
			if(counts[i]==4)return true;
		}
		// 没有
		return false;
	}
	//// 如果上家不是炸弹
	else{
		for(Level i=0;i<level_joker;i++){
			if(counts[i]==4)return true;
		}
	}

	// 常规的同牌型对比
	//// 首先考虑手牌数量够不够，接着再考虑是否有对应牌型 & 能否压制
	if(hand.size()<lastCombo.cards.size())return false;
	
	int seqL=lastCombo.findMaxSeq();	// 上家主牌连续了多少组

	// 判断副牌够不够
	//// mStart：主牌起点；mLen：主牌长度；need：需要的副牌数；wingType：1-单张，2-对子
	auto canAttach=[&](int mStart,int mLen,int need,int wingType){
		int validWing=0;
		for(Level i=0;i<=MAX_LEVEL;){
			if(i>=mStart&&i<mStart+mLen){
				i=mStart+mLen;
				continue;
			}
			if(counts[i]>=wingType){
				validWing++;
			}
			i++;
		}
		return validWing>=need;
	};

	//// 开始同牌型比较
	switch(lastCombo.comboType){
		case CardComboType::SINGLE:{
			for(Level i=lastCombo.comboLevel+1;i<=MAX_LEVEL;i++){
				if(counts[i]>=1)return true;
			}
			return false;
		}

		case CardComboType::PAIR:{
			for(Level i=lastCombo.comboLevel+1;i<=MAX_LEVEL;i++){
				if(counts[i]>=2)return true;
			}
			return false;
		}

		case CardComboType::TRIPLET:
		case CardComboType::TRIPLET1:
		case CardComboType::TRIPLET2:{
			int wingType;
			if(lastCombo.comboType==CardComboType::TRIPLET)wingType=0;
			if(lastCombo.comboType==CardComboType::TRIPLET1)wingType=1;
			if(lastCombo.comboType==CardComboType::TRIPLET2)wingType=2;
			for(Level i=lastCombo.comboLevel+1;i<=MAX_LEVEL;i++){
				if(counts[i]>=3&&(wingType==0||canAttach(i,1,1,wingType)))return true;
			}
			return false;
		}

		case CardComboType::STRAIGHT:
		case CardComboType::STRAIGHT2:{
			int type=(lastCombo.comboType==CardComboType::STRAIGHT)? 1:2;
			int start=lastCombo.comboLevel-seqL+2;	// 从上家高一级开始
			for(Level i=start;i<=MAX_STRAIGHT_LEVEL-seqL+1;i++){
				bool valid=true;
				for(int j=0;j<seqL;j++){
					if(counts[i+j]<type){
						valid=false;
						break;
					}
				}
				if(valid)return true;
			}
			return false;
		}

		case CardComboType::PLANE:
		case CardComboType::PLANE1:
		case CardComboType::PLANE2:{
			int wingType;
			if(lastCombo.comboType==CardComboType::PLANE)wingType=0;
			if(lastCombo.comboType==CardComboType::PLANE1)wingType=1;
			if(lastCombo.comboType==CardComboType::PLANE2)wingType=2;

			int start=lastCombo.comboLevel-seqL+2;
			for(Level i=start;i<=MAX_STRAIGHT_LEVEL-seqL+1;i++){
				bool valid=true;
				for(int j=0;j<seqL;j++){
					if(counts[i+j]<3){
						valid=false;
						break;
					}
				}
				if(valid||canAttach(i,seqL,seqL,wingType)){
					return true;
				}
			}
			return false;
		}
		
		case CardComboType::QUADRUPLE2:
        case CardComboType::QUADRUPLE4:{
			int wingType=(lastCombo.comboType==CardComboType::QUADRUPLE2)? 1:2;
			for(Level i=lastCombo.comboLevel+1;i<level_joker;i++){
				if(counts[i]==4&&canAttach(i,1,2,wingType)){
					return true;
				}
			}
			return false;
		}

		case CardComboType::SSHUTTLE:
        case CardComboType::SSHUTTLE2:
        case CardComboType::SSHUTTLE4:{
			int wingType;
			if(lastCombo.comboType==CardComboType::SSHUTTLE)wingType=0;
			if(lastCombo.comboType==CardComboType::SSHUTTLE2)wingType=1;
			if(lastCombo.comboType==CardComboType::SSHUTTLE4)wingType=2;

			int start=lastCombo.comboLevel-seqL+2;
			for(Level i=start;i<=MAX_STRAIGHT_LEVEL-seqL+1;i++){
				bool valid=true;
				for(int j=0;j<seqL;j++){
					if(counts[i+j]<4){
						valid=false;
						break;
					}
				}
				if(valid||canAttach(i,seqL,seqL*2,wingType)){
					return true;
				}
			}
			return false;
		}

		default: return false;
	}
}
#pragma endregion
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
    	int knownCount = deal.hands[player].size();

		if(knownCount > state.cardRemaining[player])
		{
    		deal.weight = 0.0;
    		return deal;
		}

		totalNeedCount += state.cardRemaining[player] - knownCount;

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

		// 如果 needCount 为负，说明这个玩家已知牌比剩余牌还多，状态不合法。
    	// 如果 cnt + needCount 超过 unknownCard 数量，说明未知牌不够发，也是不合法样本。
    	if(needCount < 0 || cnt + needCount > static_cast<int>(unknownCard.size()))
    	{
        	deal.weight = 0.0;
        	return deal;
    	}

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
		
		if(canBeatComboFast(deal.hands[constraint.player], constraint.requirCombo))
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
#ifndef _BOTZONE_ONLINE
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
#endif


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

	//四带二、四带两对也属于硬控牌
	if(play.comboType == CardComboType::QUADRUPLE2 || play.comboType == CardComboType::QUADRUPLE4)
		return true;
	
	//航天飞机也是
	if (play.comboType == CardComboType::SSHUTTLE ||
        play.comboType == CardComboType::SSHUTTLE2 ||
        play.comboType == CardComboType::SSHUTTLE4)
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

	// 如果当前有效出牌来自队友，不能简单放弃牌权。
	// 因为比赛收益和“谁先出完”有关，我也需要适当主动争取出完机会。
	// 只有队友已经很接近出完，而我自己还需要较多手时，才倾向让队友继续。
	if(state.lastValidPlayer >= 0 && isSameSidePlayer(state, state.lastValidPlayer))
	{
		int teammate = state.lastValidPlayer;

		if(state.cardRemaining[teammate] <= 2 && myHandCount > 3)
			return false;
	}

	//如果我的下家是对手，而他只剩几张牌，就要抢
	int nextPlayer = (state.myPosition + 1) % PLAYER_COUNT;
	if(!isSameSidePlayer(state,nextPlayer) && state.cardRemaining[nextPlayer] <= 3)
		return true;

	//地主手牌较顺，主动控局
	if(state.isLandlord() && myHandCount<=5)
		return true;
	
	// 农民看地主剩牌。
	// 地主剩 1~2 张：强危险，必须争夺牌权
	// 地主剩 3~5 张：进入警戒，要更积极，但如果当前是队友出的牌，先不要盲目抢队友
	if(!state.isLandlord())
	{

    	int landlordRemain = state.cardRemaining[state.landlordPosition];
    	// 地主只剩 1~2 张
    	if(landlordRemain <= 2)
        	return true;

    	// 地主剩 3~5 张时，进入中度警戒。
    	if(landlordRemain <= 5)
    	{
        	if(state.lastValidPlayer == state.landlordPosition)
            	return true;

        	if(state.lastValidCombo.comboType == CardComboType::PASS)
            	return true;

        	// 当前是队友出的牌
        	if(state.lastValidPlayer >= 0 && isSameSidePlayer(state, state.lastValidPlayer))
            	return false;

        	return true;
    	}
	}


		return false;
}

// [我们要自己实现的核心函数] 评估整手牌强度，主要用于叫分决策和后续参数调优。
//如果只看我自己这手牌，不看当前桌面动作，这手牌到底强不强
double evaluateHandStrength(vector<Card> &hand)
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

// 轻量叫分评估函数：
// 这个函数专门给 decideBid 使用，不能调用 decomposeHand / getMinHandCount 这类搜索函数。
// Botzone 每步只有 1 秒，叫分阶段必须用稳定的 O(牌数 + 牌面等级) 统计评估。
double evaluateBidStrength(vector<Card> &hand)
{
	// 从 0 开始累计叫分强度。
	double score = 0.0;

	// 按牌面等级分组，便于统计炸弹、对子、三条、高牌和连续牌潜力。
	auto grouped = groupCardsByLevel(hand);

	// 高牌分：K、A、2、王是叫地主时最重要的控牌资源。
	static double highCardBonus[MAX_LEVEL] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 3~Q
		0.8,                          // K
		1.5,                          // A
		2.5,                          // 2
		3.5,                          // 小王
		4.0                           // 大王
	};

	// 逐个等级累加高牌分。
	for(Level level = 0; level < MAX_LEVEL; level++)
	{
		score += highCardBonus[level] * grouped[level].size();
	}

	// 火箭是最强控制资源，额外加分。
	if(!grouped[level_joker].empty() && !grouped[level_JOKER].empty())
		score += 4.0;

	// 炸弹、三条、对子都是结构资源。
	for(Level level = 0; level < level_joker; level++)
	{
		int count = grouped[level].size();

		// 炸弹能强行拿回牌权，叫地主价值很高。
		if(count == 4)
			score += 6.0;

		// 三条容易组成三带，是比较好的整理结构。
		else if(count == 3)
			score += 2.0;

		// 对子比单张更整齐。
		else if(count == 2)
			score += 1.0;
	}

	// 低散牌惩罚：3~9 的单张当地主时较难处理。
	for(Level level = 0; level <= 6; level++)
	{
		if(grouped[level].size() == 1)
			score -= 0.7;
	}

	// 中散牌小惩罚：10、J、Q 的单张也会增加出牌压力，但比低散牌好一些。
	for(Level level = 7; level <= 9; level++)
	{
		if(grouped[level].size() == 1)
			score -= 0.3;
	}

	// 单顺潜力：只看 3~A，不含 2 和王。
	// 连续长度达到 5 就说明手牌有一定整理能力。
	int singleRun = 0;
	int bestSingleRun = 0;
	for(Level level = 0; level <= MAX_STRAIGHT_LEVEL; level++)
	{
		if(!grouped[level].empty())
			singleRun++;
		else
			singleRun = 0;

		if(singleRun > bestSingleRun)
			bestSingleRun = singleRun;
	}
	if(bestSingleRun >= 5)
		score += 1.2 + (bestSingleRun - 5) * 0.25;

	// 连对潜力：连续对子达到 3 对就有价值。
	int pairRun = 0;
	int bestPairRun = 0;
	for(Level level = 0; level <= MAX_STRAIGHT_LEVEL; level++)
	{
		if(grouped[level].size() >= 2)
			pairRun++;
		else
			pairRun = 0;

		if(pairRun > bestPairRun)
			bestPairRun = pairRun;
	}
	if(bestPairRun >= 3)
		score += 1.8 + (bestPairRun - 3) * 0.35;

	// 飞机潜力：连续三条达到 2 组就很有价值。
	int tripletRun = 0;
	int bestTripletRun = 0;
	for(Level level = 0; level <= MAX_STRAIGHT_LEVEL; level++)
	{
		if(grouped[level].size() >= 3)
			tripletRun++;
		else
			tripletRun = 0;

		if(tripletRun > bestTripletRun)
			bestTripletRun = tripletRun;
	}
	if(bestTripletRun >= 2)
		score += 2.5 + (bestTripletRun - 2) * 0.5;

	// 粗略手数惩罚：
	// 不做搜索，只按不同点数的组数估算碎片数量，避免叫分阶段超时。
	int roughGroups = 0;
	for(Level level = 0; level < MAX_LEVEL; level++)
	{
		if(!grouped[level].empty())
			roughGroups++;
	}

	// 火箭两张王实际可以一手出，粗略组数里算了两组，这里修正一下。
	if(!grouped[level_joker].empty() && !grouped[level_JOKER].empty())
		roughGroups--;

	// 牌越碎，叫地主风险越高。
	score -= roughGroups * 0.45;

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

	// 次要指标：一次打出更多牌通常更接近胜利。
	// 但手数减少已经是核心指标，这里只给弱奖励，避免长顺/长连对被重复抬得过高。
	gain += play.cards.size() * 0.15;
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


	// 第二层：细粒度评分。
	// 叫分阶段必须非常快，不能调用 decomposeHand 这类搜索型评估。
	double bidScore = evaluateBidStrength(hand);

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

//自由出牌时，评估这手牌作为主动出牌的价值
//这个函数只处理“主动出什么更顺”，不处理压对手，让队友和危险局面
double evaluateFreeTurn(CardCombo &play)
{
	double bonus = 0;

	//PASS无意义
	if(play.comboType==CardComboType::PASS)
		return -10000;
	
	//炸弹和火箭是硬控资源，自由轮不应该主动消耗
	if(play.comboType==CardComboType::BOMB)
		return -8;
	if(play.comboType==CardComboType::ROCKET)
		return -10;

	//长顺子价值很高
	if(play.comboType == CardComboType::STRAIGHT)
	{
		// 手数减少和出牌张数已经由 evaluatePlayGain 负责。
		// 这里只给顺子这种结构本身的修正分，避免重复奖励“出得多”。
		bonus += 1.5;
		bonus += play.cards.size() * 0.12;

		//高顺会消耗 QKA这种控牌，稍微扣分
		bonus -= play.comboLevel * 0.06;
	}
	//连对价值略低
	else if(play.comboType==CardComboType::STRAIGHT2)
	{
		bonus += 1.5;
		bonus += play.cards.size() * 0.10;

		//高顺子也会浪费控牌资源
		bonus -= play.comboLevel * 0.05;
	}
	//飞机价值很高
	else if(play.comboType==CardComboType::PLANE || play.comboType==CardComboType::PLANE1 || play.comboType==CardComboType::PLANE2)
	{
		bonus += 2.0;
		bonus += play.cards.size() * 0.10;

		//高飞机也会浪费资源
		bonus -= play.comboLevel * 0.03;
	}
	//四带二
	else if (play.comboType == CardComboType::QUADRUPLE2 || play.comboType == CardComboType::QUADRUPLE4)
	{
		// 四带二也能快速减少手牌，属于较强的整理牌型。
		bonus += 1.2;
		bonus += play.cards.size() * 0.06;
		bonus -= play.comboLevel * 0.05;
	}
	else if (play.comboType == CardComboType::SSHUTTLE ||
	         play.comboType == CardComboType::SSHUTTLE2 ||
	         play.comboType == CardComboType::SSHUTTLE4)
	{
		// 航天飞机是更强的连续整理牌型，价值应略高于四带二。
		bonus += 1.8;
		bonus += play.cards.size() * 0.08;
		bonus -= play.comboLevel * 0.03;
	}

	//三条带散
	else if(play.comboType==CardComboType::TRIPLET1 || play.comboType==CardComboType::TRIPLET2)
	{
		bonus += 1.2;
		bonus += play.cards.size() * 0.06;

		//高三条
		bonus -= play.comboLevel * 0.05;

		// 三带二如果带走 A、2、王这类控牌，通常不是单纯的整理牌。
		// 控牌损失函数会整体扣一次，这里只补一点自由轮结构层面的保守修正。
		for(Card card : play.cards)
		{
			Level level = card2level(card);
			if(level >= 11)
				bonus -= 0.6;
		}
	}

	else if (play.comboType == CardComboType::TRIPLET)
	{
		// 自由出牌时，裸三条通常很亏。
		// 如果能三带一或三带二，就应该顺手带走散牌，而不是只出三张。
		bonus -= 3.0;

		// 高位三条还有控牌价值，更不应该裸出。
		bonus -= play.comboLevel * 0.08;
	}

	else if (play.comboType == CardComboType::PAIR)
	{
		// 自由出牌时，低对子可以主动走掉，减少手牌碎片。
		bonus += 0.8;

		// 高对子仍然有一定控牌价值，所以点数越高扣得越多。
		bonus -= play.comboLevel * 0.08;
	}
	else if (play.comboType == CardComboType::SINGLE)
	{
		// 自由出牌时，单张通常是最弱的主动出牌。
		// 只有低单张适合先走，高单张应尽量保留控牌。
		bonus -= play.comboLevel * 0.12;
	}


	return bonus;
}

// 判断一副手牌里是否还保留明显控牌。
// 这里的控牌不是严格数学定义，而是策略层的近似：
// 2、王、炸弹都可以在关键时刻抢回牌权。
bool hasControlCard(vector<Card> &hand)
{
	// 先按点数分组，便于判断炸弹。
	auto grouped = groupCardsByLevel(hand);

	// 只要还有 2，就认为还有单牌/对子层面的控制力。
	if(!grouped[12].empty())
		return true;

	// 小王是控制牌。
	if(!grouped[level_joker].empty())
		return true;

	// 大王是控制牌。
	if(!grouped[level_JOKER].empty())
		return true;

	// 任意炸弹也是控制牌。
	for(Level level = 0; level < level_joker; level++)
	{
		if(grouped[level].size() == 4)
			return true;
	}

	// 否则认为没有明显控牌。
	return false;
}

// 计算一副手牌的控牌价值。
// 这个值不是整手牌强度，只表示关键时刻抢回牌权的能力。
// 后续可以用“出牌前控牌价值 - 出牌后控牌价值”判断一手牌消耗了多少控牌资源。
double getControlValue(vector<Card> &hand)
{
	// 按点数分组，方便统计 2、王、炸弹。
	auto grouped = groupCardsByLevel(hand);

	// 从 0 开始累计控牌价值。
	double value = 0.0;

	// 统计 2 的数量。
	int twoCount = grouped[12].size();

	// 单张 2 是基本控牌。
	if(twoCount == 1)
		value += 2.5;

	// 一对 2 可以压多数对子，也可以拆成两次单牌控制。
	else if(twoCount == 2)
		value += 5.8;

	// 三张 2 控制力更强，既能拆，也能作为三条压制。
	else if(twoCount == 3)
		value += 8.5;

	// 四个 2 极其特殊，既是高炸弹，也能拆成多次控牌。
	else if(twoCount == 4)
		value += 13.0;

	// 小王是强控牌。
	if(!grouped[level_joker].empty())
		value += 4.0;

	// 大王比小王略强。
	if(!grouped[level_JOKER].empty())
		value += 4.8;

	// 双王同时存在时，额外计算火箭价值。
	if(!grouped[level_joker].empty() && !grouped[level_JOKER].empty())
		value += 5.0;

	// 普通炸弹也有控牌价值。
	for(Level level = 0; level < level_joker; level++)
	{
		// 只统计四张同点数的炸弹。
		if(grouped[level].size() == 4)
		{
			// 低位炸弹价值较低，高位炸弹价值较高。
			value += 4.0 + level * 0.25;
		}
	}

	// 返回整手牌的控牌价值。
	return value;
}

// 评估某一手牌造成的控牌资源损失。
// 这个函数只看“我打出这手以后，手里还剩多少抢牌权能力”。
// 返回值是负分：损失越大，扣分越多。
double evaluateControlLoss(GameState &state, CardCombo &play)
{
	// PASS 不消耗手牌资源。
	if(play.comboType == CardComboType::PASS)
		return 0.0;

	// 如果这一手直接出完，控牌损失不重要。
	if(isWinningPlay(state, play))
		return 0.0;

	// 计算出牌后的手牌。
	vector<Card> handAfter = removeCardsFromHand(state.myCards, play.cards);

	// 如果出完后只剩一手，说明我已经接近胜利，不要因为控牌损失过度保守。
	if(!handAfter.empty() && getMinHandCount(handAfter) == 1)
		return 0.0;

	// 计算出牌前后的控牌价值差。
	double controlBefore = getControlValue(state.myCards);
	double controlAfter = getControlValue(handAfter);
	double controlLoss = controlBefore - controlAfter;

	// 没有消耗控牌，或者控牌价值反而没下降，就不扣分。
	if(controlLoss <= 0.0)
		return 0.0;

	// 判断当前是不是自由出牌。
	bool freeTurn = state.lastValidCombo.comboType == CardComboType::PASS;

	// 判断当前是不是压对手。
	bool followingOpponent = !freeTurn && state.lastValidPlayer >= 0 && !isSameSidePlayer(state, state.lastValidPlayer);

	// 判断当前是不是必须压住快出完的对手。
	bool followingDangerousOpponent = followingOpponent && state.cardRemaining[state.lastValidPlayer] <= 2;

	// 基础扣分系数。
	double rate = 0.35;

	// 自由轮主动交控牌更亏，因为不是被迫防守。
	if(freeTurn)
		rate = 0.45;

	// 危险自由轮更要保留控牌，避免下一轮拦不住对手。
	if(freeTurn && isDangerousSituation(state))
		rate = 0.65;

	// 如果正在压危险对手，交控牌是必要成本，扣分要降低。
	if(followingDangerousOpponent)
		rate = 0.15;

	// 计算控牌损失扣分。
	double penalty = controlLoss * rate;

	// 四个 2 不能当普通炸弹处理。
	// 除非是在压危险对手，否则主动或普通局面打出 2222 都要额外扣分。
	if(play.comboType == CardComboType::BOMB && play.comboLevel == 12 && !followingDangerousOpponent)
	{
		if(freeTurn)
			penalty += 4.0;
		else
			penalty += 2.0;
	}

	// 返回负分，表示这手牌消耗了控牌资源。
	return -penalty;
}

//评估出完这手之后，只剩一手牌的质量
// 这个函数只在 handAfter 的最小手数为 1 时有意义。
// 返回值：好的一手加分，差的一手少加甚至扣分。
double evaluateOneTurnLeftQuality(GameState &state,vector<Card> &handAfter)
{
	//如果已经出完不套路
	if(handAfter.empty())
		return 0;
	
	if(getMinHandCount(handAfter) != 1)
		return 0;

	//把剩余手牌识别成一个牌型
	CardCombo remainCombo(handAfter.begin(), handAfter.end());

	//累计质量分
	double score = 0;

	//剩一手多张牌通常更好
	score += handAfter.size() * 0.4;

	//剩单张，要看点数
	if(remainCombo.comboType == CardComboType::SINGLE)
	{
		//低单张很危险
		if(remainCombo.comboLevel <=6)
			score -= 3;
		else if(remainCombo.comboLevel<=10)
			score -= 1;
		else
			score += 2;
	}
	//剩顺子、连对等，质量较好
	else if(remainCombo.comboType==CardComboType::STRAIGHT || remainCombo.comboType==CardComboType::STRAIGHT2 || remainCombo.comboType==CardComboType::TRIPLET||
			remainCombo.comboType==CardComboType::TRIPLET1 || remainCombo.comboType==CardComboType::TRIPLET2 || remainCombo.comboType==CardComboType::PLANE ||
			remainCombo.comboType==CardComboType::PLANE1 || remainCombo.comboType==CardComboType::PLANE2)
	{
		score += 2.5;
	}

	//如果当前是危险局面，剩低单张更糟
	if(isDangerousSituation(state) && remainCombo.comboType == CardComboType::SINGLE &&
			remainCombo.comboLevel<=10)
	{
				score -= 2;
	}

	return score;
}

// 危险局面下，自由出牌额外评估。
// 这个函数只处理一种情况：我有主动权，但对手快出完了。
// 核心思想：自由轮不能只贪图“多出几张”，还要保留能拦住对手的控牌。
double evaluateDangerousFreeTurn(GameState &state, CardCombo &play)
{
	// 如果不是自由出牌，这个函数不负责处理。
	if(state.lastValidCombo.comboType != CardComboType::PASS)
		return 0;

	// PASS 在自由轮不会被选择，这里不额外处理。
	if(play.comboType == CardComboType::PASS)
		return 0;

	// 如果这一手可以直接出完，永远应该优先出完，不扣分。
	if(isWinningPlay(state, play))
		return 0;

	// 先计算出牌后的剩余手牌。
	vector<Card> handAfter = removeCardsFromHand(state.myCards, play.cards);

	// 计算出牌前后的控牌价值。
	double controlBefore = getControlValue(state.myCards);
	double controlAfter = getControlValue(handAfter);
	double controlLoss = controlBefore - controlAfter;

	// 如果出牌前有控牌，出牌后几乎没有控牌，说明这一手把最后的防守资源交掉了。
	bool lostLastControl = controlBefore > 0.0 && controlAfter <= 0.1;

	// 如果出完后只剩一手，说明我也很接近胜利，不要过度保守。
	if(!handAfter.empty() && getMinHandCount(handAfter) == 1)
		return 0;

	// 从 0 开始累计危险惩罚。
	double penalty = 0;

	// 农民视角：地主剩牌越少，我越不能乱交控牌。
	if(!state.isLandlord())
	{
		// 读取地主剩余牌数。
		int landlordRemain = state.cardRemaining[state.landlordPosition];

		// 找到我的农民队友。
		// 农民局里除了我和地主，剩下的那个玩家就是队友。
		int teammate = -1;
		for(int player = 0; player < PLAYER_COUNT; player++)
		{
			if(player != state.myPosition && player != state.landlordPosition)
				teammate = player;
		}

		// 地主剩 5 张以内已经进入预警。
		if(landlordRemain <= 5)
		{
			// 主动打出硬控牌很危险，例如炸弹、火箭、四带二。
			if(isHardControlPlay(play))
				penalty -= 8.0;

			// 主动打出 2 或王，也会削弱后续拦截能力。
			for(Card card : play.cards)
			{
				Level level = card2level(card);

				// 2 是重要控牌，地主快跑时不能随便主动打掉。
				if(level == 12)
					penalty -= 2.0;

				// 小王更重要。
				if(level == level_joker)
					penalty -= 3.0;

				// 大王最重要。
				if(level == level_JOKER)
					penalty -= 3.5;

			}

			// 如果这一手把我最后的控牌打没了，危险局面下要重罚。
			// 典型错误就是地主还剩 2 张，我自由轮主动打掉最后一对 2。
			if(lostLastControl)
				penalty -= 6.0;

			// 如果没有完全打光控牌，但控牌价值大幅下降，也要额外扣一点。
			if(controlLoss > 6.0)
				penalty -= (controlLoss - 6.0) * 0.25;

			// 地主快出完时，如果我自己也不远了，就不要轻易把主动权交出去。
			// 这里不是禁止队友接牌，而是提高我自己连续进攻的价值。
			if(state.myCards.size() <= 10 && state.cardRemaining[teammate] > state.myCards.size())
				penalty += 2.0;
		}

		// 地主剩 2 张以内是强危险状态，惩罚再加重。
		if(landlordRemain <= 2)
			penalty *= 1.6;
	}

	// 地主视角：任意农民快出完，也要避免自由轮乱交硬控。
	else
	{
		for(int player = 0; player < PLAYER_COUNT; player++)
		{
			// 跳过自己。
			if(player == state.myPosition)
				continue;

			// 地主没有队友，其他玩家都是对手。
			if(state.cardRemaining[player] <= 5)
			{
				if(isHardControlPlay(play))
					penalty -= 6.0;

				// 地主自己没有队友，如果把最后控牌打没，后面可能完全拦不住农民。
				if(lostLastControl)
					penalty -= 5.0;

				// 控牌价值大幅下降时，即使还没完全打光，也要有所保留。
				if(controlLoss > 6.0)
					penalty -= (controlLoss - 6.0) * 0.20;

				for(Card card : play.cards)
				{
					Level level = card2level(card);

					if(level == 12)
						penalty -= 1.5;
					if(level == level_joker)
						penalty -= 2.5;
					if(level == level_JOKER)
						penalty -= 3.0;
				}
			}
		}
	}

	// 返回危险自由轮的额外修正分。
	return penalty;
}


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

	// 判断当前是不是农民在跟地主的牌。
	bool followingLandlord = followingOpponent && !state.isLandlord() && state.lastValidPlayer == state.landlordPosition;

	// 判断我是不是地主下家。
	// 地主下家是地主后手的第一个农民。
	bool myIsLandlordNext = !state.isLandlord() && state.myPosition == (state.landlordPosition + 1) % PLAYER_COUNT;

	// 判断我是不是地主上家。
	// 地主上家是地主出牌前的最后一个农民，拦地主责任更重。
	bool myIsLandlordPrev = !state.isLandlord() && state.myPosition == (state.landlordPosition + 2) % PLAYER_COUNT;

	// 判断当前是不是“队友出了较大的牌，我最好别抢”。
	bool teammateHighCardShouldHold = followingTeammate &&  state.lastValidCombo.comboLevel >= 9 &&
                                  (
                                      // 我是地主上家，队友是地主下家；队友先出了大牌，我不要乱压。
                                      (myIsLandlordPrev && state.lastValidPlayer == (state.landlordPosition + 1) % PLAYER_COUNT) ||

                                      // 我是地主下家，队友是地主上家；地主已经没压队友，我也不要乱压。
                                      (myIsLandlordNext && state.lastValidPlayer == (state.landlordPosition + 2) % PLAYER_COUNT)
                                  );

	// 记录下一个出牌的人。
	int nextPlayer = (state.myPosition + 1) % PLAYER_COUNT;

	// 判断下家是不是我的队友。
	bool nextPlayerIsTeammate = isSameSidePlayer(state, nextPlayer);

	// 判断是否适合送队友报单。
	// 只有自由出牌时才考虑，因为跟牌阶段不能随便选择牌型。
	// 队友只剩 1 张时，出单张最可能让队友直接走完。	
	bool shouldFeedTeammateSingle = freeTurn && nextPlayerIsTeammate && state.cardRemaining[nextPlayer] == 1;

	// 判断地主是否只剩 1 张。
	bool landlordOnlyOneLeft = !state.isLandlord() &&  state.cardRemaining[state.landlordPosition] == 1;

	// 判断下一个出牌的人是不是地主。
	bool nextPlayerIsLandlord = nextPlayer == state.landlordPosition;

	// 判断自由出牌时是否不能出单张。
	// 如果地主只剩 1 张，或者下家就是地主，出单张风险很高。
	bool shouldAvoidSingleForLandlord = freeTurn && !state.isLandlord() && landlordOnlyOneLeft && play.comboType == CardComboType::SINGLE;

	// 判断地主是否只剩 2 张。
	bool landlordOnlyTwoLeft = !state.isLandlord() && state.cardRemaining[state.landlordPosition] == 2;

	// 判断自由出牌时是否不能随便出对子。
	bool shouldAvoidPairForLandlord = freeTurn && !state.isLandlord() && landlordOnlyTwoLeft && play.comboType == CardComboType::PAIR;

	// 判断当前是否是农民自由出牌，但地主已经进入 3~5 张警戒区。
	// 这种局面下，自由出牌不能只考虑整理自己的牌，还要考虑不能轻易把牌权交给地主。
	bool freeTurnAgainstWarningLandlord = freeTurn && !state.isLandlord() && state.cardRemaining[state.landlordPosition] <= 5 && state.cardRemaining[state.landlordPosition] > 2;


	// 地主剩 5 张以内时，就进入警戒状态。
	// 注意这不是最高危险，最高危险仍然是 <= 2。
	bool followingWarningLandlord = followingLandlord && state.cardRemaining[state.landlordPosition] <= 5;

	// 判断当前是否应该主动争夺牌权
	bool fightForControl = shouldFightControl(state);

	//复用已有的基础收益评分,只看出牌后，我的手牌有没有变好
	double score = evaluatePlayGain(state.myCards, play, state);

	// 额外计算控牌资源损失。
	// 这一步用来避免把 2、王、炸弹，尤其是 2222，轻易当普通牌型消耗掉。
	score += evaluateControlLoss(state, play);

	//压队友的情况
	if(followingTeammate && play.comboType != CardComboType::PASS)
	{
			score -= 0.3;

			 // 如果队友已经出了一手较大的牌，并且这手牌有机会卡住地主，
    		// 我们再压队友就等于把队友的防守效果拆掉，要额外扣分。
    		if(teammateHighCardShouldHold)
        		score -= 3;
			
			//不应该用炸弹
			if(isHardControlPlay(play))
			{
				//在用炸弹炸队友的情况下，考虑是不是危险局面及控牌局面
				double teammateHardControl = 10;
				if(fightForControl)
					teammateHardControl *= 0.5;
				if(dangerous)
					teammateHardControl *= 0.25;

				score -= teammateHardControl;
			}

			//用 2 或王压队友也比较亏，普通局面要扣分
			
			//如果在争夺牌权的情况下，用2或王需要分开处理
			double teammateHighCardRate = 1;

			//在抢牌权时
			if(fightForControl)
				teammateHighCardRate = 0.5;
			//危险局面下
			if(dangerous)
				teammateHighCardRate = 0.25;
			for(Card card:play.cards)
			{
				Level level = card2level(card);
				if( level == 12)
					score -= 1 * teammateHighCardRate;
				if(level==level_joker)
					score -= 1.5 * teammateHighCardRate;
				if(level== level_JOKER)
					score -= 1.8 * teammateHighCardRate;
			}
		
			// 如果我是地主下家，而队友是地主上家
    		if(myIsLandlordNext && state.lastValidPlayer == (state.landlordPosition + 2) % PLAYER_COUNT)
    		{
        		// 直接压队友会破坏队友对地主的拦截。
        		score -= 5.0;

        // 如果队友出的牌本来就比较大，说明它更可能卡住地主，
        		if(state.lastValidCombo.comboLevel >= 9)
            		score -= 3.0;
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
			{
		
    			// 如果我是地主下家，而队友是地主上家，
    			if(myIsLandlordNext && state.lastValidPlayer == (state.landlordPosition + 2) % PLAYER_COUNT)
   			 	{
        			score += 2.0;
        			// 队友牌本身越大，越可能卡住地主，PASS 更合理。
        			if(state.lastValidCombo.comboLevel >= 9)
            			score += 2.0;
    			}
    			else if(state.cardRemaining[state.lastValidPlayer] <= 2)
    			{
        			score += 1.0;
    			}
    			else if(fightForControl)
    			{
        			// 需要争牌权时，普通 PASS 队友会稍微亏
        			score -= 2.0;
    			}
    			else
    			{
        			// 普通配合局面，小幅扣分，避免无脑过
        			score -= 0.5;
    			}


			}
			else if(followingOpponent)
			{
				//如果我是农民，并且当前在跟地主的牌
				// 地主剩 3~5 张时，PASS 可能让地主直接续上牌权，所以要额外扣分。
    			// 地主剩 <=2 张的情况已经由 followingDangerousOpponent 重罚，这里不重复处理
				if(followingWarningLandlord && !followingDangerousOpponent)
				{
					//记录地主还剩几张牌
					int landlordRemain = state.cardRemaining[state.landlordPosition];

					//地主接近出完，PASS代价更高
					// 剩 5 张扣 3，剩 4 张扣 4，剩 3 张扣 5
					score -= 3 + (5 - landlordRemain) * 1;
				}
				
			// 如果当前整体判断应该争夺牌权，PASS 继续扣分。
			if(fightForControl)
				score -= 5.0;
			else
				score -= 2.0;
			}
		}

		//硬控牌
		if(isHardControlPlay(play))
		{
			if(followingDangerousOpponent)
    		{
        		// 压危险对手时，硬控牌可以接受。
        		score += 2.0;
    		}
    		else if(!followingTeammate)
    		{
        		// 不是压队友、也不是压危险对手时，正常扣资源成本。
        		score -= 6.0;

				// 如果当前要压的是炸弹，说明我也必须交更高级硬控。
        		// 普通局面下这类资源非常贵，除非危险，否则更倾向 PASS 保留。
        		if(state.lastValidCombo.comboType == CardComboType::BOMB)
            		score -= 4.0;
    		}
		}

		//普通局面下保护2和王
		if(!followingDangerousOpponent && !followingTeammate &&play.comboType!=CardComboType::PASS)
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

			// 如果我是农民，并且当前压的是地主的牌。
    		// 地主剩 3~5 张时，压地主比压普通对手更重要。
    		if(followingWarningLandlord && !followingDangerousOpponent)
    		{
        		// 记录地主剩牌数。
        		int landlordRemain = state.cardRemaining[state.landlordPosition];

        		// 地主剩牌越少，成功压住他的价值越高。
        		// 剩 5 张奖励 2.0，剩 4 张奖励 2.8，剩 3 张奖励 3.6。
        		score += 2.0 + (5 - landlordRemain) * 0.8;

        		// 如果我是地主上家，且现在轮到我最后拦地主，
        		// 这时不能指望队友再补救，所以额外鼓励压牌。
        		if(state.myPosition == (state.landlordPosition + 2) % PLAYER_COUNT)
            		score += 2.0;
    		}

			// 普通跟牌时，同牌型里优先用刚好能压过的低牌。
    		// 否则样本分可能会把 AA、2、王这种高控牌推到前面。
    		if(play.comboType == CardComboType::SINGLE ||play.comboType == CardComboType::PAIR ||play.comboType == CardComboType::TRIPLET)
    		{
        		double levelCostRate = fightForControl ? 0.08 : 0.12;
        		score -= play.comboLevel * levelCostRate;
    		}

			// 如果我是地主上家，并且当前正在压地主的牌
    		if(myIsLandlordPrev && state.lastValidPlayer == state.landlordPosition)
    		{
        		score += 3.0;

        		int landlordRemain = state.cardRemaining[state.landlordPosition];

        		if(landlordRemain <= 5)
            		score += (6 - landlordRemain) * 1.0;
    		}

		}

		//自由出牌
		// 自由出牌时，我拥有主动权，应优先打出能整理手牌的组合
		if (freeTurn && play.comboType != CardComboType::PASS)
		{
			// 如果下家队友只剩 1 张，我自由出牌时优先出单张送队友
    		if(shouldFeedTeammateSingle)
    		{
        		if(play.comboType == CardComboType::SINGLE)
        		{
            		score += 8.0;

            		// 单张越小越好
            		score -= play.comboLevel * 0.15;
        		}
        		else
        		{
            		// 不出单张会错过送队友的机会，轻微扣分。
            		score -= 4.0;
        		}
    		}
			
			// 如果地主只剩 1 张，我自由出牌时不能随便出单张
    		if(shouldAvoidSingleForLandlord)
    		{
        		score -= 10.0;

        		// 下家就是地主
        		if(nextPlayerIsLandlord)
            		score -= 4.0;
    		}

			// 如果地主只剩 2 张，我自由出牌时不要轻易出对子
    		if(shouldAvoidPairForLandlord)
    		{
        		score -= 9.0;
        		// 下家就是地主
        		if(nextPlayerIsLandlord)
            		score -= 4.0;
        		// 对子越小，风险越高
        		score -= (14 - play.comboLevel) * 0.2;

			

    		}

			// 如果我是农民自由出牌，并且地主只剩 3~5 张，
    		// 这时要优先打地主不容易接的牌，避免小牌把牌权交出去。
    		if(freeTurnAgainstWarningLandlord)
    		{
        		int landlordRemain = state.cardRemaining[state.landlordPosition];
        		double warningRate = 6.0 - landlordRemain;
        		// 出牌等级越高，地主越不容易接住，略微加分
        		score += play.comboLevel * 0.08 * warningRate;
        		// 小单张最容易把牌权送出去
        		if(play.comboType == CardComboType::SINGLE && play.comboLevel < 10)
            		score -= 2.0 * warningRate;

        		// 小对子
        		if(play.comboType == CardComboType::PAIR && play.comboLevel < 10)
            		score -= 1.5 * warningRate;

        		// 如果下家就是地主，地主马上可以接牌
        		if(nextPlayerIsLandlord && play.comboLevel < 10)
            		score -= 2.0;

				// 高单、高对、高三张在这种局面下更安全
        		if(play.comboType == CardComboType::SINGLE || play.comboType == CardComboType::PAIR || play.comboType == CardComboType::TRIPLET)
        		{
            		if(play.comboLevel >= 10)
                		score += 1.2 * warningRate;
            		if(play.comboLevel >= 12)
                		score += 1.5 * warningRate;
        		}

        		// 顺子、连对、飞机这类一次走多张的牌，如果能明显减少我的手牌，也可以主动打出去抢节奏
        		if(play.comboType == CardComboType::STRAIGHT ||
           		play.comboType == CardComboType::STRAIGHT2 ||
           		play.comboType == CardComboType::PLANE ||
           		play.comboType == CardComboType::PLANE1 ||
           		play.comboType == CardComboType::PLANE2)
       		 	{
            		score += 1.0 * warningRate;

            		// 一次出得越多越好
            		score += play.cards.size() * 0.15 * warningRate;
      		  	}

    		}


			// 自由出牌时，额外评估这手牌主动打出去是否能整理手牌结构。
			// 例如长顺、连对、飞机应该加分；炸弹、火箭主动打出应该扣分。
			score += evaluateFreeTurn(play);

			// 如果当前是危险自由轮，例如地主或对手快出完了，
			// 额外惩罚主动打掉 2、王、炸弹这类控牌的行为。
			score += evaluateDangerousFreeTurn(state, play);
		}

		// 只剩一手牌
		if (play.comboType != CardComboType::PASS)
		{
    		// 计算出牌后的剩余手牌。
    		vector<Card> handAfter = removeCardsFromHand(state.myCards, play.cards);

			//直接出完，最高奖励
			if(handAfter.empty())
				return score + 100;

			// 如果剩余手牌只需要一手出完，给较高奖励。
    		if (!handAfter.empty() && getMinHandCount(handAfter) == 1)
        	{
				//基础奖励
				score += 5;
				//质量修正
				score += evaluateOneTurnLeftQuality(state, handAfter);
			}

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
		return 100;
	
	//比较手数
	int beforeCount = getMinHandCount(myHandBefore);
	int afterCount = getMinHandCount(myHandAfter);
	bool oneTurnLeftAfter = afterCount == 1;

	//样本收益
	double score = 0;

	//减少出手数，加分
	score += (beforeCount - afterCount) * 5.0;
	//出得多，接近出完
	score += play.cards.size() * 0.3;

	//出完只剩一手，给额外奖励
	if(oneTurnLeftAfter)
	{
		score += 5.0;
		score += evaluateOneTurnLeftQuality(state, myHandAfter);
	}

	//===接入随机补全
	//估计这手牌打出去后，其他玩家是否有能力压过我
	bool opponentCanBeat = false;
	bool teammateCanBeat = false;
	bool dangerousOpponentCanBeat = false;

	for (int player = 0; player < PLAYER_COUNT;player++)
	{
		if(player==state.myPosition)
		continue;

		if(canPlayerBeatInDeal(deal,player,play))
		{
			if(isSameSidePlayer(state,player))
				teammateCanBeat = true;
			else
				{
					opponentCanBeat = true;
					//如果能压过我的对手只剩很少的牌，风险很大
					if(state.cardRemaining[player] <=3 )
						dangerousOpponentCanBeat = true;
				}
		}
	}

	//如果对手能压，这手牌的控场价值下降
	if(opponentCanBeat)
		score -= 4;
	if(dangerousOpponentCanBeat)
		score -= 6;
	//如果队友能压，风险较小
	if(teammateCanBeat)
		score -= 1;
	
	//如果没人能压，说明这手牌在该样本下很可能拿到一轮牌权
	if(!opponentCanBeat && !teammateCanBeat)
		score += 2;


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

// 返回随机补全样本分在最终评分中的权重。
// 这个函数只负责“样本分占多少比例”，不直接评价某一手牌好坏。
// debugTopPlays 和 decidePlay 都调用它，避免调试分数和真实决策使用两套规则。
double getSampleWeight(GameState &state, CardCombo &play)
{
	// 默认跟牌局面：随机补全样本有一定参考价值。
	double sampleWeight = 0.4;

	// 自由出牌主要看启发式整理牌型，样本只做弱参考。
	if(state.lastValidCombo.comboType == CardComboType::PASS)
		sampleWeight = 0.1;

	// 危险自由轮更需要判断这手牌会不会被对手接住。
	if(state.lastValidCombo.comboType == CardComboType::PASS && isDangerousSituation(state))
		sampleWeight = 0.35;

	// 普通局面下，硬控牌的样本分容易虚高。
	// 这种牌是否该出，优先交给启发式资源保护逻辑判断。
	if(isHardControlPlay(play) && !isDangerousSituation(state))
		sampleWeight = 0.0;

	return sampleWeight;
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
		double sampleWeight = getSampleWeight(state, scored.play);

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
		double sampleWeight = getSampleWeight(state, scored.play);

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
