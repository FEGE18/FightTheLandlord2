# 斗地主 AI Bot 单文件架构设计说明

> 项目：Botzone 叫分斗地主（FightTheLandlord2）AI Bot  
> 团队：2 人协作  
> 语言：C++  
> 课程约束：只允许提交单个源文件，不拆分为多个 .h / .cpp  
> 平台约束：每步决策 ≤ 1 秒；每次决策都是重新启动一次程序  
> 推荐提交文件：test.cpp

---

## 一、这份文档解决什么问题

这份文档的目标不是讲斗地主规则本身，而是解决下面 4 个工程问题：

1. 在“只能单文件”的课程要求下，代码该怎么组织，才不会写成一团
2. 两个人合作时，怎么在同一个文件里分工，减少冲突
3. 内部接口怎么设计，后续从简单规则版升级到更强版本时不用推倒重来
4. 有哪些可选方案，各自的优缺点是什么，应该选哪一种

这份设计文档默认你们最终只提交一个源文件，也就是 test.cpp。

注意：这里说的“单文件”是指你们自己的业务代码不拆成多个 .cpp/.h。第三方库 jsoncpp 仍然按平台现有方式引用 jsoncpp/json.h，这不算你们主动拆分项目结构。

---

## 二、最重要的前提：Botzone 不是常驻程序

这个前提如果理解错，后面所有架构都会歪。

Botzone 的执行模型是：

```text
轮到你决策
  -> 平台重新启动你的程序
  -> 给你一个 JSON 输入（包含本局到当前为止的全部历史）
  -> 你从头恢复局面
  -> 输出一个决策
  -> 程序立即结束
```

### 2.1 这意味着什么

| 结论 | 说明 |
|------|------|
| 没有持久内存 | 不能指望“上一轮算好的东西”留在内存里 |
| 每轮都要重建局面 | 需要根据 requests 和 responses 重放整局历史 |
| 状态结构依然需要 | 不是为了跨轮保存，而是为了本轮内部做清晰决策 |
| 1 秒包含全部时间 | 包括读 JSON、恢复局面、策略计算、输出 |

所以，正确的程序流程永远是：

```cpp
int main() {
    // 1. 读取 JSON
    // 2. 判断当前是叫分还是出牌
    // 3. 如果是出牌，则根据全部历史恢复当前局面
    // 4. 做一次决策
    // 5. 输出并结束
}
```

这也是为什么我们虽然做“单文件”，但仍然必须有清晰的内部模块边界。

---

## 三、为什么课程要求单文件后，仍然不能“随手往下写”

很多同学听到“单文件”就会直接写成这种结构：

```cpp
int main() {
    // 读输入
    // 解析历史
    // 识别牌型
    // 叫分策略
    // 出牌策略
    // 各种 if-else
    // 再来一点临时变量
    // 最后输出
}
```

这种写法短期看很快，长期一定出问题：

1. 两个人没法分工，因为所有逻辑都混在 main() 里
2. 一改策略就可能把 IO 或牌型判断改坏
3. 没法做 mock，也没法局部测试
4. 后面想加搜索、记牌器优化、残局特判时，根本找不到合适插入点

所以正确思路不是“多文件” vs “单文件”，而是：

> 单文件提交，内部仍然按模块分层设计

也就是说：

- 物理上：只有一个文件 test.cpp
- 逻辑上：仍然分成 IO、基础类型、状态恢复、出牌枚举、手牌拆分、评估、策略、主入口这些区域

---

## 四、推荐的单文件总体架构

### 4.1 推荐方案：单文件 + 逻辑分层 + 区块顺序固定

推荐你们的 test.cpp 采用下面的顺序：

