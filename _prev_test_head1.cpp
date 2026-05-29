#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring> // 娉ㄦ剰memset鏄痗string閲岀殑
#include <ctime>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include "jsoncpp/json.h" // 鍦ㄥ钩鍙颁笂锛孋++缂栬瘧鏃堕粯璁ゅ寘鍚搴?
using std::set;
using std::sort;
using std::string;
using std::unique;
using std::vector;
///

constexpr int PLAYER_COUNT = 3;

enum class Stage
{
	BIDDING, // 鍙垎闃舵
	PLAYING	 // 鎵撶墝闃舵
};

enum class CardComboType
{
	PASS,		// 杩?	SINGLE,		// 鍗曞紶
	PAIR,		// 瀵瑰瓙
	STRAIGHT,	// 椤哄瓙
	STRAIGHT2,	// 鍙岄『
	TRIPLET,	// 涓夋潯
	TRIPLET1,	// 涓夊甫涓€
	TRIPLET2,	// 涓夊甫浜?	BOMB,		// 鐐稿脊
	QUADRUPLE2, // 鍥涘甫浜岋紙鍙級
	QUADRUPLE4, // 鍥涘甫浜岋紙瀵癸級
	PLANE,		// 椋炴満
	PLANE1,		// 椋炴満甯﹀皬缈?	PLANE2,		// 椋炴満甯﹀ぇ缈?	SSHUTTLE,	// 鑸ぉ椋炴満
	SSHUTTLE2,	// 鑸ぉ椋炴満甯﹀皬缈?	SSHUTTLE4,	// 鑸ぉ椋炴満甯﹀ぇ缈?	ROCKET,		// 鐏
	INVALID		// 闈炴硶鐗屽瀷
};

