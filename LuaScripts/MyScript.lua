players = {}
players[0] = { Title = "Squire", Name = "Ciaran", Family = "Wirral", Level = 20 }
players[1] = { Title = "Lord", Name = "Diego", Family = "Brazil", Level = 50 }

function AddStuff(a, b)
	print("[LUA] AddStuff("..a..", "..b..") called \n")
	return a * b
end

function GetPlayer(n)
	return players[n]
end

-- function DoAThing(a, b)
	-- print("[LUA] DoAThing("..a..", "..b..") called \n")
	-- c = HostFunction(a + 10, b * 3)

function DoAThing()
	return 20, 0.1, 0.1 
end

coordinatesSize = { x = 10, y = 1, z = 1 }