```text
test.cpp
├─ 1. 头文件 / using / 常量定义
├─ 2. 基础类型定义
│    ├─ Card / Level / Stage / CardComboType
│    ├─ CardCombo
│    └─ HandPlan / GameState
├─ 3. 基础工具函数
│    ├─ card2level
│    ├─ 排序与计数工具
│    └─ 小型辅助函数
├─ 4. IO 与局面恢复
│    ├─ readInput
│    ├─ buildGameState
│    ├─ outputBid
│    └─ outputPlay
├─ 5. 出牌枚举层
│    ├─ enumAllValidPlays
│    └─ selectAttachment（如果采用延后带牌方案）
├─ 6. 手牌拆分层
│    ├─ decomposeHand
│    └─ getMinHandCount
├─ 7. 评估层
│    ├─ evaluateHandStrength
│    └─ evaluatePlayGain
├─ 8. 策略层
│    ├─ decideBid
│    └─ decidePlay
└─ 9. main
```

### 4.2 为什么这样排

这个顺序的好处是“从底层到上层，单向依赖”：

| 区块 | 依赖谁 | 谁依赖它 |
|------|--------|----------|
| 常量 / 基础类型 | 无 | 所有人 |
| IO / 状态恢复 | 基础类型 | 策略、main |
| 枚举 / 拆分 / 评估 | 基础类型、状态 | 策略 |
| 策略 | 上面所有工具层 | main |
| main | 全部 | 无 |

这样做的直接好处：

1. 阅读时逻辑清晰，不需要跳文件也不需要在一个文件里来回乱翻
2. 两个人可以按“代码区块”分工，而不是按文件分工
3. 后期如果课程允许升级成多文件，只需要把这些区块直接切出去，几乎不需要重构

---

## 五、三种单文件组织方案对比

在课程要求“单文件”的前提下，内部组织仍然有几种不同风格。

### 方案 A：单文件 + 平铺函数式分层

就是所有函数都放在同一个 test.cpp，按区块顺序排列。

```cpp
struct GameState { ... };
struct HandPlan { ... };

GameState buildGameState(...);
vector<CardCombo> enumAllValidPlays(...);
vector<HandPlan> decomposeHand(...);
double evaluateHandStrength(...);
int decideBid(...);
CardCombo decidePlay(...);

int main() { ... }
```

#### 优点

- 最容易上手
- 和样例代码风格接近，迁移成本低
- 对课程作业最友好，老师最容易读懂
- 两个人只要提前约定区块边界，就能并行写

#### 缺点

- 全局命名空间会变大
- 如果不控制注释区块和命名规范，后期会显得长

#### 适用结论

最推荐。

---

### 方案 B：单文件 + 一个大类 class Bot

把所有状态和方法都放进一个类里：

```cpp
class Bot {
public:
    void run();

private:
    GameState state;
    int decideBid(...);
    CardCombo decidePlay(...);
    vector<CardCombo> enumAllValidPlays(...);
    ...
};
```

#### 优点

- 名字收敛到类作用域，不容易和全局函数打架
- 从“面向对象”的角度看比较完整

#### 缺点

- 对这个项目来说有点重
- 很多函数本质上是纯函数，却被塞进类里
- 两个人改同一个类时，冲突更频繁
- 容易把 state 到处当成员变量直接读写，副作用变多

#### 适用结论

可以做，但不如方案 A 直接。

---

### 方案 C：单文件 + 多个命名空间分层

例如：

```cpp
namespace Cards { ... }
namespace IO { ... }
namespace Enumerate { ... }
namespace Evaluate { ... }
namespace Strategy { ... }
```

#### 优点

- 逻辑边界清楚
- 单文件里也有“模块感”
- 比全局平铺更不容易命名冲突

#### 缺点

- 对初学者来说会稍显正式
- 写法比方案 A 多一层包装
- 如果大家对命名空间不熟，调试时反而不顺手

#### 适用结论

如果你们两个人都对 C++ 比较熟，这个方案也不错；如果想稳妥，还是选方案 A。

---

### 结论：推荐选择

> 推荐：方案 A，单文件 + 平铺函数式分层

原因很简单：

