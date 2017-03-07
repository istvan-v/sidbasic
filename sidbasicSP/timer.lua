
clearBreakPoints()
prvXPos, y = getVideoPosition()
timerOn = false
cnt = 0
videoClockFreq = 17734475 / 2.5
videoCyclesPerLine = 456
startAddr = 0xb9a3
endAddr = 0xb9a2

function breakPointCallback(t, a, v)
  if (t == 0 or t == 3) and a == startAddr and not timerOn then
    prvXPos, y = getVideoPosition()
    timerOn = true
    cnt = 0
    return false
  end
  if timerOn then
    x, y = getVideoPosition()
    cnt = cnt + (x - prvXPos)
    if x < prvXPos then
      cnt = cnt + videoCyclesPerLine
    end
    prvXPos = x
    if (t == 0 or t == 3) and a == endAddr then
      timerOn = false
      mprint(string.format("%.1f cycles", cnt / 2 + 4))
      clearBreakPoints()
      return true
    end
  end
  return false
end

