param(
    [string[]]$ChangeNote = @('修改说明：未提供本轮具体修改说明。'),
    [string[]]$NameFilter = @(),
    [string[]]$CategoryFilter = @(),
    [int]$CaseLimit = 0
)

$workspacePath = if ([string]::IsNullOrWhiteSpace($PSScriptRoot)) { 'e:\vscode\数据结构项目' } else { $PSScriptRoot }
Set-Location $workspacePath

$exePath = Join-Path $workspacePath 'test.exe'
$bidRecordPath = Join-Path $workspacePath '叫分测试记录.txt'
$changeNotes = $ChangeNote

if (!(Test-Path $exePath)) {
    throw "Missing test executable: $exePath"
}

$rankNames = @('3','4','5','6','7','8','9','10','J','Q','K','A','2','joker','JOKER')

function Get-Level {
    param([int]$Card)
    return [int]([math]::Floor($Card / 4) + $(if ($Card -eq 53) { 1 } else { 0 }))
}

function Get-HandSummary {
    param([int[]]$Cards)

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

function Get-BidDebug {
    param(
        [int[]]$Cards,
        [int[]]$BidHistory
    )

    $counts = @(0) * 15
    foreach ($card in $Cards) {
        $counts[(Get-Level $card)] += 1
    }

    $maxBid = 0
    if ($BidHistory.Count -gt 0) {
        $maxBid = ($BidHistory | Measure-Object -Maximum).Maximum
    }

    $hasRocket = ($counts[13] -gt 0 -and $counts[14] -gt 0)
    $bombCount = 0
    for ($i = 0; $i -lt 13; $i++) {
        if ($counts[$i] -eq 4) {
            $bombCount += 1
        }
    }
    $twoCount = $counts[12]
    $aceCount = $counts[11]
    $jokerCount = $counts[13] + $counts[14]

    $hasBasicControl = $hasRocket -or $bombCount -gt 0 -or $jokerCount -gt 0 -or $twoCount -gt 0 -or $aceCount -ge 2
    $hasStrongControl = $hasRocket -or $bombCount -gt 0 -or $twoCount -ge 2 -or ($jokerCount -ge 1 -and $twoCount -ge 1) -or ($twoCount -ge 1 -and $aceCount -ge 2)
    $hasThreePointControl = $hasRocket -or $bombCount -ge 1 -or $twoCount -ge 3 -or ($jokerCount -ge 1 -and $twoCount -ge 2)

    $handCountApprox = 0
    for ($i = 0; $i -lt 15; $i++) {
        if ($counts[$i] -gt 0) {
            $handCountApprox += 1
        }
    }

    $bidCapApprox = 3
    if (!$hasBasicControl -and $bidCapApprox -gt 1) {
        $bidCapApprox = 1
    }
    if (!$hasStrongControl -and $bidCapApprox -gt 2) {
        $bidCapApprox = 2
    }
    if ($handCountApprox -ge 14 -and !$hasThreePointControl -and $bidCapApprox -gt 1) {
        $bidCapApprox = 1
    } elseif ($handCountApprox -ge 12 -and !$hasThreePointControl -and $bidCapApprox -gt 2) {
        $bidCapApprox = 2
    }
    if ($maxBid -eq 2 -and !$hasThreePointControl -and $bidCapApprox -gt 2) {
        $bidCapApprox = 2
    }

    return [pscustomobject]@{
        MaxBid = $maxBid
        HandCountApprox = $handCountApprox
        BidCapApprox = $bidCapApprox
        HasRocket = $hasRocket
        BombCount = $bombCount
        TwoCount = $twoCount
        AceCount = $aceCount
        JokerCount = $jokerCount
        HasBasicControl = $hasBasicControl
        HasStrongControl = $hasStrongControl
        HasThreePointControl = $hasThreePointControl
    }
}

function New-BidJson {
    param(
        [int[]]$Own,
        [int[]]$BidHistory
    )

    return ([ordered]@{
        requests = @(
            [ordered]@{
                own = $Own
                bid = $BidHistory
            }
        )
        responses = @()
    } | ConvertTo-Json -Compress -Depth 8)
}

function New-BidExpectedJson {
    param([int]$ExpectedBid)
    return ([ordered]@{ response = $ExpectedBid } | ConvertTo-Json -Compress)
}

function Add-BidCase {
    param(
        [System.Collections.Generic.List[object]]$Cases,
        [string]$Name,
        [string]$Category,
        [int[]]$Own,
        [int[]]$BidHistory,
        [int]$ExpectedBid,
        [string]$ExpectNote
    )

    $Cases.Add([pscustomobject]@{
        Name = $Name
        Category = $Category
        Own = $Own
        BidHistory = $BidHistory
        Input = New-BidJson -Own $Own -BidHistory $BidHistory
        Expected = New-BidExpectedJson -ExpectedBid $ExpectedBid
        ExpectNote = $ExpectNote
        HandSummary = Get-HandSummary -Cards $Own
        Debug = Get-BidDebug -Cards $Own -BidHistory $BidHistory
    })
}

$cases = New-Object System.Collections.Generic.List[object]

$bidHands = @(
    [pscustomobject]@{ Category = 'weak';      Name = 'WeakScatter1';         Cards = @(0,4,8,12,16,20,24,1,5,9,13,17,21,25,29,33,37);      NoHistory = 0; After1 = 0; After2 = 0; Note = '弱散牌，任何叫分历史下都不应主动抢地主。' },
    [pscustomobject]@{ Category = 'weak';      Name = 'WeakScatter2';         Cards = @(2,6,10,14,18,22,26,3,7,11,15,19,23,27,31,35,39);    NoHistory = 0; After1 = 0; After2 = 0; Note = '另一个弱散牌对照组，验证不会误激进。' },
    [pscustomobject]@{ Category = 'weak';      Name = 'WeakScatter3';         Cards = @(0,4,8,12,16,20,24,28,1,5,9,13,17,21,29,33,37);     NoHistory = 0; After1 = 0; After2 = 0; Note = '低位散牌更多的弱牌样例，验证程序不会被少量对子误判成可叫。' },
    [pscustomobject]@{ Category = 'weak';      Name = 'WeakScatter4';         Cards = @(0,4,8,12,16,20,24,28,32,1,5,9,13,17,21,25,44);    NoHistory = 0; After1 = 0; After2 = 0; Note = '带一张 A 的弱散牌，仍不应因为单张高牌而激进叫分。' },
    [pscustomobject]@{ Category = 'mid';       Name = 'MidControl1';          Cards = @(52,48,49,44,45,40,41,32,33,24,25,16,17,8,9,0,4);    NoHistory = 2; After1 = 0; After2 = 0; Note = '中档控制牌但高对子质量较好，最佳策略更偏向空场直接叫 2；别人已叫 1 后仍应收手。' },
    [pscustomobject]@{ Category = 'mid';       Name = 'MidControl2';          Cards = @(53,48,44,45,40,41,36,37,32,33,24,25,16,17,8,9,0);    NoHistory = 1; After1 = 0; After2 = 0; Note = '中档控制牌第二样本。' },
    [pscustomobject]@{ Category = 'mid';       Name = 'MidControl3';          Cards = @(52,48,49,44,45,40,32,33,24,25,20,21,16,17,8,9,0);    NoHistory = 1; After1 = 0; After2 = 0; Note = '中档控制牌第三样本，对子密度不错但缺少更硬的顶级控制，最佳策略下空场更像叫 1。' },
    [pscustomobject]@{ Category = 'mid';       Name = 'MidControl4';          Cards = @(53,48,44,45,40,41,36,32,33,24,25,16,17,12,13,8,9);    NoHistory = 1; After1 = 0; After2 = 0; Note = '中档控制牌第四样本，验证大王加若干对子时仍不会过度抢分。' },
    [pscustomobject]@{ Category = 'bomb-mid';  Name = 'BombHand1';            Cards = @(0,1,2,3,44,45,40,41,24,25,16,17,8,9,12,20,28);       NoHistory = 0; After1 = 0; After2 = 0; Note = '单炸但整体偏碎，不应被炸弹单独拉到激进叫分。' },
    [pscustomobject]@{ Category = 'bomb-mid';  Name = 'BombHand2';            Cards = @(24,25,26,27,44,45,48,49,32,33,16,17,8,9,0,4,12);     NoHistory = 2; After1 = 2; After2 = 0; Note = '有炸弹且 2/A 对子充足，最佳策略下空场和别人叫 1 时都可继续到 2。' },
    [pscustomobject]@{ Category = 'bomb-mid';  Name = 'BombHand3';            Cards = @(32,33,34,35,44,45,48,49,24,25,16,17,8,9,0,4,12);     NoHistory = 2; After1 = 2; After2 = 0; Note = 'J 炸配 2A 控牌，按最佳策略更接近空场 2 分、跟 1 也到 2。' },
    [pscustomobject]@{ Category = 'bomb-mid';  Name = 'BombHand4';            Cards = @(40,41,42,43,48,49,44,45,32,33,16,17,8,9,0,4,12);     NoHistory = 2; After1 = 2; After2 = 0; Note = 'K 炸加 2A 双对的强炸弹手牌，空场和别人叫 1 时继续叫 2 是合理的。' },
    [pscustomobject]@{ Category = 'threshold'; Name = 'ThresholdAggressive1'; Cards = @(52,53,44,45,40,41,32,33,24,25,16,17,8,9,0,4,12);     NoHistory = 2; After1 = 2; After2 = 0; Note = '边界强牌，验证别人叫 1 后还能继续跟到 2。' },
    [pscustomobject]@{ Category = 'threshold'; Name = 'ThresholdAggressive2'; Cards = @(52,48,49,44,45,40,41,36,37,32,33,24,25,16,17,8,9);   NoHistory = 2; After1 = 2; After2 = 0; Note = '另一手边界强牌，验证不是偶然样本。' },
    [pscustomobject]@{ Category = 'threshold'; Name = 'ThresholdAggressive3'; Cards = @(52,48,49,44,45,40,41,36,37,32,33,24,25,20,21,16,17); NoHistory = 2; After1 = 2; After2 = 0; Note = '高对子密度的边界强牌，验证降低惩罚后 After1 仍有继续叫 2 的空间。' },
    [pscustomobject]@{ Category = 'threshold'; Name = 'ThresholdAggressive4'; Cards = @(52,53,48,44,45,40,41,36,37,32,33,24,25,16,17,8,9);   NoHistory = 3; After1 = 3; After2 = 3; Note = '双王加多组高对，这手已经跨过边界线，直接归入强牌型输出。' },
    [pscustomobject]@{ Category = 'strong';    Name = 'StrongControl1';       Cards = @(52,53,48,49,50,51,44,45,46,40,41,32,33,24,25,16,17); NoHistory = 3; After1 = 3; After2 = 3; Note = '强控牌，无论前面叫到几分都应敢于顶满。' },
    [pscustomobject]@{ Category = 'strong';    Name = 'StrongControl2';       Cards = @(52,53,44,45,46,47,48,49,40,41,42,32,33,24,25,16,17); NoHistory = 3; After1 = 3; After2 = 3; Note = '第二手强控牌，验证强牌门槛稳定。' },
    [pscustomobject]@{ Category = 'strong';    Name = 'StrongControl3';       Cards = @(52,53,48,49,50,51,44,45,46,47,40,41,32,33,24,25,16); NoHistory = 3; After1 = 3; After2 = 3; Note = '火箭、四个 2、双 A 的超强地主手。' },
    [pscustomobject]@{ Category = 'strong';    Name = 'StrongControl4';       Cards = @(52,53,48,49,50,44,45,46,40,41,42,32,33,24,25,16,17); NoHistory = 3; After1 = 3; After2 = 3; Note = '双王加三张 2 和多组高对，属于稳定顶满的强牌。' }
)

$bidHistoryCases = @(
    [pscustomobject]@{ Suffix = 'NoHistory'; History = @();    ExpectKey = 'NoHistory'; HistoryNote = '无人叫分。' },
    [pscustomobject]@{ Suffix = 'After1';    History = @(1);   ExpectKey = 'After1';    HistoryNote = '前面最高叫到 1。' },
    [pscustomobject]@{ Suffix = 'After2';    History = @(2);   ExpectKey = 'After2';    HistoryNote = '前面最高叫到 2。' },
    [pscustomobject]@{ Suffix = 'After10';   History = @(1,0); ExpectKey = 'After1';    HistoryNote = '两人历史但最高仍是 1，验证不依赖历史长度。' },
    [pscustomobject]@{ Suffix = 'After12';   History = @(1,2); ExpectKey = 'After2';    HistoryNote = '两人历史且最高是 2，验证只看最高叫分。' }
)

foreach ($handCase in $bidHands) {
    foreach ($historyCase in $bidHistoryCases) {
        $expectedBid = $handCase.($historyCase.ExpectKey)
        Add-BidCase `
            -Cases $cases `
            -Name ("B_{0}_{1}" -f $handCase.Name, $historyCase.Suffix) `
            -Category $handCase.Category `
            -Own $handCase.Cards `
            -BidHistory $historyCase.History `
            -ExpectedBid $expectedBid `
            -ExpectNote ($historyCase.HistoryNote + $handCase.Note)
    }
}

$selectedCases = @($cases.ToArray())
if ($NameFilter.Count -gt 0) {
    $selectedCases = @($selectedCases | Where-Object { $NameFilter -contains $_.Name.Split('_')[1] })
}
if ($CategoryFilter.Count -gt 0) {
    $selectedCases = @($selectedCases | Where-Object { $CategoryFilter -contains $_.Category })
}
if ($CaseLimit -gt 0) {
    $selectedCases = @($selectedCases | Select-Object -First $CaseLimit)
}
if ($selectedCases.Count -eq 0) {
    throw 'No bid cases selected. Check CategoryFilter or CaseLimit.'
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('')
$lines.Add('')
$lines.Add('================ 叫分回归测试 ' + $selectedCases.Count + ' 组 ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' ================')
$lines.AddRange([string[]]$changeNotes)
$lines.Add('用途：固定叫分行为，辅助调参；MATCH 只表示符合当前设计预期，不表示策略最优。')
$lines.Add('样例结构：当前按筛选后实际运行 ' + $selectedCases.Count + ' 组。After10 / After12 用来验证只看最高叫分。')
$lines.Add('NAME_FILTER: ' + $(if ($NameFilter.Count -gt 0) { $NameFilter -join ', ' } else { 'ALL' }))
$lines.Add('CATEGORY_FILTER: ' + $(if ($CategoryFilter.Count -gt 0) { $CategoryFilter -join ', ' } else { 'ALL' }))
$lines.Add('CASE_LIMIT: ' + $(if ($CaseLimit -gt 0) { $CaseLimit } else { 'ALL' }))
$lines.Add('DEBUG 说明：为 PowerShell 复刻的近似特征，不等价于 C++ 内部 bidScore，但便于快速定位硬条件。')
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

    $debug = $case.Debug

    $lines.Add('=== ' + $case.Name + ' ===')
    $lines.Add('CATEGORY: ' + $case.Category)
    $lines.Add('HAND: ' + $case.HandSummary)
    $lines.Add('EXPECT_NOTE: ' + $case.ExpectNote)
    $lines.Add('DEBUG: maxBid=' + $debug.MaxBid +
        ' handCountApprox=' + $debug.HandCountApprox +
        ' bidCapApprox=' + $debug.BidCapApprox +
        ' rocket=' + $debug.HasRocket +
        ' bombs=' + $debug.BombCount +
        ' twos=' + $debug.TwoCount +
        ' aces=' + $debug.AceCount +
        ' jokers=' + $debug.JokerCount +
        ' basicCtrl=' + $debug.HasBasicControl +
        ' strongCtrl=' + $debug.HasStrongControl +
        ' threeCtrl=' + $debug.HasThreePointControl)
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
$lines.Add('失败用法建议：先看 CATEGORY 和 DEBUG，判断是期望写错、硬条件过强，还是阈值/扣分需要调整。')

Add-Content -Path $bidRecordPath -Value $lines -Encoding UTF8

Write-Output ('BidRecordFile => ' + $bidRecordPath)
Write-Output ('TotalCases => ' + $selectedCases.Count)
Write-Output ('MatchedCases => ' + $matchCount)