1. 最符合课程作业的审阅习惯
2. 最接近样例代码，改造成本最低
3. 两人协作时最容易按照“代码区块”分工
4. 后续如果想升级成更强策略，也能平滑扩展

---

## 六、推荐的单文件内部代码骨架

下面给出的是单文件版骨架，不是完整实现，但它定义了你们整个项目的内部接口边界。

```cpp
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cassert>
#include <cstring>
#include "jsoncpp/json.h"

using std::vector;
using std::set;

// ==================================================
// 1. 常量 / 枚举 / 基础类型
// ==================================================

using Card = short;
using Level = short;

enum class Stage { BIDDING, PLAYING };
enum class CardComboType { ... };

struct CardCombo { ... };
struct HandPlan { ... };
struct GameState { ... };

// ==================================================
// 2. 基础工具函数
// ==================================================

constexpr Level card2level(Card card);
bool isSequentialLevels(...);
vector<int> countLevels(const vector<Card>& hand);

// ==================================================
// 3. IO / 状态恢复
// ==================================================

GameState readGameState();
void outputBid(int value);
void outputPlay(const vector<Card>& cards);

// ==================================================
// 4. 枚举层
// ==================================================

vector<CardCombo> enumAllValidPlays(
    const vector<Card>& hand,
    const CardCombo& lastCombo
);

CardCombo selectAttachment(
    const vector<Card>& hand,
    const CardCombo& mainCombo,
    CardComboType targetType
);

// ==================================================
// 5. 拆分层
// ==================================================

vector<HandPlan> decomposeHand(
    const vector<Card>& hand,
    int topK = 1
);

int getMinHandCount(const vector<Card>& hand);

// ==================================================
// 6. 评估层
// ==================================================

double evaluateHandStrength(const vector<Card>& hand);

double evaluatePlayGain(
    const vector<Card>& handBefore,
    const CardCombo& play,
    const GameState& state
);

// ==================================================
// 7. 策略层
// ==================================================

int decideBid(
    const vector<Card>& hand,
    const vector<int>& bidHistory
);

CardCombo decidePlay(
    const GameState& state,
    const vector<CardCombo>& validPlays
);

// ==================================================
// 8. main
// ==================================================

int main() {
    GameState state = readGameState();
    if (state.stage == Stage::BIDDING) {
        outputBid(decideBid(state.myCards, state.bidHistory));
    } else {
        auto validPlays = enumAllValidPlays(state.myCards, state.lastValidCombo);
        CardCombo action = decidePlay(state, validPlays);
        outputPlay(action.cards);
    }
    return 0;
}
```

这个骨架的意义在于：

- 单文件要求满足了
- 内部模块边界保住了
- 两人可以先按接口并行，不需要先把实现都写完

---

## 七、核心数据结构设计

这部分是整个项目的“地基”。

### 7.1 牌的表示

继续使用样例的表示方式，不重新发明：

```cpp
using Card = short;   // 0~53，唯一标识一张牌
using Level = short;  // 0~14，表示等级：3~大王

constexpr Level card2level(Card card) {
    return card / 4 + card / 53;
}
```

为什么继续用这个设计：

1. 和规则文档完全一致
2. 和样例代码兼容，复用最稳
3. 一张牌和一个等级的概念被清楚区分开了

| 概念 | 表示什么 | 例子 |
|------|----------|------|
| Card | 具体某一张牌 | 红桃 3、黑桃 K |
| Level | 不区分花色的点数等级 | 所有 K 都是同一个 Level |

很多策略判断只关心 Level，很多输出又必须回到具体 Card，所以这两个层次都需要。

---

### 7.2 CardCombo

```cpp
struct CardCombo {
    vector<Card> cards;
    CardComboType comboType;
    Level comboLevel;

    CardCombo();

    template <typename CARD_ITERATOR>
    CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end);

    bool canBeBeatenBy(const CardCombo& b) const;
};
```

这个结构是整个项目里最值得复用样例代码的部分之一。

为什么：

