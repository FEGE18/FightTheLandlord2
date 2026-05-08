param(
    [string[]]$ChangeNote = @('调试测试：输出 TopK 表，辅助分析 PIMC 接入后的出牌偏移。')
)

$workspacePath = if ([string]::IsNullOrWhiteSpace($PSScriptRoot)) { 'e:\vscode\数据结构项目' } else { $PSScriptRoot }
Set-Location $workspacePath

$compilerPath = 'E:/mingw64/bin/g++.exe'
$testSrcPath = Join-Path $workspacePath 'test.cpp'
$exePath = Join-Path $workspacePath 'test.exe'
$probeSrcPath = Join-Path $workspacePath 'play_topk_probe.cpp'
$probeExePath = Join-Path $workspacePath 'play_topk_probe.exe'
$recordPath = Join-Path $workspacePath '出牌TopK调试记录.txt'

if (!(Test-Path $testSrcPath)) {
    throw "Missing test source: $testSrcPath"
}

if (!(Test-Path $probeSrcPath)) {
    throw "Missing probe source: $probeSrcPath"
}

& $compilerPath -std=c++17 -g $testSrcPath -o $exePath
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to build test.exe from latest test.cpp'
}

& $compilerPath -std=c++17 -g $probeSrcPath -o $probeExePath
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to build play_topk_probe.exe'
}

function Add-DebugCase {
    param(
        [System.Collections.Generic.List[object]]$Cases,
        [string]$Name,
        [string]$Category,
        [string]$ExpectNote,
        [string]$JsonInput,
        [string]$Expected
    )

    $Cases.Add([pscustomobject]@{
        Name = $Name
        Category = $Category
        ExpectNote = $ExpectNote
        JsonInput = $JsonInput
        Expected = $Expected
    })
}

function New-PlayExpectedJson {
    param($ExpectedPlay)

    $cards = if ($null -eq $ExpectedPlay) { @() } else { @($ExpectedPlay) }
    return '{"response":[' + (($cards | ForEach-Object { [string]$_ }) -join ',') + ']}'
}

function New-SimplePayload {
    param(
        [int[]]$History0,
        [int[]]$History1,
        [int[]]$PublicCard,
        [int[]]$Own,
        [int]$Landlord,
        [int]$Pos,
        [int]$FinalBid
    )

    return ([ordered]@{
        requests = @([
            ordered]@{
                history = @($History0, $History1)
                publiccard = $PublicCard
                own = $Own
                landlord = $Landlord
                pos = $Pos
                finalbid = $FinalBid
            }
        )
        responses = @()
    } | ConvertTo-Json -Compress -Depth 10)
}

function New-OpponentPressurePayload {
    param(
        [int[]]$Own,
        [int[]]$CurrentPlay,
        [int]$TargetRemaining
    )

    $priorPlayed = 20 - $TargetRemaining - $CurrentPlay.Count
    if ($priorPlayed -lt 0) {
        throw 'Invalid opponent remaining target.'
    }

    $requests = New-Object System.Collections.Generic.List[object]
    $responses = New-Object System.Collections.Generic.List[object]
    $publicCards = @(4,5,6)
    $pool = @(0..53 | Where-Object { ($_ -notin $Own) -and ($_ -notin $CurrentPlay) -and ($_ -notin $publicCards) })
    $cursor = 0
    $firstRequest = $true

    while ($priorPlayed -gt 0) {
        $take = [Math]::Min(4, $priorPlayed)
        $chunk = @($pool[$cursor..($cursor + $take - 1)])
        $cursor += $take

        if ($firstRequest) {
            $requests.Add([ordered]@{
                history = @(@(), $chunk)
                publiccard = $publicCards
                own = $Own
                landlord = 0
                pos = 1
                finalbid = 2
            })
            $firstRequest = $false
        }
        else {
            $requests.Add([ordered]@{
                history = @(@(), $chunk)
            })
        }

        $responses.Add(@())
        $priorPlayed -= $take
    }

    if ($firstRequest) {
        $requests.Add([ordered]@{
            history = @(@(), $CurrentPlay)
            publiccard = $publicCards
            own = $Own
            landlord = 0
            pos = 1
            finalbid = 2
        })
    }
    else {
        $requests.Add([ordered]@{
            history = @(@(), $CurrentPlay)
        })
    }

    return ([ordered]@{ requests = $requests; responses = $responses } | ConvertTo-Json -Compress -Depth 10)
}