int cardComboScores[] = {
	0,	// 杩?	1,	// 鍗曞紶
	2,	// 瀵瑰瓙
	6,	// 椤哄瓙
	6,	// 鍙岄『
	4,	// 涓夋潯
	4,	// 涓夊甫涓€
	4,	// 涓夊甫浜?	10, // 鐐稿脊
	8,	// 鍥涘甫浜岋紙鍙級
	8,	// 鍥涘甫浜岋紙瀵癸級
	8,	// 椋炴満
	8,	// 椋炴満甯﹀皬缈?	8,	// 椋炴満甯﹀ぇ缈?	10, // 鑸ぉ椋炴満锛堥渶瑕佺壒鍒わ細浜岃繛涓?0鍒嗭紝澶氳繛涓?0鍒嗭級
	10, // 鑸ぉ椋炴満甯﹀皬缈?	10, // 鑸ぉ椋炴満甯﹀ぇ缈?	16, // 鐏
	0	// 闈炴硶鐗屽瀷
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

// 鐢?~53杩?4涓暣鏁拌〃绀哄敮涓€鐨勪竴寮犵墝
using Card = short;
constexpr Card card_joker = 52;
constexpr Card card_JOKER = 53;

// 闄や簡鐢?~53杩?4涓暣鏁拌〃绀哄敮涓€鐨勭墝锛?// 杩欓噷杩樼敤鍙︿竴绉嶅簭鍙疯〃绀虹墝鐨勫ぇ灏忥紙涓嶇鑺辫壊锛夛紝浠ヤ究姣旇緝锛岀О浣滅瓑绾э紙Level锛?// 瀵瑰簲鍏崇郴濡備笅锛?// 3 4 5 6 7 8 9 10	J Q K	A	2	灏忕帇	澶х帇
// 0 1 2 3 4 5 6 7	8 9 10	11	12	13	14
using Level = short;
constexpr Level MAX_LEVEL = 15;
constexpr Level MAX_STRAIGHT_LEVEL = 11;
constexpr Level level_joker = 13;
constexpr Level level_JOKER = 14;

/**
* 灏咰ard鍙樻垚Level
*/
// [绀轰緥绋嬪簭鎻愪緵锛屽彲鐩存帴澶嶇敤] 鎶婂叿浣撶墝鍙疯浆鎹㈡垚涓嶅尯鍒嗚姳鑹茬殑绛夌骇锛屼緵鐗屽瀷璇嗗埆鍜屽ぇ灏忔瘮杈冧娇鐢ㄣ€?constexpr Level card2level(Card card)
{
	return card / 4 + card / 53;
}

// 鐗岀殑缁勫悎锛岀敤浜庤绠楃墝鍨?//鏌愪竴娆℃墦鍑哄幓鐨勪竴缁勭墝锛氫竴寮?7銆佷竴涓『瀛?34567
struct CardCombo
{
	// 琛ㄧず鍚岀瓑绾х殑鐗屾湁澶氬皯寮?	// 浼氭寜涓暟浠庡ぇ鍒板皬銆佺瓑绾т粠澶у埌灏忔帓搴?	//===鎶娾€滃悓涓€涓偣鏁扮殑鐗屸€濇墦鍖呮垚涓€鏉＄粺璁¤褰?==
	struct CardPack
	{
		//杩欎釜闆嗗悎鏄粈涔堢偣鏁帮紝姣斿 3銆?銆丄銆?銆佺帇
		Level level;
		// 杩欎釜鐐规暟涓€鍏辨湁鍑犲紶
		short count;

		// [绀轰緥绋嬪簭鎻愪緵锛屽彲鐩存帴澶嶇敤] 瀹氫箟鐗岀鎺掑簭瑙勫垯锛氬厛鎸夊紶鏁伴檷搴忥紝鍐嶆寜绛夌骇闄嶅簭銆?		//===寮犳暟澶氱殑鎺掑墠闈紝濡傛灉寮犳暟涓€鏍凤紝澶х偣鏁版帓鍓嶉潰===
		bool operator<(const CardPack &b) const
		{
			if (count == b.count)
				return level > b.level;
			return count > b.count;
		}
	};
	vector<Card> cards;		 // 鍘熷鐨勭墝锛屾湭鎺掑簭
	vector<CardPack> packs;	 // 鎸夋暟鐩拰澶у皬鎺掑簭鐨勭墝绉?	CardComboType comboType; // 绠楀嚭鐨勭墝鍨?	Level comboLevel = 0;	 // 绠楀嚭鐨勫ぇ灏忓簭锛堜富鐗岀殑绛夌骇锛屽鏋滄槸椤哄瓙鍨嬬殑浠ユ渶楂樼瓑绾т负鍑嗭級

	/**
						  * 妫€鏌ヤ釜鏁版渶澶氱殑CardPack閫掑噺浜嗗嚑涓?						  */
	// [绀轰緥绋嬪簭鎻愪緵锛屽彲鐩存帴澶嶇敤] 缁熻涓荤墝閮ㄥ垎杩炵画浜嗗灏戠粍锛岀敤浜庡垽鏂『瀛愩€侀鏈恒€佽埅澶╅鏈虹瓑杩炵画鐗屽瀷銆?	int findMaxSeq() const
	{
		for (unsigned c = 1; c < packs.size(); c++)
			if (packs[c].count != packs[0].count ||
				packs[c].level != packs[c - 1].level - 1)
				return c;
		return packs.size();
	}

	/**
	* 杩欎釜鐗屽瀷鏈€鍚庣畻鎬诲垎鐨勬椂鍊欑殑鏉冮噸
	*/
	// [绀轰緥绋嬪簭鎻愪緵锛屽彲鐩存帴澶嶇敤] 杩斿洖褰撳墠鐗屽瀷鐨勫熀纭€鏉冮噸锛屼富瑕佺敤浜庡悗缁墿灞曡瘎浼版垨璋冭瘯銆?	int getWeight() const
	{
		if (comboType == CardComboType::SSHUTTLE ||
			comboType == CardComboType::SSHUTTLE2 ||
			comboType == CardComboType::SSHUTTLE4)
			return cardComboScores[(int)comboType] + (findMaxSeq() > 2) * 10;
		return cardComboScores[(int)comboType];
	}

	// 鍒涘缓涓€涓┖鐗岀粍
	// [绀轰緥绋嬪簭鎻愪緵锛屽彲鐩存帴澶嶇敤] 鏋勯€犱竴涓?PASS 鐗岀粍锛岃〃绀轰笉鍑虹墝銆?	CardCombo() : comboType(CardComboType::PASS) {}

	/**
	* 閫氳繃Card锛堝嵆short锛夌被鍨嬬殑杩唬鍣ㄥ垱寤轰竴涓墝鍨?	* 骞惰绠楀嚭鐗屽瀷鍜屽ぇ灏忓簭绛?	* 鍋囪杈撳叆娌℃湁閲嶅鏁板瓧锛堝嵆閲嶅鐨凜ard锛?	*/
	template <typename CARD_ITERATOR>
	// [绀轰緥绋嬪簭鎻愪緵锛屽彲鐩存帴澶嶇敤] 鏍规嵁涓€缁勫叿浣撶墝鑷姩璇嗗埆鐗屽瀷銆佷富鐗岀瓑绾у拰鍐呴儴鐗岀缁撴瀯銆?	//鏋勯€犲嚱鏁帮紝杈撳叆涓€缁勭墝锛岃嚜鍔ㄨ瘑鍒墝鍨?	CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end)
	{
		// 鐗瑰垽锛氱┖
		if (begin == end)
		{
			comboType = CardComboType::PASS;
			return;
		}

		// 姣忕鐗屾湁澶氬皯涓?		short counts[MAX_LEVEL + 1] = {};

		// 鍚岀鐗岀殑寮犳暟锛堟湁澶氬皯涓崟寮犮€佸瀛愩€佷笁鏉°€佸洓鏉★級
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

		// 鐢ㄦ渶澶氱殑閭ｇ鐗屾€绘槸鍙互姣旇緝澶у皬鐨?		comboLevel = packs[0].level;

		// 璁＄畻鐗屽瀷
		// 鎸夌収 鍚岀鐗岀殑寮犳暟 鏈夊嚑绉?杩涜鍒嗙被
		vector<int> kindOfCountOfCount;
		for (int i = 0; i <= 4; i++)
			if (countOfCount[i])
				kindOfCountOfCount.push_back(i);
		sort(kindOfCountOfCount.begin(), kindOfCountOfCount.end());

		int curr, lesser;

		switch (kindOfCountOfCount.size())
		{
		case 1: // 鍙湁涓€绫荤墝
			curr = countOfCount[kindOfCountOfCount[0]];
			switch (kindOfCountOfCount[0])
			{
			case 1:
				// 鍙湁鑻ュ共鍗曞紶
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
				// 鍙湁鑻ュ共瀵瑰瓙
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
				// 鍙湁鑻ュ共涓夋潯
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
				// 鍙湁鑻ュ共鍥涙潯
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
		case 2: // 鏈変袱绫荤墝
			curr = countOfCount[kindOfCountOfCount[1]];
			lesser = countOfCount[kindOfCountOfCount[0]];
			if (kindOfCountOfCount[1] == 3)
			{
				// 涓夋潯甯︼紵
				if (kindOfCountOfCount[0] == 1)
				{
					// 涓夊甫涓€
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
					// 涓夊甫浜?					if (curr == 1 && lesser == 1)
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
				// 鍥涙潯甯︼紵
				if (kindOfCountOfCount[0] == 1)
				{
					// 鍥涙潯甯︿袱鍙?* n
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
					// 鍥涙潯甯︿袱瀵?* n
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
	* 鍒ゆ柇鎸囧畾鐗岀粍鑳藉惁澶ц繃褰撳墠鐗岀粍锛堣繖涓嚱鏁颁笉鑰冭檻杩囩墝鐨勬儏鍐碉紒锛?	*/
	// [绀轰緥绋嬪簭鎻愪緵锛屽彲鐩存帴澶嶇敤] 鍒ゆ柇鍙︿竴鎵嬬墝 b 鏄惁鑳藉悎娉曞帇杩囧綋鍓嶇墝缁勩€?	bool canBeBeatenBy(const CardCombo &b) const
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
	* 浠庢寚瀹氭墜鐗屼腑瀵绘壘绗竴涓兘澶ц繃褰撳墠鐗岀粍鐨勭墝缁?	* 濡傛灉闅忎究鍑虹殑璇濆彧鍑虹涓€寮?	* 濡傛灉涓嶅瓨鍦ㄥ垯杩斿洖涓€涓狿ASS鐨勭墝缁?	*/
	template <typename CARD_ITERATOR>
	// [绀轰緥绋嬪簭鎻愪緵锛屽彲浣滀负杩囨浮鏂规] 浠庢墜鐗岄噷璐績鎵剧涓€鎵嬭兘鍑虹殑鐗岋紱鍚庣画涓荤瓥鐣ヤ細閫愭琚?enumAllValidPlays 鏇夸唬銆?	// todo
	CardCombo findFirstValid(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		if (comboType == CardComboType::PASS) // 濡傛灉涓嶉渶瑕佸ぇ杩囪皝锛屽彧闇€瑕侀殢渚垮嚭
		{
			CARD_ITERATOR second = begin;
			second++;
			return CardCombo(begin, second); // 閭ｄ箞灏卞嚭绗竴寮犵墝鈥︹€?		}

		// 鐒跺悗鍏堢湅涓€涓嬫槸涓嶆槸鐏锛屾槸鐨勮瘽灏辫繃
		if (comboType == CardComboType::ROCKET)
			return CardCombo();

		// 鐜板湪鎵撶畻浠庢墜鐗屼腑鍑戝嚭鍚岀墝鍨嬬殑鐗?		auto deck = vector<Card>(begin, end); // 鎵嬬墝
		short counts[MAX_LEVEL + 1] = {};

		unsigned short kindCount = 0;

		// 鍏堟暟涓€涓嬫墜鐗岄噷姣忕鐗屾湁澶氬皯涓?		for (Card c : deck)
			counts[card2level(c)]++;

		// 鎵嬬墝濡傛灉涓嶅鐢紝鐩存帴涓嶇敤鍑戜簡锛岀湅鐪嬭兘涓嶈兘鐐稿惂
		if (deck.size() < cards.size())
			goto failure;

		// 鍐嶆暟涓€涓嬫墜鐗岄噷鏈夊灏戠鐗?		for (short c : counts)
			if (c)
				kindCount++;

		// 鍚﹀垯涓嶆柇澧炲ぇ褰撳墠鐗岀粍鐨勪富鐗岋紝鐪嬬湅鑳戒笉鑳芥壘鍒板尮閰嶇殑鐗岀粍
		{
			// 寮€濮嬪澶т富鐗?			int mainPackCount = findMaxSeq();
			bool isSequential =
				comboType == CardComboType::STRAIGHT ||
				comboType == CardComboType::STRAIGHT2 ||
				comboType == CardComboType::PLANE ||
				comboType == CardComboType::PLANE1 ||
				comboType == CardComboType::PLANE2 ||
				comboType == CardComboType::SSHUTTLE ||
				comboType == CardComboType::SSHUTTLE2 ||
				comboType == CardComboType::SSHUTTLE4;
			for (Level i = 1;; i++) // 澧炲ぇ澶氬皯
			{
				for (int j = 0; j < mainPackCount; j++)
				{
					int level = packs[j].level + i;

					// 鍚勭杩炵画鐗屽瀷鐨勪富鐗屼笉鑳藉埌2锛岄潪杩炵画鐗屽瀷鐨勪富鐗屼笉鑳藉埌灏忕帇锛屽崟寮犵殑涓荤墝涓嶈兘瓒呰繃澶х帇
					if ((comboType == CardComboType::SINGLE && level > MAX_LEVEL) ||
						(isSequential && level > MAX_STRAIGHT_LEVEL) ||
						(comboType != CardComboType::SINGLE && !isSequential && level >= level_joker))
						goto failure;

					// 濡傛灉鎵嬬墝涓繖绉嶇墝涓嶅锛屽氨涓嶇敤缁х画澧炰簡
					if (counts[level] < packs[j].count)
						goto next;
				}

				{
					// 鎵惧埌浜嗗悎閫傜殑涓荤墝锛岄偅涔堜粠鐗屽憿锛?					// 濡傛灉鎵嬬墝鐨勭绫绘暟涓嶅锛岄偅浠庣墝鐨勭绫绘暟灏变笉澶燂紝涔熶笉琛?					if (kindCount < packs.size())
						continue;

					// 濂界粓浜庡彲浠ヤ簡
					// 璁＄畻姣忕鐗岀殑瑕佹眰鏁扮洰鍚?					short requiredCounts[MAX_LEVEL + 1] = {};
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
						if (k == MAX_LEVEL + 1) // 濡傛灉鏄兘涓嶇鍚堣姹傗€︹€﹀氨涓嶈浜?							goto next;
					}

					// 寮€濮嬩骇鐢熻В
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

			next:; // 鍐嶅澶?			}
		}

	failure:
		// 瀹炲湪鎵句笉鍒板晩
		// 鏈€鍚庣湅涓€涓嬭兘涓嶈兘鐐稿惂

		for (Level i = 0; i < level_joker; i++)
			if (counts[i] == 4 && (comboType != CardComboType::BOMB || i > packs[0].level)) // 濡傛灉瀵规柟鏄偢寮癸紝鑳界偢鐨勮繃鎵嶈
			{
				// 杩樼湡鍙互鍟娾€︹€?				Card bomb[] = {Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3)};
				return CardCombo(bomb, bomb + 4);
			}

		// 鏈夋病鏈夌伀绠紵
		if (counts[level_joker] + counts[level_JOKER] == 2)
		{
			Card rocket[] = {card_joker, card_JOKER};
			return CardCombo(rocket, rocket + 2);
		}

		// 鈥︹€?		return CardCombo();
	}

	// [绀轰緥绋嬪簭鎻愪緵锛屽彲鐩存帴澶嶇敤] 鏈湴璋冭瘯鏃舵墦鍗板綋鍓嶇墝缁勭殑鐗屽瀷鍜屽ぇ灏忎俊鎭€?	void debugPrint()
	{
#ifndef _BOTZONE_ONLINE
		std::cout << "銆? << cardComboStrings[(int)comboType] << "鍏? << cards.size() << "寮狅紝澶у皬搴? << comboLevel << "銆?;
#endif
	}
};

// ==================================================
// 鍗曟枃浠堕鏋讹細鐘舵€佺粨鏋?// ==================================================

// 琛ㄧず鎵嬬墝鍙互鎷嗗垎鎴愮殑鐗屽瀷缁勫悎锛氭瘡涓€涓猤roups[]閮芥槸涓€绉嶇墝鍨嬶紝鎵€鏈塯roups[]鍔犲拰灏辨槸褰撳墠鐨勬墜鐗岄泦鍚堬級
// handCount琛ㄧず鍚屼竴鎵嬬墝鐨勪笉鍚屽嚭娉曠殑涓暟
struct HandPlan
{
	vector<CardCombo> groups;
	int handCount=0;
};

//涓€娆″嚭鐗岀殑浜嬩欢涓婁笅鏂?struct PlayEvent
{
	//褰撳墠琛屽姩鐨勭帺瀹剁紪鍙?	int player = -1;
	//褰撳墠鐜╁瀹為檯鎵撳嚭鐨勭墝
	vector<Card> cards;
	//褰撳墠鐜╁瀹為檯鎵撳嚭鐨勭墝鍨?	CardCombo combo;
	//褰撳墠鐜╁琛屽姩鍓嶏紝闇€瑕佸帇杩囩殑鐗?	CardCombo requiredCombo;
	//requiredCombo鏄皝鍑虹殑
	int requiredPlayer = -1;
	//褰撳墠鐜╁鏄惁閫夋嫨PASS
	bool isPass = false;
};

//涓€鏉ASS绾︽潫
struct PassConstraint
{
	//閫夋嫨PASS鐨勭帺瀹?	int player = -1;
	//浠栧綋鏃惰鍘嬭繃鐨勭墝
	CardCombo requirCombo;
	//杩欑墝鏄皝鍑虹殑
	int requirPlayer = -1;
	
	//绾︽潫寮哄害锛屾暟鍊艰秺灏忚瘉鎹秺寮?	//渚嬪鍗遍櫓娈嬪眬閲岀殑 PASS 鍙互鏇村己锛屾櫘閫氳窡鐗?PASS 鍙互鏇村急
	double strength = 1.0;
};


/// 褰撳墠鏁村眬灞€闈?//杩欎釜缁撴瀯琛ㄧず鐨勬槸锛屽綋鍓嶆暣鐩樻父鎴忚疆鍒版垜鏃剁殑瀹屾暣灞€闈?struct GameState
{
	Stage stage = Stage::BIDDING;
	//鎴戞槸 0/1/2 鍙蜂綅閲岀殑璋?	int myPosition = 0;
	//鍦颁富鏄皝
	int landlordPosition = -1;
	int finalBid = -1;
	//鍓嶉潰鐜╁鏄€庝箞鍙垎鐨?	vector<int> bidHistory;
	//鎴戠幇鍦ㄦ墜閲岃繕鍓╁摢浜涚墝
	vector<Card> myCards;
	//鍦颁富鐨勪笁寮犳槑鐗?	vector<Card> publicCards;
	//褰撳墠妗岄潰涓婇渶瑕佸帇杩囩殑鐗?	CardCombo lastValidCombo;
	//褰撳墠妗岄潰鏈€鍚庝竴鎵嬫湁鏁堢墝鏄皝鍑虹殑,-1琛ㄧず鑷敱鍑虹墝锛?	int lastValidPlayer = -1;
	//涓変釜鐜╁鍚勮嚜杩樺墿鍑犲紶鐗?	int cardRemaining[PLAYER_COUNT] = {17, 17, 17};
	//姣忎釜浜哄巻鍙蹭笂鍑鸿繃浠€涔?	vector<vector<Card>> playHistory[PLAYER_COUNT];

	//鎸夋椂闂撮『搴忚褰曟瘡涓€娆″嚭鐗屼簨浠?	vector<PlayEvent> playEvents;
	//浠嶱ASS涓彁鍙栫殑绾︽潫
	vector<PassConstraint> passConstraints;

	//璁扮墝鍣ㄥ熀纭€鏁版嵁
	bool cardPlayed[54] = {};
	short levelRemaining[MAX_LEVEL] = {};

	//杩欏紶鐗屽綋鍓嶆槸鍚﹀睘浜庢湭鐭ョ墝鍖?	bool cardUnknown[54] = {};
	//姣忎釜鐜╁褰撳墠纭畾杩樻寔鏈夌殑鏄庣墝
	vector<Card> konwCardOfPlayer[PLAYER_COUNT];

	// [鎴戜滑瀹炵幇] 鍒ゆ柇鎴戞槸鍚︿负鍦颁富锛屼緵绛栫暐灞傚揩閫熷尯鍒嗚鑹蹭娇鐢ㄣ€?	bool isLandlord() const
	{
		return myPosition == landlordPosition;
	}

	// [鎴戜滑瀹炵幇] 鍒ゆ柇鎸囧畾浣嶇疆鐨勭帺瀹舵槸鍚︽槸鎴戠殑闃熷弸锛岀洰鍓嶄富瑕佺敤浜庡啘姘戦厤鍚堥€昏緫銆?	bool isTeammate(int pos) const
	{
		return !isLandlord() && pos != myPosition && pos != landlordPosition;
	}

	// [鎴戜滑瀹炵幇] 杩斿洖鎴戠殑闃熷弸浣嶇疆锛涘鏋滄垜鏄湴涓诲垯杩斿洖 -1銆?	int getTeammatePos() const
	{
		if (isLandlord())
			return -1;
		for (int pos = 0; pos < PLAYER_COUNT; ++pos)
			if (pos != myPosition && pos != landlordPosition)
				return pos;
		return -1;
	}

	// [鎴戜滑瀹炵幇] 缁熻褰撳墠鏈煡鍖哄煙杩樺墿澶氬皯寮犵墝锛屽悗缁彲鐢ㄤ簬璁扮墝鍜屾鐜囨帹鏂€?	int getUnknownCardCount() const
	{
		int total = 0;
		for (short count : levelRemaining)
			total += count;
		return total;
	}
};

//涓€娆″彲鑳界殑瀹屾暣鍙戠墝缁撴灉
struct InferredDeal
{
	//姣忎釜鐜╁鍦ㄨ繖涓牱鏈腑鐨勬墜鐗?	vector<Card> hands[PLAYER_COUNT];
	//杩欎釜鏍锋湰鐨勫彲淇℃潈閲?	double weight = 1.0;
};

// ==================================================
// 鍗曟枃浠堕鏋讹細鍩虹宸ュ叿
// ==================================================

// [鎴戜滑瀹炵幇] 鎸夌瓑绾т紭鍏堛€佺墝鍙锋涔嬪鎵嬬墝鎺掑簭锛岀粺涓€鍚庣画鏋氫妇銆佽緭鍑哄拰璋冭瘯琛屼负
// todo_done
void sortCards(vector<Card> &cards)
{
	sort(cards.begin(), cards.end(), [](Card left, Card right) 
	{
		Level leftLevel = card2level(left);
		Level rightLevel = card2level(right);
		if (leftLevel == rightLevel)
			return left < right;	//鍗囧簭鎺掑垪
		return leftLevel < rightLevel;
	});
}

// [鎴戜滑瀹炵幇] 浠庝竴鎵嬬墝閲屽垹鎺夊凡缁忔墦鍑虹殑鍏蜂綋鐗岋紝杩斿洖鍒犻櫎鍚庣殑鏂版墜鐗屽壇鏈€?// todo_done
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

// [鎴戜滑瀹炵幇] 鎸夌瓑绾ф妸鎵嬬墝鍒嗙粍锛屾柟渚垮仛瀵瑰瓙銆佷笁鏉°€佺偢寮圭瓑缁熻鍜屾灇涓俱€?// todo_done
vector<vector<Card> > groupCardsByLevel(const vector<Card> &hand)
{
	vector<vector<Card> > grouped(MAX_LEVEL);
	for (Card card : hand)
		grouped[card2level(card)].push_back(card);
	return grouped;
}

void initRandomSeed()
{
	// time(nullptr) 鎻愪緵绉掔骇鏃堕棿锛宑lock() 鎻愪緵褰撳墠杩涚▼杩愯鏃堕棿銆?    // 涓よ€呭紓鎴栧悗浣滀负绉嶅瓙锛岄伩鍏嶆瘡娆¤繘绋嬪惎鍔ㄩ兘浣跨敤榛樿鍥哄畾绉嶅瓙銆?	std::srand(static_cast<unsigned>(std::time(nullptr)) ^ static_cast<unsigned>(clock()));
}


// ==================================================
// 鍗曟枃浠堕鏋讹細IO / 鐘舵€佹仮澶?// ==================================================

void RebuildCard(GameState &state);
void recordPlayEvent(GameState &state, int player, vector<Card> &playedCard);
bool isSameSidePlayer(GameState &state, int palyer);
bool canBeatComboFast(vector<Card> &hand, CardCombo &requiredCombo);
bool canPlayerBeatInDeal(InferredDeal &deal, int player, CardCombo &requiredCombo);
 
// [鍩轰簬绀轰緥绋嬪簭閫昏緫鏀归€狅紝寤鸿浼樺厛淇濈暀] 璇诲彇 Botzone 杈撳叆骞堕噸寤哄綋鍓嶅眬闈紝杩斿洖鏈疆鍐崇瓥鎵€闇€鐨勫畬鏁寸姸鎬併€?GameState readGameState()
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

			// 鍏堣褰曚簨浠讹紝鍐嶆洿鏂版闈㈢姸鎬併€?			// 杩欐牱 event 閲屾嬁鍒扮殑鏄€滆鍔ㄥ墠鈥濈殑 requiredCombo 鍜?requiredPlayer銆?			recordPlayEvent(state, player, playedCards);

			state.playHistory[player].push_back(playedCards);
			state.cardRemaining[player] -= static_cast<int>(playerAction.size());

			// 濡傛灉杩欎釜鐜╁閫夋嫨 PASS锛屽彧璁板綍杩炵画 PASS 鏁伴噺銆?			if (playedCards.empty())
			{
    			++howManyPass;
			}
				// 濡傛灉杩欎釜鐜╁鍑轰簡鏈夋晥鐗岋紝灏辨洿鏂板綋鍓嶉渶瑕佸帇杩囩殑鐗屻€?				else
			{
    			// 璁板綍褰撳墠妗岄潰鏈€鍚庝竴鎵嬫湁鏁堢墝鏄粈涔堛€?    			state.lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
				//璁板綍杩欐槸璋佸嚭鐨?    			state.lastValidPlayer = player;
			}

		}

		// 濡傛灉鍓嶉潰涓ゅ閮?PASS锛岃鏄庝笂涓€杞墝鏉冨凡缁忓洖鍒板綋鍓嶇帺瀹躲€?		// 褰撳墠鐜╁鍙互鑷敱鍑虹墝锛屼笉闇€瑕佸啀鍘嬩换浣曚汉銆?		if (howManyPass == 2)
		{
    		// PASS 鐗屽瀷琛ㄧず鑷敱鍑虹墝銆?    		state.lastValidCombo = CardCombo();

    		// -1 琛ㄧず褰撳墠娌℃湁闇€瑕佸帇杩囩殑鍑虹墝鑰呫€?    		state.lastValidPlayer = -1;
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

			// 璁板綍鎴戣嚜宸辩殑鍘嗗彶琛屽姩銆?			// 杩欓噷鍚屾牱瑕佸湪鏇存柊浠讳綍浼氬奖鍝嶅悗缁垽鏂殑鐘舵€佷箣鍓嶈褰曘€?			recordPlayEvent(state, state.myPosition, playedCards);

			// 濡傛灉鎴戝綋鏃跺嚭浜嗘湁鏁堢墝锛岃繖鎵嬬墝涔熶細鎴愪负鍚庣画鐜╁闇€瑕佸帇杩囩殑妗岄潰鐗屻€?			if (!playedCards.empty())
			{
				// 鏇存柊褰撳墠妗岄潰鏈€鍚庝竴鎵嬫湁鏁堢墝銆?				state.lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());

				// 璁板綍杩欐墜鏈夋晥鐗屾槸鎴戝嚭鐨勩€?				state.lastValidPlayer = state.myPosition;
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
			//鍙湁杩樻病琚墦鍑虹殑鍦颁富鏄庣墝锛屾墠闇€瑕佷粠鏈煡鐗岄噷鎺掗櫎
			if(!state.cardPlayed[card])
				--state.levelRemaining[card2level(card)];
		}
	}

	//鍦ㄦ墍鏈夊巻鍙查兘閲嶆斁瀹屾垚涔嬪悗锛岀粺涓€閲嶅缓纭畾鐗屽拰鏈煡鐗屽尯
	RebuildCard(state);

	return state;
}

// [鍩轰簬绀轰緥绋嬪簭閫昏緫鏀归€狅紝寤鸿浼樺厛淇濈暀] 鎸夊钩鍙拌姹傝緭鍑哄彨鍒嗗喅绛?JSON銆?void outputBid(int value)
{
	Json::Value result;
	result["response"] = value;
	Json::FastWriter writer;
	std::cout << writer.write(result) << std::endl;
}

// [鍩轰簬绀轰緥绋嬪簭閫昏緫鏀归€狅紝寤鸿浼樺厛淇濈暀] 鎸夊钩鍙拌姹傝緭鍑哄嚭鐗屽喅绛?JSON锛岀┖鏁扮粍琛ㄧず PASS銆?void outputPlay(const vector<Card> &cards)
{
	Json::Value result, response(Json::arrayValue);
	for (Card card : cards)
		response.append(card);
	result["response"] = response;
	Json::FastWriter writer;
	std::cout << writer.write(result) << std::endl;
}

// ==================================================
// 鍗曟枃浠堕鏋讹細鏋氫妇灞?// ==================================================
#pragma region enum
// [鎴戜滑瑕佽嚜宸卞疄鐜扮殑鏍稿績鍑芥暟] 鏋氫妇褰撳墠灞€闈笅鎵€鏈夊悎娉曞嚭鐗岋紝渚涚瓥鐣ュ眰璇勪及鍜屾瘮杈冿紱濡傛灉娌℃湁鍚堟硶鍑虹墝鍒欒繑鍥炰竴涓彧鍖呭惈 PASS 鐨勫垪琛?// todo
/* 
瀹炵幇閫昏緫锛?鍏堟壂鎻忎竴閬嶅綋鍓嶆墜鐗岋紝璁板綍姣忎釜level鍚勬湁鍑犲紶鐗岋紝鍙互蹇€熺瓫閫夊彲鑳界殑鐗屽瀷锛?鐒跺悗瀹氬悜鏋氫妇鎸戠墝锛屾瘮濡傚崟寮犮€佸瀛愩€佷笁甯︿竴绛夌瓑锛涙瘡鎵惧埌涓€绉嶅悎娉曠粍鍚堬紝灏卞皢鍏舵斁鍏andidate涓紱
鎺ョ潃鎶奵andidate浼犲叆鏋勯€犲嚱鏁癈ardCombo(start,end)涓紝寰楀埌comboType鍜宑omboLevel锛?鏈€鍚庡垽鏂繖缁刢andidate鏄惁鍚堟硶
*/
vector<CardCombo> enumAllValidPlays(vector<Card>& hand,CardCombo& lastCombo){
	vector<CardCombo> validPlays; // 鍙兘鐨勫嚭鐗屽垪琛?	validPlays.push_back(CardCombo());	// pass

	// 涓婂鍑虹伀绠紝鐩存帴pass
	if(lastCombo.comboType==CardComboType::ROCKET)	
		return validPlays;
	
	int counts[MAX_LEVEL+1]={0};	// 缁熻鎵嬬墝涓悇涓猯evel鐨勭墝鏈夊灏戝紶
	vector<Card> cardsByLevel[MAX_LEVEL+1];
	for(Card c:hand){
		Level l=card2level(c);
		counts[l]++;
		cardsByLevel[l].push_back(c);
	}

	set<string> uniqueFP;//锛侊紒锛佺敤浜庡幓閲嶏紙鍚岀偣鏁颁笉鍚岃姳鑹诧級锛侊紒锛?
	// 妫€鏌ユ墜鐗岋紝骞跺皢鍚堟硶缁勫悎鍔犲叆鍚堟硶鍑虹墝搴忓垪锛堢嫭绔嬩簬涓婇潰鎵€璇寸殑瀹炵幇閫昏緫锛侊級
	auto addPlay=[&](vector<Card> candidates){
		CardCombo choice(candidates.begin(),candidates.end());
		// 涓婂鏈嚭鐗岋紝鎴栬嚜宸辩殑鐗岃兘澶ц繃涓婂锛屽氨灏哻hoice鏀惧叆鏈夋晥鍑虹墝搴忓垪涓?		if(lastCombo.comboType==CardComboType::PASS||lastCombo.canBeBeatenBy(choice)){
			// 鎶娾€滅瓑绾р€?鈥滅偣鏁扳€濈粍鍚堟垚鎸囩汗fp
			string fp="";
			for(auto pack:choice.packs){
				fp+=std::to_string(pack.level)+"&"+std::to_string(pack.count);	// 杩欎釜count鏄€滅瓑绾т负level鐨勭墝鐨勫紶鏁扳€?			}

			if(uniqueFP.insert(fp).second){	// set涓嶅厑璁稿瓨鍌ㄩ噸澶嶅厓绱狅紝浠呭綋fp鏄娆¤妫€娴嬫墠灏嗗叾瀛樺叆
				//// if閲岀殑鏉′欢绠€鏋愶細
				//// 璋冪敤uniqueFP.insert(fp)鏃讹紝瀹冨湪鎶婂厓绱犲杩涘幓鐨勫悓鏃惰繑鍥炰竴涓猻td::pair<iterator, bool>绫诲瀷鐨勭粨鏋滐紙涓€涓寘鍚袱涓厓绱犵殑閿€煎锛?				//// .first(涓€涓猧terator)鏄泦鍚堜腑瀹為檯瀛樻斁杩欎釜fp鐨勪綅缃紱
				//// .second(bool)鏍囧織杩欐鎻掑叆鏄惁鎴愬姛
				validPlays.push_back(choice);
			}
		}
	};

	// 寰楀埌绛夌骇涓簂鐨勫墠count寮犵墝
	auto getCards=[&](Level l,int count){
		vector<Card> res;
		for(int i=0;i<count;i++)res.push_back(cardsByLevel[l][i]);
		return res;
	};

	// 褰撲富浣撶墝鍨嬬‘瀹氬悗锛屼负涓夊甫涓€銆侀鏈哄甫缈肩瓑鐗屽瀷琛ュ叏鏈€鍚堥€傜殑甯︾墝
	// curCombo涓轰富鐗岋紝need涓洪渶瑕佸甫鍑犵粍鍓墝锛堜富瑕佺敤浜庨鏈哄拰鑸ぉ椋炴満锛岃埅澶╅鏈虹殑鍓墝缁勬暟鏄富鐗岀粍鏁扮殑涓ゅ€嶏紝鍥犱负鍙兘鏄洓甯︿簩/鍥涘甫涓ゅ锛?	// type涓哄甫鐗岀殑绉嶇被锛堝崟寮爋r瀵瑰瓙锛夛紝ex涓鸿涓荤墝鍗犵敤鐨刲evel锛堝嵆鍓墝涓笉鑳藉嚭鐜扮殑level绉嶇被锛?	// 鍐呴儴浣跨敤寮曠敤鍙橀噺锛岀洿鎺ュ湪鍑芥暟鍐呭鍙鐗岀粍杩涜鎻掑叆
	auto selectAttachment=[&](vector<Card> curCombo,int need,int type,set<Level>& ex){	
		vector<vector<Card> > res;	// 璁板綍鍙缁撴灉
		// 瑕佸湪lambda鍐呴儴璋冪敤鑷韩锛屽彧鑳界敤self鍙傛暟锛侊紒锛堝洜涓哄湪鍑芥暟鍐呴儴dfs鑷韩浠嶆湭瀹氫箟锛?		// startL涓哄垵濮嬬殑鐗岀殑绛夌骇锛宺emain涓鸿繕闇€瑕佺殑鍓墝鐨勫紶鏁帮紝path涓烘殏瀛樼粨鏋?		auto dfs=[&](auto& self,int startL,int remain,vector<Card> path){
            if(remain==0){
                res.push_back(path);
                return;
            }
            for(Level i=startL;i<=MAX_LEVEL;i++){
				// 濡傛灉i涓嶅湪ex锛堝嵆涓荤墝锛変腑
                if(ex.count(i)==0&&counts[i]>=type){
                    vector<Card> nextPath=path;
                    for(int j=0;j<type;j++)nextPath.push_back(cardsByLevel[i][j]);
                    self(self,i+1,remain-1,nextPath);
                }
            }
        };
        dfs(dfs,0,need,curCombo);
        for(auto& aRes:res)addPlay(aRes);	// -----浣跨敤&鍙紭鍖栨€ц兘锛?----
	};

	// 瀹氬悜鏋氫妇
	//// 鐐稿脊+鐏
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

	//// 鍗曞紶
	auto getSingle=[&](){
		for(Level i=0;i<=MAX_LEVEL;i++){
			if(counts[i]>=1)addPlay(getCards(i,1));
		}
	};

	//// 瀵瑰瓙
	auto getPair=[&](){
		for(Level i=0;i<level_joker;i++){
			if(counts[i]>=2)addPlay(getCards(i,2));
		}
	};

	//// 涓?甯︿竴/瀵?	//// with:0=甯﹂浂锛?=甯︿竴锛?=甯︿竴瀵?	auto getTriplet=[&](int with){
		for(Level i=0;i<level_joker;i++){
			if(counts[i]>=3){
				vector<Card> body=getCards(i,3);
				set<Level> ex={i};	//excluded-鍗冲甫鐗屼腑涓嶅簲鍑虹幇鐨勭墝锛屼篃灏辨槸涓荤墝鐨刲evel
				if (with==0) addPlay(body);
                if (with==1) selectAttachment(body, 1, 1, ex);
                if (with==2) selectAttachment(body, 1, 2, ex);
			}
		}
	};

	//// 鈥滆繛缁簭鍒椻€濆瀷锛泃ype锛?=鍗曢『锛?=鍙岄『锛?=椋炴満锛?=鑸ぉ椋炴満锛屽悓鏃秚ype涔熶唬琛ㄦ墍灞炵墝鍨嬬殑涓荤墝涓悇涓猚ard鐨勫紶鏁?	//// 鍥涚绫诲瀷鐨勪富鐗岄兘涓嶈兘鏈?(level<=MAX_STRAIGHT_LEVEL)
	//// minLen銆乵axLen鍒嗗埆鏍囪瘑瀵瑰簲鐗屽瀷鐨勪富鐗宭evel闀垮害闄愬埗
	auto getStraightAndPlane=[&](int minLen,int maxLen,int type){
		for(int l=minLen;l<=maxLen;l++){
			// 婊戝姩绐楀彛閬嶅巻锛宻tart鏍囪瘑绐楀彛璧风偣
			for(Level start=0;start<=MAX_STRAIGHT_LEVEL-l+1;start++){
				bool valid=true;
				for(int k=0;k<l;k++){
					// 鍒ゆ柇璧风偣鏄惁鏈夋晥
					if(counts[start+k]<type){
						valid=false;
						break;
					}
				}
				
				if(valid){
					vector<Card> body;
					set<Level> ex;
					// 濡傛灉杩炵画l涓偣鏁扮殑寮犳暟閮借揪鏍囷紝鍒欏垎鍒皟鐢╣etCards鎻愬彇type寮犵墝锛屽姞鍏ody鏁扮粍涓?					for(int k=0;k<l;k++){
						vector<Card> cards=getCards(start+k,type);
						body.insert(body.end(),cards.begin(),cards.end());
						ex.insert(start+k);
					}

					// 鍗曢『銆佸弻椤?					if(type==1||type==2){
						addPlay(body);
					}

					// 椋炴満
					if(type==3){
						addPlay(body);
						selectAttachment(body,l,1,ex);
						selectAttachment(body,l,2,ex);
					}

					// 鑸ぉ椋炴満
					if(type==4){
						addPlay(body);
						selectAttachment(body,l*2,1,ex);
						selectAttachment(body,l*2,2,ex);
					}
				}
			}
		}
	};

	//// 鍥涘甫浜?	auto getQuadruple=[&]{
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

	// 璋冪敤lambda鏉ユ灇涓?	getBombAndRocket();
	
	if(lastCombo.comboType==CardComboType::PASS){
		getSingle();
		getPair();
		getTriplet(0);
		getTriplet(1);
		getTriplet(2);
		getStraightAndPlane(5,12,1);	// 鍗曢『锛屼富鐗岀鏁颁粙浜?锝?2
		getStraightAndPlane(3,10,2);	// 鍙岄『锛屼富鐗岀殑绉嶆暟浠嬩簬3锝?0锛堜竴涓帺瀹舵渶澶氬彧鑳芥湁20寮犵墝锛屼篃灏辨槸鍦颁富锛?		getStraightAndPlane(2,6,3);		// 椋炴満锛屼富鐗岀殑绉嶆暟浠嬩簬2锝?
		getStraightAndPlane(2,5,4);		// 鑸ぉ椋炴満
		getQuadruple();
	}else{
		switch(lastCombo.comboType){
			case CardComboType::SINGLE:		getSingle();break;

			case CardComboType::PAIR:		getPair();break;

			case CardComboType::TRIPLET:	getTriplet(0);break;
			case CardComboType::TRIPLET1:	getTriplet(1);break;
			case CardComboType::TRIPLET2:	getTriplet(2);break;

			// 椤哄瓙绫荤殑锛岄暱搴﹀拰涓婂涓€鏍?            case CardComboType::STRAIGHT:    getStraightAndPlane(lastCombo.packs.size(), lastCombo.packs.size(), 1); break;
            case CardComboType::STRAIGHT2:   getStraightAndPlane(lastCombo.packs.size(), lastCombo.packs.size(), 2); break;
            
            case CardComboType::PLANE:
            case CardComboType::PLANE1:
            case CardComboType::PLANE2: 
            case CardComboType::SSHUTTLE:
            case CardComboType::SSHUTTLE2:
            case CardComboType::SSHUTTLE4:{
				// 椋炴満闀垮害
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

// 濞佽儊淇℃伅
struct ResponseThreatInfo{
    bool canBeat=false;          // 鏄惁瀛樺湪鍚堟硶鍝嶅簲
    bool canWinNow=false;        // 鏄惁瀛樺湪涓€鎵嬬洿鎺ュ帇瀹?    bool canLeaveOneHand=false;  // 鏄惁瀛樺湪鍘嬪畬鍚庡彧鍓╀竴鎵?    int minRemainCards=100;      // 鎵€鏈夊悎娉曞搷搴斾腑锛屽帇瀹屽悗鏈€灏戝墿浣欑墝鏁?    int minRemainHands=100;      // 鎵€鏈夊悎娉曞搷搴斾腑锛屽帇瀹屽悗鏈€灏戣繕闇€鍑犳墜
    CardCombo bestResponse;      // 鏈€鍗遍櫓鐨勪竴鎵嬪搷搴?};

int getMinHandCount(vector<Card> &hand);

/*杩欎釜鎺ュ彛鍐呴儴鍙互澶嶇敤 enumAllValidPlays锛?涓嶈閲嶆柊鍐欑墝鍨嬪垽鏂紱
PASS 鍜?INVALID 涓嶇畻鏈夋晥鍝嶅簲锛?bestResponse 寤鸿閫?minRemainHands 鏈€灏忕殑鍝嶅簲锛屽鏋滃苟鍒楀彲浠ラ€?remainCards 鏇村皯鐨勶紱
requiredCombo == PASS 鏃跺彲浠ョ洿鎺ヨ繑鍥?canBeat = false锛屽洜涓鸿嚜鐢卞嚭鐗屼笉鏄€滃帇鐗屽▉鑳佸垎鏋愨€濈殑鍦烘櫙*/

ResponseThreatInfo analyzeResponseThreat(vector<Card>& hand,CardCombo& requiredCombo){
    ResponseThreatInfo info;
    vector<CardCombo> responses=enumAllValidPlays(hand, requiredCombo);

    for(CardCombo &response:responses){
        if(response.comboType==CardComboType::PASS||response.comboType==CardComboType::INVALID)
            continue;

        info.canBeat=true;
        vector<Card> handAfter=removeCardsFromHand(hand,response.cards);	// 鎵撳嚭褰撳墠鐗岀粍鍚庡墿浣欑殑鎵嬬墝
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
// 鍗曟枃浠堕鏋讹細鎷嗗垎灞?// ==================================================
#pragma region decompose
int getMinHandCount(vector<Card> &hand);
void searchDecompose(vector<Card> curHand,HandPlan& curPlan,vector<HandPlan>& res,int maxD);
// [鎴戜滑瑕佽嚜宸卞疄鐜扮殑鏍稿績鍑芥暟] 鎶婃墜鐗屾媶鎴愯嫢骞茬粍鍚堟硶鐗屽瀷锛屼緵绛栫暐灞傝瘎浼扳€滄渶灏戣繕瑕佸嚑鎵嬪嚭瀹屸€濄€?// 浣跨敤 MCTS锛岄€氳繃妯℃嫙鏉ラ€夊彇鏈€浼樼殑鑻ュ共缁勭墝 <- 杩欏彞璇濇槸缁欑瓥鐣ュ眰鐪嬬殑
// todo
// 杩斿洖鏈€浼樼殑鍓峵opK缁勬媶娉曪紝鐢╞eam search
vector<HandPlan> decomposeHand(vector<Card> &hand, int topK = 1){
	vector<HandPlan> allPlans;
	HandPlan ini;

	if(hand.empty()){
		allPlans.push_back(ini);
		return allPlans;
	}

	int dyLimit=getMinHandCount(hand)+3;	// 鎼滅储娣卞害鐢辨渶灏戞墜鏁板姞涓€涓€兼潵闄愬埗锛岃繖涓?鍚庣画鍙啀璋冩暣
	searchDecompose(hand,ini,allPlans,dyLimit);

	// 鎸夋墜鏁板崌搴忔帓搴廰llPlans
	// 鎵嬫暟鐩稿悓鏃朵紭鍏堜繚鐣欑偢寮广€佺伀绠瓑楂樹环鍊肩墝鍨嬶紙閫氳繃绱姞鏉冮噸浣淭ie-breaker锛?	std::sort(allPlans.begin(),allPlans.end(),[](HandPlan& a,HandPlan& b){
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

// 鍥炴函
// curHand鏄綋鍓嶄粛鏈尮閰嶇殑鐗岋紝curPlan鏄亶鍘嗚矾寰勶紝res淇濆瓨缁撴灉锛宮axDep鍓灊
void searchDecompose(vector<Card> curHand,HandPlan& curPlan,vector<HandPlan>& res,int maxD){
	if(curHand.empty()){
		res.push_back(curPlan);
		return;
	}
	if(curPlan.groups.size()>=maxD)return;

	CardCombo empty;	// PASS鐗屽瀷
	auto play=enumAllValidPlays(curHand,empty);	// 鎵€鏈夊彲鑳界殑鐗屽瀷

	// 鍏堝鐗岀粍鎺掑簭锛屽紶鏁版秷鑰楀緱瓒婂鐨勭墝缁勪紭鍏堢骇瓒婇珮
	std::sort(play.begin(),play.end(),[](CardCombo& a,CardCombo& b){
		// 濡傛灉鐗岀粍鐨勫紶鏁扮浉鍚岋紝杩炵墝浼樺厛锛堥『瀛愩€侀鏈恒€佽埅澶╅鏈猴級
		if(a.cards.size()==b.cards.size()){
			return a.comboType>b.comboType;
		}
		return a.cards.size()>b.cards.size();
	});

	int limit=5;	// 姣忎釜鑺傜偣鏈€澶氭帰绱㈡帓鍚嶅墠limit鐨勭粍娉?	int curBranch=0;// 褰撳墠鍒嗘敮鏁?	// 鍥炴函涓讳綋
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

// 浣跨敤鐘舵€佸帇缂ヾp鏉ヤ紭鍖栨€ц兘锛屾€讳綋鎬濊矾鏄洖婧硶+璐績
static std::unordered_map<uint64_t,int> memo;	// 鐢ㄤ簬缂撳瓨getMinHandCount鐨勭粨鏋滐紝瀹炵幇鍓灊
											 	// 閿細鏌愪竴鏃跺埢鎵嬬墝鐨勭墝鍨嬬姸鎬乻tate锛涘€硷細鍦ㄥ綋鍓嶅瓨妗ｏ紙閿級鐨勭姸鎬佷笅锛屽嚭瀹屾墍鏈夌墝鐨勬渶灏戞墜鏁?
// counts[]琛ㄧず褰撳墠姣忎竴绾х墝杩樻湁鍑犲紶
// wings琛ㄧず棰濆鐨勫甫鐗岀┖浣嶏細姣斿鎷垮嚭浜嗕竴涓鏈猴紙333444锛夛紝涓夋潯鍙互甯﹀崟寮犳垨瀵瑰瓙锛屾墍浠ラ鏈轰富骞茶鎷胯蛋鍚庣暀涓嬩簡2涓繀鑶€绌轰綅锛屽嵆wing=2锛?//// 褰撹繘鍏ュ埌浜嗛€掑綊鐨勫簳灞傦紙姝ゆ椂鍙墿涓嬫暎鐗岋級鏃讹紝灏卞彲浠ュ皢wing缁勬暎鐗岃杩涢鏈轰腑锛屼篃灏辫繘涓€姝ュ噺灏戜簡hands
int dfs(short counts[15],int wings){
	// 灏哻ounts杞寲涓哄崟涓€鐘舵€乻tate
	// 鍘嬬缉褰撳墠鍓╀綑鎵嬫暟鎯呭喌鍙婇澶栧甫鐗屽悕棰濓紝浣滀负state閿?	// 鐢╱int64_t鏄洜涓哄叾鎷疯礉绛夋搷浣滃揩浜巌nt,string绛?	uint64_t state=0;
	for(int i=0;i<15;i++){
		state=(state<<3)|counts[i];	// 灏唖tate宸︾Щ涓変綅锛岀劧鍚庢妸counts[i]琛ュ湪state鐨勪綆涓変綅涓?	}
	state=(state<<6)|(wings&0b111111);	// 鎶妛ings鍔犲湪state鏈叚浣嶏紝鍥犱负wing涔熸槸鎵嬬墝鎷嗗垎鐘舵€佺殑缁勬垚閮ㄥ垎
										// 杩欓噷鐨勶紙wings&0b111111锛夋槸涓轰簡鍙繚鐣檞ings鐨勬湯鍏綅锛岄槻姝ings鍊艰繃澶у鑷存薄鏌撳墠闈㈢殑counts鏁版嵁

	if(memo.count(state))return memo[state];	// 澶囧繕褰曚腑宸叉湁state鐘舵€侊紝鐩存帴杩斿洖

	// 缁熻鍥涖€佷笁銆佷簩銆佸崟鐗岀殑鍏蜂綋鏁扮洰锛宑[i]琛ㄧずi鐗屾湁c[i]缁?	// hands鍒濆鍖栦负鏈€绗ㄨ泲鐨勫嚭娉?	int c[5]={0};
	for(int i=0;i<15;i++)c[counts[i]]++;
	int hands=c[1]+c[2]+c[3]+c[4];	//

	// 绛涢€夌伀绠?	if(counts[level_JOKER]==1&&counts[level_joker]==1){
		hands-=1;
		c[1]-=2;
	}

	// 绛涢€夌繀鑶€
	int posWing=c[1]+c[2];	// 鍙兘鐨勭繀鑶€鏁?	int needWing=c[3]+c[4]*2+wings;	// 鎵嬬墝涓疄闄呴渶瑕佺殑缈呰唨鏁帮紝wings鏄粠涓婁竴灞傜户鎵夸笅鏉ョ殑锛屼篃瑕佸姞杩涙潵
	int matched=std::min(posWing,needWing);		// 瀹為檯鐢ㄥ埌鐨勭繀鑶€鏁帮紝涓轰簩鑰呰緝灏忚€?	hands-=matched;

	int ans=hands;
	// 鍗曢『锛屼笅闈㈢殑绛涢€夐兘浣跨敤婊戝姩绐楀彛
	for(int l=5;l<=12;l++){	// 鍒嗗埆绛涢€夐暱搴︿负5锝?2鐨勫崟椤?		for(int start=0;start<=MAX_STRAIGHT_LEVEL-l+1;start++){
			bool valid=true;
			for(int i=0;i<l;i++){	// l鏄獥鍙ｉ暱搴︼紝i閬嶅巻绐楀彛鏌ョ湅鏄惁鍙
				if(counts[start+i]==0){	// 椤哄瓙閲岀己鐗屼簡锛屼笉琛?										// 鐩存帴璺冲嚭鍐呭眰寰幆锛屽洖鍒皊tart鎵€鍦ㄥ惊鐜紝start++锛屼粠涓嬩竴绛夌骇鐨勭墝寮€濮嬮亶鍘?					valid=false;
					break;
				}
			}
			if(valid){
				for(int i=0;i<l;i++)
					// 鏈夊悎娉曠殑鍗曢『锛屽氨鎶婂崟椤轰腑鐨勭墝浠庢墜鐗岃鏁板櫒涓垹鎺夛紝鐒跺悗鎷跨潃鏂扮殑鎵嬬墝璁℃暟鍣ㄥ幓dfs
					counts[start+i]-=1;
				ans=std::min(ans,dfs(counts,wings)+1);
				for(int i=0;i<l;i++)
					// 鍥炴函锛岀户缁璼tart鐨勫惊鐜?					counts[start+i]+=1;
			}
		}
	}

	// 鍙岄『
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

	// 椋炴満
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
				// wings鏇存柊涓簑ings+l锛堟瘡缁勪笁鎺掑甫涓€涓繀鑶€锛?				ans=std::min(ans,dfs(counts,wings+l)+1);
				for(int i=0;i<l;i++)
					counts[start+i]+=3;
			}
		}
	}
	memo[state]=ans;
	return memo[state];
}

// [鎴戜滑瑕佽嚜宸卞疄鐜扮殑鏍稿績鍑芥暟] 蹇€熻繑鍥炲綋鍓嶆墜鐗屽嚭瀹屾渶灏戣繕闇€瑕佸嚑鎵嬶紝渚涜瘎浼板眰棰戠箒璋冪敤
// todo

int getMinHandCount(vector<Card> &hand){
	if(hand.empty())return 0;
	short counts[15]={0};
	for(Card c:hand)
		counts[card2level(c)]++;
	return dfs(counts,0);
}

// 蹇€熷垽鏂?hand 鏄惁瀛樺湪浠绘剰涓€鎵嬬墝鍙互鍘嬭繃 lastCombo
// 杩欎釜鍑芥暟鍙洖绛斺€滆兘涓嶈兘鍘嬧€濓紝涓嶉渶瑕佽繑鍥炲叿浣撳嚭鍝嚑寮犵墝
// 鐢╟ounts棰戠巼琛ㄥ姞蹇煡鎵?bool canBeatComboFast(vector<Card>& hand,CardCombo& lastCombo){
	if(lastCombo.comboType==CardComboType::PASS)return true;
	if(hand.empty()||lastCombo.comboType==CardComboType::ROCKET)return false;	// hand鍒ょ┖濂藉儚鏈夌偣娌″繀瑕侊紝浣嗚繕鏄啓涓€涓?
	short counts[15]={0};
	for(Card c:hand){
		counts[card2level(c)]++;
	}

	// 鏈夌伀绠紝蹇呰兘鍘嬭繃
	if(counts[level_JOKER]==1&&counts[level_joker]==1)return true;

	// 鏈夌偢寮?	//// 濡傛灉涓婂鏄偢寮?	if(lastCombo.comboType==CardComboType::BOMB){
		for(Level i=lastCombo.comboLevel;i<level_joker;i++){
			// 鎵嬬墝涓湁鑳藉帇杩囦笂瀹剁殑鐐稿脊
			if(counts[i]==4)return true;
		}
		// 娌℃湁
		return false;
	}
	//// 濡傛灉涓婂涓嶆槸鐐稿脊
	else{
		for(Level i=0;i<level_joker;i++){
			if(counts[i]==4)return true;
		}
	}

	// 甯歌鐨勫悓鐗屽瀷瀵规瘮
	//// 棣栧厛鑰冭檻鎵嬬墝鏁伴噺澶熶笉澶燂紝鎺ョ潃鍐嶈€冭檻鏄惁鏈夊搴旂墝鍨?& 鑳藉惁鍘嬪埗
	if(hand.size()<lastCombo.cards.size())return false;
	
	int seqL=lastCombo.findMaxSeq();	// 涓婂涓荤墝杩炵画浜嗗灏戠粍

	// 鍒ゆ柇鍓墝澶熶笉澶?	//// mStart锛氫富鐗岃捣鐐癸紱mLen锛氫富鐗岄暱搴︼紱need锛氶渶瑕佺殑鍓墝鏁帮紱wingType锛?-鍗曞紶锛?-瀵瑰瓙
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

	//// 寮€濮嬪悓鐗屽瀷姣旇緝
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
			int start=lastCombo.comboLevel-seqL+2;	// 浠庝笂瀹堕珮涓€绾у紑濮?			for(Level i=start;i<=MAX_STRAIGHT_LEVEL-seqL+1;i++){
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
// 鍗曟枃浠堕鏋讹細璇勪及灞?// ==================================================


//鏍规嵁涓€娆ASS浜嬩欢鐨勪笂涓嬫枃锛屽喅瀹氬畠浣滀负娌℃湁鍘嬪埗鐗岀殑璇佹嵁鏈夊寮?//瓒婂皬璇存槑璇佹嵁瓒婂己,1.0璇存槑鍑犱箮涓嶅仛绾︽潫
double getPassStrength(GameState &state,int player,CardCombo &requiredCombo,int requiredPlayer)
{
	//濡傛灉鏄嚜鐢辨淳锛孭ASS涓嶄細鍑虹幇
	if(requiredCombo.comboType==CardComboType::PASS || requiredPlayer<0)
		return 1.0;

	//濡傛灉鍜屼笂涓€涓嚭鐗岀殑浜哄悓涓€闃佃惀锛屽己搴﹁緝寮?	if(isSameSidePlayer(state,player) && isSameSidePlayer(state,requiredPlayer))
		return 0.9;

	//鎴戞槸鍦颁富
	if(state.isLandlord())
	{
		//蹇病鐗屼簡杩楶ASS锛屽ぇ姒傜巼鍘嬩笉浜?		if(state.cardRemaining[player]<=3)
			return 0.5;
		//鏅€氬眬闈?		return 0.7;
	}

	//鎴戞槸鍐滄皯
	if(requiredPlayer==state.landlordPosition)
	{
		//鍦颁富蹇窇瀹屼簡锛屽啘姘戝帇涓嶄簡
		if(state.cardRemaining[state.landlordPosition]<3)
			return 0.3;

		return 0.7;
	}
	//鎴戞槸鍐滄皯锛屽嚭鐗岀殑鍘嬬殑鏄垜鐨勭墝
	return 0.9; 
}
//璁板綍涓€娆″嚭鐗屼簨浠?void recordPlayEvent(GameState &state,int player,vector<Card> &playedCard)
{
	PlayEvent event;
	//璁板綍鏄皝鍦ㄨ鍔?鍑轰簡浠€涔堢墝
	event.player = player;
	event.cards = playedCard;

	//璁板綍褰撳墠鐜╁琛屽姩鍓嶆闈笂闇€瑕佸帇杩囩殑鐗屽強鏄皝鍑虹殑
	event.requiredCombo = state.lastValidCombo;
	event.requiredPlayer = state.lastValidPlayer;

	//鍒ゆ柇鏄惁PASS
	event.isPass = playedCard.empty();
	//璁板綍涔嬪墠鍑虹殑浠€涔堢墝
	if(event.isPass)
		event.combo = CardCombo();
	else
		event.combo = CardCombo(playedCard.begin(), playedCard.end());

	if(player != state.myPosition && event.isPass && event.requiredCombo.comboType != CardComboType::PASS && event.requiredPlayer >= 0)
	{
		//鍒涘缓涓€鏉ASS绾︽潫
		PassConstraint constraint;
		//璁板綍璋丳ASS
		constraint.player = player;
		constraint.requirCombo = event.requiredCombo;
		constraint.requirPlayer = event.requiredPlayer;
		constraint.strength = getPassStrength(state, player, event.requiredCombo, event.requiredPlayer);

		//璁板綍绾︽潫
		state.passConstraints.push_back(constraint);
	}
	//鎸夋椂闂撮『搴忎繚瀛樹簨浠?	state.playEvents.push_back(event);
}

//鏍规嵁褰撳墠宸茬粡鎭㈠鍑烘潵鐨勫眬闈紝閲嶅缓宸茬煡鐗屽拰鏈煡鐗屽尯
void RebuildCard(GameState &state)
{
	//鍏堥粯璁?4寮犵墝閮藉睘浜庢湭鐭ョ墝
	for (Card i = 0; i < 54;++i)
		state.cardUnknown[i] = true;

	//娓呯┖姣忎釜鐜╁鐨勭‘瀹氭寔鐗岃褰?	for (int i = 0; i < PLAYER_COUNT;++i)
		state.konwCardOfPlayer[i].clear();

	//鎴戠殑鎵嬬墝鏄‘瀹氫俊鎭?	for(Card i:state.myCards)
		state.cardUnknown[i] = false;

	//宸茬粡鎵撳嚭鍘荤殑鐗屼篃鏄‘瀹氫俊鎭?	for (Card i = 0; i < 54;++i)
	{
		if(state.cardPlayed[i])
			state.cardUnknown[i] = false;
	}

	//鍦颁富鏄庣墝
	if(state.landlordPosition >=0&&state.landlordPosition!=state.myPosition)
	{
		for(Card i:state.publicCards)
		{
			if(!state.cardPlayed[i])
			{
				//濡傛灉鍦颁富娌℃墦鍑猴紝杩欏紶鐗屽睘浜庡湴涓?				state.konwCardOfPlayer[state.landlordPosition].push_back(i);
				//绉诲嚭鏈煡鐗屽尯
				state.cardUnknown[i] = false;
			}
		}
	}
}

//鏀堕泦褰撳墠鎵€鏈夋湭鐭ョ墝
vector<Card> collectUnknowCard(GameState &state)
{
	//淇濆瓨鎵€鏈変换鐒舵湭鐭ョ殑鍏蜂綋鐗?	vector<Card> unknownCard;

	for (Card i = 0; i < 54;++i)
	{
		if(state.cardUnknown[i])
			unknownCard.push_back(i);
	}

	sortCards(unknownCard);

	return unknownCard;
}

// 妫€鏌ユ湭鐭ョ墝鏁伴噺鏄惁鍜岀帺瀹跺墿浣欐墜鐗屾暟涓€鑷?bool checkUnknownCard(GameState &state)
{
	//鏀堕泦鎵€鏈夋湭鐭ョ墝
	vector<Card> unknownCard = collectUnknowCard(state);

	//缁熻鍏朵粬鐜╁杩樺墿澶氬皯寮犵墝鏄棤娉曠‘瀹氱殑
	int expectedUnknownCount = 0;

	for (int i = 0; i < PLAYER_COUNT;++i)
	{
		//鎴戠殑鎵嬬墝宸茬煡锛屼笉灞炰簬鏈煡鐗?		if(i==state.myPosition)
		continue;
		
		//杩欎釜鐜╁鍓╀綑鐗屼腑鐨勬槑鐗?		int knownCount = state.konwCardOfPlayer[i].size();

		//鍓╀笅閭ｉ儴鍒嗘墠鏄湭鐭ョ墝
		expectedUnknownCount += state.cardRemaining[i] - knownCount;
	}
	//濡傛灉鏁伴噺涓€鑷达紝璇存槑鍚堢悊
	return unknownCard.size() == expectedUnknownCount;
}

//鏋勯€犱竴娆￠殢鏈鸿ˉ鍏ㄧ殑瀹屾暣灞€闈?InferredDeal bulidOneRandomDeal(GameState &state)
{
	//鍒涘缓涓€涓牱鏈?	InferredDeal deal;

	//鎴戠殑鎵嬬墝鏄‘瀹氱殑锛岀洿鎺ュ鍒惰繘鍘?	deal.hands[state.myPosition] = state.myCards;

	//鍏朵粬鐜╁鐨勭‘瀹氭墜鐗屼篃鏀捐繘鍘?	//鍦颁富鏈墦鍑虹殑搴曠墝
	for (int i = 0; i < PLAYER_COUNT;++i)
	{
		if(i==state.myPosition)
			continue;
		deal.hands[i] = state.konwCardOfPlayer[i];
	}

	//鏀堕泦鎵€鏈夋湭鐭ョ墝
	vector<Card> unknownCard = collectUnknowCard(state);

	//闅忔満鎵撲贡鏈煡鐗?	std::random_shuffle(unknownCard.begin(), unknownCard.end());

	// 璁＄畻鐞嗚涓婇渶瑕佸垎閰嶅嚭鍘荤殑鏈煡鐗屾暟閲忋€?	int totalNeedCount = 0;

	// 閬嶅巻鍏朵粬涓や釜鐜╁锛岀粺璁′粬浠繕缂哄灏戝紶鏈煡鐗屻€?	for (int player = 0; player < PLAYER_COUNT; ++player)
	{
    	// 鎴戠殑鎵嬬墝宸茬粡纭畾锛屼笉闇€瑕佷粠鏈煡鐗屼腑琛ャ€?    	if (player == state.myPosition)
        	continue;

    	// 杩欎釜鐜╁闇€瑕佽ˉ鐨勬湭鐭ョ墝鏁伴噺 = 鍓╀綑鐗屾暟 - 宸茬煡纭畾鐗屾暟銆?    	totalNeedCount += state.cardRemaining[player] - static_cast<int>(deal.hands[player].size());
	}

	// 濡傛灉鏈煡鐗屾暟閲忓拰闇€瑕佽ˉ鐨勬暟閲忎笉涓€鑷达紝璇存槑鐘舵€佹仮澶嶆垨鏈煡鐗岄噸寤烘湁闂銆?	if (totalNeedCount != unknownCard.size())
	{
    	// 杩斿洖涓€涓┖鏉冮噸鏍锋湰锛岃〃绀鸿繖娆¤ˉ鍏ㄤ笉鍙敤銆?    	deal.weight = 0.0;
    	return deal;
	}

	//褰撳墠宸茬粡鍙戝埌unknownCards 鐨勫摢涓綅缃?	int cnt = 0;

	//缁欏叾浠栫帺瀹惰ˉ瓒虫墜鐗?	for (int i = 0; i < PLAYER_COUNT;++i)
	{
		if(i==state.myPosition)
		continue;

		//杩欎釜鐜╁瑕佽ˉ澶氬皯寮?		int needCount = state.cardRemaining[i] - deal.hands[i].size();
		//浠庢湭鐭ョ墝涓彇缁欒繖涓帺瀹?		for (int j = 0; j < needCount;++j)
		{
			deal.hands[i].push_back(unknownCard[cnt]);
			++cnt;
		}

		// 姣忎釜鐜╁鎵嬬墝鎺掑簭
    	sortCards(deal.hands[i]);
	}
	//鏍锋湰鏉冮噸璁句负1
	deal.weight = 1.0;

	return deal;
}

// 妫€鏌ヤ竴娆￠殢鏈鸿ˉ鍏ㄦ牱鏈殑鎵嬬墝寮犳暟鏄惁姝ｇ‘銆?bool checkInferredDeal(GameState &state, InferredDeal &deal)
{
    // 濡傛灉鏍锋湰鏈韩宸茬粡鏍囪涓烘棤鏁堬紝鐩存帴杩斿洖 false銆?    if (deal.weight <= 0.0)
        return false;

    // 閬嶅巻涓変釜鐜╁銆?    for (int player = 0; player < PLAYER_COUNT; ++player)
    {
        // 姣忎釜鐜╁鏍锋湰鎵嬬墝鏁伴噺蹇呴』绛変簬褰撳墠灞€闈㈣褰曠殑鍓╀綑鐗屾暟銆?        if (static_cast<int>(deal.hands[player].size()) != state.cardRemaining[player])
            return false;
    }

    // 鎵€鏈夌帺瀹跺紶鏁伴兘瀵癸紝璇存槑杩欎釜鏍锋湰鍦ㄦ暟閲忓眰闈㈠悎娉曘€?    return true;
}

// 妫€鏌ヤ竴娆￠殢鏈鸿ˉ鍏ㄦ牱鏈腑鏄惁瀛樺湪閲嶅鐗屻€?bool checkInferredDealNoDuplicate(InferredDeal &deal)
{
    // 鏍囪姣忓紶鐗屾槸鍚﹀凡缁忓湪鏍锋湰鎵嬬墝涓嚭鐜拌繃銆?    bool seen[54] = {};

    // 閬嶅巻涓変釜鐜╁銆?    for (int player = 0; player < PLAYER_COUNT; ++player)
    {
        // 閬嶅巻杩欎釜鐜╁鏍锋湰鎵嬬墝閲岀殑姣忓紶鐗屻€?        for (Card card : deal.hands[player])
        {
            // 濡傛灉杩欏紶鐗屼箣鍓嶅凡缁忓嚭鐜拌繃锛岃鏄庨噸澶嶄簡銆?            if (seen[card])
                return false;

            // 鏍囪杩欏紶鐗屽凡缁忓嚭鐜般€?            seen[card] = true;
        }
    }

    // 娌℃湁鍙戠幇閲嶅鐗岋紝璇存槑鏍锋湰鍦ㄥ敮涓€鎬у眰闈㈠悎娉曘€?    return true;
}

//鍒ゆ柇鍦ㄦ煇涓殢鏈鸿ˉ鍏ㄦ牱鏈腑锛屾寚瀹氱帺瀹舵槸鍚︽湁鑳藉姏鍘嬭繃 requiredCombo
bool canPlayerBeatInDeal(InferredDeal &deal,int player,CardCombo &requiredCombo)
{
	if(requiredCombo.comboType ==CardComboType::PASS)
		return false;

	//鏋氫妇杩欎釜鐜╁鍦ㄦ牱鏈墜鐗屼腑鎵€鏈夎兘鍑虹殑鍚堟硶鍝嶅簲
	vector<CardCombo> vaildPlays = enumAllValidPlays(deal.hands[player], requiredCombo);

	for(CardCombo &play:vaildPlays)
	{
		if(play.comboType !=CardComboType::PASS && play.comboType !=CardComboType::INVALID)
			return true;
	}

	return false;
}

//鏍规嵁鍘嗗彶PASS绾︽潫锛岃瘎浼颁竴涓殢鏈鸿ˉ鍏ㄦ牱鏈殑鍙俊搴?//濡傛灉鏌愪釜鐜╁鍘嗗彶涓?PASS 浜嗭紝浣嗚繖涓牱鏈噷浠栧叾瀹炶兘鍘嬭繃褰撴椂閭ｆ墜鐗岋紝灏遍檷浣庤繖涓牱鏈殑鏉冮噸
double evaluateDealByPass(GameState &state,InferredDeal &deal)
{
	//鍒濆鏉冮噸涓?锛岃〃绀哄畬鍏ㄥ彲淇?	double weight = 1.0;

	//閬嶅巻鎵€鏈変粠PASS涓彁鍙栧嚭鏉ョ殑绾︽潫
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

//鎵归噺鐢熸垚鑻ュ共涓殢鏈鸿ˉ鍏ㄦ牱鏈?//sampleCount 琛ㄧず甯屾湜鐢熸垚澶氬皯绉嶅彲鑳藉眬闈?vector<InferredDeal> buildRandomDeals(GameState &state, int sampleCount)
{
	//淇濆瓨鎵€鏈夋湁鏁堟牱鏈?	vector<InferredDeal> deals;

	for (int i = 0; i < sampleCount;++i)
	{
		//鐢熸垚涓€涓殢鏈鸿ˉ鍏ㄦ牱鏈?		InferredDeal deal = bulidOneRandomDeal(state);

		//濡傛灉鏍锋湰鏉冮噸涓?锛岃鏄庡け璐ワ紝璺宠繃
		if(deal.weight<=0.0)
		continue;

		//鏍规嵁鍘嗗彶PASS绾︽潫淇
		deal.weight *= evaluateDealByPass(state, deal);

		if(deal.weight<=0.0)
		continue;

		//濡傛灉鏍锋湰鎵嬬墝寮犳暟涓嶅锛屾帓闄?		if(!checkInferredDeal(state,deal))
		continue;

		//濡傛灉鏈夐噸澶嶇墝锛屾帓闄?		if(!checkInferredDealNoDuplicate(deal))
			continue;

		//淇濆瓨鏈夋晥鏍锋湰
		deals.push_back(deal);
	}
	return deals;
}

// 璋冭瘯鐢細妫€鏌ラ殢鏈鸿ˉ鍏ㄦā鍧楁槸鍚﹁兘鐢熸垚鍚堟硶鏍锋湰銆?// 娉ㄦ剰锛氳繖涓嚱鏁颁笉搴旇鍦?Botzone 姝ｅ紡杈撳嚭鍓嶆墦鍗板唴瀹癸紝鍚﹀垯浼氭薄鏌?JSON 杈撳嚭銆?void debugRandomDeals(GameState &state)
{
    // 鍏堟鏌ョ‘瀹氭湭鐭ョ墝鏁伴噺鏄惁涓€鑷淬€?    bool unknownOk = checkUnknownCard(state);

    // 鐢熸垚 20 涓殢鏈鸿ˉ鍏ㄦ牱鏈€?    vector<InferredDeal> deals = buildRandomDeals(state, 20);

    // 杈撳嚭璋冭瘯淇℃伅鍒?cerr锛屼笉褰卞搷姝ｅ父 JSON 杈撳嚭銆?    std::cerr << "[debug] unknownOk=" << unknownOk
              << " sampleCount=" << deals.size()
              << " unknownCards=" << collectUnknowCard(state).size()
              << std::endl;

    // 鏈€澶氭墦鍗板墠涓変釜鏍锋湰鐨勪笁涓帺瀹舵墜鐗屾暟閲忥紝閬垮厤杈撳嚭澶銆?    for (int i = 0; i < static_cast<int>(deals.size()) && i < 3; ++i)
    {
        std::cerr << "[debug] deal " << i
                  << " p0=" << deals[i].hands[0].size()
                  << " p1=" << deals[i].hands[1].size()
                  << " p2=" << deals[i].hands[2].size()
                  << std::endl;
    }
}
// 璋冭瘯鐢細妫€鏌?PASS 绾︽潫鏄惁姝ｇ‘鐢熸垚锛屽苟瑙傚療闅忔満鏍锋湰鏉冮噸銆?// 娉ㄦ剰锛氭寮?Botzone 杈撳嚭鍓嶄笉瑕佽皟鐢紝閬垮厤璋冭瘯淇℃伅骞叉壈銆?void debugPassConstraints(GameState &state)
{
    // 杈撳嚭鍘嗗彶浜嬩欢鏁伴噺鍜?PASS 绾︽潫鏁伴噺銆?    std::cerr << "[debug-pass] events=" << state.playEvents.size()
              << " constraints=" << state.passConstraints.size()
              << std::endl;

    // 鐢熸垚涓€鎵规牱鏈紝瑙傚療鏈夊灏戞牱鏈 PASS 绾︽潫闄嶆潈銆?    vector<InferredDeal> deals = buildRandomDeals(state, 50);

    int penalizedCount = 0;
    double minWeight = deals.empty() ? 0.0 : deals[0].weight;
    double maxWeight = deals.empty() ? 0.0 : deals[0].weight;
    double totalWeight = 0.0;

    for (InferredDeal &deal : deals)
    {
        // 鏉冮噸灏忎簬 1锛岃鏄庤嚦灏戣Е鍙戣繃涓€娆?PASS 闄嶆潈銆?        if (deal.weight < 1.0)
            ++penalizedCount;

        if (deal.weight < minWeight)
            minWeight = deal.weight;

        if (deal.weight > maxWeight)
            maxWeight = deal.weight;

        totalWeight += deal.weight;
    }

    // 鎵撳嵃鍓嶅嚑鏉?PASS 绾︽潫锛岄伩鍏嶈緭鍑哄お澶氥€?    for (int i = 0; i < static_cast<int>(state.passConstraints.size()) && i < 8; ++i)
    {
        PassConstraint &constraint = state.passConstraints[i];
        int matchedDeals = 0;

        // 缁熻杩欐潯绾︽潫鍦ㄥ灏戜釜鏍锋湰涓瑙﹀彂銆?        // 濡傛灉鏍锋湰涓鐜╁鑳藉帇杩囧綋鏃堕偅鎵嬬墝锛屽嵈鍘嗗彶涓婇€夋嫨 PASS锛岃繖涓牱鏈氨浼氳闄嶆潈銆?        for (InferredDeal &deal : deals)
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


//鍒ゆ柇鎸囧畾鐜╁鏄惁鍜屾垜灞炰簬鍚屼竴闃佃惀
bool isSameSidePlayer(GameState &state, int palyer)
{
	//-1琛ㄧず褰撳墠娌℃湁鍑虹墝鑰?	if(palyer<0)
		return false;

	//鎴戞槸鍦颁富
	if(state.isLandlord())
		return palyer == state.landlordPosition;

	//鎴戞槸鍐滄皯
	return palyer != state.landlordPosition;
}

//鍒ゆ柇鏌愪竴鎵嬪€欓€夌墝鏄惁鑳界洿鎺ユ妸鑷繁鐨勬墜鐗屽嚭瀹?鏈€楂樹紭鍏堢骇瑙勫垯锛氬鏋滆兘鐩存帴璧紝閫氬父涓嶉渶瑕佸啀璇勫垎
bool isWinningPlay(GameState &state,CardCombo &play)
{
	//PASS涓嶈兘璧?	if(play.comboType == CardComboType::PASS)
		return false;

	//闈炴硶鐗屼笉鑳借耽
	if(play.comboType==CardComboType::INVALID)
		return false;

	//濡傛灉杩欎竴鎵嬪嚭鐗屾暟绛変簬鎴戝綋鍓嶆墜鐗屾暟锛岃鏄庢垜鑳界洿鎺ュ嚭瀹?	return play.cards.size() == state.myCards.size();
}

//鍒ゆ柇褰撳墠鏄惁瀛樺湪蹇呴』鎶㈢墝鏉冪殑鍗遍櫓灞€闈?bool isDangerousSituation(GameState &state)
{
	//鎴戞槸鍦颁富
	if(state.isLandlord())
	{
		//鎵惧埌鎵€鏈夊啘鍚?		for (int i = 0; i < PLAYER_COUNT;i++)
		{
			if(i==state.landlordPosition)
				continue;

			//浠绘剰涓€涓啘姘戝彧鍓?-2寮犵墝锛屽氨绠楀嵄闄?			if(state.cardRemaining[i]<=2)
				return true;
		}
		return false;
	}

	//鎴戞槸鍐滄皯
	return state.cardRemaining[state.landlordPosition] <= 2;
}

// 鍒ゆ柇涓€鎵嬬墝鏄惁鏄‖鎺х墝銆?// 鐐稿脊鍜岀伀绠彲浠ュ帇澶у鏁扮墝锛屼絾浼氬鑷村簳鍒嗙炕鍊嶏紝涔熶細娑堣€楀叧閿帶鍒惰祫婧愩€?bool isHardControlPlay(CardCombo &play)
{
    // 鐐稿脊鏄‖鎺х墝銆?    if (play.comboType == CardComboType::BOMB)
        return true;

    // 鐏鏄渶楂樼‖鎺х墝銆?    if (play.comboType == CardComboType::ROCKET)
        return true;

    // 鍏朵粬鐗屽瀷涓嶇畻纭帶鐗屻€?    return false;
}

//鍒ゆ柇鏄惁搴旇涓诲姩浜夊ず鐗屾潈
bool shouldFightControl(GameState &state)
{
	//浼拌鎴戝綋鍓嶆墜鐗岃繕闇€瑕佸嚑鎵?	int myHandCount = getMinHandCount(state.myCards);

	//濡傛灉鎴戝綋鍓嶆墜鐗岃繕闇€瑕佸嚑鎵嬪嚭瀹?	if(myHandCount<=3)
		return true;

	//鍦颁富鎵嬬墝杈冮『锛屼富鍔ㄦ帶灞€
	if(state.isLandlord() && myHandCount<=5)
		return true;
	
		//鍐滄皯鐪嬪湴涓?		if(!state.isLandlord() && state.cardRemaining[state.landlordPosition]<=5)
			return true;

		return false;
}

// [鎴戜滑瑕佽嚜宸卞疄鐜扮殑鏍稿績鍑芥暟] 璇勪及鏁存墜鐗屽己搴︼紝涓昏鐢ㄤ簬鍙垎鍐崇瓥鍜屽悗缁弬鏁拌皟浼樸€?//濡傛灉鍙湅鎴戣嚜宸辫繖鎵嬬墝锛屼笉鐪嬪綋鍓嶆闈㈠姩浣滐紝杩欐墜鐗屽埌搴曞己涓嶅己
double evaluateHandStrength(vector<Card> &hand)
{
//===涓烘墜鐗岃瘎鍒嗭細鎬诲垎=澶х墝鍒?鐐稿脊鍒?缁撴瀯鍒?纰庣墝鎯╃綒-鎵嬬墝鎯╃綒===
	double score = 0.0;
	auto grouped = groupCardsByLevel(hand);

	// 澶х墝鍒嗭細鎴戞墜閲屾湁娌℃湁鎶㈢墝鏉冪殑鑳藉姏
	//涓虹瓑绾ц祴鍒嗭紝璁板綍鎴愪竴涓暟缁?	static const double highCardBonus[MAX_LEVEL] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 3~Q
		0.8,						  // K
		1.5,						  // A
		2.5,						  // 2
		3.5,						  //灏忛
		4.0};						  //澶ч

	for (Level i = 0; i < MAX_LEVEL;i++)
	{
		//grouped[i] 杩斿洖鐨勬槸鎵嬬墝琚媶鍒嗘垚涓嶅悓鐨勬暟缁勪箣鍚庝笉鍚岀瓑绾х殑鐗屾暟銆俫rouped[10] = 2 琛ㄧずK鏈?寮?		score += highCardBonus[i] * grouped[i].size();
	}
	// 鐐稿脊鍒嗭細鎴戞湁娌℃湁纭帇鑳藉姏
	//姣忎釜鐐稿脊 +6
	for (Level i = 0; i < level_joker;i++)
	{
		if(grouped[i].size()==4)
			score += 6.0;
	}

	//鐏鍦ㄥぇ鐗屽垎鐨勫熀纭€涓?+4
	if(!grouped[level_joker].empty() && !grouped[level_JOKER].empty())
		score += 4.0;

	// 缁撴瀯鍒嗭細杩欏壇鐗屾暣涓嶆暣榻愶紝瀹规槗涓嶅鏄撶粍缁囨垚楂樿川閲忓嚭鐗?	//涓夋潯+2.0锛屽瀛?1.0
	for (Level i = 0; i < level_joker;i++)
	{
		size_t cnt = grouped[i].size();
		if(cnt==3)
			score += 2.0;
		else if(cnt == 2)
			score += 1.0;
	}

	//杩炵画鐗屽瀷娼滃姏锛氳皟鐢?decomposeHand 鎺ュ彛 锛岃繖涓?鏄寚"杩斿洖鍓?K 绉嶆渶浼樻媶娉?
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
	// 纰庣墝鎯╃綒锛氳繖鎵嬬墝鏄笉鏄お纰庯紝鍚庨潰寰堥毦澶勭悊
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
	// 鎵嬫暟鎯╃綒锛氳繖鍓墝鎬讳綋杩樿鍑哄灏戞墜鎵嶈兘鎵撳畬
	// 渚濊禆 getMinHandCount
	score -= 1.2 * getMinHandCount(hand);

	return score;
}

// [鎴戜滑瑕佽嚜宸卞疄鐜扮殑鏍稿績鍑芥暟] 璇勪及鏌愪竴鎵嬪€欓€夊嚭鐗屽灞€闈㈢殑鏀剁泭锛屼緵鍑虹墝绛栫暐姣旇緝澶氫釜閫夐」銆?//鐪嬧€滃嚭杩欎竴鎵嬩箣鍚庘€濆眬闈㈡湁娌℃湁鍙樺ソ
//杈撳叆鐨勬槸 鍑虹墝鍓嶆墜鐗岋紝浣犳墦绠楀嚭鐨勮繖涓€鎵嬶紝褰撳墠灞€闈?double evaluatePlayGain(vector<Card> &handBefore, CardCombo &play, GameState &state)
{
	//PASS 涓嶄細鏀瑰杽鑷繁鐨勬墜鐗岋紝鏀剁泭涓?
	if(play.comboType == CardComboType::PASS)
		return 0.0;

	//璁＄畻鍑虹墝鍚庣殑鍓╀綑鎵嬬墝
	vector<Card> handAfter = removeCardsFromHand(handBefore, play.cards);

	//濡傛灉杩欎竴鎵嬬洿鎺ュ嚭瀹岋紝缁欐瀬楂樺垎
	if(handAfter.empty())
		return 10000.0;

	//鍑虹墝鍓嶄及璁¤繕闇€瑕佸嚑鎵?	int beforeCount = getMinHandCount(handBefore);

	//鍑虹墝鍚庝及璁¤繕闇€鍑犳墜
	int afterCount = getMinHandCount(handAfter);

	//浠?寮€濮嬬疮璁℃敹鐩婂垎
	double gain = 0.0;

	// 鏈€鏍稿績鎸囨爣锛氳繖涓€鎵嬫槸鍚﹀噺灏戜簡鍓╀綑鎵嬫暟
    // 鍑忓皯 1 鎵嬫瘮鍗曠函澶氬嚭鍑犲紶鐗屾洿閲嶈
	gain += (beforeCount - afterCount) * 5.0;

	//娆¤鎸囨爣锛氫竴娆℃墦鍑烘洿澶氱墝閫氬父闇€瑕佹洿鎺ヨ繎鑳滃埄
	gain += (play.cards.size()) * 0.3;
	// 鏅€氬眬闈笅锛岀偢寮瑰拰鐏鏄弽璐垫帶鍒惰祫婧愶紝鍏堟墸鍒嗕繚瀹堜娇鐢ㄣ€?    if (play.comboType == CardComboType::BOMB)
        gain -= 6.0;

    if (play.comboType == CardComboType::ROCKET)
        gain -= 8.0;

    // 褰撳墠濡傛灉鏄嵄闄╁眬闈紝瀵规柟蹇嚭瀹屼簡锛岀‖鎺х墝鎯╃綒闄嶄綆銆?    // 娉ㄦ剰锛氳繖閲屼笉鏄紦鍔变贡鐐革紝鑰屾槸閬垮厤鍗遍櫓鏃朵粛鐒惰繃搴︿繚瀹堛€?    if (isDangerousSituation(state) && isHardControlPlay(play))
        gain += 4.0;

    return gain;
}

// ==================================================
// 鍗曟枃浠堕鏋讹細绛栫暐灞?// ==================================================

// [鎴戜滑瑕佽嚜宸卞疄鐜扮殑鏍稿績鍑芥暟] 鏍规嵁鎵嬬墝寮哄害鍜屽墠搴忓彨鍒嗙粨鏋滃喅瀹氭湰杞槸鍚﹀彨鍒嗐€佸彨鍑犲垎銆?//杩欎釜鍑芥暟涓嶆槸鑷繁鐬庣畻鎵€鏈変笢瑗匡紝瀹冨簲璇ュ缓绔嬪湪 evaluateHandStrength 涔嬩笂
int decideBid(vector<Card> &hand, vector<int> &bidHistory)
{
	//鎵惧埌鍘嗗彶鍙垎涓殑鏈€楂樺垎
	int maxBid = bidHistory.empty() ? 0 : *std ::max_element(bidHistory.begin(), bidHistory.end());
	//鏈€楂樺垎澶т簬3锛岀洿鎺ヨ烦杩?	if(maxBid>=3)	return 0;

	//===蹇€熺粺璁″叧閿墝===
	// 鎸夌墝闈㈢瓑绾у垎缁勶紝grouped[12] 琛ㄧず鎵€鏈?2锛実rouped[13] 琛ㄧず灏忕帇
	auto grouped = groupCardsByLevel(hand);
	// 鍒ゆ柇鏄惁鏈夌伀绠?	bool hasRocket = !grouped[level_joker].empty() && !grouped[level_JOKER].empty();
	//缁熻鐐稿脊鏁伴噺
	int bombCount = 0;
	for (Level i = 0; i < level_joker;++i)
	{
		if(grouped[i].size() == 4)
			++bombCount;
	}
	// 缁熻 2 鐨勬暟閲?	int twoCount = grouped[12].size();
	// 缁熻 A 鐨勬暟閲?	int aceCount = grouped[11].size();
	// 缁熻鐜嬬殑鏁伴噺
	int jokerCount = grouped[level_joker].size() + grouped[level_JOKER].size();
	//缁熻楂樺瀛愮殑鏁伴噺
	int highPairCount = 0;
	if(grouped[10].size()>=2)
		++highPairCount;
	if(grouped[11].size()>=2)
		++highPairCount;
	if(grouped[12].size()>=2)
		++highPairCount;
	

	// 鎴戣繖鎵嬬墝鏈€澶氭効鎰忓彨鍒板嚑鍒?	int targetBid = 0;

	// 绗竴灞傦細纭潯浠舵帶鍒跺彨鍒嗕笂闄愩€傚厛榛樿鏈€澶氬彲浠ュ彨 3锛屽悗闈㈠啀閫愭鏀剁揣銆?	int bidCap = 3;

	//鏄惁鍏锋湁鍩虹鎺х墝鑳藉姏
	bool hasBasicControl = hasRocket || bombCount > 0 || jokerCount > 0 || twoCount > 0 || aceCount >= 2;

	//鏄惁鍏锋湁寮烘帶鐗岃兘鍔?	bool hasStrongControl = hasRocket || bombCount > 0 || twoCount >= 2 || (jokerCount >= 1 && twoCount >= 1) || (twoCount >= 1 && aceCount >= 2);

	if(!hasBasicControl && bidCap>1)
		bidCap = 1;
	if(!hasStrongControl&&bidCap>2)
		bidCap = 2;

	//璁＄畻褰撳墠鎵嬬墝鑷冲皯澶ф瑕佸垎鍑犳墜鍑哄畬锛屾墜鏁拌秺澶氾紝褰撳湴涓婚闄╄秺澶с€?	int handCount = getMinHandCount(hand);
	//鍒ゆ柇鐗屽瀷鏄惁鍙互鎺ュ彈
	bool handShape = handCount <= 11;

	//濡傛灉鍓嶉潰宸茬粡鏈変汉鍙埌2锛岄闄╁彉澶?	bool mustBidThree = (maxBid == 2);
	
	//鍙?鐨勫己鐗岄棬妲涳紝蹇呴』鏈夐潪甯告槑纭殑纭帶鐗岃兘鍔?	bool hasThreePointControl = hasRocket || bombCount >= 1 || twoCount >= 3 || (jokerCount >= 1 && twoCount >= 2);

	//濡傛灉鎵嬫暟闈炲父澶氾紝璇存槑鐗屽緢纰庯紝鍗充娇鏈変竴瀹氭墦鐗岋紝涔熶笉閫傚悎鍙湴涓?	if(handCount>=14 && !hasThreePointControl &&bidCap>1)
		bidCap = 1;
	//濡傛灉鎵嬫暟鍋忓锛屼笉瑕佸啋闄╁彨3
	else if(handCount>=12 && !hasThreePointControl &&bidCap>2)
		bidCap = 2;


	//濡傛灉鏈変汉鍙?锛岃€屾垜娌℃湁鍙?鐨勬潯浠讹紝灏卞己鍒朵笉鍏佽鍙埌3
	if(mustBidThree&&!hasThreePointControl&&bidCap>2)
		bidCap = 2;

	
	// 浣庢暎鐗屾暟閲忥細缁熻 3~9 涓彧鏈変竴寮犵殑鐗岋紝杩欎簺鐗屽綋鍦颁富鏃跺緢闅句富鍔ㄥ鐞嗐€?

	// 绗簩灞傦細缁嗙矑搴﹁瘎鍒嗐€傚厛鐩存帴澶嶇敤璇勪及灞傦紝涓嶅湪绛栫暐灞傞噸澶嶇畻鏁存墜鐗屽己搴︺€?	double bidScore = evaluateHandStrength(hand);

	//鐏瀵瑰彨鍦颁富浠峰€煎緢楂橈紝棰濆鍔犲垎
	if(hasRocket)
		bidScore += 3.0;

	// 姣忎釜鐐稿脊閮借兘寮鸿鎷垮洖鐗屾潈锛岄澶栧姞鍒?	bidScore += bombCount * 2.5;

	// 2鏄彨鍦颁富鏃舵渶鍏抽敭鐨勫父瑙勬帶鍒剁墝锛屾寜鏁伴噺鍔犲垎
	bidScore += twoCount * 0.8;

	// 鐜嬩篃鏄潪甯稿己鐨勫崟鐗屾帶鍒惰祫婧愶紝鎸夋暟閲忓姞鍒?	bidScore += jokerCount * 1.0;

	// 鍓嶉潰鏈€楂樺彨鍒嗚秺楂橈紝缁х画鍔犲彨鐨勯闄╄秺澶с€?	if (maxBid == 1)
    	bidScore -= 0.3;

		// 濡傛灉宸茬粡鏈変汉鍙埌 2锛屾垜瑕佽耽鍙垎鍙兘鍙?3锛屾墍浠ラ渶瑕佹洿璋ㄦ厧銆?	else if (maxBid == 2)
    	bidScore -= 2.0;

	// A 鐨勬帶鍒跺姏寮变簬 2 鍜岀帇锛屼絾涓€瀵逛互涓婁粛鐒舵湁浠峰€笺€?	if (aceCount >= 2)
    	bidScore += 0.8;

	//鍙湪鐗屽瀷鍙互鎺ュ彈鐨勬儏鍐典笅婵€杩?	if(handShape)
	{
		//涓。鎺у埗鍔犲垎
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

	// 纭潯浠堕檺鍒舵渶缁堝彨鍒嗕笂闄?	if (targetBid > bidCap)
    	targetBid = bidCap;

	//濡傛灉涓嶅鏈€楂樺垎锛屼笉鍙?	if(targetBid<=maxBid)
		return 0;

	return targetBid;
}

//甯﹀垎鏁扮殑鍊欓€夊嚭鐗?struct ScoredPlay
{
	//鍊欓€夊嚭鐗屾湰韬?	CardCombo play;
	//杩欐墜鐗屽綋鍓嶇殑鍚彂寮忚瘎鍒?	double score = 0;
	//鍦ㄩ殢鏈鸿ˉ鍏ㄦ牱鏈腑鐨勫钩鍧囪瘎浼板垎
	double sampleScore = 0;
};


//璇勪及涓€鎵嬪€欓€夌墝鍦ㄥ綋鍓嶇湡瀹炲眬闈笅鐨勫惎鍙戝紡鍒嗘暟
double evaluatePlayHScore(GameState &state ,CardCombo &play)
{
	// 鍒ゆ柇褰撳墠鏄笉鏄嚜鐢卞嚭鐗?	bool freeTurn = state.lastValidCombo.comboType == CardComboType::PASS;

	// 鍒ゆ柇褰撳墠鏄笉鏄嵄闄╁眬闈?	bool dangerous = isDangerousSituation(state);

	// 鍒ゆ柇褰撳墠闇€瑕佸帇鐨勭墝鏄笉鏄槦鍙嬪嚭鐨?	bool followingTeammate = !freeTurn && state.lastValidPlayer >= 0 && isSameSidePlayer(state, state.lastValidPlayer);

	// 鍒ゆ柇褰撳墠闇€瑕佸帇鐨勭墝鏄笉鏄鎵嬪嚭鐨?	bool followingOpponent = !freeTurn && state.lastValidPlayer >= 0 && !isSameSidePlayer(state, state.lastValidPlayer);

	// 鍒ゆ柇褰撳墠鏄惁鏄湪鍘嬪嵄闄╁鎵?	bool followingDangerousOpponent = followingOpponent && state.cardRemaining[state.lastValidPlayer] <= 2;

	// 鍒ゆ柇褰撳墠鏄惁搴旇涓诲姩浜夊ず鐗屾潈
	bool fightForControl = shouldFightControl(state);

	//澶嶇敤宸叉湁鐨勫熀纭€鏀剁泭璇勫垎,鍙湅鍑虹墝鍚庯紝鎴戠殑鎵嬬墝鏈夋病鏈夊彉濂?	double score = evaluatePlayGain(state.myCards, play, state);

	//鍘嬮槦鍙嬬殑鎯呭喌
	if(followingTeammate)
	{
		if(play.comboType == CardComboType::PASS)
			//璁╅槦鍙嬬户缁帶鐗?			score += 1.0;
		else
		{
			//鏅€氬眬闈㈠帇闃熷弸鎵ｅ垎
			score -= 1.0;
			
			//涓嶅簲璇ョ敤鐐稿脊
			if(isHardControlPlay(play) && !dangerous)
				score -= 10.0;

			//鐢?2 鎴栫帇鍘嬮槦鍙嬩篃姣旇緝浜忥紝鏅€氬眬闈㈣鎵ｅ垎
			for(Card card:play.cards)
			{
				Level level = card2level(card);
				if(level>=12 && !dangerous)
					score -= 2.0;
			}
		}
	}

	// 濡傛灉褰撳墠瑕佸帇鐨勬槸鍗遍櫓瀵规墜鍑虹殑鐗岋紝PASS 椋庨櫓鏋侀珮銆?		if (followingDangerousOpponent)
		{
    		// 涓嶅帇鍗遍櫓瀵规墜锛屽彲鑳界洿鎺ヨ瀵规柟缁х画璧板畬锛岄噸缃氥€?    		if (play.comboType == CardComboType::PASS)
    		{
        		score -= 25.0;
    		}
    		else
    		{
        		// 鎰挎剰鍑虹墝鍘嬪埗鍗遍櫓瀵规墜锛岀粰鏄庢樉濂栧姳銆?        		score += 8.0;

        		// 鍗遍櫓瀵规墜蹇窇鏃讹紝鐐稿脊鍜岀伀绠彲浠ユ帴鍙椼€?        		if (isHardControlPlay(play))
            		score += 6.0;
    		}
		}	

		//澶勭悊杩囩墝
		if(play.comboType == CardComboType::PASS)
		{
			//濡傛灉鍘嬮槦鍙嬶紝閰嶅悎缁欏鍔?			if(followingTeammate)
				score += 1.5;
			else if(followingOpponent)
			{
				//濡傛灉瑕佷富鍔ㄥ嚭鍑伙紝PASS浠ｄ环鏇撮珮
				if(fightForControl)
					score -= 5.0;
				else
					score -= 2.0;
			}
		}

		//纭帶鐗?		if(isHardControlPlay(play))
		{
			//濡傛灉涓嶆槸鍦ㄥ帇鍗遍櫓瀵规墜锛屾墸鍒?			if(!followingDangerousOpponent)
				score -= 6.0;
			
			//濡傛灉鍦ㄥ帇鍗遍櫓瀵规墜锛屽姞鍒?			else
				score += 2.0;
		}

		//鏅€氬眬闈笅淇濇姢2鍜岀帇
		if(!followingDangerousOpponent && play.comboType!=CardComboType::PASS)
		{
			//寮虹墝鏉冩椂锛屾儵缃氫綆
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

		//鍘嬪鎵?		if (followingOpponent && play.comboType != CardComboType::PASS)
		{
    		// 鏅€氬鎵嬶細灏忓箙濂栧姳锛岄紦鍔卞悎鐞嗗帇鍒躲€?    		score += 1.0;

			if(fightForControl)
				score += 3.0;
		}

		//鑷敱鍑虹墝
		// 鑷敱鍑虹墝鏃讹紝鎴戞嫢鏈変富鍔ㄦ潈锛屽簲浼樺厛鎵撳嚭鑳芥暣鐞嗘墜鐗岀殑缁勫悎銆?		if (freeTurn && play.comboType != CardComboType::PASS)
		{
    		// 澶氬紶鐗岀粍鍚堥€氬父姣斿崟寮犳洿鑳芥帹杩涘嚭瀹岃繘搴︺€?    		if (play.cards.size() >= 5)
        		score += 2.0;

			//鑷敱鍑虹墝鏃讹紝鍚岀墝鍨嬫洿鍊惧悜鍏堣蛋浣庣墝
			//杩欐牱鍙互閬垮厤 333 鍜?AAA 鍚彂寮忔墦骞冲悗锛岃鏍锋湰鍒嗘帹鍘诲厛鍑?AAA
			if(play.comboType == CardComboType::SINGLE ||
			   play.comboType == CardComboType::PAIR ||
			   play.comboType == CardComboType::TRIPLET)
			{
				score -= play.comboLevel * 0.08;
			}

    		// 涓夊甫銆侀『瀛愩€佽繛瀵广€侀鏈虹瓑缁勫悎鐗屼紭鍏堢骇鏇撮珮銆?    		if (play.comboType == CardComboType::STRAIGHT ||
        		play.comboType == CardComboType::STRAIGHT2 ||
        		play.comboType == CardComboType::TRIPLET1 ||
        		play.comboType == CardComboType::TRIPLET2 ||
        		play.comboType == CardComboType::PLANE ||
        		play.comboType == CardComboType::PLANE1 ||
        		play.comboType == CardComboType::PLANE2)
    		{
        		score += 2.0;
    		}

    		// 鑷敱鍑虹墝鏃朵笉榧撳姳鍏堝嚭鐐稿脊鎴栫伀绠?    		if (isHardControlPlay(play))
        		score -= 8.0;
		}

		// 鍙墿涓€鎵嬬墝
		if (play.comboType != CardComboType::PASS)
		{
    		// 璁＄畻鍑虹墝鍚庣殑鍓╀綑鎵嬬墝銆?    		vector<Card> handAfter = removeCardsFromHand(state.myCards, play.cards);

    		// 濡傛灉鍓╀綑鎵嬬墝鍙渶瑕佷竴鎵嬪嚭瀹岋紝缁欒緝楂樺鍔便€?    		if (!handAfter.empty() && getMinHandCount(handAfter) == 1)
        		score += 8.0;

			if(fightForControl)
			{
				int before = getMinHandCount(state.myCards);
				int after = getMinHandCount(handAfter);

				//濡傛灉杩欎竴鎵嬪噺灏戯紝閭ｄ箞寰堟湁蹇呰
				if(before>after)
					score += 3.0 * (before - after);
			}
		}


	return score;
}

//鍦ㄤ竴涓殢鏈鸿ˉ鍏ㄦ牱鏈腑锛岃瘎浼版垜鎵撳嚭Play涔嬪悗鐨勫眬闈㈡敹鐩?double evaluatePlayInDeal(GameState &state,InferredDeal &deal,CardCombo &play)
{
	//PASS鏃犳敹鐩?	if(play.comboType == CardComboType::PASS)
		return 0;

	//鍙栧嚭鎴戠殑鎵嬬墝
	vector<Card> myHandBefore = deal.hands[state.myPosition];

	//鍋囪鎵撳嚭play锛屽嚭鐗屽悗鐨勬墜鐗?	vector<Card> myHandAfter = removeCardsFromHand(myHandBefore, play.cards);

	//濡傛灉鍑哄畬锛岀洿鎺ュ嚭
	if(myHandAfter.empty())
		return 10000;
	
	//姣旇緝鎵嬫暟
	int beforeCount = getMinHandCount(myHandBefore);
	int afterCount = getMinHandCount(myHandAfter);

	//鏍锋湰鏀剁泭
	double score = 0;

	//鍑忓皯鍑烘墜鏁帮紝鍔犲垎
	score += (beforeCount - afterCount) * 5.0;
	//鍑哄緱澶氾紝鎺ヨ繎鍑哄畬
	score += play.cards.size() * 0.3;

	//===鎺ュ叆闅忔満琛ュ叏
	int nextPlayer = (state.myPosition + 1) % PLAYER_COUNT;
	bool nextPlaySameSide = isSameSidePlayer(state, nextPlayer);

	if(canPlayerBeatInDeal(deal,nextPlayer,play))
	{
		//涓嬪鏄鎵?		if(!nextPlaySameSide)
		{
			//鏅€氭儏鍐?			score -= 4.0;

			//濡傛灉鍙墿3寮犵墝锛岄闄╁ぇ
			if(state.cardRemaining[nextPlayer]<=3)
				score -= 8.0;
		}
		//涓嬪鏄槦鍙?		else
		{
			score -= 1.0;
		}
	}
	else
	{
		//涓嬪鍘嬩笉浜嗘垜
		score += 2.0;
	}

	return score;
}

//鐢ㄤ竴鎵归殢鏈鸿ˉ鍏ㄦ牱鏈紝璇勪及鏌愪竴鍊欓€夌墝鐨勫钩鍧囧眬闈环鍊?double evaluatePlayBySamples(GameState &state,vector<InferredDeal> &deals,CardCombo &play)
{
	if(deals.empty())
		return 0;

	//鍔犳潈鎬诲垎
	double totalScore = 0;
	//鏍锋湰鎬绘潈閲?	double totalWeight = 0;

	for(InferredDeal &deal:deals)
	{
		if(deal.weight<=0)
		continue;

		//鐢ㄦ牱鏈瘎浼板€欓€夌墝
		double oneScore = evaluatePlayInDeal(state, deal, play);

		//鎸夋牱鏈彲淇″害鍔犳潈
		totalScore += oneScore * deal.weight;
		//绱鏍锋湰鍔犳潈
		totalWeight += deal.weight;
	}
	if(totalWeight<=0)
		return 0;

	return totalScore / totalWeight;
}

//浠庢墍鏈夊悎娉曞嚭鐗屼腑閫夊嚭鍚彂寮忚瘎鍒嗘渶楂樼殑鍓?topK涓€欓€?vector<ScoredPlay> selectTopPlays(GameState &state,vector<CardCombo> &validPlays,int topK)
{
	//淇濆瓨鎵€鏈夊甫鍒嗘暟鐨勫€欓€?	vector<ScoredPlay> scoredPlays;

	//鍒ゆ柇褰撳墠鏄笉鏄嚜鐢卞嚭鐗?	bool freeTurn = state.lastValidCombo.comboType == CardComboType::PASS;

	for(CardCombo &play : validPlays)
	{
		if(freeTurn && play.comboType ==CardComboType::PASS)
			continue;

		//鍒涘缓涓€鏉″甫鍒嗘暟鐨勫€欓€?		ScoredPlay scored;
		scored.play = play;
		//璁＄畻杩欐墜鐗岀殑鍚彂寮忚瘎鍒?		scored.score = evaluatePlayHScore(state, play);

		//鏀惧叆鍊欓€?		scoredPlays.push_back(scored);
	}

	//鎸夊垎鏁版帓搴?	std::sort(scoredPlays.begin(), scoredPlays.end(),
			  [](ScoredPlay &a, ScoredPlay &b)
			  {
				  return a.score > b.score;
			  });
	
	//鍙繚鐣欏墠topK涓?	if(topK>0 && scoredPlays.size()>topK)
		scoredPlays.resize(topK);

	return scoredPlays;
}

// 璋冭瘯鐢細杈撳嚭 TopK 鍊欓€夌殑鍚彂寮忓垎銆佹牱鏈垎鍜屾渶缁堣瀺鍚堝垎銆?// 娉ㄦ剰锛氳繖涓嚱鏁板彧鍐?cerr锛屼笉搴旇鍦?Botzone 姝ｅ紡杈撳嚭鍓嶉粯璁よ皟鐢ㄣ€?void debugTopPlays(GameState &state,vector<CardCombo> &validPlays)
{
	// 鍏堢敤鍜?decidePlay 涓€鑷寸殑鏂瑰紡閫夊嚭 TopK銆?	vector<ScoredPlay> topPlays = selectTopPlays(state, validPlays, 6);

	// 鍙敓鎴愪竴娆￠殢鏈鸿ˉ鍏ㄦ牱鏈紝淇濊瘉姣忎釜鍊欓€変娇鐢ㄥ悓涓€鎵规牱鏈瘮杈冦€?	vector<InferredDeal> deals = buildRandomDeals(state, 20);

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

		// 璁＄畻鏍锋湰骞冲潎鍒嗐€?		scored.sampleScore = evaluatePlayBySamples(state, deals, scored.play);

		// 浣跨敤鍜?decidePlay 涓€鑷寸殑鏍锋湰鏉冮噸瑙勫垯銆?		double sampleWeight = 0.4;
		if(state.lastValidCombo.comboType == CardComboType::PASS)
			sampleWeight = 0.1;
		if(isHardControlPlay(scored.play) && !isDangerousSituation(state))
			sampleWeight = 0;

		// 铻嶅悎鎴愭渶缁堝垎銆?		double finalScore = scored.score + scored.sampleScore * sampleWeight;

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

// [鎴戜滑瑕佽嚜宸卞疄鐜扮殑鏍稿績鍑芥暟] 鍦ㄦ墍鏈夊悎娉曞嚭鐗屼腑閫夊嚭褰撳墠鏈€浼樼殑涓€鎵嬶紝鏄悗缁瓥鐣ュ崌绾х殑涓诲叆鍙ｃ€?//渚濊禆浜巈valuatePlayGain
CardCombo decidePlay(GameState &state, vector<CardCombo> &validPlays)
{
	//濡傛灉娌℃湁浠讳綍鍚堟硶鍊欓€夛紝杩斿洖PASS
	if(validPlays.empty())
		return CardCombo();

	
	//===璇勫垎绯荤粺
	//鍒濆鍖栦负PASS
	CardCombo bestplay;
	//褰撳墠鏈€楂樺垎璁句负涓€涓緢灏忕殑鏁?	double bestScore = -1e18;

	//绗竴浼樺厛绾э細濡傛灉鑳界洿鎺ュ嚭瀹岀墝锛屽氨绔嬪埢鍑?	for(CardCombo &play : validPlays)
	{
		if(isWinningPlay(state,play))
			return play;
	}

	//鍏堢敤鍚彂寮忚瘎鍒嗛€夊嚭鍓嶅嚑涓€欓€夛紝鍚庣画 PIMC 鍙湪杩欎簺鍊欓€夐噷妯℃嫙
	vector<ScoredPlay> topPlays = selectTopPlays(state, validPlays, 6);

	//鍙敓鎴愪竴娆￠殢鏈鸿ˉ鍏ㄦ牱鏈?	vector<InferredDeal> deals = buildRandomDeals(state, 20);
	//涓烘瘡涓猅opK鍊欓€夎绠楁牱鏈钩鍧囧垎
	for(ScoredPlay &scored:topPlays)
	{
		scored.sampleScore = evaluatePlayBySamples(state, deals, scored.play);
	}


	//閫夊惎鍙戝紡璇勫垎鏈€楂樼殑涓€鎵?	for(ScoredPlay &scored:topPlays)
	{
		//璁惧畾闅忔満鏍锋湰鐨勬潈閲?		double sampleWeight = 0.4;
		if(state.lastValidCombo.comboType == CardComboType::PASS)
			sampleWeight = 0.1;
		if(isHardControlPlay(scored.play) && !isDangerousSituation(state))
			sampleWeight = 0;

		//鍚彂寮忚瘎鍒嗗拰鏍锋湰璇勫垎
		double finalScore = scored.score + scored.sampleScore * sampleWeight;
		if(finalScore > bestScore)
		{
			bestScore = finalScore;
			bestplay = scored.play;
		}
	}

	//濡傛灉鎵€鏈夊€欓€夐兘琚烦杩囷紝杩斿洖PASS
	if(bestScore==-1e18)
		return CardCombo();

	return bestplay;
}

// ==================================================
// 鍗曟枃浠堕鏋讹細涓诲叆鍙?// ==================================================

// [鎴戜滑瀹炵幇鐨勭▼搴忓叆鍙 璐熻矗涓茶仈鈥滆鐘舵€?-> 璋冪瓥鐣?-> 杈撳嚭缁撴灉鈥濓紝灏介噺涓嶆壙杞藉叿浣撲笟鍔＄粏鑺傘€?int main()
{
	//鍒濆鍖栭殢鏈烘暟
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