1. 斗地主牌型判断细节很多，自己重写最容易出 bug
2. 18 种牌型 + 大小比较，样例已经帮你踩过坑了
3. 你们真正的竞争力不在“重新写一遍牌型识别”，而在策略层

建议：

- 牌型识别逻辑大体复用
- 不再使用样例里的 findFirstValid() 作为主策略
- 改成在此基础上自己写 enumAllValidPlays()

---

### 7.3 GameState

虽然程序每次都重新运行，但本轮内部仍然需要一个完整状态结构。

```cpp
struct GameState {
    Stage stage;

    int myPosition;
    int landlordPosition;
    int finalBid;

    vector<int> bidHistory;

    vector<Card> myCards;
    vector<Card> publicCards;

    CardCombo lastValidCombo;

    int cardRemaining[3];
    vector<vector<Card>> playHistory[3];

    bool cardPlayed[54];
    short levelRemaining[15];

    bool isLandlord() const;
    bool isTeammate(int pos) const;
    int getTeammatePos() const;
    int getUnknownCardCount() const;
};
```

为什么这些字段是必要的：

| 字段 | 用途 |
|------|------|
| myCards | 决策最基础的数据 |
| bidHistory | 叫分阶段要用 |
| lastValidCombo | 决定当前能否 PASS，和要压什么牌 |
| cardRemaining | 判断对手是否快跑完，残局很关键 |
| playHistory | 用来做记牌和行为判断 |
| cardPlayed / levelRemaining | 做记牌器和概率推断的基础 |

GameState 的意义不是跨轮保存，而是：

> 把“本轮从历史恢复出来的局面”集中放到一个结构里，避免策略函数依赖一堆散乱全局变量

---

### 7.4 HandPlan

```cpp
struct HandPlan {
    vector<CardCombo> groups;
    int handCount;
};
```

这个结构服务于手牌拆分。

例子：

手牌 `3 3 3 4 5 6 7 8 K K A A 2`

一种拆分可能是：

- `345678` 顺子
- `333` 三不带
- `KK` 对子
- `AA` 对子
- `2` 单张

那对应的 HandPlan：

```cpp
groups = { 顺子, 三不带, 对子, 对子, 单张 }
handCount = 5
```

为什么要单独有这个结构：

1. 策略层要看“我还剩几手”
2. 有时候不止一个最优拆法，后面容易扩展成返回 topK
3. 可以作为评估函数的输入和解释依据

---

## 八、单文件下的内部接口设计

这里是最核心的部分。虽然只有一个文件，但内部仍然应该像“项目接口说明书”一样清晰。

### 8.1 IO / 状态恢复接口

```cpp
GameState readGameState();
void outputBid(int value);
void outputPlay(const vector<Card>& cards);
```

#### 作用

- readGameState()：读取标准输入 JSON，重建当前局面
- outputBid()：输出叫分 JSON
- outputPlay()：输出出牌 JSON

#### 为什么这样设计

不要把 JSON 解析散落到策略函数里。策略层应该只看到 GameState，而不应该关心 Json::Value 的细节。

这叫数据格式和业务逻辑解耦。

#### 具体建议

这部分逻辑尽量直接复用规则文档里的样例 BotzoneIO::read()，只做一件事：

> 把原来写到全局变量里的内容，改成写进 GameState state

这样最稳。

---

### 8.2 出牌枚举接口

```cpp
vector<CardCombo> enumAllValidPlays(
    const vector<Card>& hand,
    const CardCombo& lastCombo
);
```

#### 作用

列出当前手牌在当前局面下的所有合法出牌。

#### 为什么必须有这个函数

样例的 findFirstValid() 只能回答：

> “随便找一个能打的牌”

但真正做策略时，需要回答的是：

> “我现在所有能出的牌有哪些，哪一种最值得出”

所以必须从“找一个”升级成“列全部”。

#### 输入输出含义

| 参数 | 含义 |
|------|------|
| hand | 当前手牌 |
| lastCombo | 需要压过的牌；如果是 PASS，表示当前自由出牌 |

