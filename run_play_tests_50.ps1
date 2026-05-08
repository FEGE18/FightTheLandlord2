param(
    [string[]]$ChangeNote = @('修改说明：未提供本轮具体修改说明。'),
    [string[]]$CategoryFilter = @(),
    [int]$CaseLimit = 0
)

$workspacePath = if ([string]::IsNullOrWhiteSpace($PSScriptRoot)) { 'e:\vscode\数据结构项目' } else { $PSScriptRoot }
Set-Location $workspacePath

$compilerPath = 'E:/mingw64/bin/g++.exe'
$testSrcPath = Join-Path $workspacePath 'test.cpp'
$exePath = Join-Path $workspacePath 'test.exe'
$playRecordPath = Join-Path $workspacePath '出牌测试记录.txt'
$changeNotes = $ChangeNote

if (!(Test-Path $testSrcPath)) {
    throw "Missing test source: $testSrcPath"
}

& $compilerPath -std=c++17 -g $testSrcPath -o $exePath
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to build test.exe from latest test.cpp'
}

$rankNames = @('3','4','5','6','7','8','9','10','J','Q','K','A','2','joker','JOKER')

function Get-Level {
    param([int]$Card)
    return [int]([math]::Floor($Card / 4) + $(if ($Card -eq 53) { 1 } else { 0 }))
}

function Get-HandSummary {
    param([int[]]$Cards)

    if (!$Cards -or $Cards.Count -eq 0) {
        return 'PASS'
    }

    $counts = @(0) * 15
    foreach ($card in $Cards) {
        $counts[(Get-Level $card)] += 1
    }

    $parts = New-Object System.Collections.Generic.List[string]
    for ($i = 0; $i -lt 15; $i++) {
        if ($counts[$i] -gt 0) {
            $parts.Add(($rankNames[$i] + 'x' + $counts[$i]))
        }
    }
    return ($parts -join ' ')
}

function Get-RoleLabel {
    param(
        [int]$Landlord,
        [int]$Pos
    )

    if ($Landlord -eq $Pos) {
        return 'landlord'
    }
    return 'farmer'
}

