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

bool arePartners(GameState &state, int playerA, int playerB);
bool isHardControlPlay(CardCombo &play);

struct RResult
{
	//本次模拟的分数，正数表示我方更可能赢
	double score=0;
	//模拟是否正常走到有人出完，如果达到步数上限，就用局面估值收尾
	bool finished = false;
	//最终谁出完牌
	int winner = -1;
};
struct RState
{
	//手牌
	vector<Card> hands[PLAYER_COUNT];
	//当前谁出牌
	int currentPlayer = 0;
	//当前要压的牌
	CardCombo lastCombo;
	int lastplayer = -1;
	//模拟了多少步
	int step = 0;
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
double getPassStrength(GameState &state, int player, CardCombo &requiredCombo, int requiredPlayer);

// 记录一次出牌事件，并在对手 PASS 时提取约束。
void recordPlayEvent(GameState &state, int player, vector<Card> &playedCard)
{
	PlayEvent event;
	event.player = player;
	event.cards = playedCard;
	event.requiredCombo = state.lastValidCombo;
	event.requiredPlayer = state.lastValidPlayer;
	event.isPass = playedCard.empty();
	if (event.isPass)
		event.combo = CardCombo();
	else
		event.combo = CardCombo(playedCard.begin(), playedCard.end());

	// 对手 PASS（不是我自己，且当时有需要压的牌）时，记录 PASS 约束供后续推断使用。
	if (player != state.myPosition && event.isPass
		&& event.requiredCombo.comboType != CardComboType::PASS
		&& event.requiredPlayer >= 0)
	{
		PassConstraint constraint;
		constraint.player = player;
		constraint.requirCombo = event.requiredCombo;
		constraint.requirPlayer = event.requiredPlayer;
		constraint.strength = getPassStrength(state, player, event.requiredCombo, event.requiredPlayer);
		state.passConstraints.push_back(constraint);
	}
	state.playEvents.push_back(event);
}

// 根据已恢复的局面重建确定牌区和未知牌区。
void RebuildCard(GameState &state)
{
	// 默认全部未知
	for (Card i = 0; i < 54; ++i)
		state.cardUnknown[i] = true;

	// 清空每个玩家的明牌记录
	for (int i = 0; i < PLAYER_COUNT; ++i)
		state.konwCardOfPlayer[i].clear();

	// 我的手牌是确定的
	for (Card i : state.myCards)
		state.cardUnknown[i] = false;

	// 已打出的牌也是确定的
	for (Card i = 0; i < 54; ++i)
		if (state.cardPlayed[i])
			state.cardUnknown[i] = false;

	// 地主明牌：如果地主不是我，且这张牌还没打出，就是地主的确定持牌
	if (state.landlordPosition >= 0 && state.landlordPosition != state.myPosition)
	{
		for (Card i : state.publicCards)
		{
			if (!state.cardPlayed[i])
			{
				state.konwCardOfPlayer[state.landlordPosition].push_back(i);
				state.cardUnknown[i] = false;
			}
		}
	}
}
 
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


// 根据一次 PASS 事件的上下文，决定它作为“当时压不了牌”的证据有多强。
// 返回值越小，说明这条 PASS 对样本的约束越强；
// 返回值越接近 1，说明这条 PASS 很可能只是战术选择，不适合强约束。
double getPassStrength(GameState &state, int player, CardCombo &requiredCombo, int requiredPlayer)
{
    // 自由出牌时不会出现真正意义上的 PASS 约束，这里直接忽略。
    if(requiredCombo.comboType == CardComboType::PASS || requiredPlayer < 0)
        return 1.0;

    // 如果 PASS 的人和桌面最后有效出牌者是同一阵营，
    // 那么这条 PASS 很可能只是“让队友继续掌牌”，约束应明显减弱。
    if(arePartners(state, player, requiredPlayer))
    {
        // 队友自己已经快跑完了，这种让牌更合理，约束进一步放松。
        if(state.cardRemaining[requiredPlayer] <= 3)
            return 0.98;

        return 0.93;
    }

    // 从这里往下，说明 PASS 的人面对的是对手的牌。
    // 这类 PASS 才更像“压不了”或“压不起”，约束应更强。
    double strength = 0.78;

    // 如果桌面这手本身很强、很高，或者属于复杂带牌，
    // 那么不去压它是更可以理解的，约束不要太狠。
    if(requiredCombo.comboType == CardComboType::BOMB ||
       requiredCombo.comboType == CardComboType::ROCKET)
    {
        strength = 0.90;
    }
    else if(requiredCombo.comboLevel >= 11)
    {
        strength = 0.86;
    }
    else if(requiredCombo.comboType == CardComboType::PLANE1 ||
            requiredCombo.comboType == CardComboType::PLANE2 ||
            requiredCombo.comboType == CardComboType::SSHUTTLE ||
            requiredCombo.comboType == CardComboType::SSHUTTLE2 ||
            requiredCombo.comboType == CardComboType::SSHUTTLE4)
    {
        strength = 0.84;
    }

    // 如果 PASS 的人是地主，那么他面对的是农民的牌。
    // 农民越接近出完，地主还 PASS 就越不合理，因此约束更强。
    if(player == state.landlordPosition)
    {
        int farmerMinCards = 100;
        for(int i = 0; i < PLAYER_COUNT; ++i)
        {
            if(i == state.landlordPosition)
                continue;

            if(state.cardRemaining[i] < farmerMinCards)
                farmerMinCards = state.cardRemaining[i];
        }

        if(farmerMinCards <= 2)
            strength *= 0.55;
        else if(farmerMinCards <= 4)
            strength *= 0.75;

        return strength;
    }

    // 从这里往下，PASS 的人是农民。
    // 如果他面对的是地主的牌，那么地主越接近出完，这条 PASS 就越像“真压不了”。
    if(requiredPlayer == state.landlordPosition)
    {
        int landlordCards = state.cardRemaining[state.landlordPosition];

        if(landlordCards <= 2)
            strength *= 0.35;
        else if(landlordCards <= 4)
            strength *= 0.55;
        else if(landlordCards <= 6)
            strength *= 0.75;

        return strength;
    }

    // 理论上走不到这里；保守返回当前强度。
    return strength;
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

// 评估一条 PASS 约束在当前随机补全样本中的违背程度
double evaluateOnePassConstraint(GameState &state, InferredDeal &deal, PassConstraint &constraint)
{
    if(constraint.player < 0 || constraint.player >= PLAYER_COUNT)
        return 1.0;

    vector<Card> &hand = deal.hands[constraint.player];

    // 分析：如果当时他真的面对 requiredCombo，这手牌能怎么响应。
    ResponseThreatInfo threat = analyzeResponseThreat(hand, constraint.requirCombo);

    // 如果当前样本里他确实完全压不了，那么这个样本和历史 PASS 一致，不扣权重。
    if(!threat.canBeat)
        return 1.0;

    // 基础惩罚：历史上 PASS 了，但当前样本里其实能压，说明这个样本有矛盾
    double factor = constraint.strength;

    // 如果当前样本里他不仅能压，而且压完直接赢，
    // 那么“历史上却选择 PASS”就非常不合理，应该强烈降权
    if(threat.canWinNow)
        factor *= 0.35;

    // 如果压完后只剩一手，虽然不一定当场赢，但已经很危险了，
    // 这种情况下历史 PASS 也明显更不合理
    else if(threat.canLeaveOneHand)
        factor *= 0.6;

    // 如果虽然能压，但压完之后仍然还要很多手
    // 那么 PASS 仍然是相对可以理解的，惩罚不要太狠
    else if(threat.minRemainHands >= 3)
        factor = std::max(factor, 0.85);

    // 如果最优响应本身是硬控牌（炸弹、火箭、四带二等），
    // 那么历史上选择不压是更能理解的，适当放松惩罚
    if(threat.canBeat && isHardControlPlay(threat.bestResponse))
        factor = std::max(factor, 0.8);

    return factor;
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
		// 根据当前样本中这名玩家“到底能怎么压”，
        // 动态决定这一条 PASS 对样本权重的惩罚力度。
        weight *= evaluateOnePassConstraint(state, deal, constraint);

        // 如果权重已经很低，就可以提前结束，避免无意义计算。
        if(weight <= 1e-4)
            return 0;
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

// 判断任意两个玩家是否属于同一阵营。
bool arePartners(GameState &state, int playerA, int playerB)
{
    if(playerA < 0 || playerB < 0)
        return false;

    // 地主只和自己同边；两个农民彼此同边。
    if(playerA == state.landlordPosition || playerB == state.landlordPosition)
        return playerA == playerB;

    return true;
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
			if(state.cardRemaining[i]<=3)
				return true;
		}
		return false;
	}

	//我是农民
	return state.cardRemaining[state.landlordPosition] <= 6;
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
    	// Reference 的思路也是接近收官时再明显提高压制强度，不能太早烧控牌。
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
	//样本权重
	double weight = 0;
	//完成多少次 rollout
	int visits = 0;
};

void logPIMCResult(GameState &state, vector<ScoredPlay> &topPlays);

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
		// 顺子本身仍然是整理牌，但不能奖励过头。
		int straightLength = (int)play.cards.size();