#### 返回值约定

- 如果当前可以 PASS，返回值里应该包含一个 comboType == PASS 的 CardCombo
- 如果当前是自由出牌，也可以约定不返回 PASS，但这个约定需要全项目统一

建议：

> 有上家有效出牌时返回 PASS；自由出牌时不返回 PASS

这样语义更自然。

---

### 8.3 带牌选择接口

这是一个可选接口，看你们选哪种枚举方案。

```cpp
CardCombo selectAttachment(
    const vector<Card>& hand,
    const CardCombo& mainCombo,
    CardComboType targetType
);
```

#### 什么时候需要它

如果你们在枚举时采用“只枚举主体，带牌延后决定”的方案，就需要这个函数。

#### 举例

比如已经决定要出“三带一，主牌是 KKK”，那 selectAttachment() 决定带哪张：

- 带最小废牌
- 带不破坏顺子的牌
- 带不影响剩余手数的牌

#### 两种方案对比

| 方案 | 做法 | 优点 | 缺点 |
|------|------|------|------|
| 完全枚举 | enumAllValidPlays() 直接返回 KKK3、KKK4、KKK5... | 简单直观 | 候选爆炸 |
| 延后带牌 | enumAllValidPlays() 只返回“KKK 三带一主体” | 搜索空间小，策略更清楚 | 多一步决策 |

#### 推荐

> 推荐延后带牌，也就是保留 selectAttachment() 这个接口

因为后面不管你们做规则版还是搜索版，这个接口都很有用。

---

### 8.4 手牌拆分接口

```cpp
vector<HandPlan> decomposeHand(
    const vector<Card>& hand,
    int topK = 1
);

int getMinHandCount(const vector<Card>& hand);
```

#### 作用

- decomposeHand()：真正把手牌拆成若干组牌型
- getMinHandCount()：快速返回最少还要几手，不关心具体怎么拆

#### 为什么拆成两个接口

因为这两个需求虽然相关，但不完全一样。

| 场景 | 需要具体拆法吗 |
|------|----------------|
| 策略层想知道“我下一手该先出顺子还是对子” | 需要 |
| 评估层只想知道“出这手后剩余手数是 4 还是 5” | 不需要 |

如果只保留 decomposeHand()，那每次评估都要把完整方案算一遍，代价更高。

#### 为什么这是核心接口

斗地主策略里非常重要的概念是“手数”。

如果一手牌出出去之后，虽然牌少了，但剩下的牌更碎了，可能总体反而更差。

例如：

| 出牌前 | 最少手数 |
|--------|----------|
| 某手牌 | 4 |

| 候选出法 | 出牌后最少手数 | 评价 |
|----------|----------------|------|
| 出单张 5 | 4 | 表面少一张，实际没变好 |
| 出对子 K | 3 | 真正推进了整体出完进度 |

所以 getMinHandCount() 是一个会被反复调用的关键工具。

---

### 8.5 手牌评估接口

```cpp
double evaluateHandStrength(const vector<Card>& hand);

double evaluatePlayGain(
    const vector<Card>& handBefore,
    const CardCombo& play,
    const GameState& state
);
```

#### 作用

- evaluateHandStrength()：评估一手牌整体强弱，主要用于叫分
- evaluatePlayGain()：评估某个候选出牌对当前局面的改善程度，主要用于出牌选择

#### 为什么评估层不能和策略层混写

因为两者职责不同：

| 层 | 做什么 |
|----|--------|
| 评估层 | 给数字，衡量好坏 |
| 策略层 | 拿数字 + 结合局面，做最终决定 |

例子：

上家出一对 Q，我手里能出 KK 或 AA。

评估可能认为：

- 出 KK：+3.5
- 出 AA：+3.0

但如果地主只剩 2 张牌，策略层可能还是选 AA，因为它更稳、更保险。

所以：

> 评估是顾问，策略是拍板的人

把两者拆开，后面调参会轻松很多。

