# 斗地主 AI 策略层与搜索设计说明

> 本文用于记录当前已经确定的策略层设计，方便后续换会话时快速恢复上下文。  
> 负责人视角：B，负责评估层和策略层。  
> 当前约束：C++ 单文件提交，Botzone 每步重新启动程序，每步决策不超过 1 秒，不能使用深度学习、机器学习、强化学习等 AI 技术。

---

## 1. 总体原则

本项目采用分层架构：

```text
枚举层：我现在合法能出什么
拆分层：我的手牌可以怎样组织
评估层：某手牌或某个动作值多少分
策略层：结合角色、队友、威胁和规则，最终决定出什么
```

B 负责的核心不是重新识别牌型，而是在已有接口之上做决策：

```cpp
double evaluateHandStrength(const vector<Card>& hand);
double evaluatePlayGain(const vector<Card>& handBefore, const CardCombo& play, const GameState& state);
int decideBid(const vector<Card>& hand, const vector<int>& bidHistory);
CardCombo decidePlay(const GameState& state, const vector<CardCombo>& validPlays);
```

重要分工：

- `evaluateHandStrength`：通用手牌强度评分。
- `decideBid`：叫地主风险控制，基于手牌强度再加入叫分专用硬条件。
- `evaluatePlayGain`：候选出牌的通用收益评分。
- `decidePlay`：最终出牌决策，负责硬规则、角色判断、队友配合、威胁处理、搜索调度。

---

## 2. 已确定的叫分 V1 思路

叫分采用两层结构：

```text
硬条件：用特征限制最高叫分 bidCap
细粒度：用 evaluateHandStrength 和叫地主修正分得到 targetBid
```

### 2.1 硬条件负责风险上限

硬条件不直接判断最终叫几分，而是限制 `bidCap`。

主要特征：

- 是否有火箭。
- 炸弹数量。
- 王数量。
- 2 的数量。
- A 的数量。
- `getMinHandCount(hand)` 估计的最少手数。

当前已经明确的经验：

- 不能简单用低单张数量压叫分上限。
- 例如 `3 4 5 6 7 8 9 10 J AAAA 22 小王 大王` 是天胡牌，虽然有很多低单张，但它们实际可以组成顺子。
- 在 A 的 `decomposeHand` 能正确识别顺子、连对、飞机之前，B 区不应把 `lowSingleCount` 作为硬条件。

### 2.2 细粒度评分负责目标叫分

基础分来自：

```cpp
double bidScore = evaluateHandStrength(hand);
```

然后加入叫地主专用修正：

- 火箭额外加分。
- 炸弹额外加分。
- 2、王、A 对子提供控制力加分。
- 如果前面已经有人叫分，则扣除风险分。

最后映射：

```text
bidScore >= 高阈值 -> targetBid = 3
bidScore >= 中阈值 -> targetBid = 2
bidScore >= 低阈值 -> targetBid = 1
否则不叫
```

最终收口：

```cpp
targetBid = min(targetBid, bidCap);
if (targetBid <= maxBid) return 0;
return targetBid;
```

---

## 3. 后续出牌决策的顶层设计

`decidePlay` 是整个程序最重要的函数，直接影响胜负。  
不能只做简单贪心，也不适合直接做完整 MCTS。

在不能使用机器学习的前提下，推荐方案是：

```text
硬规则层 + 候选裁剪 + PIMC 随机补全 + 启发式 rollout
```

PIMC 即 Perfect Information Monte Carlo，可以理解为“随机补全确定化搜索”：

```text
不知道别人手牌
  -> 根据已知信息随机补全其他玩家手牌
  -> 得到一个假设的完整局面
  -> 在该完整局面中模拟后续出牌
  -> 多次采样，比较不同候选动作的平均收益
```

这属于传统搜索和随机模拟，不属于机器学习。

---

## 4. decidePlay 的推荐流程

推荐把 `decidePlay` 设计成调度器：

```text
decidePlay(state, validPlays)
  1. 绝对硬规则
  2. 用 evaluatePlayGain 粗评分
  3. 保留 top K 候选
  4. 对 top K 做 PIMC 模拟
  5. 返回期望收益最高的一手
```

### 4.1 绝对硬规则

这些规则优先级高于搜索：

- 如果某个候选能一手出完，直接出。
- 如果当前是自由出牌，不能 PASS。
- 如果地主只剩 1 到 2 张牌，农民必须优先考虑拦截。
- 如果上家是队友，默认不压队友，除非：
  - 自己能直接出完；
  - 地主即将跑完，必须抢牌权；
  - 队友出的牌明显无法阻止地主。
- 炸弹和火箭默认保留，只有在危险局面或直接赢牌时才放开。

### 4.2 候选裁剪

完整枚举后候选可能很多，不能全部模拟。

先用 `evaluatePlayGain` 给每个候选打分，然后保留少量候选：

```text
自由出牌：保留前 6 到 10 个
跟牌压制：保留前 4 到 8 个
残局阶段：可以适当放宽
```

候选裁剪的目的不是最终决策，而是控制 1 秒时间限制。

### 4.3 PIMC 随机补全

根据当前 `GameState` 可以知道：

- 我的手牌。
- 地主明牌。
- 已经打出的牌。
- 每个玩家剩余牌数。
- 未知牌集合。

