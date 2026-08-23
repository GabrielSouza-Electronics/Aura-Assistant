param(
    [string]$OutputWav = "startup_voice.wav"
)

$voice = New-Object -ComObject SAPI.SpVoice
$voices = $voice.GetVoices()
$david = $null

for ($index = 0; $index -lt $voices.Count; $index++) {
    $candidate = $voices.Item($index)
    if ($candidate.GetDescription() -like "Microsoft David*") {
        $david = $candidate
        break
    }
}

if ($null -eq $david) {
    throw "Microsoft David en-US voice is not installed."
}

$stream = New-Object -ComObject SAPI.SpFileStream
$stream.Open($OutputWav, 3, $false)

$voice.Voice = $david
$voice.Rate = -2
$voice.Volume = 82
$voice.AudioOutputStream = $stream
[void]$voice.Speak("Hello, welcome to Aura.")

$stream.Close()