---

### 8.6 策略接口

```cpp
int decideBid(
    const vector<Card>& hand,
    const vector<int>& bidHistory
);

CardCombo decidePlay(
    const GameState& state,
    const vector<CardCombo>& validPlays
);
```

#### decideBid() 为什么只传这两个参数

叫分阶段还没有完整的出牌局面，不需要 GameState 里那一堆字段。

直接传：

- hand
- bidHistory

就足够了。

这样好处是接口更干净，也更方便单独测试。

#### decidePlay() 为什么传 GameState + validPlays

出牌决策需要同时看到：

1. 当前整体局面是什么
2. 自己合法能出的有哪些牌

如果把 validPlays 的生成塞进 decidePlay() 里，也不是不行，但会带来两个问题：

- 策略层职责变重
- 后面如果想在别的地方复用合法出牌枚举，不方便

所以我们保留这种两步结构：

```cpp
auto validPlays = enumAllValidPlays(...);
auto action = decidePlay(state, validPlays);
```

这也是最典型的“工具层 + 决策层”分工。

---

## 九、单文件下的整体调用链

把上面的接口串起来，整条调用链是这样的：

```text
main
  -> readGameState
  -> 判断阶段
      -> decideBid
      -> 或 enumAllValidPlays
             -> decidePlay
                    -> decomposeHand / getMinHandCount
                    -> evaluatePlayGain
                    -> selectAttachment（可选）
  -> outputBid / outputPlay
```

如果你们后面想加搜索，推荐加在策略层内部，而不是破坏这一整条链。

例如：

```text
decidePlay
  -> 先用规则层把候选缩小到 top 3
  -> 再对 top 3 做轻量模拟
  -> 选最优
```

这样原有接口完全不用推翻。

---

## 十、两人合作时，单文件怎么分工

这部分非常重要，因为课程要求单文件后，团队协作最大的风险不再是“不会写”，而是“互相覆盖”。

### 10.1 推荐分工方式：按区块分，不按行数分

不要说“你写前 500 行，我写后 500 行”。

正确分法是：

| 人员 | 负责区块 | 主要职责 |
|------|----------|----------|
| 开发者 A | 基础类型、IO、状态恢复、出牌枚举、手牌拆分、评估 | 把工具层搭好 |
| 开发者 B | 叫分策略、出牌策略、特殊局面规则、参数调优 | 把决策层搭好 |

也就是：

- A 负责“能算什么”
- B 负责“该怎么选”

### 10.2 单文件里的合作区块标记

建议你们在 test.cpp 里明确打区块注释：

```cpp
// ==================================================
// [A 区] 基础类型与 IO
// ==================================================

// ==================================================
// [A 区] 出牌枚举 / 拆分 / 评估
// ==================================================

// ==================================================
// [B 区] 叫分策略 / 出牌策略
// ==================================================
```

好处：

1. 谁负责哪里，一眼就清楚
2. 合并代码时更容易看冲突
3. 你们讨论 bug 时，不会连模块归属都说不清

### 10.3 先定接口，再并行实现

你们的第一步不是马上写全部逻辑，而是：

1. 先把所有 struct 和函数声明写出来
2. 每个函数先放一个 mock 实现
3. 让程序先能完整编译和跑通
4. 再分别替换真实实现

例如：

```cpp
int decideBid(const vector<Card>& hand, const vector<int>& bidHistory) {
    return 0;
}

CardCombo decidePlay(const GameState& state, const vector<CardCombo>& validPlays) {
    return validPlays.empty() ? CardCombo() : validPlays.front();
}
```

这个步骤很值钱，因为它保证：

> 你们协作时讨论的是“接口与职责”，而不是一上来就陷进实现细节

### 10.4 单文件协作时的合并纪律

建议严格遵守下面这 4 条：

1. 一个人不要随便改另一个人负责区块里的函数签名
2. 如果要改函数签名，先同步文档和接口表
3. 每天先合一次基础骨架，再各写各的
4. 尽量新增函数，不要随手把别人函数内部逻辑大改掉