		// 基础奖励略降，避免自由轮无脑起长顺。
		bonus += 1.2;
		bonus += straightLength * 0.08;

		// comboLevel 对顺子表示最高张，越高越容易浪费 Q/K/A 这类控牌。
		bonus -= play.comboLevel * 0.10;

		// 过长的顺子通常会一次性交掉太多灵活性，额外扣分。
		if(straightLength >= 7)
			bonus -= 0.6 + (straightLength - 7) * 0.25;

		// 高位长顺再额外保守一点，重点抑制 7~K、8~A 这类起手。
		if(play.comboLevel >= 9)
			bonus -= 0.8 + (play.comboLevel - 9) * 0.35;
	}
	//连对
		else if(play.comboType==CardComboType::STRAIGHT2)
	{
		int pairStraightLength = (int)play.cards.size() / 2;

		// 连对仍有整理价值，但不应在自由轮被过度偏爱。
		bonus += 1.3;
		bonus += play.cards.size() * 0.07;

		// 高位连对会消耗对子控牌，扣分比原来更明显。
		bonus -= play.comboLevel * 0.08;

		// 过长连对会显著降低后续灵活性，额外保守一点。
		if(pairStraightLength >= 4)
			bonus -= 0.4 + (pairStraightLength - 4) * 0.20;

		// 高位长连对进一步扣分，抑制 88~JJ、99~QQ 这类过早甩掉。
		if(play.comboLevel >= 8)
			bonus -= 0.5 + (play.comboLevel - 8) * 0.25;
	}

	//飞机价值很高
	else if(play.comboType==CardComboType::PLANE || play.comboType==CardComboType::PLANE1 || play.comboType==CardComboType::PLANE2)
	{
		bonus += 1.6;
		bonus += play.cards.size() * 0.07;

		//高飞机也会浪费资源
		bonus -= play.comboLevel * 0.08;
	}
	//四带二
		else if (play.comboType == CardComboType::QUADRUPLE2 || play.comboType == CardComboType::QUADRUPLE4)
	{
		int wingCardCount = (int)play.cards.size() - 4;

		// 四带类不是普通整理牌，自由轮主动打出要明显保守。
		bonus += 0.4;
		bonus += play.cards.size() * 0.03;

		// 主四条越高，炸弹和控场价值越强。
		bonus -= play.comboLevel * 0.12;

		// 带牌越多，后续灵活性损失越大。
		bonus -= wingCardCount * 0.12;

		// AAAA、2222 基本不该在普通自由轮主动当整理牌交掉。
		if(play.comboLevel >= 11)
			bonus -= 2.0 + (play.comboLevel - 11) * 1.2;
	}


	//三条带散
	else if(play.comboType==CardComboType::TRIPLET1 || play.comboType==CardComboType::TRIPLET2)
	{
		Level main = play.comboLevel;
		int wingCard = 0;
		int highWing = 0;

		// 三带是整理牌，但奖励要比顺子、飞机更克制。
		bonus += 0.9;
		bonus += play.cards.size() * 0.04;

		// 主三条越高，控牌价值越强，自由轮越不该轻易打掉。
		bonus -= main * 0.09;

		// 只统计“带牌”部分，不把主三条重复误罚进去。
		for(Card card : play.cards)
		{
			Level level = card2level(card);
			if(level == main)
				continue;

			++wingCard;

			// 带出去的是高牌时，明显更亏。
			if(level >= 11)
			{
				++highWing;
				bonus -= 0.9;
			}
			else if(level >= 9)
			{
				bonus -= 0.35;
			}
		}
		if(play.comboType == CardComboType::TRIPLET2 && highWing >= 2)
			bonus -= 0.8;
		if(main >= 11)
			bonus -= 1.0;
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
		if(play.comboLevel <= 4)              
			bonus += 0.9;
		else if(play.comboLevel <= 7)        
			bonus += 0.3;
		else                                   
			bonus -= 0.4;

		if(play.comboLevel >= 8)              
			bonus -= 0.35 * (play.comboLevel - 7);

		if(play.comboLevel == 12)
			bonus -= 1.2;
	}
	else if (play.comboType == CardComboType::SINGLE)
	{
		if(play.comboLevel <= 4)          
			bonus += 0.5;
		else if(play.comboLevel <= 7)       
			bonus += 0.1;
		else                               
			bonus -= 0.3;

		if(play.comboLevel >= 8)            
			bonus -= 0.40 * (play.comboLevel - 7);

		if(play.comboLevel >= 12)
			bonus -= 1.0 + (play.comboLevel - 12) * 0.8;
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

	// 三带一 / 三带二如果把 A 或 2 当主体打出去，
	// 往往是在提前拆掉最重要的中后期控牌资源。
	if(play.comboType == CardComboType::TRIPLET1 || play.comboType == CardComboType::TRIPLET2)
	{
		Level mainLevel = play.comboLevel;

		// 222 带牌最亏，除非已经是危险跟牌
		if(mainLevel == 12 && !followingDangerousOpponent)
		{
			if(freeTurn)
				penalty += 5.0;
			else
				penalty += 3.0;
		}

		// AAA 带牌也要明显保守一些
		if(mainLevel == 11 && !followingDangerousOpponent)
		{
			if(freeTurn)
				penalty += 2.5;
			else
				penalty += 1.5;
		}

		// 四带二 / 四带两对会一次性交掉大量资源。
		if(play.comboType == CardComboType::QUADRUPLE2 || play.comboType == CardComboType::QUADRUPLE4)
		{
			Level mainLevel = play.comboLevel;
			int highWingCount = 0;

			// 2222 最不该轻易主动当整理牌交掉。
			if(mainLevel == 12 && !followingDangerousOpponent)
			{
				if(freeTurn)
					penalty += 6.0;
				else
					penalty += 3.5;
			}
			// AAAA 也应明显保留。
			else if(mainLevel == 11 && !followingDangerousOpponent)
			{
				if(freeTurn)
					penalty += 3.0;
				else
					penalty += 1.8;
			}

			// 只看带牌部分，不重复统计主四条。
			for(Card card : play.cards)
			{
				Level level = card2level(card);
				if(level == mainLevel)
					continue;

				if(level >= 11)
				{
					++highWingCount;
					penalty += 0.9;
				}
				else if(level >= 9)
				{
					penalty += 0.35;
				}
			}

			// 四带两对如果还顺手带走高对子，再额外扣一点。
			if(play.comboType == CardComboType::QUADRUPLE4 && highWingCount >= 2)
				penalty += 1.0;
		}

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

// 根据一个随机补全样本，构造我先打出 firstPlay 之后的模拟局面,这个函数只负责搭建初始状态，不负责继续模拟后续出牌
RState buildInitRState(GameState &state,InferredDeal &deal,CardCombo &firstPlay)
{
	RState rollout;

	//把随机补全的三家手牌复制进模拟局面
	for (int i = 0; i < PLAYER_COUNT;++i)
		rollout.hands[i] = deal.hands[i];

	rollout.hands[state.myPosition] = removeCardsFromHand(rollout.hands[state.myPosition], firstPlay.cards);

	rollout.currentPlayer = (state.myPosition + 1) % PLAYER_COUNT;
	if(firstPlay.comboType!=CardComboType::PASS)
	{
		rollout.lastCombo = firstPlay;
		rollout.lastplayer = state.myPosition;
	}
	else
	{
		rollout.lastCombo = state.lastValidCombo;
		rollout.lastplayer = state.lastValidPlayer;
	}

	rollout.step = 0;

	return rollout;
}

// 检查模拟局面里是否已经有人出完牌
int getRWinner(RState &rollout)
{
	for (int i = 0; i < PLAYER_COUNT;++i)
	{
		if(rollout.hands[i].empty())
			return i;
	}
	return -1;
}

// 给一次已经结束的模拟局面打分
double evaluateRTerminal(GameState &state,RState &rollout,int winner)
{
	if(winner<0)
		return 0;

	if(state.isLandlord())
	{
		if(winner==state.myPosition)
			return 24;
		return -24;
	}
	if(winner!=state.landlordPosition)
	{
		if(winner==state.myPosition)
			return 24;
		return 16;
	}
	return -24;
}

//当模拟没有走到终局时，用当前局面做一个近似评分
double evaluateRNonTerminal(GameState &state, RState &rollout)
{
    double score = 0;

    int myCardsLeft = rollout.hands[state.myPosition].size();
    int myHandsLeft = getMinHandCount(rollout.hands[state.myPosition]);

    //我剩得越少越好
    score += (20 - myCardsLeft) * 0.35;
    score += (10 - myHandsLeft) * 0.9;

    //看敌方
    for(int i = 0; i < PLAYER_COUNT; i++)
    {
        if(i == state.myPosition)
            continue;

        int cardsLeft = rollout.hands[i].size();
        int handsLeft = getMinHandCount(rollout.hands[i]);

        //对手越接近出完，对我越不利。
        if(!isSameSidePlayer(state, i))
        {
            score -= (20 - cardsLeft) * 0.30;
            score -= (10 - handsLeft) * 0.80;

            // 对手只剩一手，这是强危险信号
            if(handsLeft == 1)
                score -= 5.0;
            if(cardsLeft <= 2)
                score -= 3.0;
        }
        // 队友接近出完，对我方有利，但弱于自己出完
        else
        {
            score += (20 - cardsLeft) * 0.20;
            score += (10 - handsLeft) * 0.55;

            if(handsLeft == 1)
                score += 3.0;
        }
    }

    return score;
}

// 在模拟局面中，为某个玩家选择一手轻量出牌
// 这里用简化规则：能赢先赢；跟牌选较小的合理牌；自由出牌优先减少手数
CardCombo selectRPlay(GameState &state, RState &rollout, int player)
{
    vector<Card> &hand = rollout.hands[player];
    vector<CardCombo> validPlays = enumAllValidPlays(hand, rollout.lastCombo);

    if(validPlays.empty())
        return CardCombo();

    for(CardCombo &play : validPlays)
    {
        if(play.comboType != CardComboType::PASS &&
           play.comboType != CardComboType::INVALID &&
           play.cards.size() == hand.size())
        {
            return play;
        }
    }

    bool freeTurn = rollout.lastCombo.comboType == CardComboType::PASS;
    bool followingTeammate = !freeTurn && arePartners(state, player, rollout.lastplayer);
    bool followingOpponent = !freeTurn && rollout.lastplayer >= 0 && !followingTeammate;
    int nextPlayer = (player + 1) % PLAYER_COUNT;
    int beforeHands = getMinHandCount(hand);

    int enemyMinCards = 100;
    int enemyMinHands = 100;
    for(int other = 0; other < PLAYER_COUNT; ++other)
    {
        if(other == player)
            continue;
        if(arePartners(state, player, other))
            continue;

        int cardsLeft = static_cast<int>(rollout.hands[other].size());
        int handsLeft = getMinHandCount(rollout.hands[other]);
        if(cardsLeft < enemyMinCards)
            enemyMinCards = cardsLeft;
        if(handsLeft < enemyMinHands)
            enemyMinHands = handsLeft;
    }

    bool enemyDanger = enemyMinCards <= 3 || enemyMinHands <= 1;
    bool enemyVeryDanger = enemyMinCards <= 2 || enemyMinHands <= 1;

    // 农民跟农民时，默认继续让队友处理；只有在对手危险、或对手明显能接住队友这手牌时才考虑接管。
    ResponseThreatInfo teammateThreat;
    bool teammateThreatReady = false;
    if(followingTeammate && !arePartners(state, player, nextPlayer))
    {
        teammateThreat = analyzeResponseThreat(rollout.hands[nextPlayer], rollout.lastCombo);
        teammateThreatReady = true;

        if(!teammateThreat.canBeat)
            return CardCombo();

        if(!enemyDanger && !teammateThreat.canLeaveOneHand)
            return CardCombo();
    }

    CardCombo bestPlay;
    double bestScore = -1e18;

    for(CardCombo &play : validPlays)
    {
        if(freeTurn && play.comboType == CardComboType::PASS)
            continue;

        if(play.comboType == CardComboType::INVALID)
            continue;

        double score = 0;

        // PASS 只在跟牌局面出现。
        // 跟对手时，普通局面适合保留资源；残局危险时，PASS 要明显扣分。
        // 跟队友时，PASS 默认是好事，除非对手已经能接住并接近跑完。
        if(play.comboType == CardComboType::PASS)
        {
            if(followingTeammate)
            {
                score += 8.0;
                if(teammateThreatReady)
                {
                    if(teammateThreat.canLeaveOneHand)
                        score -= 8.0;
                    if(teammateThreat.canWinNow)
                        score -= 20.0;
                }
                if(enemyDanger)
                    score -= 6.0;
                if(enemyVeryDanger)
                    score -= 8.0;
            }
            else if(followingOpponent)
            {
                score += 1.5;
                if(enemyDanger)
                    score -= 12.0;
                if(enemyVeryDanger)
                    score -= 6.0;
            }
        }
        else
        {
            vector<Card> handAfter = removeCardsFromHand(hand, play.cards);
            int afterHands = handAfter.empty() ? 0 : getMinHandCount(handAfter);

            // rollout 里最核心的目标仍然是尽快减少手数、接近出完。
            score += (beforeHands - afterHands) * 8.0;
            score += play.cards.size() * 0.5;

            if(afterHands == 1)
                score += 10.0;

            // 自由出牌优先整理结构，轻度保护高牌。
            if(freeTurn)
            {
                if(play.comboType == CardComboType::STRAIGHT)
                    score += 1.8 + play.cards.size() * 0.12;
                else if(play.comboType == CardComboType::STRAIGHT2)
                    score += 1.4 + play.cards.size() * 0.10;
                else if(play.comboType == CardComboType::PLANE ||
                        play.comboType == CardComboType::PLANE1 ||
                        play.comboType == CardComboType::PLANE2)
                    score += 1.4 + play.cards.size() * 0.08;
                else if(play.comboType == CardComboType::TRIPLET1 ||
                        play.comboType == CardComboType::TRIPLET2)
                    score += 0.8;
                else if(play.comboType == CardComboType::SINGLE)
                    score -= 0.5;

                score -= play.comboLevel * 0.12;
            }
            else if(followingTeammate)
            {
                // 接管队友牌权本身有成本，只有危险时才应该这么做。
                score -= 5.0;
                score -= play.comboLevel * (enemyDanger ? 0.28 : 0.55);

                if(teammateThreatReady)
                {
                    if(teammateThreat.canLeaveOneHand)
                        score += 6.0;
                    if(teammateThreat.canWinNow)
                        score += 18.0;
                }
            }
            else
            {
                // 跟对手时，普通局面优先小压；残局危险时提高压牌意愿。
                score -= play.comboLevel * (enemyDanger ? 0.15 : 0.35);
                if(enemyDanger)
                    score += 6.0;
                if(enemyVeryDanger)
                    score += 6.0;
            }

            // 2、王、炸弹这类硬控资源仍然要保护，只有危险局面才放松惩罚。
            if(isHardControlPlay(play))
            {
                if(followingOpponent && enemyDanger)
                    score -= 2.0;
                else if(followingTeammate && teammateThreatReady && teammateThreat.canWinNow)
                    score -= 4.0;
                else if(freeTurn)
                    score -= 10.0;
                else if(followingTeammate)
                    score -= 12.0;
                else
                    score -= 8.0;
            }

            if(play.comboLevel >= 12)
            {
                if(freeTurn)
                    score -= 1.5;
                else if(followingTeammate)
                    score -= 2.5;
                else if(!enemyDanger)
                    score -= 2.0;
            }

            // 如果下一位是对手，这手牌能否保住牌权很关键。
            if(!arePartners(state, player, nextPlayer))
            {
                bool nextCanBeat = canBeatComboFast(rollout.hands[nextPlayer], play);
                if(!nextCanBeat)
                    score += enemyDanger ? 8.0 : 3.0;
                else if(enemyDanger)
                    score -= 4.0;
            }
            else if(!freeTurn)
            {
                score += 1.0;
            }
        }

        if(score > bestScore)
        {
            bestScore = score;
            bestPlay = play;
        }
    }

    return bestPlay;
}

// 在模拟局面里执行某个玩家的一手牌，并更新桌面状态;返回值表示这一步之后是否有人已经出完
bool applyRPlay(GameState &state, RState &rollout, int player, CardCombo &play)
{
    if(play.comboType != CardComboType::PASS && play.comboType != CardComboType::INVALID)
    {
        rollout.hands[player] = removeCardsFromHand(rollout.hands[player], play.cards);

        rollout.lastCombo = play;
        rollout.lastplayer = player;
    }
    else
    {
        // 如果 PASS 后，轮回到最后有效出牌者，说明这一轮结束
        // 下一位变成自由出牌，需要清空 lastCombo
        if(rollout.lastplayer >= 0 && (player + 1) % PLAYER_COUNT == rollout.lastplayer)
        {
            rollout.lastCombo = CardCombo();
            rollout.lastplayer = -1;
        }
    }

    // 这一步已经执行完，步数加一
    rollout.step++;

    rollout.currentPlayer = (player + 1) % PLAYER_COUNT;

    // 如果有人出完，返回 true
    return getRWinner(rollout) >= 0;
}

// 在一个随机补全样本中，模拟我打出 firstPlay 之后的后续对局。
// 这是轻量 PIMC 的核心：它不是标准 MCTS，而是随机补全后的启发式 rollout。
RResult simulateDealAfterPlay(GameState &state, InferredDeal &deal, CardCombo &firstPlay, clock_t deadline)
{
    RResult result;

    // 构造“我已经打出 firstPlay 之后”的模拟局面。
    RState rollout = buildInitRState(state, deal, firstPlay);

    // 如果 firstPlay 已经让我出完，直接终局。
    int winner = getRWinner(rollout);
    if(winner >= 0)
    {
        result.finished = true;
        result.winner = winner;
        result.score = evaluateRTerminal(state, rollout, winner);
        return result;
    }

	// 这里保留一个很大的步数上限，如果模拟逻辑哪里出了问题，至少不会无限循环
    int maxSteps = 200;

    while(rollout.step < maxSteps)
    {
		//如果到达搜索时间，就截止
		if(clock()>=deadline)
		{
			result.finished = false;
			result.winner = -1;
			result.score = evaluateRNonTerminal(state, rollout);
			return result;
		}
        // 当前行动玩家。
        int player = rollout.currentPlayer;

        // 选择这个玩家在模拟中的出牌。
        CardCombo play = selectRPlay(state, rollout, player);

        // 执行这手牌。
        bool ended = applyRPlay(state, rollout, player, play);

        // 如果有人出完，返回终局分。
        if(ended)
        {
            winner = getRWinner(rollout);
            result.finished = true;
            result.winner = winner;
            result.score = evaluateRTerminal(state, rollout, winner);
            return result;
        }
    }

    // 没有模拟到终局，用局面估值收尾。
    result.finished = false;
    result.winner = -1;
    result.score = evaluateRNonTerminal(state, rollout);
    return result;
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

	// 记录下一个出牌的人
	int nextPlayer = (state.myPosition + 1) % PLAYER_COUNT;

	// 判断下家是不是我的队友
	bool nextPlayerIsTeammate = isSameSidePlayer(state, nextPlayer);

	// 判断是否适合送队友报单
	// 只有自由出牌时才考虑，因为跟牌阶段不能随便选择牌型
	// 队友只剩 1 张时，出单张最可能让队友直接走完
	bool shouldFeedTeammateSingle = freeTurn && nextPlayerIsTeammate && state.cardRemaining[nextPlayer] == 1;

	// 判断地主是否只剩 1 张
	bool landlordOnlyOneLeft = !state.isLandlord() &&  state.cardRemaining[state.landlordPosition] == 1;

	// 判断下一个出牌的人是不是地主。
	bool nextPlayerIsLandlord = nextPlayer == state.landlordPosition;

	// 判断自由出牌时是否不能出单张。
	// 如果地主只剩 1 张，或者下家就是地主，出单张风险很高
	bool shouldAvoidSingleForLandlord = freeTurn && !state.isLandlord() && landlordOnlyOneLeft && play.comboType == CardComboType::SINGLE;

	// 判断地主是否只剩 2 张
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
				double teammateHardControl = 12;
				if(fightForControl)
					teammateHardControl *= 0.6;
				if(dangerous)
					teammateHardControl *= 0.35;

				score -= teammateHardControl;
			}

			//用 2 或王压队友也比较亏，普通局面要扣分
			
			//如果在争夺牌权的情况下，用2或王需要分开处理
			double teammateHighCardRate = 1.4;

			//在抢牌权时
			if(fightForControl)
				teammateHighCardRate = 0.8;
			//危险局面下
			if(dangerous)
				teammateHighCardRate = 0.5;
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
        			score += 3.0;
        			// 队友牌本身越大，越可能卡住地主，PASS 更合理。
        			if(state.lastValidCombo.comboLevel >= 9)
            			score += 3.0;
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
				double pass = 0;

				// 地主进入收官区时，农民 PASS 的代价要明显上升
				// 但不能再用“预警扣分 + 争牌权扣分”两层硬叠，不然会把策略推向另一个极端
				if(followingLandlord && !followingDangerousOpponent)
				{
					int llRemain = state.cardRemaining[state.landlordPosition];

					if(llRemain == 6)
						pass += myIsLandlordPrev ? 2.2 : 1.4;
					else if(llRemain == 5)
						pass += myIsLandlordPrev ? 3.2 : 2.4;
					else if(llRemain == 4)
						pass += myIsLandlordPrev ? 4.4 : 3.4;
					else if(llRemain == 3)
						pass += myIsLandlordPrev ? 5.6 : 4.4;
				}
				// 需要争牌权时，PASS 仍然要扣，但幅度更平滑。
				if(fightForControl)
				{
					if(followingLandlord)
						pass += 2.2;
					else
						pass += 3.0;
				}
				else
				{
					if(followingLandlord)
						pass += 1.0;
					else
						pass += 1.6;
				}

				score -= pass;
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

			// 农民压地主但地主还没进入收官危险时，不要轻易用 2 或王抢普通牌权。
			// 复盘里典型错误是地主还剩很多张，我有 QQ 却直接打 22 压地主的 JJ。
			if(!state.isLandlord() && state.lastValidPlayer == state.landlordPosition &&
			   state.cardRemaining[state.landlordPosition] > 5)
			{
				if(rate < 5.0)
					rate = 5.0;
			}

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
		if(followingOpponent && play.comboType != CardComboType::PASS)
		{
			// 能压住对手，本身有一点正收益
			score += 0.8;

			// 当前如果本来就需要争牌权，压制收益再提高
			if(fightForControl)
				score += 2.0;

			// 我这手比对面高出去多少
			int overLevel = play.comboLevel - state.lastValidCombo.comboLevel;
			if(overLevel < 0)
				overLevel = 0;

			// 小牌跟牌局面里，单张/对子/三条更适合做“刚好压住”的控制
			bool simpleFollow = play.comboType == CardComboType::SINGLE || play.comboType == CardComboType::PAIR || play.comboType == CardComboType::TRIPLET;

			// 农民压地主：既要拦住，也不能把地主的小牌一路抬高
			if(followingLandlord)
			{
				int landlordRemain = state.cardRemaining[state.landlordPosition];

				// 地主进入收官区后，成功拦住本身更有价值
				if(landlordRemain <= 6 && !followingDangerousOpponent)
					score += 1.5 + (6 - landlordRemain) * 0.8;

				// 地主上家是最后一道闸，拦地主责任更重
				if(myIsLandlordPrev)
				{
					score += 2.5;
					if(landlordRemain <= 5)
						score += (6 - landlordRemain) * 0.8;
				}

				// 但地主还没到强危险区时，
				// 对小单/小对/三条要尽量“刚好压住”，别帮地主抬牌
								if(simpleFollow && !followingDangerousOpponent)
				{
					double overCostRate = 0.0;

					if(state.lastValidCombo.comboType == CardComboType::SINGLE ||
					   state.lastValidCombo.comboType == CardComboType::PAIR)
					{
						if(landlordRemain > 6)
							overCostRate = fightForControl ? 0.50 : 0.80;
						else if(landlordRemain > 4)
							overCostRate = fightForControl ? 0.38 : 0.60;
						else
							overCostRate = fightForControl ? 0.20 : 0.32;
					}
					else
					{
						overCostRate = fightForControl ? 0.12 : 0.18;
					}

					if(myIsLandlordPrev)
						overCostRate *= 0.75;

					score -= overLevel * overCostRate;

					// 地主出的是明显小牌时，不要用 A/2 这种高牌去抬
					if((state.lastValidCombo.comboType == CardComboType::SINGLE ||
					    state.lastValidCombo.comboType == CardComboType::PAIR) &&
					   state.lastValidCombo.comboLevel <= 8 &&
					   landlordRemain > 2)
					{
						if(play.comboLevel >= 11)
							score -= fightForControl ? 1.8 : 3.0;
						else if(play.comboLevel >= 9)
							score -= fightForControl ? 0.8 : 1.5;
					}
				}

			}
			else if(simpleFollow)
			{
				// 普通压制也优先用刚好能压住的低牌
				double overCostRate = fightForControl ? 0.08 : 0.12;
				score -= overLevel * overCostRate;
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
	bool freeTurn = state.lastValidCombo.comboType == CardComboType::PASS;
	bool dangerous = isDangerousSituation(state);

	// 基础权重：
	// 跟牌局面更依赖“这手压完之后会发生什么”，所以基础权重高于自由轮。
	double sampleWeight = freeTurn ? 0.12 : 0.24;

	// 隐藏牌越少，随机补全越可信，PIMC 应该有更大发言权。
	int hiddenCards = 0;
	for(int i = 0; i < PLAYER_COUNT; ++i)
	{
		if(i == state.myPosition)
			continue;
		hiddenCards += state.cardRemaining[i];
	}

	if(hiddenCards <= 8)
		sampleWeight += 0.12;
	else if(hiddenCards <= 12)
		sampleWeight += 0.08;
	else if(hiddenCards <= 18)
		sampleWeight += 0.04;
	else if(hiddenCards <= 24)
		sampleWeight += 0.02;

	// 危险局面下，需要更重视“这一手会不会把主动权交回去”。
	if(dangerous)
	{
		if(freeTurn)
			sampleWeight += 0.08;
		else
			sampleWeight += 0.10;
	}

	// 危险局面下，PASS 是否会被对手顺走牌权尤其关键。
	if(play.comboType == CardComboType::PASS && dangerous)
		sampleWeight += 0.04;

	// 硬控牌在普通局面容易被 rollout 虚高，所以只保留一部分样本影响力，
	// 但不再直接压成 0，避免搜索完全失去纠错能力。
	if(isHardControlPlay(play))
	{
		if(!dangerous)
			sampleWeight *= 0.3;
		else
			sampleWeight *= 0.75;
	}

	if(sampleWeight > 0.42)
		sampleWeight = 0.42;

	return sampleWeight;
}

//从所有合法出牌中选出启发式评分最高的前 topK个候选
vector<ScoredPlay> selectTopPlays(GameState &state,vector<CardCombo> &validPlays,int topK)
{
	struct TopPlayCandidate
	{
		ScoredPlay scored;
		int comboWeight = 0;
		bool hardControl = false;
		long long familyKey = 0;
		int afterHands = -1;
		int remainCards = -1;
		bool oneTurnLeft = false;
	};

	vector<TopPlayCandidate> candidates;
	bool freeTurn = state.lastValidCombo.comboType == CardComboType::PASS;
	int beforeHands = state.myCards.empty() ? 0 : getMinHandCount(state.myCards);

	auto buildFamilyKey = [](CardCombo &play) -> long long
	{
		long long key = static_cast<int>(play.comboType);
		key = key * 32 + play.comboLevel;
		key = key * 32 + static_cast<int>(play.cards.size());
		return key;
	};

	for(CardCombo &play : validPlays)
	{
		if(freeTurn && play.comboType == CardComboType::PASS)
			continue;

		TopPlayCandidate candidate;
		candidate.scored.play = play;
		candidate.scored.score = evaluatePlayHScore(state, play);
		candidate.comboWeight = play.getWeight();
		candidate.hardControl = isHardControlPlay(play);
		candidate.familyKey = buildFamilyKey(play);
		candidates.push_back(candidate);
	}

	if(candidates.empty())
		return vector<ScoredPlay>();

	auto heuristicBetter = [](TopPlayCandidate &a, TopPlayCandidate &b) -> bool
	{
		if(a.scored.score != b.scored.score)
			return a.scored.score > b.scored.score;
		if(a.hardControl != b.hardControl)
			return !a.hardControl;
		if(a.comboWeight != b.comboWeight)
			return a.comboWeight < b.comboWeight;
		if(a.scored.play.cards.size() != b.scored.play.cards.size())
			return a.scored.play.cards.size() < b.scored.play.cards.size();
		if(a.scored.play.comboLevel != b.scored.play.comboLevel)
			return a.scored.play.comboLevel < b.scored.play.comboLevel;
		return a.scored.play.cards < b.scored.play.cards;
	};

	std::sort(candidates.begin(), candidates.end(), heuristicBetter);

	auto ensureAfterInfo = [&](TopPlayCandidate &candidate)
	{
		if(candidate.afterHands >= 0)
			return;

		if(candidate.scored.play.comboType == CardComboType::PASS)
		{
			candidate.afterHands = beforeHands;
			candidate.remainCards = static_cast<int>(state.myCards.size());
			candidate.oneTurnLeft = candidate.afterHands == 1;
			return;
		}

		vector<Card> handAfter = removeCardsFromHand(state.myCards, candidate.scored.play.cards);
		candidate.remainCards = static_cast<int>(handAfter.size());
		candidate.afterHands = handAfter.empty() ? 0 : getMinHandCount(handAfter);
		candidate.oneTurnLeft = candidate.afterHands == 1;
	};

	vector<ScoredPlay> result;
	vector<int> used(candidates.size(), 0);
	std::unordered_map<long long, int> familyCount;
	vector<int> comboTypeCount(static_cast<int>(CardComboType::INVALID) + 1, 0);

	auto addCandidate = [&](int index)
	{
		if(index < 0 || index >= static_cast<int>(candidates.size()))
			return;
		if(used[index])
			return;
		if(topK > 0 && static_cast<int>(result.size()) >= topK)
			return;

		used[index] = 1;
		familyCount[candidates[index].familyKey]++;
		comboTypeCount[static_cast<int>(candidates[index].scored.play.comboType)]++;
		result.push_back(candidates[index].scored);
	};

	auto findBestIndex = [&](auto better, bool requireNewFamily, bool requireNewType, bool skipPass, bool requireHardControl, bool requireOneTurnLeft) -> int
	{
		int bestIndex = -1;

		for(int i = 0; i < static_cast<int>(candidates.size()); ++i)
		{
			if(used[i])
				continue;

			CardCombo &play = candidates[i].scored.play;

			if(skipPass && play.comboType == CardComboType::PASS)
				continue;
			if(requireNewFamily && familyCount[candidates[i].familyKey] > 0)
				continue;
			if(requireNewType && comboTypeCount[static_cast<int>(play.comboType)] > 0)
				continue;
			if(requireHardControl && !candidates[i].hardControl)
				continue;

			ensureAfterInfo(candidates[i]);
			if(requireOneTurnLeft && !candidates[i].oneTurnLeft)
				continue;

			if(bestIndex < 0 || better(candidates[i], candidates[bestIndex]))
				bestIndex = i;
		}

		return bestIndex;
	};

	auto structureBetter = [&](TopPlayCandidate &a, TopPlayCandidate &b) -> bool
	{
		ensureAfterInfo(a);
		ensureAfterInfo(b);
		if(a.afterHands != b.afterHands)
			return a.afterHands < b.afterHands;
		if(a.oneTurnLeft != b.oneTurnLeft)
			return a.oneTurnLeft;
		if(a.hardControl != b.hardControl)
			return !a.hardControl;
		if(a.remainCards != b.remainCards)
			return a.remainCards < b.remainCards;
		return heuristicBetter(a, b);
	};

	auto cheapBeatBetter = [&](TopPlayCandidate &a, TopPlayCandidate &b) -> bool
	{
		ensureAfterInfo(a);
		ensureAfterInfo(b);
		if(a.hardControl != b.hardControl)
			return !a.hardControl;
		if(a.comboWeight != b.comboWeight)
			return a.comboWeight < b.comboWeight;
		if(a.scored.play.comboLevel != b.scored.play.comboLevel)
			return a.scored.play.comboLevel < b.scored.play.comboLevel;
		if(a.scored.play.cards.size() != b.scored.play.cards.size())
			return a.scored.play.cards.size() < b.scored.play.cards.size();
		if(a.afterHands != b.afterHands)
			return a.afterHands < b.afterHands;
		return a.scored.score > b.scored.score;
	};

	auto strongBeatBetter = [&](TopPlayCandidate &a, TopPlayCandidate &b) -> bool
	{
		ensureAfterInfo(a);
		ensureAfterInfo(b);
		if(a.oneTurnLeft != b.oneTurnLeft)
			return a.oneTurnLeft;
		if(a.hardControl != b.hardControl)
			return a.hardControl;
		if(a.comboWeight != b.comboWeight)
			return a.comboWeight > b.comboWeight;
		if(a.scored.play.comboLevel != b.scored.play.comboLevel)
			return a.scored.play.comboLevel > b.scored.play.comboLevel;
		if(a.remainCards != b.remainCards)
			return a.remainCards < b.remainCards;
		return a.scored.score > b.scored.score;
	};

	auto lightOpenerBetter = [&](TopPlayCandidate &a, TopPlayCandidate &b) -> bool
	{
		ensureAfterInfo(a);
		ensureAfterInfo(b);
		if(a.hardControl != b.hardControl)
			return !a.hardControl;
		if(a.comboWeight != b.comboWeight)
			return a.comboWeight < b.comboWeight;
		if(a.scored.play.cards.size() != b.scored.play.cards.size())
			return a.scored.play.cards.size() < b.scored.play.cards.size();
		if(a.scored.play.comboLevel != b.scored.play.comboLevel)
			return a.scored.play.comboLevel < b.scored.play.comboLevel;
		if(a.afterHands != b.afterHands)
			return a.afterHands < b.afterHands;
		return a.scored.score > b.scored.score;
	};

	// 1. 先保留启发式第一名，确保当前主策略不会被完全推翻。
	addCandidate(0);

	// 2. 跟牌局面下，PASS 是一个独立策略分支，必须显式保留给 PIMC 对比。
	if(!freeTurn)
	{
		int passIndex = findBestIndex(heuristicBetter, true, false, false, false, false);
		if(passIndex >= 0 && candidates[passIndex].scored.play.comboType == CardComboType::PASS)
			addCandidate(passIndex);
		else
		{
			for(int i = 0; i < static_cast<int>(candidates.size()); ++i)
			{
				if(candidates[i].scored.play.comboType == CardComboType::PASS)
				{
					addCandidate(i);
					break;
				}
			}
		}
	}

	// 3. 能把手牌压到“一手收完”的候选必须单独保留。
	addCandidate(findBestIndex(structureBetter, true, false, true, false, true));

	// 4. 保留一个最强调整结构的分支。
	addCandidate(findBestIndex(structureBetter, true, false, true, false, false));

	// 5. 自由轮保留一个轻量起手；跟牌轮保留一个最省资源的应手。
	if(freeTurn)
		addCandidate(findBestIndex(lightOpenerBetter, true, false, true, false, false));
	else
		addCandidate(findBestIndex(cheapBeatBetter, true, false, true, false, false));

	// 6. 保留一个强压/抢控分支，避免候选全是保守打法。
	addCandidate(findBestIndex(strongBeatBetter, true, false, true, false, false));

	// 7. 单独保留一个硬控牌分支，让 PIMC 有机会判断是否值得提前交资源。
	addCandidate(findBestIndex(strongBeatBetter, true, false, true, true, false));

	// 8. 先补没有出现过的牌型，扩大策略面。
	for(int i = 0; i < static_cast<int>(candidates.size()); ++i)
	{
		if(topK > 0 && static_cast<int>(result.size()) >= topK)
			break;
		if(used[i])
			continue;
		if(comboTypeCount[static_cast<int>(candidates[i].scored.play.comboType)] > 0)
			continue;
		addCandidate(i);
	}

	// 9. 再补同家族未出现过的高分候选，避免 TopK 被同一种带牌写满。
	for(int i = 0; i < static_cast<int>(candidates.size()); ++i)
	{
		if(topK > 0 && static_cast<int>(result.size()) >= topK)
			break;
		if(used[i])
			continue;
		if(familyCount[candidates[i].familyKey] > 0)
			continue;
		addCandidate(i);
	}

	// 10. 如果还不够，再允许少量同家族备选，兼顾多样性和覆盖率。
	for(int i = 0; i < static_cast<int>(candidates.size()); ++i)
	{
		if(topK > 0 && static_cast<int>(result.size()) >= topK)
			break;
		if(used[i])
			continue;
		if(familyCount[candidates[i].familyKey] >= 2)
			continue;
		addCandidate(i);
	}

	return result;
}

// 调试用：输出 TopK 候选的启发式分、样本分和最终融合分。
// 注意：这个函数只写 cerr，不应该在 Botzone 正式输出前默认调用。
void debugTopPlays(GameState &state,vector<CardCombo> &validPlays)
{
	vector<ScoredPlay> topPlays = selectTopPlays(state, validPlays, 6);
	int timeLimit = 400;
	clock_t searchTime = clock() + timeLimit * CLOCKS_PER_SEC / 1000;
	int round = 0;

	while(clock() < searchTime)
	{
		vector<InferredDeal> deals = buildRandomDeals(state, 1);
		if(deals.empty())
			continue;

		InferredDeal &deal = deals[0];

		for(int i = 0; i < static_cast<int>(topPlays.size()); ++i)
		{
			if(clock() >= searchTime)
				break;

			int index = (round + i) % static_cast<int>(topPlays.size());
			ScoredPlay &scored = topPlays[index];
			RResult rolloutResult = simulateDealAfterPlay(state, deal, scored.play, searchTime);
			double oneScore = rolloutResult.score;

			scored.sampleScore += oneScore * deal.weight;
			scored.weight += deal.weight;
			scored.visits++;
		}

		if(!topPlays.empty())
			round = (round + 1) % static_cast<int>(topPlays.size());
	}

	logPIMCResult(state, topPlays);
}

// 诊断函数：接收 PIMC rollout 结束后的 topPlays，打印完整决策表到 stderr。
// 用法：在 decidePlay 选完 bestplay 之后、return 之前调用。
// 参数 topPlays 必须已经完成 rollout（visits/sampleScore/weight 均已累计）。
// 不修改任何数据，不影响 stdout，不影响决策结果。
void logPIMCResult(GameState &state, vector<ScoredPlay> &topPlays)
{
	bool freeTurn = state.lastValidCombo.comboType == CardComboType::PASS;
	bool dangerous = isDangerousSituation(state);

	int totalVisits = 0;
	for(ScoredPlay &s : topPlays) totalVisits += s.visits;

	// 先过一遍，计算最终分并找出 bestIndex（与 decidePlay 逻辑完全一致）
	double bestFinal = -1e18;
	int bestIndex = 0;
	struct Row {
		double avgSample;
		double sw;
		double finalScore;
	};
	vector<Row> rows(topPlays.size());

	for(int i = 0; i < (int)topPlays.size(); i++)
	{
		ScoredPlay &sc = topPlays[i];
		double avg = (sc.weight > 0) ? sc.sampleScore / sc.weight : 0.0;
		double trust = (sc.visits > 0) ? sc.visits / (sc.visits + 6.0) : 0.0;
		double sw = getSampleWeight(state, sc.play) * trust;
		double fs = sc.score + avg * sw;
		rows[i] = {avg, sw, fs};
		if(fs > bestFinal) { bestFinal = fs; bestIndex = i; }
	}

	// 输出表头
	std::cerr << "[PIMC] freeTurn=" << freeTurn
	          << " dangerous=" << dangerous
	          << " topCount=" << topPlays.size()
	          << " totalVisits=" << totalVisits
	          << std::endl;

	// 输出每个候选行
	for(int i = 0; i < (int)topPlays.size(); i++)
	{
		ScoredPlay &sc = topPlays[i];
		Row &r = rows[i];
		std::cerr << "[PIMC]  [" << i << "]"
		          << " type=" << static_cast<int>(sc.play.comboType)
		          << " lvl=" << sc.play.comboLevel
		          << " sz=" << sc.play.cards.size()
		          << " cards=";
		for(Card c : sc.play.cards) std::cerr << c << ",";
		std::cerr << " heur=" << sc.score
		          << " avgSample=" << r.avgSample
		          << " visits=" << sc.visits
		          << " sw=" << r.sw
		          << " final=" << r.finalScore;
		if(i == bestIndex) std::cerr << " <BEST>";
		std::cerr << std::endl;
	}
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

	//选启发式评分最高的几手
	vector<ScoredPlay> topPlays = selectTopPlays(state, validPlays, 6);

	//给整次PIMC一个时间限制
	int timeLimit = 500;

	//记录本次搜索时间
	clock_t searchTime = clock() + timeLimit * CLOCKS_PER_SEC / 1000;
	// 记录这一轮从哪个候选开始，避免总是前面的候选先吃到时间预算。
	int round = 0;

	//在时间内不断生成随机补全，并轮流给每个候选做模拟
	while(clock()<searchTime)
	{
		//每次只生成一个样本
		vector<InferredDeal> deals = buildRandomDeals(state, 1);

		if(deals.empty())
			continue;

		InferredDeal &deal = deals[0];

		//让每个候选都在这一份样本上评估一次
		for (int i = 0; i < topPlays.size(); ++i)
		{
			if(clock()>=searchTime)
			break;

			int index = (round + i) % topPlays.size();
			ScoredPlay &scored = topPlays[index];
			//在当前这份随机补全样本上，模拟打出这一手后的后续对局。所有候选共享这一次决策的总时间预算
			RResult rolloutResult = simulateDealAfterPlay(state, deal, scored.play, searchTime);

			//这一次rollout返回的分数
			double oneScore = rolloutResult.score;

			// 按样本权重累计。
        	scored.sampleScore += oneScore * deal.weight;
        	scored.weight += deal.weight;
        	scored.visits++;
		}

		//下一轮从下一个候选开始
		if(!topPlays.empty())
			round = (round + 1) % topPlays.size();
	}

	//选评分最高的一手
	for(ScoredPlay &scored:topPlays)
	{
		double avgSampleScore = 0;
		if(scored.weight > 0)
    		avgSampleScore = scored.sampleScore / scored.weight;

		//可信度
		double trust = 0;
		if(scored.visits > 0)
    		trust = scored.visits / (scored.visits + 6.0);

		double sampleWeight = getSampleWeight(state, scored.play) * trust;
		double finalScore = scored.score + avgSampleScore * sampleWeight;

		if(finalScore > bestScore)
		{
			bestScore = finalScore;
			bestplay = scored.play;
		}
	}

	//如果所有候选都被跳过，返回PASS
	if(bestScore==-1e18)
		return CardCombo();

	logPIMCResult(state, topPlays);
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