随机补全步骤：

```text
1. 构造 unknownCards
2. 打乱 unknownCards
3. 按 cardRemaining 分给其他两个玩家
4. 得到一个完整可见局面
```

需要注意：

- 地主视角：两个农民的手牌未知。
- 农民视角：地主和队友手牌都未知，但队友目标和自己一致。
- 已经打出的牌、自己的牌、地主明牌不能进入未知集合。

### 4.4 启发式 rollout

每次补全后，对候选动作做快速模拟：

```text
1. 先执行候选动作
2. 轮到下家
3. 每个玩家用轻量规则策略出牌
4. 模拟到游戏结束或达到步数上限
5. 记录我方胜负
```

rollout 策略不追求完美，只要求快且稳定：

- 能出完就出完。
- 能让自己剩一手优先。
- 跟牌时用最小代价压制。
- 农民不乱压队友。
- 地主快出完时优先拦截。
- 炸弹、火箭谨慎使用。

收益定义 V1：

```text
我方胜利：+1
我方失败：-1
```

后续可以扩展：

```text
考虑 finalBid
考虑炸弹和火箭翻倍
考虑春天和反春
考虑剩余牌数作为未结束时的估值
```

---

## 5. evaluatePlayGain 的职责

`evaluatePlayGain` 是评分器，不是最终决策器。

它应该回答：

```text
如果只看这手牌本身和一般局面收益，这个候选动作值多少分？
```

建议评分项：

- 出完奖励：如果这一手直接出完，给极高分。
- 手数收益：`getMinHandCount(handBefore) - getMinHandCount(handAfter)`。
- 出牌张数收益：一次出更多牌通常更好。
- 结构收益：顺子、连对、飞机等组合牌比普通散牌更好。
- 控制牌保留：不轻易打出 2、王、炸弹、火箭。
- 跟牌代价：能用更小的牌压住就不要浪费大牌。
- 残局收益：出完后只剩一手，给较高分。

注意：

- `evaluatePlayGain` 不应该直接处理“是否压队友”这种强策略规则。
- 队友关系、地主威胁、是否必须拦截，应该放在 `decidePlay`。

---

## 6. decidePlay 的职责

`decidePlay` 是最终拍板者。

它应该综合：

- `validPlays`。
- `GameState` 中的身份信息。
- 当前是否自由出牌。
- 上家有效出牌是谁出的。
- 地主和农民剩余牌数。
- 队友关系。
- `evaluatePlayGain` 粗评分。
- PIMC 模拟结果。

推荐评分合成：

```text
finalScore(action)
  = heuristicScore(action)
  + monteCarloWinRate(action) * weight
  + hardRuleBonusOrPenalty(action)
```

其中：

- `heuristicScore` 来自 `evaluatePlayGain`。
- `monteCarloWinRate` 来自 PIMC 模拟。
- `hardRuleBonusOrPenalty` 处理拦地主、让队友、保炸弹等规则。

---

## 7. 需要 A 提供或完善的接口

PIMC 和强策略依赖 A 区的基础能力。

至少需要：

```cpp
vector<CardCombo> enumAllValidPlays(const vector<Card>& hand, const CardCombo& lastCombo);
vector<HandPlan> decomposeHand(const vector<Card>& hand, int topK = 1);
int getMinHandCount(const vector<Card>& hand);
```

为了搜索，还建议后续补充：

```cpp
GameState applyPlay(const GameState& state, int player, const CardCombo& play);
bool isGameOver(const GameState& state);
int getWinnerSide(const GameState& state);
```

如果不想扩展 `GameState`，也可以为模拟单独设计轻量结构：

```cpp
struct SimState {
    vector<Card> hands[3];
    int currentPlayer;
    int landlordPosition;
    CardCombo lastValidCombo;
    int lastValidPlayer;
};
```

这样可以避免污染真实 Botzone 状态恢复逻辑。

---

## 8. 推荐开发路线

### V1：规则评分版

目标：先得到稳定可用版本。

```text
decideBid：已基本完成 V1
evaluatePlayGain：实现启发式评分
decidePlay：硬规则 + 评分选最大
```

### V2：候选裁剪 + PIMC

目标：在 1 秒内加入传统搜索。

```text
1. evaluatePlayGain 排序候选
2. 选 top K
3. 每个候选做少量随机补全
4. 每个补全做快速 rollout
5. 用胜率修正最终选择
```

### V3：浅层确定化搜索

如果时间允许，在每个 PIMC 补全局面中加入 1 到 2 层搜索：

```text
当前动作
  -> 下家若干响应
  -> 再评估局面
```

这可以比纯 rollout 更稳定，但复杂度更高。

---

## 9. 当前重要结论

1. 不能使用机器学习、深度学习、强化学习。
2. 不建议做完整 MCTS，斗地主动作空间大且非完全信息复杂。
3. 推荐使用 PIMC 随机补全确定化搜索，这是传统算法。
4. `decidePlay` 应该是调度器，不应该写成一大堆散乱 if-else。
5. `evaluatePlayGain` 是候选评分器，不能替代最终策略。
6. A 的 `decomposeHand` 非常关键，尤其决定顺子、连对、飞机能否被正确估值。
7. 在 `decomposeHand` 完善前，B 不应使用低单张数量作为强硬散牌判断。