$cases = New-Object System.Collections.Generic.List[object]

Add-DebugCase -Cases $cases -Name 'D01_FreeLowTripleVsHighTriple' -Category 'free-anti-interference' -ExpectNote '自由出牌时，低三张不该被高三张顶掉。' -JsonInput '{"requests":[{"history":[[],[]],"publiccard":[44,45,46],"own":[0,1,2,8,9,16,17,24,32,40,48,52,4,12,20,28,36],"landlord":0,"pos":0,"finalbid":3}],"responses":[]}' -Expected (New-PlayExpectedJson -ExpectedPlay @(0,1,2))
Add-DebugCase -Cases $cases -Name 'D02_FreeLowPairVsAA' -Category 'free-anti-interference' -ExpectNote '自由出牌时，低对子不该被 AA 强行顶掉。' -JsonInput '{"requests":[{"history":[[],[]],"publiccard":[4,5,6],"own":[44,45,48,52,8,9,12,13,16,17,20,21,24,25,28,29,32],"landlord":0,"pos":1,"finalbid":2}],"responses":[]}' -Expected (New-PlayExpectedJson -ExpectedPlay @(8,9))
Add-DebugCase -Cases $cases -Name 'D03_FreeWithBomb_NoLead' -Category 'free-anti-interference' -ExpectNote '自由轮有炸弹时，不应该被样本分推到先炸。' -JsonInput '{"requests":[{"history":[[],[]],"publiccard":[4,5,6],"own":[0,1,2,3,44,45,8,9,16,17,24,25,32,33,40,41,52],"landlord":0,"pos":0,"finalbid":2}],"responses":[]}' -Expected (New-PlayExpectedJson -ExpectedPlay @(4,5,6))
Add-DebugCase -Cases $cases -Name 'D04_FreeWithRocket_NoLead' -Category 'free-anti-interference' -ExpectNote '自由轮有火箭时，不应该被样本分推到先火箭。' -JsonInput '{"requests":[{"history":[[],[]],"publiccard":[53,4,5],"own":[52,40,41,8,9,16,17,24,25,32,33,44,45,48,49,12,20],"landlord":0,"pos":0,"finalbid":3}],"responses":[]}' -Expected (New-PlayExpectedJson -ExpectedPlay @(4,5))
Add-DebugCase -Cases $cases -Name 'D05_BombOnlyVsSingle_Normal' -Category 'hard-control-protection' -ExpectNote '普通局面里，只有炸弹能压单张时应保炸。' -JsonInput '{"requests":[{"history":[[],[44]],"publiccard":[4,5,6],"own":[0,1,2,3,8,9,12,13,16,17,20,21,24,25,28,29,32],"landlord":0,"pos":1,"finalbid":2}],"responses":[]}' -Expected (New-PlayExpectedJson -ExpectedPlay @())
Add-DebugCase -Cases $cases -Name 'D06_BombOnlyVsPair_Normal' -Category 'hard-control-protection' -ExpectNote '普通局面里，只有炸弹能压对子时应保炸。' -JsonInput '{"requests":[{"history":[[],[44,45]],"publiccard":[4,5,6],"own":[0,1,2,3,8,12,16,20,24,28,32,36,40,48,52,9,13],"landlord":0,"pos":1,"finalbid":2}],"responses":[]}' -Expected (New-PlayExpectedJson -ExpectedPlay @())
Add-DebugCase -Cases $cases -Name 'D07_RocketOnlyVsBomb_Normal' -Category 'hard-control-protection' -ExpectNote '普通局面里，只有火箭能压炸弹时应保火箭。' -JsonInput '{"requests":[{"history":[[],[44,45,46,47]],"publiccard":[4,5,6],"own":[52,53,8,9,12,13,16,17,20,21,24,25,28,29,32,36,40],"landlord":0,"pos":1,"finalbid":2}],"responses":[]}' -Expected (New-PlayExpectedJson -ExpectedPlay @())
Add-DebugCase -Cases $cases -Name 'D08_DangerOpponentRemain1_AllowBomb' -Category 'hard-control-protection' -ExpectNote '对手只剩 1 张时，允许炸单张。' -JsonInput (New-OpponentPressurePayload -Own @(0,1,2,3,8,9,12,13,16,17,20,21,24,25,28,29,32) -CurrentPlay @(44) -TargetRemaining 1) -Expected (New-PlayExpectedJson -ExpectedPlay @(0,1,2,3))
Add-DebugCase -Cases $cases -Name 'D09_DangerOpponentRemain2_AllowBomb' -Category 'hard-control-protection' -ExpectNote '对手只剩 2 张时，允许炸单张。' -JsonInput (New-OpponentPressurePayload -Own @(0,1,2,3,8,9,12,13,16,17,20,21,24,25,28,29,32) -CurrentPlay @(44) -TargetRemaining 2) -Expected (New-PlayExpectedJson -ExpectedPlay @(0,1,2,3))
Add-DebugCase -Cases $cases -Name 'D10_LandlordRemain5_AllowBomb' -Category 'hard-control-protection' -ExpectNote '地主剩余不多时，农民允许用炸弹争牌权。' -JsonInput (New-OpponentPressurePayload -Own @(0,1,2,3,8,12,16,20,24,28,32,36,40,48,52,9,13) -CurrentPlay @(44,45) -TargetRemaining 5) -Expected (New-PlayExpectedJson -ExpectedPlay @(0,1,2,3))
Add-DebugCase -Cases $cases -Name 'D11_MyHandCount3_AllowBomb' -Category 'hard-control-protection' -ExpectNote '自己只剩约 3 手牌时，再不抢牌权就可能输，应允许炸。' -JsonInput '{"requests":[{"history":[[],[44]],"publiccard":[4,5,6],"own":[0,1,2,3,4,5,6,7,8,12,16,20,24,28,32,36,40],"landlord":0,"pos":1,"finalbid":2}],"responses":[]}' -Expected (New-PlayExpectedJson -ExpectedPlay @(0,1,2,3))
Add-DebugCase -Cases $cases -Name 'D12_PassConstraint_Base' -Category 'pimc-helpful' -ExpectNote '无 PASS 约束的基线样例，用来看 TopK 和权重。' -JsonInput '{"requests":[{"history":[[],[36,37]],"publiccard":[4,5,6],"own":[44,45,48,49,8,9,12,13,16,17,20,21,24,25,28,29,52],"landlord":0,"pos":0,"finalbid":2}],"responses":[]}' -Expected ''
Add-DebugCase -Cases $cases -Name 'D13_PassConstraint_WithPass' -Category 'pimc-helpful' -ExpectNote '加入前序 PASS 约束后，观察 sampleWeight 和 TopK 是否变化。' -JsonInput '{"requests":[{"history":[[32,33],[]],"publiccard":[4,5,6],"own":[44,45,48,49,8,9,12,13,16,17,20,21,24,25,28,29,52],"landlord":0,"pos":0,"finalbid":2},{"history":[[],[36,37]]}],"responses":[[]]}' -Expected ''
Add-DebugCase -Cases $cases -Name 'D14_FreeLandlord_PairLead' -Category 'free-anti-interference' -ExpectNote '另一手地主自由轮样例，优先走小对子。' -JsonInput (New-SimplePayload -History0 @() -History1 @() -PublicCard @(44,45,4) -Own @(0,1,8,9,16,17,24,25,32,33,40,41,48,49,52,12,20) -Landlord 0 -Pos 0 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @(0,1))
Add-DebugCase -Cases $cases -Name 'D15_FreeWithBomb_SecondHand' -Category 'free-anti-interference' -ExpectNote '第二手带炸弹的自由轮样例，仍应优先普通三张。' -JsonInput (New-SimplePayload -History0 @() -History1 @() -PublicCard @(4,5,6) -Own @(24,25,26,27,44,45,8,9,16,17,32,33,40,41,48,52,12) -Landlord 0 -Pos 0 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @(4,5,6))
Add-DebugCase -Cases $cases -Name 'D16_FreeWithRocket_SecondHand' -Category 'free-anti-interference' -ExpectNote '另一手带王的自由轮样例，也不应该先交火箭。' -JsonInput (New-SimplePayload -History0 @() -History1 @() -PublicCard @(52,4,5) -Own @(53,40,41,8,9,16,17,24,25,32,33,44,45,48,49,12,20) -Landlord 0 -Pos 0 -FinalBid 3) -Expected (New-PlayExpectedJson -ExpectedPlay @(4,5))
Add-DebugCase -Cases $cases -Name 'D17_FreeFarmer_TripleLead' -Category 'free-anti-interference' -ExpectNote '农民自由轮补一个三张样例，检查是否也会被高控牌干扰。' -JsonInput (New-SimplePayload -History0 @() -History1 @() -PublicCard @(44,45,46) -Own @(0,1,2,8,9,12,13,16,17,20,21,24,25,28,29,32,52) -Landlord 2 -Pos 1 -FinalBid 1) -Expected (New-PlayExpectedJson -ExpectedPlay @(0,1,2))
Add-DebugCase -Cases $cases -Name 'D18_FollowTeammateSingle_LetPassA' -Category 'teammate-basic' -ExpectNote '跟队友单张时应优先让牌。' -JsonInput (New-SimplePayload -History0 @(40) -History1 @() -PublicCard @(4,5,6) -Own @(44,45,48,52,8,9,12,13,16,17,20,21,24,25,28,29,32) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @())
Add-DebugCase -Cases $cases -Name 'D19_FollowTeammateSingle_LetPassB' -Category 'teammate-basic' -ExpectNote '跟队友较大单张时同样应优先让牌。' -JsonInput (New-SimplePayload -History0 @(44) -History1 @() -PublicCard @(4,5,6) -Own @(48,49,52,53,8,9,12,13,16,17,20,21,24,25,28,29,32) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @())
Add-DebugCase -Cases $cases -Name 'D20_FollowTeammatePair_GrabA' -Category 'teammate-basic' -ExpectNote '跟队友小对子时，当前策略会抢牌，这类样例也需要 TopK 可视化。' -JsonInput (New-SimplePayload -History0 @(40,41) -History1 @() -PublicCard @(4,5,6) -Own @(44,45,48,52,8,9,12,13,16,17,20,21,24,25,28,29,32) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @(44,45))
Add-DebugCase -Cases $cases -Name 'D21_FollowTeammatePair_GrabB' -Category 'teammate-basic' -ExpectNote '第二手跟队友对子样例，观察 TopK 是否同样把 AA 顶到最前。' -JsonInput (New-SimplePayload -History0 @(32,33) -History1 @() -PublicCard @(4,5,6) -Own @(44,45,48,49,52,8,9,12,13,16,17,20,21,24,25,28,29) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @(44,45))
Add-DebugCase -Cases $cases -Name 'D22_FollowOpponentSingle_Normal' -Category 'opponent-basic' -ExpectNote '跟对手小单张时，应该正常用最小可接单张压牌。' -JsonInput (New-SimplePayload -History0 @() -History1 @(24) -PublicCard @(4,5,6) -Own @(44,45,48,52,8,9,12,13,16,17,20,21,25,28,29,32,36) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @(28))
Add-DebugCase -Cases $cases -Name 'D23_FollowOpponentHighSingle_JokerTake' -Category 'opponent-basic' -ExpectNote '跟对手更高单张时，用小王接管，确认基本跟牌仍正常。' -JsonInput (New-SimplePayload -History0 @() -History1 @(48) -PublicCard @(4,5,6) -Own @(52,53,44,45,8,9,12,13,16,17,20,21,24,25,28,29,32) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @(52))
Add-DebugCase -Cases $cases -Name 'D24_FollowOpponentPair_Normal' -Category 'opponent-basic' -ExpectNote '跟对手小对子时，应该正常用更大对子压牌。' -JsonInput (New-SimplePayload -History0 @() -History1 @(24,25) -PublicCard @(4,5,6) -Own @(44,45,48,52,8,9,12,13,16,17,20,21,28,29,32,33,36) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @(28,29))
Add-DebugCase -Cases $cases -Name 'D25_FollowOpponentTriple_Normal' -Category 'opponent-basic' -ExpectNote '跟对手三条时，确认基本三条接管逻辑不受 PIMC 干扰。' -JsonInput (New-SimplePayload -History0 @() -History1 @(24,25,26) -PublicCard @(4,5,6) -Own @(32,33,34,44,45,48,8,9,12,13,16,17,20,21,28,29,52) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @(32,33,34))
Add-DebugCase -Cases $cases -Name 'D26_CannotBeatBigJoker' -Category 'cannot-beat' -ExpectNote '无法压过大王时必须 PASS。' -JsonInput (New-SimplePayload -History0 @() -History1 @(53) -PublicCard @(4,5,6) -Own @(44,45,48,8,9,12,13,16,17,20,21,24,25,28,29,32,33) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @())
Add-DebugCase -Cases $cases -Name 'D27_CannotBeatSmallJokerWithoutBig' -Category 'cannot-beat' -ExpectNote '没有大王时也不能压小王。' -JsonInput (New-SimplePayload -History0 @() -History1 @(52) -PublicCard @(4,5,6) -Own @(44,45,48,8,9,12,13,16,17,20,21,24,25,28,29,32,33) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @())
Add-DebugCase -Cases $cases -Name 'D28_CannotBeatHighPair' -Category 'cannot-beat' -ExpectNote '没有更大的对子时，应稳定 PASS。' -JsonInput (New-SimplePayload -History0 @() -History1 @(44,45) -PublicCard @(4,5,6) -Own @(32,33,40,41,8,9,12,13,16,17,20,21,24,25,28,36,52) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @())
Add-DebugCase -Cases $cases -Name 'D29_CannotBeatHighTriple' -Category 'cannot-beat' -ExpectNote '没有更大的三条时，应稳定 PASS。' -JsonInput (New-SimplePayload -History0 @() -History1 @(48,49,50) -PublicCard @(4,5,6) -Own @(44,45,46,32,33,34,8,9,12,13,16,17,20,21,24,25,52) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @())
Add-DebugCase -Cases $cases -Name 'D30_BombOnlyVsTriple_Normal' -Category 'hard-control-restraint' -ExpectNote '只有炸弹能压三条时，普通局面应保炸不交。' -JsonInput (New-SimplePayload -History0 @() -History1 @(44,45,46) -PublicCard @(4,5,6) -Own @(0,1,2,3,8,12,16,20,24,28,32,36,40,48,52,9,13) -Landlord 0 -Pos 1 -FinalBid 2) -Expected (New-PlayExpectedJson -ExpectedPlay @())

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('')
$lines.Add('')
$lines.Add('================ 出牌 TopK 调试测试 ' + $cases.Count + ' 组 ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' ================')
$lines.AddRange([string[]]$ChangeNote)
$lines.Add('说明：test.exe 反映当前真实策略输出；play_topk_probe.exe 使用固定随机种子 20260508 输出 TopK 表，便于稳定调 PIMC。')
$lines.Add('说明：本文件偏调试分析，不是纯 EXPECTED 回归，因此部分 exploratory 用例不做 MATCH 判定。')
$lines.Add('说明：test.cpp 当前以 time(nullptr) ^ clock() 作为随机种子；连续进程若落在同一秒，稳定性结果可能高估。')
$lines.Add('')

$crossMomentInput = '{"requests":[{"history":[[],[]],"publiccard":[4,5,6],"own":[0,1,2,3,44,45,8,9,16,17,24,25,32,33,40,41,52],"landlord":0,"pos":0,"finalbid":2}],"responses":[]}'
$crossMomentFirstTime = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
$crossMomentFirstOutput = ($crossMomentInput | & $exePath | Out-String).Trim()

foreach ($case in $cases) {
    $actual = ($case.JsonInput | & $exePath | Out-String).Trim()
    $topk = ($case.JsonInput | & $probeExePath | Out-String).Trim()
    $isExploratory = [string]::IsNullOrWhiteSpace($case.Expected)
    $match = if ($isExploratory) { 'N/A' } elseif ($actual -eq $case.Expected) { 'YES' } else { 'NO' }

    $lines.Add('=== ' + $case.Name + ' ===')
    $lines.Add('CATEGORY: ' + $case.Category)
    $lines.Add('EXPECT_NOTE: ' + $case.ExpectNote)
    $lines.Add('INPUT: ' + $case.JsonInput)
    $lines.Add('EXPECTED: ' + $(if ($isExploratory) { 'EXPLORATORY' } else { $case.Expected }))
    $lines.Add('OUTPUT: ' + $actual)
    $lines.Add('MATCH: ' + $match)
    $lines.Add('TOPK:')
    foreach ($line in ($topk -split "`r?`n")) {
        $lines.Add($line)
    }
    $lines.Add('')

    Write-Output ($case.Name + ' => actual ' + $actual + ' | match ' + $match)
}

$stabilityInput = '{"requests":[{"history":[[],[]],"publiccard":[4,5,6],"own":[44,45,48,52,8,9,12,13,16,17,20,21,24,25,28,29,32],"landlord":0,"pos":1,"finalbid":2}],"responses":[]}'
$frequency = @{}
$stabilityRuns = New-Object System.Collections.Generic.List[string]
for ($run = 1; $run -le 10; $run++) {
    $stamp = Get-Date -Format 'HH:mm:ss.fff'
    $output = ($stabilityInput | & $exePath | Out-String).Trim()
    if (!$frequency.ContainsKey($output)) {
        $frequency[$output] = 0
    }
    $frequency[$output] += 1
    $stabilityRuns.Add(('RUN {0}: {1} => {2}' -f $run, $stamp, $output))
}

$lines.Add('=== Stability_FreeLowPairVsAA_10Runs ===')
$lines.Add('CATEGORY: random-stability')
$lines.Add('EXPECT_NOTE: 同一输入连续运行 10 次，观察输出是否稳定。')
$lines.Add('INPUT: ' + $stabilityInput)
$lines.Add('RUNS: 10')
foreach ($runLine in $stabilityRuns) {
    $lines.Add($runLine)
}
foreach ($key in ($frequency.Keys | Sort-Object)) {
    $lines.Add(('FREQUENCY: {0} => {1}' -f $key, $frequency[$key]))
}
$lines.Add('')

$crossMomentSecondTime = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
$crossMomentSecondOutput = ($crossMomentInput | & $exePath | Out-String).Trim()

$lines.Add('=== CrossMoment_FreeWithBomb_NoLead ===')
$lines.Add('CATEGORY: random-stability')
$lines.Add('EXPECT_NOTE: 利用整套脚本的自然耗时做跨时刻复测，观察同一敏感输入是否漂移。')
$lines.Add('INPUT: ' + $crossMomentInput)
$lines.Add('RUN EARLY: ' + $crossMomentFirstTime + ' => ' + $crossMomentFirstOutput)
$lines.Add('RUN LATE: ' + $crossMomentSecondTime + ' => ' + $crossMomentSecondOutput)
$lines.Add('MATCH: ' + $(if ($crossMomentFirstOutput -eq $crossMomentSecondOutput) { 'YES' } else { 'NO' }))
$lines.Add('')

Add-Content -Path $recordPath -Value $lines -Encoding UTF8

Write-Output ('TopKDebugRecordFile => ' + $recordPath)
Write-Output ('TopKDebugCaseCount => ' + $cases.Count)