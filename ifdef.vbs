Set argv  = WScript.Arguments
Set stdin = WScript.stdin
printFlag = true
do Until stdin.AtEndOfStream
    line = stdin.ReadLine
    if left(line,3) = "---" then
	if len(line) <= 4  then
	    printFlag = true
	else
	    printFlag = false
	    for each e in split(line)
		for i=0 to argv.count-1
		    if argv(i) = e then
			printFlag = true
			exit for
		    end if
		next
	    next
	end if
    elseif printFlag then
	WScript.Echo line
    end if
Loop
Set stdin = nothing
Set argv  = nothing