function New-PlayJson {
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
        requests = @(
            [ordered]@{
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

function New-PlayExpectedJson {
    param($ExpectedPlay)

    $cards = if ($null -eq $ExpectedPlay) { @() } else { @($ExpectedPlay) }
    return '{"response":[' + (($cards | ForEach-Object { [string]$_ }) -join ',') + ']}'
}

function Add-SimplePlayCase {
    param(
        [System.Collections.Generic.List[object]]$Cases,
        [string]$Name,
        [string]$Category,
        [string]$Route,
        [int[]]$History0,
        [int[]]$History1,
        [int[]]$PublicCard,
        [int[]]$Own,
        [int]$Landlord,
        [int]$Pos,
        [int]$FinalBid,
        [int[]]$ExpectedPlay,
        [string]$ExpectNote
    )

    $Cases.Add([pscustomobject]@{
        Name = $Name
        Category = $Category
        Route = $Route
        Input = New-PlayJson -History0 $History0 -History1 $History1 -PublicCard $PublicCard -Own $Own -Landlord $Landlord -Pos $Pos -FinalBid $FinalBid
        Expected = New-PlayExpectedJson -ExpectedPlay $ExpectedPlay
        ExpectNote = $ExpectNote
        HandSummary = Get-HandSummary -Cards $Own
        LastSummary = 'history0=' + (Get-HandSummary -Cards $History0) + ' history1=' + (Get-HandSummary -Cards $History1)
        Debug = 'role=' + (Get-RoleLabel -Landlord $Landlord -Pos $Pos) + ' route=' + $Route + ' public=' + (Get-HandSummary -Cards $PublicCard)
    })
}

function New-TeammateFocusPayload {
    param(
        [int[]]$Own,
        [int[]]$CurrentPlay,
        [int]$TargetRemaining
    )

    $priorPlayed = 17 - $TargetRemaining - $CurrentPlay.Count
    if ($priorPlayed -lt 0) {
        throw 'Invalid teammate remaining target.'
    }

    $requests = New-Object System.Collections.Generic.List[object]
    $responses = New-Object System.Collections.Generic.List[object]
    $pool = 0..23
    $cursor = 0
    $firstRequest = $true

    while ($priorPlayed -gt 0) {
        $take = [Math]::Min(4, $priorPlayed)
        $chunk = @($pool[$cursor..($cursor + $take - 1)])
        $cursor += $take

        if ($firstRequest) {
            $requests.Add([ordered]@{
                history = @($chunk, @())
                publiccard = @(4,5,6)
                own = $Own
                landlord = 0
                pos = 1
                finalbid = 2
            })
            $firstRequest = $false
        }
        else {
            $requests.Add([ordered]@{
                history = @($chunk, @())
            })
        }

        $responses.Add(@())
        $priorPlayed -= $take
    }

    if ($firstRequest) {
        $requests.Add([ordered]@{
            history = @($CurrentPlay, @())
            publiccard = @(4,5,6)
            own = $Own
            landlord = 0
            pos = 1
            finalbid = 2
        })
    }
    else {
        $requests.Add([ordered]@{
            history = @($CurrentPlay, @())
        })
    }

    return [pscustomobject]@{
        Json = ([ordered]@{ requests = $requests; responses = $responses } | ConvertTo-Json -Compress -Depth 10)
        HistoryDepth = $requests.Count
    }
}

function Add-TeammateFocusCase {
    param(
        [System.Collections.Generic.List[object]]$Cases,
        [string]$Name,
        [string]$Category,
        [string]$Variant,
        [int[]]$Own,
        [int[]]$CurrentPlay,
        [int]$TargetRemaining,
        [int[]]$ExpectedPlay,
        [string]$ExpectNote
    )

    $payload = New-TeammateFocusPayload -Own $Own -CurrentPlay $CurrentPlay -TargetRemaining $TargetRemaining
    $Cases.Add([pscustomobject]@{
        Name = $Name
        Category = $Category
        Route = 'follow-teammate-focus'
        Input = $payload.Json
        Expected = New-PlayExpectedJson -ExpectedPlay $ExpectedPlay
        ExpectNote = $ExpectNote
        HandSummary = Get-HandSummary -Cards $Own
        LastSummary = 'teammatePlay=' + (Get-HandSummary -Cards $CurrentPlay) + ' teammateRemaining=' + $TargetRemaining + ' variant=' + $Variant
        Debug = 'role=farmer route=follow-teammate-focus historyDepth=' + $payload.HistoryDepth + ' teammateRemaining=' + $TargetRemaining
    })
}

$cases = New-Object System.Collections.Generic.List[object]

$simpleCases = @(
    [pscustomobject]@{ Name = 'P01_FreeLandlord_A'; Category = 'free'; Route = 'free'; History0 = @(); History1 = @(); PublicCard = @(44,45,46); Own = @(0,1,2,8,9,16,17,24,32,40,48,52,4,12,20,28,36); Landlord = 0; Pos = 0; FinalBid = 3; Expected = @(0,1,2); Note = '地主自由出牌，优先走普通三张而不是先拆硬控。' },
    [pscustomobject]@{ Name = 'P02_FreeLandlord_B'; Category = 'free'; Route = 'free'; History0 = @(); History1 = @(); PublicCard = @(44,45,4); Own = @(0,1,8,9,16,17,24,25,32,33,40,41,48,49,52,12,20); Landlord = 0; Pos = 0; FinalBid = 2; Expected = @(0,1); Note = '地主自由出牌，偏向先走对子。' },
    [pscustomobject]@{ Name = 'P03_FreeWithBomb_A'; Category = 'free'; Route = 'free'; History0 = @(); History1 = @(); PublicCard = @(4,5,6); Own = @(0,1,2,3,44,45,8,9,16,17,24,25,32,33,40,41,52); Landlord = 0; Pos = 0; FinalBid = 2; Expected = @(4,5,6); Note = '有炸弹时自由出牌仍应优先普通三张。' },
    [pscustomobject]@{ Name = 'P04_FreeWithBomb_B'; Category = 'free'; Route = 'free'; History0 = @(); History1 = @(); PublicCard = @(4,5,6); Own = @(24,25,26,27,44,45,8,9,16,17,32,33,40,41,48,52,12); Landlord = 0; Pos = 0; FinalBid = 2; Expected = @(4,5,6); Note = '另一手带炸弹的自由出牌样例。' },
    [pscustomobject]@{ Name = 'P05_FreeWithRocket_A'; Category = 'free'; Route = 'free'; History0 = @(); History1 = @(); PublicCard = @(53,4,5); Own = @(52,40,41,8,9,16,17,24,25,32,33,44,45,48,49,12,20); Landlord = 0; Pos = 0; FinalBid = 3; Expected = @(4,5); Note = '有火箭时仍不应先把火箭交掉。' },
    [pscustomobject]@{ Name = 'P06_FreeWithRocket_B'; Category = 'free'; Route = 'free'; History0 = @(); History1 = @(); PublicCard = @(52,4,5); Own = @(53,40,41,8,9,16,17,24,25,32,33,44,45,48,49,12,20); Landlord = 0; Pos = 0; FinalBid = 3; Expected = @(4,5); Note = '另一手带王的自由出牌样例。' },
    [pscustomobject]@{ Name = 'P07_FollowTeammateSingle_A'; Category = 'teammate-basic'; Route = 'follow-teammate'; History0 = @(40); History1 = @(); PublicCard = @(4,5,6); Own = @(44,45,48,52,8,9,12,13,16,17,20,21,24,25,28,29,32); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(); Note = '跟队友单张时选择让牌。' },
    [pscustomobject]@{ Name = 'P08_FollowTeammateSingle_B'; Category = 'teammate-basic'; Route = 'follow-teammate'; History0 = @(44); History1 = @(); PublicCard = @(4,5,6); Own = @(48,49,52,53,8,9,12,13,16,17,20,21,24,25,28,29,32); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(); Note = '跟队友大单张时同样让牌。' },
    [pscustomobject]@{ Name = 'P09_FollowTeammatePair_A'; Category = 'teammate-basic'; Route = 'follow-teammate'; History0 = @(40,41); History1 = @(); PublicCard = @(4,5,6); Own = @(44,45,48,52,8,9,12,13,16,17,20,21,24,25,28,29,32); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(44,45); Note = '跟队友对子时当前策略会抢牌。' },
    [pscustomobject]@{ Name = 'P10_FollowTeammatePair_B'; Category = 'teammate-basic'; Route = 'follow-teammate'; History0 = @(32,33); History1 = @(); PublicCard = @(4,5,6); Own = @(44,45,48,49,52,8,9,12,13,16,17,20,21,24,25,28,29); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(44,45); Note = '第二个跟队友对子样例。' },
    [pscustomobject]@{ Name = 'P11_FollowOpponentSingle_A'; Category = 'opponent-basic'; Route = 'follow-opponent'; History0 = @(); History1 = @(24); PublicCard = @(4,5,6); Own = @(44,45,48,52,8,9,12,13,16,17,20,21,25,28,29,32,36); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(28); Note = '跟对手小单张，应该正常压牌。' },
    [pscustomobject]@{ Name = 'P12_FollowOpponentSingle_B'; Category = 'opponent-basic'; Route = 'follow-opponent'; History0 = @(); History1 = @(40); PublicCard = @(4,5,6); Own = @(44,45,48,52,8,9,12,13,16,17,20,21,24,25,28,29,32); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(44); Note = '跟对手较大单张，仍会正常接管。' },
    [pscustomobject]@{ Name = 'P13_FollowOpponentHighSingle_A'; Category = 'opponent-basic'; Route = 'follow-opponent'; History0 = @(); History1 = @(44); PublicCard = @(4,5,6); Own = @(48,49,52,53,8,9,12,13,16,17,20,21,24,25,28,29,32); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(48); Note = '跟对手高单张时用更高单张接。' },
    [pscustomobject]@{ Name = 'P14_FollowOpponentHighSingle_B'; Category = 'opponent-basic'; Route = 'follow-opponent'; History0 = @(); History1 = @(48); PublicCard = @(4,5,6); Own = @(52,53,44,45,8,9,12,13,16,17,20,21,24,25,28,29,32); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(52); Note = '跟对手更高单张时用小王接。' },
    [pscustomobject]@{ Name = 'P15_FollowOpponentPair_A'; Category = 'opponent-basic'; Route = 'follow-opponent'; History0 = @(); History1 = @(24,25); PublicCard = @(4,5,6); Own = @(44,45,48,52,8,9,12,13,16,17,20,21,28,29,32,33,36); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(28,29); Note = '跟对手小对子。' },
    [pscustomobject]@{ Name = 'P16_FollowOpponentPair_B'; Category = 'opponent-basic'; Route = 'follow-opponent'; History0 = @(); History1 = @(32,33); PublicCard = @(4,5,6); Own = @(44,45,48,49,52,8,9,12,13,16,17,20,21,28,29,36,37); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(36,37); Note = '跟对手中对子。' },
    [pscustomobject]@{ Name = 'P17_FollowOpponentTriple_A'; Category = 'opponent-basic'; Route = 'follow-opponent'; History0 = @(); History1 = @(24,25,26); PublicCard = @(4,5,6); Own = @(32,33,34,44,45,48,8,9,12,13,16,17,20,21,28,29,52); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(32,33,34); Note = '跟对手三条时正常用更大三条接。' },
    [pscustomobject]@{ Name = 'P18_FollowOpponentTriple_B'; Category = 'opponent-basic'; Route = 'follow-opponent'; History0 = @(); History1 = @(32,33,34); PublicCard = @(4,5,6); Own = @(44,45,46,48,8,9,12,13,16,17,20,21,24,25,28,29,52); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(44,45,46); Note = '跟对手更大三条。' },
    [pscustomobject]@{ Name = 'P19_CannotBeatBigJoker_A'; Category = 'cannot-beat'; Route = 'follow-opponent'; History0 = @(); History1 = @(53); PublicCard = @(4,5,6); Own = @(44,45,48,8,9,12,13,16,17,20,21,24,25,28,29,32,33); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(); Note = '无法压过大王，必须 PASS。' },
    [pscustomobject]@{ Name = 'P20_CannotBeatBigJoker_B'; Category = 'cannot-beat'; Route = 'follow-opponent'; History0 = @(); History1 = @(52); PublicCard = @(4,5,6); Own = @(44,45,48,8,9,12,13,16,17,20,21,24,25,28,29,32,33); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(); Note = '没有大王时也不能压小王。' },
    [pscustomobject]@{ Name = 'P21_CannotBeatPairA'; Category = 'cannot-beat'; Route = 'follow-opponent'; History0 = @(); History1 = @(44,45); PublicCard = @(4,5,6); Own = @(32,33,40,41,8,9,12,13,16,17,20,21,24,25,28,36,52); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(); Note = '没有更大的对子时选择 PASS。' },
    [pscustomobject]@{ Name = 'P22_CannotBeatTriple2'; Category = 'cannot-beat'; Route = 'follow-opponent'; History0 = @(); History1 = @(48,49,50); PublicCard = @(4,5,6); Own = @(44,45,46,32,33,34,8,9,12,13,16,17,20,21,24,25,52); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(); Note = '没有更大的三条时 PASS。' },
    [pscustomobject]@{ Name = 'P23_BombOnlyVsPairA'; Category = 'hard-control-restraint'; Route = 'follow-opponent'; History0 = @(); History1 = @(44,45); PublicCard = @(4,5,6); Own = @(0,1,2,3,8,12,16,20,24,28,32,36,40,48,52,9,13); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(); Note = '只有炸弹能压对子时，当前策略选择保守 PASS。' },
    [pscustomobject]@{ Name = 'P24_BombOnlyVsTripleA'; Category = 'hard-control-restraint'; Route = 'follow-opponent'; History0 = @(); History1 = @(44,45,46); PublicCard = @(4,5,6); Own = @(0,1,2,3,8,12,16,20,24,28,32,36,40,48,52,9,13); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(); Note = '只有炸弹能压三条时仍选择保守 PASS。' },
    [pscustomobject]@{ Name = 'P25_FreeFarmer_A'; Category = 'free'; Route = 'free'; History0 = @(); History1 = @(); PublicCard = @(4,5,6); Own = @(44,45,48,52,8,9,12,13,16,17,20,21,24,25,28,29,32); Landlord = 0; Pos = 1; FinalBid = 2; Expected = @(8,9); Note = '农民自由出牌，优先走小对子。' },
    [pscustomobject]@{ Name = 'P26_FreeFarmer_B'; Category = 'free'; Route = 'free'; History0 = @(); History1 = @(); PublicCard = @(44,45,46); Own = @(0,1,2,8,9,12,13,16,17,20,21,24,25,28,29,32,52); Landlord = 2; Pos = 1; FinalBid = 1; Expected = @(0,1,2); Note = '农民自由出牌的第二个样例。' }
)

foreach ($case in $simpleCases) {
    Add-SimplePlayCase -Cases $cases -Name $case.Name -Category $case.Category -Route $case.Route -History0 $case.History0 -History1 $case.History1 -PublicCard $case.PublicCard -Own $case.Own -Landlord $case.Landlord -Pos $case.Pos -FinalBid $case.FinalBid -ExpectedPlay $case.Expected -ExpectNote $case.Note
}

$handVariants = @(
    [pscustomobject]@{ Label = 'A'; Own = @(28,29,32,33,36,37,40,41,44,45,48,49,52,53,20,21,12) },
    [pscustomobject]@{ Label = 'B'; Own = @(28,29,30,31,32,33,36,37,40,41,44,45,48,49,52,53,16) }
)

$focusTemplates = @(
    [pscustomobject]@{ Category = 'teammate-focus-single-low';  Type = 'SmallSingle9'; Current = @(24);       Remaining = @(16,12,8,4,2);  ExpectedA = @();       ExpectedB = @();       Note = '队友出小单张时，当前策略整体选择让牌。' },
    [pscustomobject]@{ Category = 'teammate-focus-single-mid';  Type = 'MediumSingleK'; Current = @(40);       Remaining = @(16,12,8,4,2);  ExpectedA = @();       ExpectedB = @();       Note = '队友出中等单张时，当前策略整体选择让牌。' },
    [pscustomobject]@{ Category = 'teammate-focus-single-high'; Type = 'HighSingleA';   Current = @(44);       Remaining = @(16,12,8,4,2);  ExpectedA = @();       ExpectedB = @();       Note = '队友出高单张时，当前策略整体选择让牌。' },
    [pscustomobject]@{ Category = 'teammate-focus-pair-low';    Type = 'SmallPair99';   Current = @(24,25);    Remaining = @(15,11,7,3,1); ExpectedA = @(28,29); ExpectedB = @();       Note = '队友出小对子时，A 变体会抢，B 变体会让。' },
    [pscustomobject]@{ Category = 'teammate-focus-pair-high';   Type = 'HighPairAA';    Current = @(44,45);    Remaining = @(15,11,7,3,1); ExpectedA = @();       ExpectedB = @();       Note = '队友出高对子时，两种手牌都选择让牌。' }
)

$caseIndex = 1
foreach ($template in $focusTemplates) {
    foreach ($remaining in $template.Remaining) {
        foreach ($variant in $handVariants) {
            $expectedPlay = if ($variant.Label -eq 'A') { $template.ExpectedA } else { $template.ExpectedB }
            Add-TeammateFocusCase -Cases $cases -Name ('T{0:D2}_{1}_Remain{2}_{3}' -f $caseIndex, $template.Type, $remaining, $variant.Label) -Category $template.Category -Variant $variant.Label -Own $variant.Own -CurrentPlay $template.Current -TargetRemaining $remaining -ExpectedPlay $expectedPlay -ExpectNote ('队友剩余 ' + $remaining + ' 张。' + $template.Note)
            $caseIndex += 1
        }
    }
}

$selectedCases = @($cases.ToArray())
if ($CategoryFilter.Count -gt 0) {
    $selectedCases = @($selectedCases | Where-Object { $CategoryFilter -contains $_.Category })
}
if ($CaseLimit -gt 0) {
    $selectedCases = @($selectedCases | Select-Object -First $CaseLimit)
}
if ($selectedCases.Count -eq 0) {
    throw 'No play cases selected. Check CategoryFilter or CaseLimit.'
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('')
$lines.Add('')
$lines.Add('================ 出牌回归测试 ' + $selectedCases.Count + ' 组 ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' ================')
$lines.AddRange([string[]]$changeNotes)
$lines.Add('用途：固定出牌行为，辅助调参；MATCH 只表示符合当前设计预期，不表示策略最优。')
$lines.Add('样例结构：当前按筛选后实际运行 ' + $selectedCases.Count + ' 组，可通过 CategoryFilter / CaseLimit 调整。')
$lines.Add('CATEGORY_FILTER: ' + $(if ($CategoryFilter.Count -gt 0) { $CategoryFilter -join ', ' } else { 'ALL' }))
$lines.Add('CASE_LIMIT: ' + $(if ($CaseLimit -gt 0) { $CaseLimit } else { 'ALL' }))
$lines.Add('DEBUG 说明：route 用来看场景来源，LastSummary 用来看当前跟牌目标和关键剩余牌数。')
$lines.Add('')

$matchCount = 0
$categoryStats = @{}

foreach ($case in $selectedCases) {
    $output = ($case.Input | & $exePath | Out-String).Trim()
    $isMatch = if ($output -eq $case.Expected) { 'YES' } else { 'NO' }
    if ($isMatch -eq 'YES') {
        $matchCount += 1
    }

    if (!$categoryStats.ContainsKey($case.Category)) {
        $categoryStats[$case.Category] = [pscustomobject]@{ Total = 0; Match = 0 }
    }
    $categoryStats[$case.Category].Total += 1
    if ($isMatch -eq 'YES') {
        $categoryStats[$case.Category].Match += 1
    }

    $lines.Add('=== ' + $case.Name + ' ===')
    $lines.Add('CATEGORY: ' + $case.Category)
    $lines.Add('HAND: ' + $case.HandSummary)
    $lines.Add('EXPECT_NOTE: ' + $case.ExpectNote)
    $lines.Add('DEBUG: ' + $case.Debug)
    $lines.Add('LAST: ' + $case.LastSummary)
    $lines.Add('INPUT: ' + $case.Input)
    $lines.Add('EXPECTED: ' + $case.Expected)
    $lines.Add('OUTPUT: ' + $output)
    $lines.Add('MATCH: ' + $isMatch)
    $lines.Add('')

    Write-Output ($case.Name + ' => expected ' + $case.Expected + ' | actual ' + $output + ' | match ' + $isMatch)
}

$lines.Add('================ 汇总 ================')
$lines.Add('TOTAL: ' + $selectedCases.Count)
$lines.Add('MATCHED: ' + $matchCount)
$lines.Add('FAILED: ' + ($selectedCases.Count - $matchCount))
foreach ($key in ($categoryStats.Keys | Sort-Object)) {
    $stat = $categoryStats[$key]
    $lines.Add(('CATEGORY {0}: {1}/{2} matched' -f $key, $stat.Match, $stat.Total))
}
$lines.Add('')
$lines.Add('失败用法建议：先看 CATEGORY、DEBUG 和 LAST，判断是期望写错、跟牌场景理解错，还是策略行为真的回归了。')

Add-Content -Path $playRecordPath -Value $lines -Encoding UTF8

Write-Output ('PlayRecordFile => ' + $playRecordPath)
Write-Output ('TotalCases => ' + $selectedCases.Count)
Write-Output ('MatchedCases => ' + $matchCount)