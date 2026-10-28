$ErrorActionPreference='Stop';$files=@(Get-ChildItem lang -Filter '*.lang');if($files.Count-lt 2){throw 'Missing language files'}
function Keys($p){@(Get-Content $p -Encoding utf8|ForEach-Object{$x=$_.Trim();if($x-and-not $x.StartsWith('#')-and-not $x.StartsWith(';')){$i=$x.IndexOf('=');if($i-lt 1){throw "Invalid line in $p"};$x.Substring(0,$i).Trim()}}|Sort-Object -Unique)}
$ref=Keys 'lang/en-US.lang';foreach($f in $files){$k=Keys $f.FullName;$missing=$ref|Where-Object{$_-notin $k};$extra=$k|Where-Object{$_-notin $ref};if($missing-or $extra){throw "$($f.Name) key mismatch. Missing: $missing Extra: $extra"}}
Write-Host "Validated $($files.Count) language files with $($ref.Count) keys."