这 4 条看起来朴素，但真能减少很多低级冲突。

---

## 十一、单文件下的三种策略路线选择

这里说的是策略路线，不是代码组织方式。即便都是单文件，策略层仍然有不同强度版本。

### 方案 1：纯规则版

#### 思路

- 叫分靠手牌评分
- 出牌靠固定优先级
- 跟牌时尽量用最小代价压
- 农民尽量不压队友

#### 优点

- 最稳
- 最快能做完
- 最容易在 1 秒内稳定运行

#### 缺点

- 上限一般
- 复杂局面不够聪明

#### 适合什么情况

- 时间紧
- 课程更看重结构清晰，而不是顶级胜率

---

### 方案 2：规则 + 轻量搜索

#### 思路

- 先用规则层筛出少量候选
- 再对候选做快速模拟或前瞻评分

#### 优点

- 比纯规则明显强
- 不需要一上来就做完整 MCTS
- 和当前接口兼容，升级成本低

#### 缺点

- 实现复杂度会上升
- 要小心 1 秒时限

#### 适合什么情况

- 你们先做完纯规则版后还有时间
- 想冲更好成绩

---

### 方案 3：完整搜索导向

#### 思路

做更系统的蒙特卡洛树搜索或更重的前瞻。

#### 优点

- 理论上上限最高

#### 缺点

- 不完全信息处理很复杂
- 开发风险大
- 在单文件里维护成本更高

#### 适合什么情况

- 你们非常熟搜索
- 剩余时间很多

---

### 推荐路线

> 课程作业推荐路线：先做方案 1，再预留升级到方案 2 的接口

也就是：

1. 当前先按纯规则版实现
2. 保留 enumAllValidPlays / decomposeHand / evaluatePlayGain 这些中间层
3. 以后真要加搜索，只改 decidePlay() 的内部逻辑，不推翻整体结构

---

## 十二、为什么这套接口设计适合单文件

这里把核心设计理由再系统总结一次。

### 12.1 为什么不把所有逻辑塞进 main()

因为 main() 只应该做流程编排，不应该做业务细节。

main() 最理想的样子就是：

```cpp
int main() {
    GameState state = readGameState();

    if (state.stage == Stage::BIDDING) {
        outputBid(decideBid(state.myCards, state.bidHistory));
    } else {
        auto validPlays = enumAllValidPlays(state.myCards, state.lastValidCombo);
        outputPlay(decidePlay(state, validPlays).cards);
    }
}
```

这种写法的价值是：

- 一眼看懂程序流程
- 策略和 IO 完全分开
- 以后换策略不动入口

### 12.2 为什么 GameState 还需要存在

虽然程序每轮都重启，但本轮内部依然要有统一的状态容器。

否则你们会得到一堆散乱变量：

```cpp
vector<Card> myCards;
int landlordPosition;
int myPosition;
int finalBid;
int cardRemaining[3];
bool cardPlayed[54];
...
```

然后每个函数参数都传一长串，或者到处访问全局变量，最后很难维护。

### 12.3 为什么要分枚举、拆分、评估、策略

因为这四件事本质不同：

| 层 | 核心问题 |
|----|----------|
| 枚举 | 我能出什么 |
| 拆分 | 我的牌怎样组织最好 |
| 评估 | 这个选择值不值 |
| 策略 | 我最终选哪一个 |

如果混在一起，最后一定是一大堆 if-else 无法维护。

### 12.4 为什么这套设计方便两人合作

因为它天然支持“工具层 / 决策层”分工：

- A 可以独立写 enumAllValidPlays、decomposeHand、evaluatePlayGain
- B 可以独立写 decideBid、decidePlay

两边通过函数签名对接，不需要互相等待太多。

---

## 十三、推荐的最小可行开发顺序

这里给一个非常现实的开发顺序，适合两人协作。

### 第一步：先把单文件骨架和接口写出来

目标：

- test.cpp 能编译
- 所有函数签名已经定下来
- main() 跑得通

这一步可以先全部用 mock：

```cpp
vector<CardCombo> enumAllValidPlays(...) { return {}; }
vector<HandPlan> decomposeHand(...) { return {}; }
double evaluateHandStrength(...) { return 0.0; }
int decideBid(...) { return 0; }
```

### 第二步：复用样例 IO 和 CardCombo

目标：

- 能正确识别叫分阶段 / 出牌阶段
- 能根据历史恢复当前手牌与局面
- 能完成一局，不崩溃

### 第三步：先做一个最弱但结构正确的策略版

例如：

- 叫分：基于大牌数量做简单评分
- 出牌：从合法牌里选最小能出的

这样你们先得到一个“结构对、功能通”的版本。

### 第四步：逐步替换核心能力

顺序建议：

1. enumAllValidPlays
2. decomposeHand
3. evaluateHandStrength
4. evaluatePlayGain
5. decidePlay

为什么这个顺序合理：

因为 decidePlay 依赖前面几个工具层，工具层没搭好，策略层只能瞎猜。

---

## 十四、最终建议

如果结合你们当前的场景：

- 两人合作
- 课程要求单文件
- 斗地主项目有明显的工具层 / 决策层分界

那最合适的选择是：

> 用一个 test.cpp 提交，但内部严格按模块分区；代码组织选“单文件平铺函数式分层”；策略路线先做纯规则版，再预留升级到规则+轻量搜索

这是在“课程要求、协作效率、可维护性、后续扩展”之间最均衡的方案。

---

## 十五、附：单文件版接口总表

下面是可以直接抄到文档首页或代码顶部的接口总表。

```cpp
// =========================
// 基础类型
// =========================
using Card = short;
using Level = short;

enum class Stage { BIDDING, PLAYING };
enum class CardComboType { ... };

struct CardCombo {
    vector<Card> cards;
    CardComboType comboType;
    Level comboLevel;

    CardCombo();

    template <typename CARD_ITERATOR>
    CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end);

    bool canBeBeatenBy(const CardCombo& b) const;
};

struct HandPlan {
    vector<CardCombo> groups;
    int handCount;
};

struct GameState {
    Stage stage;
    int myPosition;
    int landlordPosition;
    int finalBid;
    vector<int> bidHistory;
    vector<Card> myCards;
    vector<Card> publicCards;
    CardCombo lastValidCombo;
    int cardRemaining[3];
    vector<vector<Card>> playHistory[3];
    bool cardPlayed[54];
    short levelRemaining[15];

    bool isLandlord() const;
    bool isTeammate(int pos) const;
    int getTeammatePos() const;
    int getUnknownCardCount() const;
};

// =========================
// IO / 状态恢复
// =========================
GameState readGameState();
void outputBid(int value);
void outputPlay(const vector<Card>& cards);

// =========================
// 枚举
// =========================
vector<CardCombo> enumAllValidPlays(
    const vector<Card>& hand,
    const CardCombo& lastCombo
);

CardCombo selectAttachment(
    const vector<Card>& hand,
    const CardCombo& mainCombo,
    CardComboType targetType
);

// =========================
// 拆分
// =========================
vector<HandPlan> decomposeHand(
    const vector<Card>& hand,
    int topK = 1
);

int getMinHandCount(const vector<Card>& hand);

// =========================
// 评估
// =========================
double evaluateHandStrength(const vector<Card>& hand);

double evaluatePlayGain(
    const vector<Card>& handBefore,
    const CardCombo& play,
    const GameState& state
);

// =========================
// 策略
// =========================
int decideBid(
    const vector<Card>& hand,
    const vector<int>& bidHistory
);

CardCombo decidePlay(
    const GameState& state,
    const vector<CardCombo>& validPlays
);
```

如果你们接下来要正式开写代码，建议就按这张接口表先把 test.cpp 的骨架搭起来。
